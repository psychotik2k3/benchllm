/*
 ============================================================================
  deye_fan.ino — Simulateur de signal tachymétrique (RPM) pour onduleur Deye
 ============================================================================
  Cible      : LOLIN(WEMOS) D1 mini V2.3.0 (ESP-12S / ESP8266)
  IDE        : Arduino IDE, carte "LOLIN(WEMOS) D1 R2 & mini"
  Dépendances: aucune librairie externe (uniquement le core ESP8266 >= 3.x)

  FONCTION
  --------
  - Lit le signal tach (2 impulsions/tour, collecteur ouvert) de 2 ventilateurs
    Noctua : canal 1 = NF-A9-FLX (9 cm), canal 2 = NF-A6x25-FLX (6 cm).
  - Multiplie la fréquence lue par un ratio configurable PAR CANAL et régénère
    un signal tach simulé vers l'onduleur Deye (sortie collecteur ouvert NPN,
    compatible 3,3 V / 5 V / 12 V sans modification matérielle).
  - Interface web (AP + STA simultanés) : réglage des ratios SANS recompiler,
    configuration WiFi STA, affichage temps réel des RPM lus et simulés.
  - Configuration persistée en EEPROM (survit aux redémarrages).
  - LED intégrée : allumée fixe = simulation active ; clignote = attente signal.

  ARCHITECTURE LOGICIELLE — ISOLATION TEMPS RÉEL / WIFI
  -----------------------------------------------------
  L'ESP8266 est mono-cœur : le WiFi masque brièvement les interruptions.
  Pour garantir un signal de sortie stable et sans jitter mesurable :
    1. La GÉNÉRATION du signal de sortie est confiée au TIMER MATÉRIEL n°1
       (timer1, haute priorité, compteur 23 bits). L'ISR du timer ne fait que
       basculer les GPIO (accès directs GPOS/GPOC) et reprogrammer la prochaine
       échéance : quelques microsecondes, jamais de malloc, jamais de String.
       Même pendant une requête HTTP ou un burst WiFi, le timer matériel
       continue de cadencer les fronts ; la latence ISR résiduelle (< ~10 µs)
       est négligeable devant les demi-périodes tach (>= ~300 µs).
    2. La MESURE d'entrée est faite par ISR sur front descendant (IRAM),
       horodatage micros() + filtre exponentiel entier. Aucun traitement
       lourd dans l'ISR.
    3. Tout le reste (serveur web, WiFi, calculs de RPM, EEPROM, LED) tourne
       dans loop() et ne touche jamais aux GPIO de sortie : il se contente
       d'écrire périodiquement une demi-période cible (écriture 32 bits
       atomique) que l'ISR du timer consomme.
    4. Toutes les ISR sont en IRAM (IRAM_ATTR) : aucun accès flash possible
       pendant le WiFi ou une écriture EEPROM.

  BROCHAGE
  --------
    D5 (GPIO14) = entrée tach canal 1 (Noctua 9 cm)
    D6 (GPIO12) = entrée tach canal 2 (Noctua 6 cm)
    D1 (GPIO5)  = sortie tach simulée canal 1 -> base NPN -> Deye
    D2 (GPIO4)  = sortie tach simulée canal 2 -> base NPN -> Deye
    D4 (GPIO2)  = LED intégrée (active à l'état bas)
  (D5/D6/D1/D2 sont sans effet sur le boot ; D0/D8 évités volontairement.)

  UTILISATION
  -----------
  1. Au 1er démarrage, se connecter au WiFi "DeyeFanSim" (mdp: deyefan123).
  2. Ouvrir http://192.168.4.1  (ou http://deyefan.local si STA connecté).
  3. Régler les ratios (ex. 2.5) et éventuellement les identifiants WiFi STA.
  4. Les réglages sont sauvegardés en EEPROM immédiatement.
 ============================================================================
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>

// ============================ PARAMÈTRES ===================================
#define AP_SSID       "DeyeFanSim"
#define AP_PASSWORD   "deyefan123"        // 8 caractères minimum (WPA2)
#define EEPROM_SIZE   512
#define CONFIG_MAGIC  0xDEFA0123u

// ============================ BROCHAGE =====================================
constexpr uint8_t PIN_TACH_IN_1  = 14;    // D5 — entrée tach canal 1 (9 cm)
constexpr uint8_t PIN_TACH_IN_2  = 12;    // D6 — entrée tach canal 2 (6 cm)
constexpr uint8_t PIN_TACH_OUT_1 = 5;     // D1 — sortie simulée canal 1
constexpr uint8_t PIN_TACH_OUT_2 = 4;     // D2 — sortie simulée canal 2
constexpr uint8_t PIN_LED        = 2;     // D4 — LED intégrée, active bas

constexpr uint8_t  PULSES_PER_REV = 2;        // Noctua : 2 impulsions / tour
constexpr uint32_t MIN_PULSE_US   = 300;      // anti-rebond / anti-parasite
constexpr uint32_t TIMEOUT_US     = 1500000;  // silence > 1,5 s => 0 RPM
constexpr float    RATIO_MIN      = 0.5f;
constexpr float    RATIO_MAX      = 10.0f;

// ============================ ÉTAT CANAL ===================================
struct Channel {
  // --- mesure d'entrée (écrit par ISR, lu par loop) ---
  volatile uint32_t lastPulseUs  = 0;   // horodatage de la dernière impulsion
  volatile uint32_t emaPeriodUs  = 0;   // période filtrée (moyenne expo 1/4)
  // --- génération de sortie (écrit par loop, lu par ISR timer) ---
  volatile uint32_t halfPeriodUs = 0;   // demi-période cible (0 = repos)
  volatile uint32_t nextToggleUs = 0;   // prochaine échéance de bascule
  volatile bool     outLevel     = false;
  // --- état applicatif ---
  float ratio = 2.0f;
  float rpmIn = 0.0f;
};
Channel ch[2];

// ============================ CONFIG PERSISTANTE ===========================
struct Config {
  uint32_t magic;
  float    ratio1;
  float    ratio2;
  char     ssid[33];
  char     pass[65];
  uint16_t crc;
};
Config cfg;

static uint16_t crc16(const uint8_t* data, size_t len) {
  uint16_t c = 0xFFFF;
  while (len--) {
    c ^= *data++;
    for (uint8_t k = 0; k < 8; k++) c = (c & 1) ? (c >> 1) ^ 0xA001 : c >> 1;
  }
  return c;
}

static void configDefaults() {
  memset(&cfg, 0, sizeof(cfg));
  cfg.magic  = CONFIG_MAGIC;
  cfg.ratio1 = 2.0f;
  cfg.ratio2 = 2.0f;
  cfg.ssid[0] = '\0';
  cfg.pass[0] = '\0';
}

static void loadConfig() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, cfg);
  uint16_t crc = cfg.crc;
  if (cfg.magic != CONFIG_MAGIC ||
      crc != crc16((const uint8_t*)&cfg, sizeof(Config) - sizeof(uint16_t)) ||
      cfg.ratio1 < RATIO_MIN || cfg.ratio1 > RATIO_MAX ||
      cfg.ratio2 < RATIO_MIN || cfg.ratio2 > RATIO_MAX) {
    configDefaults();
  }
  cfg.ssid[32] = '\0';
  cfg.pass[64] = '\0';
  ch[0].ratio = cfg.ratio1;
  ch[1].ratio = cfg.ratio2;
}

static void saveConfig() {
  cfg.magic   = CONFIG_MAGIC;
  cfg.ratio1  = ch[0].ratio;
  cfg.ratio2  = ch[1].ratio;
  cfg.crc     = crc16((const uint8_t*)&cfg, sizeof(Config) - sizeof(uint16_t));
  EEPROM.put(0, cfg);
  EEPROM.commit();
}

// ============================ ISR — MESURE D'ENTRÉE ========================
// Front descendant sur la ligne tach (collecteur ouvert + pull-up 3,3 V).
// Travail minimal : horodatage + filtre exponentiel entier (gain 1/4).
static inline void IRAM_ATTR onPulse(uint8_t i) {
  uint32_t now = micros();
  uint32_t dt  = now - ch[i].lastPulseUs;
  if (dt < MIN_PULSE_US) return;                 // parasite / rebond
  ch[i].lastPulseUs = now;
  uint32_t ema = ch[i].emaPeriodUs;
  ch[i].emaPeriodUs = (ema == 0) ? dt : ((ema * 3u) + dt) >> 2;
}
void IRAM_ATTR isrTach1() { onPulse(0); }
void IRAM_ATTR isrTach2() { onPulse(1); }

// ============================ ISR — GÉNÉRATION DE SORTIE ===================
// Timer matériel n°1 (TIM_DIV16 => 5 ticks/µs, compteur 23 bits, one-shot).
// À chaque échéance : bascule le(s) GPIO concerné(s) puis reprogramme le
// timer pour la prochaine échéance la plus proche des deux canaux.
// Logique collecteur ouvert : GPIO haut = transistor ON = ligne tach BASSE.
void IRAM_ATTR isrTimer() {
  uint32_t now  = micros();
  uint32_t next = now + 1000000u;                // par défaut : dans 1 s

  for (uint8_t i = 0; i < 2; i++) {
    uint8_t  pin  = (i == 0) ? PIN_TACH_OUT_1 : PIN_TACH_OUT_2;
    uint32_t half = ch[i].halfPeriodUs;

    if (half == 0) {                             // canal au repos
      if (ch[i].outLevel) { GPOC = (1u << pin); ch[i].outLevel = false; }
      continue;
    }
    if ((int32_t)(now - ch[i].nextToggleUs) >= 0) {
      bool lvl = !ch[i].outLevel;
      if (lvl) GPOS = (1u << pin); else GPOC = (1u << pin);
      ch[i].outLevel = lvl;
      ch[i].nextToggleUs += half;
      // Resynchronisation si l'échéance a été ratée (changement de ratio...)
      if ((int32_t)(now - ch[i].nextToggleUs) >= (int32_t)half)
        ch[i].nextToggleUs = now + half;
    }
    if ((int32_t)(ch[i].nextToggleUs - next) < 0) next = ch[i].nextToggleUs;
  }

  uint32_t d = next - now;
  if (d < 30u)       d = 30u;                    // plancher anti-famine
  if (d > 1000000u)  d = 1000000u;               // 5 000 000 ticks < 2^23
  timer1_write(d * 5u);                          // réarmement one-shot
}

// ============================ SERVEUR WEB ==================================
ESP8266WebServer server(80);

static const char PAGE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Deye Fan Simulator</title>
<style>
body{font-family:sans-serif;background:#12151c;color:#e6e6e6;max-width:640px;margin:auto;padding:1em}
h1{font-size:1.3em;color:#7ec8ff}
.card{background:#1c2230;border-radius:10px;padding:1em;margin:1em 0}
.rpm{font-size:2em;font-weight:bold;color:#7dff9b}
.rpm2{font-size:1.4em;color:#ffd97d}
label{display:inline-block;width:110px}
input{padding:.4em;margin:.2em 0;border-radius:6px;border:1px solid #444;background:#0d1017;color:#e6e6e6}
button{padding:.5em 1.2em;border:0;border-radius:6px;background:#2f7dd1;color:#fff;cursor:pointer}
.small{color:#9aa4b2;font-size:.85em}
</style></head><body>
<h1>Deye Fan Simulator</h1>
<div class="card"><h2>Canal 1 — Noctua 9 cm</h2>
<div>RPM lu : <span id="r1" class="rpm">-</span></div>
<div>RPM simulé : <span id="o1" class="rpm2">-</span> <span class="small">(ratio <span id="x1">-</span>)</span></div></div>
<div class="card"><h2>Canal 2 — Noctua 6 cm</h2>
<div>RPM lu : <span id="r2" class="rpm">-</span></div>
<div>RPM simulé : <span id="o2" class="rpm2">-</span> <span class="small">(ratio <span id="x2">-</span>)</span></div></div>
<div class="card"><h2>Ratios</h2>
<form method="POST" action="/api/config">
<label>Ratio canal 1</label><input name="ratio1" type="number" step="0.1" min="0.5" max="10" id="i1"><br>
<label>Ratio canal 2</label><input name="ratio2" type="number" step="0.1" min="0.5" max="10" id="i2"><br><br>
<button type="submit">Enregistrer</button></form></div>
<div class="card"><h2>WiFi (STA)</h2>
<form method="POST" action="/api/wifi">
<label>SSID</label><input name="ssid" maxlength="32"><br>
<label>Mot de passe</label><input name="pass" type="password" maxlength="64"><br><br>
<button type="submit">Connecter</button>
<div class="small" id="net">-</div></form></div>
<script>
async function tick(){try{const s=await(await fetch('/api/status')).json();
r1.textContent=s.c1.in.toFixed(0);o1.textContent=s.c1.out.toFixed(0);x1.textContent=s.c1.ratio;
r2.textContent=s.c2.in.toFixed(0);o2.textContent=s.c2.out.toFixed(0);x2.textContent=s.c2.ratio;
i1.value=s.c1.ratio;i2.value=s.c2.ratio;
net.textContent='AP: '+s.ap+' | STA: '+s.sta;}catch(e){}}
setInterval(tick,1000);tick();
</script></body></html>)rawliteral";

static void handleRoot() {
  server.send_P(200, "text/html", PAGE_HTML);
}

static void handleStatus() {
  char buf[320];
  char sta[40];
  if (WiFi.status() == WL_CONNECTED)
    snprintf(sta, sizeof(sta), "%s", WiFi.localIP().toString().c_str());
  else
    snprintf(sta, sizeof(sta), "non connecté");
  snprintf(buf, sizeof(buf),
    "{\"c1\":{\"in\":%.1f,\"out\":%.1f,\"ratio\":%.2f},"
    "\"c2\":{\"in\":%.1f,\"out\":%.1f,\"ratio\":%.2f},"
    "\"ap\":\"%s\",\"sta\":\"%s\"}",
    ch[0].rpmIn, ch[0].rpmIn * ch[0].ratio, ch[0].ratio,
    ch[1].rpmIn, ch[1].rpmIn * ch[1].ratio, ch[1].ratio,
    WiFi.softAPIP().toString().c_str(), sta);
  server.send(200, "application/json", buf);
}

static void handleConfig() {
  float r1 = server.arg("ratio1").toFloat();
  float r2 = server.arg("ratio2").toFloat();
  if (r1 >= RATIO_MIN && r1 <= RATIO_MAX) ch[0].ratio = r1;
  if (r2 >= RATIO_MIN && r2 <= RATIO_MAX) ch[1].ratio = r2;
  saveConfig();
  server.sendHeader("Location", "/");
  server.send(303);
}

static void handleWifi() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  ssid.toCharArray(cfg.ssid, sizeof(cfg.ssid));
  pass.toCharArray(cfg.pass, sizeof(cfg.pass));
  saveConfig();
  if (ssid.length() > 0) {
    WiFi.begin(cfg.ssid, cfg.pass);   // reconnexion avec les nouveaux identifiants
  } else {
    WiFi.disconnect(false);           // SSID vide => STA désactivé
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// ============================ TÂCHES PÉRIODIQUES (loop) ====================
// Recalcule RPM et demi-période cible. Tournant hors ISR, ce code peut être
// interrompu par le WiFi sans aucune conséquence sur le signal émis.
static void updateChannels() {
  uint32_t now = micros();
  for (uint8_t i = 0; i < 2; i++) {
    uint32_t ema  = ch[i].emaPeriodUs;
    uint32_t last = ch[i].lastPulseUs;
    float rpm = 0.0f;
    if (ema > 0 && (now - last) <= TIMEOUT_US) {
      rpm = 60000000.0f / ((float)ema * PULSES_PER_REV);
    } else if ((now - last) > TIMEOUT_US) {
      ch[i].emaPeriodUs = 0;            // réarme le filtre au prochain signal
    }
    ch[i].rpmIn = rpm;

    // Fréquence de sortie = (RPM/60) * 2 imp/tour * ratio ; demi-période en µs
    float fout = rpm / 60.0f * PULSES_PER_REV * ch[i].ratio;
    uint32_t half = (fout >= 0.5f) ? (uint32_t)(500000.0f / fout) : 0;
    ch[i].halfPeriodUs = half;          // écriture 32 bits atomique
  }
}

static void updateLed() {
  bool active = (ch[0].rpmIn > 0.0f) || (ch[1].rpmIn > 0.0f);
  if (active) {
    digitalWrite(PIN_LED, LOW);                       // fixe = simulation active
  } else {
    digitalWrite(PIN_LED, (millis() / 500) % 2 ? HIGH : LOW);  // clignote = attente
  }
}

// ============================ SETUP ========================================
void setup() {
  Serial.begin(115200);
  Serial.println(F("\nDeye Fan Simulator — démarrage"));

  pinMode(PIN_TACH_OUT_1, OUTPUT);
  pinMode(PIN_TACH_OUT_2, OUTPUT);
  digitalWrite(PIN_TACH_OUT_1, LOW);    // transistors OFF => lignes tach relâchées
  digitalWrite(PIN_TACH_OUT_2, LOW);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  // Entrées tach : la résistance de pull-up 10 k vers 3,3 V est EXTERNE
  // (voir schéma) ; on active quand même le pull-up interne en secours.
  pinMode(PIN_TACH_IN_1, INPUT_PULLUP);
  pinMode(PIN_TACH_IN_2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_TACH_IN_1), isrTach1, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_TACH_IN_2), isrTach2, FALLING);

  loadConfig();

  // Timer matériel n°1 : génération des signaux de sortie (haute priorité)
  timer1_attachInterrupt(isrTimer);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
  timer1_write(5000);                   // première échéance dans 1 ms

  // WiFi : AP + STA simultanés, persistance flash du SDK désactivée
  // (la persistance est gérée par notre EEPROM => pas d'usure flash).
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  if (strlen(cfg.ssid) > 0) {
    WiFi.begin(cfg.ssid, cfg.pass);
    WiFi.setAutoReconnect(true);
  }
  MDNS.begin("deyefan");

  server.on("/",            HTTP_GET,  handleRoot);
  server.on("/api/status",  HTTP_GET,  handleStatus);
  server.on("/api/config",  HTTP_POST, handleConfig);
  server.on("/api/wifi",    HTTP_POST, handleWifi);
  server.begin();

  Serial.printf("AP '%s' : http://%s\n", AP_SSID,
                WiFi.softAPIP().toString().c_str());
}

// ============================ LOOP =========================================
void loop() {
  server.handleClient();
  MDNS.update();

  static uint32_t lastUpdate = 0;
  uint32_t nowMs = millis();
  if (nowMs - lastUpdate >= 100) {      // recalcul 10 fois/s, largement suffisant
    lastUpdate = nowMs;
    updateChannels();
    updateLed();
  }
}
