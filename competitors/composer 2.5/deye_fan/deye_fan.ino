/*
 * deye_fan.ino — Simulateur de signal tachymétrique pour onduleur Deye
 *
 * Carte : LOLIN(WEMOS) D1 mini (ESP8266)
 *
 * Fonction :
 *   Lit le tach d'un ventilateur Noctua 9 cm et d'un 6 cm, applique un ratio
 *   multiplicateur configurable par canal, et réémet un signal tach open-collector
 *   vers l'onduleur Deye SUN-8K-SG05LP1-EU-AM2-P.
 *
 * Architecture logicielle :
 *   - Timer1 matériel (10 µs) : génération des sorties tach — totalement isolé
 *     du WiFi, du serveur web et de la boucle principale.
 *   - ISR GPIO (IRAM) : mesure des périodes d'entrée — mise à jour atomique
 *     des demi-périodes cibles pour le timer.
 *   - Boucle principale : WiFi, serveur HTTP, LED d'état — n'intervient jamais
 *     dans la génération des impulsions.
 *
 * Formule RPM : RPM = fréquence_Hz × 60 / 2  (2 impulsions par tour)
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

// ─── Brochage Wemos D1 mini ───────────────────────────────────────────────────
// Canal 0 : ventilateur 9 cm (NF-A9-FLX)
#define PIN_TACH_IN_0   4   // D2 — entrée tach Noctua 9 cm
#define PIN_TACH_OUT_0  12  // D6 — sortie open-collector vers Deye 9 cm

// Canal 1 : ventilateur 6 cm (NF-A6x25-FLX)
#define PIN_TACH_IN_1   5   // D1 — entrée tach Noctua 6 cm
#define PIN_TACH_OUT_1  13  // D7 — sortie open-collector vers Deye 6 cm

#define PIN_LED_STATUS  2   // D4 — LED intégrée (active LOW sur Wemos)

// ─── Constantes tachymètre ────────────────────────────────────────────────────
#define PULSES_PER_REV      2       // Standard ventilateurs PC / Noctua
#define TIMER_TICK_US       10      // Résolution du Timer1 (µs)
#define INPUT_TIMEOUT_US    3000000UL // 3 s sans impulsion → arrêt simulation
#define MIN_PERIOD_US       500UL   // Période min valide (~72000 RPM simulés)
#define MAX_PERIOD_US       600000UL // Période max valide (~200 RPM)

// Limites des ratios configurables
#define RATIO_MIN           1.0f
#define RATIO_MAX           6.0f
#define RATIO_DEFAULT       2.5f

// ─── Configuration WiFi persistante (EEPROM) ────────────────────────────────
#define EEPROM_SIZE         512
#define EEPROM_MAGIC        0xDEYE
#define AP_SSID_DEFAULT     "DeyeFanSim"
#define AP_PASS_DEFAULT     "deye1234"
#define AP_CHANNEL          6
#define AP_MAX_CONN         4

// ─── Structures ─────────────────────────────────────────────────────────────

// État d'un canal tach (partagé ISR ↔ boucle principale)
struct TachChannel {
  volatile uint32_t lastEdgeUs;       // Horodatage dernière impulsion entrée
  volatile uint32_t inputPeriodUs;  // Période mesurée (µs)
  volatile uint32_t halfPeriodTicks; // Demi-période sortie en ticks Timer1
  volatile uint32_t pulseCount;     // Compteur impulsions entrée (affichage)
  volatile bool     active;         // Signal d'entrée récent détecté
  volatile uint8_t  outPhase;       // 0 = ligne haute, 1 = ligne basse (ISR timer)
  volatile uint32_t tickCounter;    // Compteur ticks dans la demi-période courante
  float             ratio;          // Multiplicateur RPM (modifié via web, lu par ISR)
  uint8_t           outPin;         // Broche sortie transistor
};

TachChannel channels[2];

// Configuration persistante
struct Config {
  uint16_t magic;
  char     staSsid[33];
  char     staPass[65];
  float    ratio0;
  float    ratio1;
  uint16_t crc;
};

Config config;
ESP8266WebServer server(80);

// Timer1
volatile bool timerRunning = false;

// LED : état global (lu dans loop, pas dans ISR critique)
enum LedMode { LED_WAITING, LED_ACTIVE, LED_WIFI_SETUP };
volatile LedMode ledMode = LED_WAITING;
uint32_t ledLastToggle = 0;
bool ledState = false;

// ─── CRC16 simple pour validation EEPROM ────────────────────────────────────
uint16_t calcCrc(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++)
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
  }
  return crc;
}

// ─── Accès GPIO rapide (IRAM) pour le Timer1 ────────────────────────────────
// Sortie open-collector via NPN : HIGH = transistor ON = ligne tirée à GND
//                                  LOW  = transistor OFF = ligne flottante (pull-up Deye)

static inline void IRAM_ATTR gpioOutHigh(uint8_t pin) {
  if (pin < 16)
    GPIO_REG_WRITE(GPIO_OUT_ADDRESS, GPIO_REG_READ(GPIO_OUT_ADDRESS) | (1 << pin));
}

static inline void IRAM_ATTR gpioOutLow(uint8_t pin) {
  if (pin < 16)
    GPIO_REG_WRITE(GPIO_OUT_ADDRESS, GPIO_REG_READ(GPIO_OUT_ADDRESS) & ~(1 << pin));
}

// ─── ISR Timer1 : génération des signaux tach de sortie ─────────────────────
// Cette ISR est le SEUL endroit qui pilote les broches de sortie tach.
// Elle tourne à 100 kHz (toutes les 10 µs) et ne fait aucun appel bloquant.

void IRAM_ATTR onTimer1() {
  for (uint8_t ch = 0; ch < 2; ch++) {
    TachChannel* c = &channels[ch];

    if (!c->active || c->halfPeriodTicks == 0) {
      // Pas de signal d'entrée : relâcher la ligne (transistor OFF)
      gpioOutLow(c->outPin);
      c->outPhase = 0;
      c->tickCounter = 0;
      continue;
    }

    c->tickCounter++;
    if (c->tickCounter >= c->halfPeriodTicks) {
      c->tickCounter = 0;
      c->outPhase ^= 1;
      if (c->outPhase)
        gpioOutHigh(c->outPin);  // Tirer à GND
      else
        gpioOutLow(c->outPin);   // Relâcher
    }
  }
}

// ─── ISR GPIO : mesure de période sur les entrées tach ──────────────────────
// Déclenchée sur front descendant (ventilateur tire la ligne à GND).

void IRAM_ATTR handleTachEdge(uint8_t ch) {
  TachChannel* c = &channels[ch];
  uint32_t now = micros();
  uint32_t period = now - c->lastEdgeUs;
  c->lastEdgeUs = now;

  // Filtrer les rebonds et les périodes hors plage
  if (period < MIN_PERIOD_US || period > MAX_PERIOD_US)
    return;

  c->inputPeriodUs = period;
  c->pulseCount++;

  // Calculer la demi-période de sortie : T_out = T_in / ratio
  // halfPeriodTicks = (T_out / 2) / TIMER_TICK_US
  float ratio = c->ratio;
  if (ratio < RATIO_MIN) ratio = RATIO_MIN;
  if (ratio > RATIO_MAX) ratio = RATIO_MAX;

  uint32_t outPeriodUs = (uint32_t)(period / ratio);
  uint32_t halfUs = outPeriodUs / 2;
  uint32_t ticks = halfUs / TIMER_TICK_US;
  if (ticks < 1) ticks = 1;
  c->halfPeriodTicks = ticks;
  c->active = true;
}

void IRAM_ATTR onTachEdge0() { handleTachEdge(0); }
void IRAM_ATTR onTachEdge1() { handleTachEdge(1); }

// ─── Conversion période → RPM ─────────────────────────────────────────────────
uint32_t periodToRpm(uint32_t periodUs) {
  if (periodUs == 0) return 0;
  // RPM = (1e6 / periodUs) * 60 / PULSES_PER_REV
  return (uint32_t)(60000000UL / (periodUs * PULSES_PER_REV));
}

// ─── Gestion LED (boucle principale uniquement) ─────────────────────────────
void updateLed() {
  uint32_t now = millis();
  uint32_t interval;

  switch (ledMode) {
    case LED_ACTIVE:
      // LED fixe allumée quand au moins un canal simule
      digitalWrite(PIN_LED_STATUS, LOW);  // active LOW
      return;
    case LED_WIFI_SETUP:
      interval = 100;  // Clignotement rapide = mode AP config
      break;
    default:
      interval = 1000; // Clignotement lent = en attente de signal tach
      break;
  }

  if (now - ledLastToggle >= interval) {
    ledLastToggle = now;
    ledState = !ledState;
    digitalWrite(PIN_LED_STATUS, ledState ? LOW : HIGH);
  }
}

// ─── Vérification timeout des entrées (boucle principale) ───────────────────
void checkInputTimeouts() {
  uint32_t now = micros();
  bool anyActive = false;

  for (uint8_t ch = 0; ch < 2; ch++) {
    TachChannel* c = &channels[ch];
    if (c->active && (now - c->lastEdgeUs) > INPUT_TIMEOUT_US) {
      c->active = false;
      c->halfPeriodTicks = 0;
      c->inputPeriodUs = 0;
    }
    if (c->active) anyActive = true;
  }

  ledMode = anyActive ? LED_ACTIVE : LED_WAITING;
}

// ─── EEPROM : chargement / sauvegarde ────────────────────────────────────────
void loadConfig() {
  EEPROM.get(0, config);
  uint16_t stored = config.crc;
  config.crc = 0;
  uint16_t computed = calcCrc((uint8_t*)&config, sizeof(Config) - 2);
  config.crc = stored;

  if (config.magic != EEPROM_MAGIC || stored != computed) {
    // Valeurs par défaut
    config.magic = EEPROM_MAGIC;
    config.staSsid[0] = '\0';
    config.staPass[0] = '\0';
    config.ratio0 = RATIO_DEFAULT;
    config.ratio1 = RATIO_DEFAULT;
    saveConfig();
  }

  channels[0].ratio = config.ratio0;
  channels[1].ratio = config.ratio1;
}

void saveConfig() {
  config.crc = 0;
  config.crc = calcCrc((uint8_t*)&config, sizeof(Config) - 2);
  EEPROM.put(0, config);
  EEPROM.commit();
}

// ─── WiFi : mode AP + STA simultané ─────────────────────────────────────────
void setupWiFi() {
  WiFi.persistent(false);
  WiFi.setSleepMode(WIFI_NONE);  // Réduit le jitter WiFi sur le CPU
  WiFi.mode(WIFI_AP_STA);

  // Point d'accès permanent pour configuration
  WiFi.softAP(AP_SSID_DEFAULT, AP_PASS_DEFAULT, AP_CHANNEL, false, AP_MAX_CONN);
  IPAddress apIp(192, 168, 4, 1);
  WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0));

  // Connexion STA si credentials sauvegardés
  if (strlen(config.staSsid) > 0) {
    WiFi.begin(config.staSsid, config.staPass);
    // Non bloquant : la connexion se fait en arrière-plan
  }
}

// ─── Serveur web ──────────────────────────────────────────────────────────────

String htmlPage() {
  uint32_t rpmIn0  = periodToRpm(channels[0].inputPeriodUs);
  uint32_t rpmOut0 = (uint32_t)(rpmIn0 * channels[0].ratio);
  uint32_t rpmIn1  = periodToRpm(channels[1].inputPeriodUs);
  uint32_t rpmOut1 = (uint32_t)(rpmIn1 * channels[1].ratio);

  String s;
  s  = F("<!DOCTYPE html><html><head>");
  s += F("<meta charset='utf-8'>");
  s += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  s += F("<meta http-equiv='refresh' content='2'>");
  s += F("<title>Deye Fan Simulator</title>");
  s += F("<style>");
  s += F("body{font-family:sans-serif;max-width:600px;margin:1em auto;padding:0 1em}");
  s += F("h1{color:#e67e00}table{width:100%;border-collapse:collapse;margin:1em 0}");
  s += F("td,th{border:1px solid #ccc;padding:8px;text-align:center}");
  s += F("th{background:#f5f5f5}.active{color:green;font-weight:bold}");
  s += F(".waiting{color:#999}input[type=number]{width:80px}input[type=text],input[type=password]{width:200px}");
  s += F("</style></head><body>");
  s += F("<h1>Deye Fan Tach Simulator</h1>");

  // Tableau RPM temps réel
  s += F("<h2>RPM en temps réel</h2><table>");
  s += F("<tr><th>Canal</th><th>Ventilateur</th><th>RPM lu</th><th>Ratio</th><th>RPM simulé</th><th>État</th></tr>");

  s += F("<tr><td>0</td><td>9 cm (NF-A9)</td><td>"); s += rpmIn0;
  s += F("</td><td>"); s += String(channels[0].ratio, 2);
  s += F("</td><td>"); s += rpmOut0;
  s += F("</td><td class='"); s += channels[0].active ? "active'>Actif" : "waiting'>En attente";
  s += F("</td></tr>");

  s += F("<tr><td>1</td><td>6 cm (NF-A6)</td><td>"); s += rpmIn1;
  s += F("</td><td>"); s += String(channels[1].ratio, 2);
  s += F("</td><td>"); s += rpmOut1;
  s += F("</td><td class='"); s += channels[1].active ? "active'>Actif" : "waiting'>En attente";
  s += F("</td></tr></table>");

  // Formulaire ratios
  s += F("<h2>Configuration des ratios</h2>");
  s += F("<form method='POST' action='/config'>");
  s += F("<label>Ratio canal 0 (9 cm) : <input type='number' name='ratio0' step='0.1' min='");
  s += RATIO_MIN; s += F("' max='"); s += RATIO_MAX;
  s += F("' value='"); s += String(channels[0].ratio, 1); s += F("'></label><br><br>");
  s += F("<label>Ratio canal 1 (6 cm) : <input type='number' name='ratio1' step='0.1' min='");
  s += RATIO_MIN; s += F("' max='"); s += RATIO_MAX;
  s += F("' value='"); s += String(channels[1].ratio, 1); s += F("'></label><br><br>");
  s += F("<input type='submit' value='Enregistrer les ratios'></form>");

  // Formulaire WiFi
  s += F("<h2>Configuration WiFi (STA)</h2>");
  s += F("<form method='POST' action='/wifi'>");
  s += F("<label>SSID : <input type='text' name='ssid' value='");
  s += config.staSsid;
  s += F("'></label><br><br>");
  s += F("<label>Mot de passe : <input type='password' name='pass' value='");
  s += config.staPass;
  s += F("'></label><br><br>");
  s += F("<input type='submit' value='Enregistrer et reconnecter'></form>");

  // Infos réseau
  s += F("<h2>Réseau</h2><p>AP : <b>"); s += AP_SSID_DEFAULT;
  s += F("</b> ("); s += WiFi.softAPIP().toString();
  s += F(")<br>STA : ");
  if (WiFi.status() == WL_CONNECTED) {
    s += F("<b>"); s += WiFi.SSID();
    s += F("</b> — "); s += WiFi.localIP().toString();
  } else {
    s += F("<i>non connecté</i>");
  }
  s += F("</p>");

  s += F("</body></html>");
  return s;
}

void handleRoot() {
  server.send(200, F("text/html; charset=utf-8"), htmlPage());
}

void handleConfig() {
  if (server.hasArg("ratio0")) {
    float r0 = server.arg("ratio0").toFloat();
    if (r0 >= RATIO_MIN && r0 <= RATIO_MAX) {
      channels[0].ratio = r0;
      config.ratio0 = r0;
    }
  }
  if (server.hasArg("ratio1")) {
    float r1 = server.arg("ratio1").toFloat();
    if (r1 >= RATIO_MIN && r1 <= RATIO_MAX) {
      channels[1].ratio = r1;
      config.ratio1 = r1;
    }
  }
  saveConfig();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleWifi() {
  if (server.hasArg("ssid")) {
    strncpy(config.staSsid, server.arg("ssid").c_str(), 32);
    config.staSsid[32] = '\0';
  }
  if (server.hasArg("pass")) {
    strncpy(config.staPass, server.arg("pass").c_str(), 64);
    config.staPass[64] = '\0';
  }
  saveConfig();

  WiFi.disconnect();
  if (strlen(config.staSsid) > 0)
    WiFi.begin(config.staSsid, config.staPass);

  server.sendHeader("Location", "/");
  server.send(303);
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/config", HTTP_POST, handleConfig);
  server.on("/wifi", HTTP_POST, handleWifi);
  server.begin();
}

// ─── Initialisation Timer1 ────────────────────────────────────────────────────
void setupTimer1() {
  timer1_attachInterrupt(onTimer1);
  // timer1_write() prend des microsecondes : interruption toutes les 10 µs
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
  timer1_write(TIMER_TICK_US);
  timerRunning = true;
}

// ─── setup() ─────────────────────────────────────────────────────────────────
void setup() {
  // Initialiser les canaux
  channels[0] = {0, 0, 0, 0, false, 0, 0, RATIO_DEFAULT, PIN_TACH_OUT_0};
  channels[1] = {0, 0, 0, 0, false, 0, 0, RATIO_DEFAULT, PIN_TACH_OUT_1};

  pinMode(PIN_LED_STATUS, OUTPUT);
  digitalWrite(PIN_LED_STATUS, HIGH);  // LED éteinte (active LOW)

  // Broches de sortie : repos à LOW (transistor coupé, ligne flottante)
  pinMode(PIN_TACH_OUT_0, OUTPUT);
  pinMode(PIN_TACH_OUT_1, OUTPUT);
  digitalWrite(PIN_TACH_OUT_0, LOW);
  digitalWrite(PIN_TACH_OUT_1, LOW);

  // Broches d'entrée : pull-up interne + résistance externe 4.7 kΩ recommandée
  pinMode(PIN_TACH_IN_0, INPUT);
  pinMode(PIN_TACH_IN_1, INPUT);

  // Interruptions sur front descendant (ventilateur tire à GND)
  attachInterrupt(digitalPinToInterrupt(PIN_TACH_IN_0), onTachEdge0, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_TACH_IN_1), onTachEdge1, FALLING);

  // Charger config EEPROM
  EEPROM.begin(EEPROM_SIZE);
  loadConfig();

  // Démarrer Timer1 AVANT le WiFi pour garantir l'isolation temporelle
  setupTimer1();

  // WiFi et serveur web
  setupWiFi();
  setupWebServer();

  ledMode = LED_WIFI_SETUP;
}

// ─── loop() ──────────────────────────────────────────────────────────────────
// La boucle principale ne touche JAMAIS aux broches tach de sortie.
void loop() {
  server.handleClient();
  checkInputTimeouts();
  updateLed();

  // Yield court pour le stack TCP/IP sans bloquer longtemps
  yield();
}
