/* =============================================================================
 *  deye_fan.ino  —  Simulateur de tachymetrie pour onduleur Deye SUN-8K-SG05LP1-EU
 * -----------------------------------------------------------------------------
 *  Carte cible : LOLIN (WEMOS) D1 mini V2.3.0  (ESP-12S / ESP8266, 80 MHz)
 *  Arduino IDE : selectionner "LOLIN(WEMOS) D1 mini"
 *
 *  Rôle : remplacer les ventilateurs NMB d'origine d'un onduleur Deye par des
 *         Noctua silencieux (NF-A9-FLX x2 et NF-A6x25-FLX x2) plus lents, et
 *         faire croire a l'onduleur que les ventilateurs tournent plus vite
 *         qu'en realite en renvoyant un signal tach "multiplie".
 *
 *  Fonctionnalites :
 *    - Lecture de 2 canaux tach Noctua (open-collector, 2 pulses/rev)
 *    - Application d'un ratio multiplicateur configurable par canal
 *    - Generation d'un signal tach simule vers l'onduleur (open-collector NPN)
 *    - Interface Web (AP + STA simultanes, parametres persistants)
 *    - LED d'etat (simulation active / attente de signal)
 *
 *  Architecture d'isolation (point critique du cahier des charges) :
 *    - La generation du signal de SORTIE est confiee au moteur de waveform
 *      logiciel du core ESP8266 (startWaveform), qui s'appuie sur Timer1 en
 *      interruption NMI (Non-Maskable Interrupt). Le NMI a une priorite
 *      SUPERIEURE a celle du WiFi : aucune requete HTTP ni activite reseau
 *      ne peut retarder ou jitterer les flancs de sortie. De plus, le moteur
 *      de waveform se synchronise sur le compteur de cycles CPU (80 MHz)
 *      plutot que sur le timer lui-meme, ce qui elimine la jitter residuelle
 *      due a la latence d'entree en ISR.
 *    - La LECTURE des entrees tach utilise des interruptions "edge" classiques
 *      (attachInterrupt) qui horodotent chaque flanc montant avec
 *      ESP.getCycleCount() (compteur materiel 80 MHz, monotone, insensible au
 *      WiFi). Meme si le WiFi retarde le declenchement de l'ISR, le delta de
 *      cycles entre deux flancs reste exact : la frequence mesuree est donc
 *      immunisee au jitter d'ordonnancement. Les signaux tach Noctua etant
 *      tres lents (< ~300 Hz), le risque de "manquer" un flanc a cause du WiFi
 *      est negligible (espacement inter-flanc > 3 ms, bien superieur a la
 *      fenetre de desactivation IRQ du WiFi).
 *    - Le serveur web et la logique d'affichage tournent dans loop() : ils ne
 *      touchent jamais directement aux broches de signal ; ils ne font que
 *      mettre a jour des variables volatiles (protegees par noInterrupts()
 *      lors des lectures/ecritures multi-mots) qui sont exploitees par les
 *      ISR et le moteur de waveform.
 * ========================================================================== */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <core_esp8266_waveform.h>   // startWaveform() / stopWaveform() (Timer1 NMI)

// -----------------------------------------------------------------------------
//  Correspondance broches Wemos D1 mini <-> GPIO ESP8266
// -----------------------------------------------------------------------------
//  D0 = GPIO16  (pas d'interruption, pas de PWM)   -> libre
//  D1 = GPIO5   (int. ok, PWM ok, SCL)             -> bouton config (optionnel)
//  D2 = GPIO4   (int. ok, PWM ok, SDA)             -> SORTIE tach canal B (6 cm)
//  D3 = GPIO0   (boot pull-up, eviter)             -> libre
//  D4 = GPIO2   (LED integree, active low)         -> LED d'etat
//  D5 = GPIO14  (int. ok, PWM ok, SCK)             -> SORTIE tach canal A (9 cm)
//  D6 = GPIO12  (int. ok, PWM ok, MISO)            -> ENTREE tach canal A (9 cm)
//  D7 = GPIO13  (int. ok, PWM ok, MOSI)            -> ENTREE tach canal B (6 cm)
//  D8 = GPIO15  (boot pull-down, eviter)           -> libre
//  A0 = ADC0    (analogique uniquement)            -> libre
// -----------------------------------------------------------------------------

static const uint8_t NB_CANALS = 2;

// Index 0 = canal A (ventilateur 9 cm), index 1 = canal B (ventilateur 6 cm)
struct CanalMap {
  const char* nom;
  const char* label;
  uint8_t     pinIn;     // broche ESP lisant le tach Noctua
  uint8_t     pinOut;    // broche ESP pilotant le transistor de sortie
};

static const CanalMap CANALS[NB_CANALS] = {
  { "A", "Noctua NF-A9 x2 (9 cm)",   GPIO12, GPIO14 },  // D6 entree, D5 sortie
  { "B", "Noctua NF-A6x25 x2 (6 cm)", GPIO13, GPIO4  }, // D7 entree, D2 sortie
};

static const uint8_t PIN_LED = LED_BUILTIN;   // GPIO2, active LOW sur Wemos D1 mini
static const uint8_t PIN_BTN = GPIO5;         // D1, tire a GND pour activer

// ---- Constantes physiques ---------------------------------------------------
static const uint32_t CPU_HZ            = 80000000UL;   // ESP8266 @ 80 MHz
static const uint8_t  PULSES_PAR_REV    = 2;             // Noctua : 2 pulses / rev
static const uint32_t TIMEOUT_SIGNAL_US = 1500000UL;     // 1.5 s sans flanc = "attente"
static const uint32_t MAJ_SORTIE_DELTA_MILLIHZ = 200;   // hysteresis maj sortie

// ---- Configuration persistante ---------------------------------------------
struct Config {
  char     apSsid[33];
  char     apPass[65];
  char     staSsid[33];
  char     staPass[65];
  float    ratio[NB_CANALS];
  bool     staActive;
  uint32_t magic;
};

static const uint32_t CONFIG_MAGIC = 0xDE7EFA01;
static const char*    CONFIG_PATH  = "/deye_fan.cfg";
static Config cfg;

// ---- Etat d'execution par canal --------------------------------------------
struct CanalState {
  volatile uint32_t lastEdgeCycle;    // cycle count au dernier flanc montant
  volatile uint32_t prevEdgeCycle;    // cycle count au flanc precedent
  volatile bool     edgeFresh;        // un nouveau flanc est arrive (non lu)
  volatile uint32_t lastEdgeMicros;   // micros() au dernier flanc (timeout)

  float    freqInHz;        // frequence lue moyenne (Hz)
  float    rpmIn;           // RPM reels du Noctua
  float    rpmOut;          // RPM simule envoye a l'onduleur
  float    freqOutHz;       // frequence de sortie demandee
  float    freqOutApplied;  // derniere frequence reellement programmee
  uint32_t periodUs;        // periode du signal d'entree (us)
  bool     signalPresent;   // true si un flanc a ete vu recemment
};

static CanalState canals[NB_CANALS];

// ---- Buffer de moyenne glissante des periodes d'entree ---------------------
static const uint8_t PERIOD_BUF_N = 4;
static uint32_t periodBuf[NB_CANALS][PERIOD_BUF_N];
static uint8_t  periodIdx[NB_CANALS];

// ---- Serveur web ------------------------------------------------------------
static ESP8266WebServer server(80);
static bool ledOn = false;
static uint32_t lastBlinkMs = 0;
// =============================================================================
//  ISR de lecture tach d'entree
// =============================================================================
//  On horodate avec ESP.getCycleCount() (compteur materiel 32 bits @ 80 MHz,
//  incremente a chaque cycle, insensible au WiFi). Le delta de cycles entre
//  deux flancs montants donne la periode exacte, meme si l'ISR est declenchee
//  en retard par une activite WiFi.

static void IRAM_ATTR isrTachA() {
  canals[0].prevEdgeCycle  = canals[0].lastEdgeCycle;
  canals[0].lastEdgeCycle  = ESP.getCycleCount();
  canals[0].edgeFresh      = true;
  canals[0].lastEdgeMicros = micros();
}

static void IRAM_ATTR isrTachB() {
  canals[1].prevEdgeCycle  = canals[1].lastEdgeCycle;
  canals[1].lastEdgeCycle  = ESP.getCycleCount();
  canals[1].edgeFresh      = true;
  canals[1].lastEdgeMicros = micros();
}

// =============================================================================
//  Configuration persistante (LittleFS)
// =============================================================================

static void configDefaut(Config& c) {
  memset(&c, 0, sizeof(c));
  strcpy(c.apSsid, "DeyeFanSim");
  strcpy(c.apPass, "deye12345");
  c.staSsid[0] = 0;
  c.staPass[0] = 0;
  c.staActive  = false;
  c.ratio[0]   = 2.0f;   // canal A (9 cm) : x2 par defaut
  c.ratio[1]   = 2.0f;   // canal B (6 cm) : x2 par defaut
  c.magic      = CONFIG_MAGIC;
}

static bool configCharger(Config& c) {
  configDefaut(c);
  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) return false;
  size_t n = f.read((uint8_t*)&c, sizeof(c));
  f.close();
  if (n != sizeof(c) || c.magic != CONFIG_MAGIC) {
    configDefaut(c);
    return false;
  }
  for (uint8_t i = 0; i < NB_CANALS; ++i) {
    if (!(c.ratio[i] > 0.0f) || c.ratio[i] > 100.0f) c.ratio[i] = 2.0f;
  }
  return true;
}

static bool configSauver(const Config& c) {
  File f = LittleFS.open(CONFIG_PATH, "w");
  if (!f) return false;
  size_t n = f.write((const uint8_t*)&c, sizeof(c));
  f.close();
  return n == sizeof(c);
}

// =============================================================================
//  WiFi : AP + STA simultanes, parametres persistants
// =============================================================================

static void wifiDemarrage() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(cfg.apSsid, (cfg.apPass[0] ? cfg.apPass : nullptr));
  Serial.printf("AP '%s' actif @ %s\n", cfg.apSsid, WiFi.softAPIP().toString().c_str());

  if (cfg.staActive && cfg.staSsid[0]) {
    WiFi.persistent(true);   // le SDK stocke les creds STA en flash
    WiFi.begin(cfg.staSsid, cfg.staPass);
    Serial.printf("Connexion STA a '%s' en cours...\n", cfg.staSsid);
  } else {
    Serial.println("STA inactif (mode AP seul).");
  }
}
// =============================================================================
//  Generation du signal de SORTIE (isole du WiFi via Timer1 NMI)
// =============================================================================
//  startWaveform(pin, timeHighUs, timeLowUs) programme le moteur de waveform
//  du core ESP8266. Ce moteur est pilote par Timer1 en NMI : il PREEMPE le
//  WiFi (qui tourne en interruption niveau 1). Aucune requete HTTP, aucun
//  scan, aucune activite reseau ne peut donc introduire de jitter sur la
//  sortie. Les transitions sont de plus recalees sur le compteur de cycles
//  CPU, ce qui annule la jitter de latence d'ISR.
//
//  On genere un signal carre 50 % (timeHigh == timeLow). La frequence est
//  f = 1 / (timeHigh + timeLow). Le transistor de sortie (collecteur ouvert)
//  inverse le signal : la ligne cote onduleur est basse quand la broche ESP
//  est haute. La frequence est preservee, seul le sens logique est inverse,
//  ce qui n'a aucune importance pour un comptage de flancs.

static void appliquerSortie(uint8_t i) {
  CanalState& s = canals[i];
  float f = s.freqOutHz;
  if (f <= 0.0f) {
    stopWaveform(CANALS[i].pinOut);
    digitalWrite(CANALS[i].pinOut, LOW);   // transistor bloque -> ligne tiree haut
    s.freqOutApplied = 0.0f;
    return;
  }
  float periodUs = 1.0e6f / f;
  if (periodUs < 50.0f)   periodUs = 50.0f;     // limite haute ~20 kHz
  if (periodUs > 4.0e6f)  periodUs = 4.0e6f;    // limite basse ~0.25 Hz
  uint32_t th = (uint32_t)(periodUs * 0.5f);
  uint32_t tl = (uint32_t)(periodUs * 0.5f);
  startWaveform(CANALS[i].pinOut, th, tl);
  s.freqOutApplied = f;
}

// =============================================================================
//  Calcul des frequences / RPM a partir des flancs lus
// =============================================================================

static void traiterCanal(uint8_t i) {
  CanalState& s = canals[i];

  uint32_t c0, c1, dernierUs;
  bool fresh;
  noInterrupts();
  c0 = s.prevEdgeCycle;
  c1 = s.lastEdgeCycle;
  fresh = s.edgeFresh;
  s.edgeFresh = false;
  dernierUs = s.lastEdgeMicros;
  interrupts();

  // Timeout : pas de flanc depuis longtemps -> signal absent
  uint32_t maintenantUs = micros();
  if ((maintenantUs - dernierUs) > TIMEOUT_SIGNAL_US) {
    s.signalPresent = false;
  }

  // Si un nouveau flanc est arrive, integrer la periode dans le buffer
  if (fresh && c0 != 0 && c1 > c0) {
    uint32_t deltaCycles = c1 - c0;
    uint32_t periodUsLocal = (deltaCycles * 1000000UL) / CPU_HZ;
    if (periodUsLocal >= 1000 && periodUsLocal <= 1000000) {
      periodBuf[i][periodIdx[i]] = periodUsLocal;
      periodIdx[i] = (periodIdx[i] + 1) % PERIOD_BUF_N;
      s.signalPresent = true;
    }
  }

  // Moyenne glissante des periodes
  uint32_t somme = 0;
  uint8_t  n = 0;
  for (uint8_t k = 0; k < PERIOD_BUF_N; ++k) {
    if (periodBuf[i][k] != 0) { somme += periodBuf[i][k]; ++n; }
  }
  if (n > 0) {
    s.periodUs = somme / n;
    s.freqInHz = 1.0e6f / (float)s.periodUs;
    s.rpmIn    = s.freqInHz * 60.0f / (float)PULSES_PAR_REV;
  } else {
    s.freqInHz = 0.0f;
    s.rpmIn    = 0.0f;
  }

  // Calcul de la sortie : RPM_out = RPM_in * ratio (si signal present)
  float ratio;
  noInterrupts(); ratio = cfg.ratio[i]; interrupts();
  if (s.signalPresent && s.freqInHz > 0.0f) {
    s.rpmOut   = s.rpmIn * ratio;
    s.freqOutHz = s.freqInHz * ratio;
  } else {
    s.rpmOut    = 0.0f;
    s.freqOutHz = 0.0f;
  }

  // Mise a jour de la sortie avec hysteresis (evite les reprogrammations
  // incessantes du moteur de waveform, source de micro-transitions)
  float delta = fabsf(s.freqOutHz - s.freqOutApplied);
  float seuil = s.freqOutApplied * (MAJ_SORTIE_DELTA_MILLIHZ / 1000.0f) + 0.05f;
  if (s.freqOutHz <= 0.0f && s.freqOutApplied > 0.0f) {
    appliquerSortie(i);
  } else if (s.freqOutHz > 0.0f && delta > seuil) {
    appliquerSortie(i);
  }
}
// =============================================================================
//  LED d'etat
// =============================================================================
//  - Simulation active (signal present sur au moins un canal) : LED allumee fixe
//  - Attente (aucun signal)                                     : LED clignote lente
//  La LED integree du Wemos D1 mini est active LOW (allumee quand GPIO2 = LOW).

static void majLed() {
  bool simulation = false;
  for (uint8_t i = 0; i < NB_CANALS; ++i) {
    if (canals[i].signalPresent && canals[i].freqOutApplied > 0.0f) simulation = true;
  }
  if (simulation) {
    digitalWrite(PIN_LED, LOW);   // allumee fixe
    ledOn = true;
  } else {
    // clignotement lent ~2 Hz
    uint32_t m = millis();
    if (m - lastBlinkMs > 250) {
      lastBlinkMs = m;
      ledOn = !ledOn;
      digitalWrite(PIN_LED, ledOn ? LOW : HIGH);
    }
  }
}

// =============================================================================
//  Pages web : HTML + JSON
// =============================================================================

static String escapeHtml(const String& s) {
  String r;
  r.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (c == '&')      r += "&amp;";
    else if (c == '<') r += "&lt;";
    else if (c == '>') r += "&gt;";
    else if (c == '"') r += "&quot;";
    else               r += c;
  }
  return r;
}

static String pageHTML() {
  String s;
  s.reserve(3000);
  s += F("<!DOCTYPE html><html lang='fr'><head><meta charset='utf-8'>");
  s += F("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  s += F("<title>Deye Fan Simulator</title><style>");
  s += F("body{font-family:Arial,sans-serif;margin:16px;max-width:720px}");
  s += F("h1{font-size:1.3em}table{border-collapse:collapse;width:100%}");
  s += F("td,th{border:1px solid #ccc;padding:6px;text-align:center}");
  s += F("th{background:#eee}input[type=number]{width:80px}");
  s += F(".ok{color:green}.att{color:#a00}.box{margin:12px 0;padding:10px;border:1px solid #ddd;border-radius:6px}");
  s += F("</style></head><body>");
  s += F("<h1>Deye Fan Simulator</h1>");

  s += F("<div class='box'><h2>Etat temps reel</h2>");
  s += F("<table><tr><th>Canal</th><th>RPM lu</th><th>RPM simule</th><th>Ratio</th><th>Signal</th></tr>");
  for (uint8_t i = 0; i < NB_CANALS; ++i) {
    s += "<tr><td>" + String(CANALS[i].nom) + " (" + CANALS[i].label + ")</td>";
    s += "<td id='rin" + String(i) + "'>--</td>";
    s += "<td id='rout" + String(i) + "'>--</td>";
    s += "<td id='rat" + String(i) + "'>--</td>";
    s += "<td id='sig" + String(i) + "'>--</td></tr>";
  }
  s += F("</table></div>");

  s += F("<div class='box'><h2>Reglages des ratios</h2>");
  s += F("<form method='post' action='/setRatio'>");
  for (uint8_t i = 0; i < NB_CANALS; ++i) {
    s += "<label>Canal " + String(CANALS[i].nom) + " (" + CANALS[i].label + "): </label>";
    s += "<input type='number' step='0.1' min='0.1' max='100' name='r" + String(i) + "' value='";
    s += String(cfg.ratio[i], 2);
    s += "'><br><br>";
  }
  s += F("<button type='submit'>Appliquer</button></form></div>");

  s += F("<div class='box'><h2>WiFi AP</h2>");
  s += F("<form method='post' action='/setAp'>");
  s += F("<label>SSID AP: </label><input name='ssid' value='");
  s += escapeHtml(cfg.apSsid) + "'><br><br>";
  s += F("<label>Mot de passe AP (vide = ouvert): </label><input name='pass' value='");
  s += escapeHtml(cfg.apPass) + "'><br><br>");
  s += F("<button type='submit'>Appliquer (AP redemarre)</button></form></div>");

  s += F("<div class='box'><h2>WiFi STA (optionnel)</h2>");
  s += F("<form method='post' action='/setSta'>");
  s += F("<label>SSID STA: </label><input name='ssid' value='");
  s += escapeHtml(cfg.staSsid) + "'><br><br>");
  s += F("<label>Mot de passe STA: </label><input name='pass' value='");
  s += escapeHtml(cfg.staPass) + "'><br><br>");
  s += F("<label><input type='checkbox' name='active' ");
  s += (cfg.staActive ? "checked" : "");
  s += F("> Activer la connexion STA au demarrage</label><br><br>");
  s += F("<button type='submit'>Appliquer</button></form></div>");

  s += F("<script>setInterval(function(){fetch('/status.json').then(r=>r.json()).then(d=>{");
  for (uint8_t i = 0; i < NB_CANALS; ++i) {
    s += "document.getElementById('rin" + String(i) + "').textContent=d.c[" + String(i) + "].rin;";
    s += "document.getElementById('rout" + String(i) + "').textContent=d.c[" + String(i) + "].rout;";
    s += "document.getElementById('rat" + String(i) + "').textContent=d.c[" + String(i) + "].rat;";
    s += "document.getElementById('sig" + String(i) + "').textContent=d.c[" + String(i) + "].sig;";
    s += "document.getElementById('sig" + String(i) + "').className=d.c[" + String(i) + "].ok?'ok':'att';";
  }
  s += F("});},1000);</script>");
  s += F("</body></html>");
  return s;
}

static void handleRoot() {
  server.send(200, "text/html; charset=utf-8", pageHTML());
}

static void handleStatus() {
  String j = "{\"c\":[";
  for (uint8_t i = 0; i < NB_CANALS; ++i) {
    CanalState& s = canals[i];
    float rin, rout, rat; bool sig;
    noInterrupts();
    rin = s.rpmIn; rout = s.rpmOut; rat = cfg.ratio[i]; sig = s.signalPresent;
    interrupts();
    if (i) j += ",";
    j += "{\"rin\":" + String((int)rin) + ",\"rout\":" + String((int)rout);
    j += ",\"rat\":" + String(rat, 2) + ",\"sig\":" + (sig ? "true" : "false");
    j += ",\"ok\":" + ((sig && rout > 0) ? "true" : "false") + "}";
  }
  j += "]}";
  server.send(200, "application/json", j);
}

static void handleSetRatio() {
  bool ok = true;
  for (uint8_t i = 0; i < NB_CANALS; ++i) {
    String key = "r" + String(i);
    if (server.hasArg(key)) {
      float v = server.arg(key).toFloat();
      if (v > 0.0f && v <= 100.0f) {
        noInterrupts(); cfg.ratio[i] = v; interrupts();
      } else ok = false;
    }
  }
  if (ok) configSauver(cfg);
  server.sendHeader("Location", "/");
  server.send(303);
}

static void handleSetAp() {
  if (server.hasArg("ssid")) {
    strncpy(cfg.apSsid, server.arg("ssid").c_str(), sizeof(cfg.apSsid) - 1);
    cfg.apSsid[sizeof(cfg.apSsid) - 1] = 0;
  }
  if (server.hasArg("pass")) {
    strncpy(cfg.apPass, server.arg("pass").c_str(), sizeof(cfg.apPass) - 1);
    cfg.apPass[sizeof(cfg.apPass) - 1] = 0;
  }
  configSauver(cfg);
  // Reconfigurer l'AP a chaud
  WiFi.softAPdisconnect(true);
  WiFi.softAP(cfg.apSsid, (cfg.apPass[0] ? cfg.apPass : nullptr));
  server.sendHeader("Location", "/");
  server.send(303);
}

static void handleSetSta() {
  if (server.hasArg("ssid")) {
    strncpy(cfg.staSsid, server.arg("ssid").c_str(), sizeof(cfg.staSsid) - 1);
    cfg.staSsid[sizeof(cfg.staSsid) - 1] = 0;
  }
  if (server.hasArg("pass")) {
    strncpy(cfg.staPass, server.arg("pass").c_str(), sizeof(cfg.staPass) - 1);
    cfg.staPass[sizeof(cfg.staPass) - 1] = 0;
  }
  cfg.staActive = server.hasArg("active");
  configSauver(cfg);
  server.sendHeader("Location", "/");
  server.send(303);
}

static void configurerServeur() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status.json", HTTP_GET, handleStatus);
  server.on("/setRatio", HTTP_POST, handleSetRatio);
  server.on("/setAp", HTTP_POST, handleSetAp);
  server.on("/setSta", HTTP_POST, handleSetSta);
  server.onNotFound([]() { server.send(404, "text/plain", "404"); });
  server.begin();
}
// =============================================================================
//  setup()
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println(F("\n=== Deye Fan Simulator ==="));

  // LED en sortie (eteinte au depart : HIGH = eteint sur active-low)
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);
  ledOn = false;

  // Bouton config (optionnel)
  pinMode(PIN_BTN, INPUT_PULLUP);

  // Broches de sortie tach : LOW par defaut (transistor bloque, ligne tiree haut)
  for (uint8_t i = 0; i < NB_CANALS; ++i) {
    pinMode(CANALS[i].pinOut, OUTPUT);
    digitalWrite(CANALS[i].pinOut, LOW);
  }

  // Broches d'entree tach : pull-up interne (3.3V) -> le tach Noctua est
  // open-collector, le pull-up defini la tension logique cote ESP (3.3V).
  // Une resistance de pull-up externe de 4.7 kΩ est egalement recommandee
  // (voir schema) pour une meilleure qualite de front.
  pinMode(CANALS[0].pinIn, INPUT_PULLUP);
  pinMode(CANALS[1].pinIn, INPUT_PULLUP);

  // Initialisation des etats
  for (uint8_t i = 0; i < NB_CANALS; ++i) {
    canals[i].lastEdgeCycle   = 0;
    canals[i].prevEdgeCycle   = 0;
    canals[i].edgeFresh       = false;
    canals[i].lastEdgeMicros  = 0;
    canals[i].freqInHz        = 0;
    canals[i].rpmIn           = 0;
    canals[i].rpmOut          = 0;
    canals[i].freqOutHz       = 0;
    canals[i].freqOutApplied  = 0;
    canals[i].periodUs        = 0;
    canals[i].signalPresent   = false;
    periodIdx[i] = 0;
    for (uint8_t k = 0; k < PERIOD_BUF_N; ++k) periodBuf[i][k] = 0;
  }

  // Systeme de fichiers pour la config persistante
  if (!LittleFS.begin()) {
    Serial.println(F("LittleFS: formatage..."));
    LittleFS.format();
    LittleFS.begin();
  }
  configCharger(cfg);
  Serial.printf("Ratios: A=%.2f  B=%.2f\n", cfg.ratio[0], cfg.ratio[1]);

  // WiFi
  wifiDemarrage();

  // Serveur web
  configurerServeur();

  // Interruptions de lecture tach (flanc montant)
  // On les attache EN DERNIER, une fois tout le reste en place.
  attachInterrupt(digitalPinToInterrupt(CANALS[0].pinIn), isrTachA, RISING);
  attachInterrupt(digitalPinToInterrupt(CANALS[1].pinIn), isrTachB, RISING);

  Serial.println(F("Setup OK."));
}

// =============================================================================
//  loop()
// =============================================================================

void loop() {
  // 1) Traitement des canaux (lecture -> calcul -> maj sortie)
  for (uint8_t i = 0; i < NB_CANALS; ++i) {
    traiterCanal(i);
  }

  // 2) LED d'etat
  majLed();

  // 3) Serveur web (les requetes HTTP ne touchent jamais aux broches
  //    de signal : elles ne font que lire/ecrire des variables volatiles
  //    protegees ; la sortie reste generee par Timer1 NMI en parallele).
  server.handleClient();

  // 4) Bouton config : appui long (>=3 s) remet la config AP/STA par defaut
  static uint32_t btnStart = 0;
  if (digitalRead(PIN_BTN) == LOW) {
    if (btnStart == 0) btnStart = millis();
    else if (millis() - btnStart > 3000) {
      Serial.println(F("Reset config demande..."));
      configDefaut(cfg);
      configSauver(cfg);
      digitalWrite(PIN_LED, LOW);
      delay(1500);
      digitalWrite(PIN_LED, HIGH);
      btnStart = 0;
      ESP.restart();
    }
  } else {
    btnStart = 0;
  }

  // 5) Diagnostic serie toutes les 2 s (desactivable)
  static uint32_t lastDiag = 0;
  if (millis() - lastDiag > 2000) {
    lastDiag = millis();
    for (uint8_t i = 0; i < NB_CANALS; ++i) {
      CanalState& s = canals[i];
      Serial.printf("[%s] %s | f_in=%.2f Hz | RPM_in=%.0f | ratio=%.2f | RPM_out=%.0f | f_out=%.2f Hz | %s\n",
        CANALS[i].nom, CANALS[i].label,
        s.freqInHz, s.rpmIn, cfg.ratio[i], s.rpmOut, s.freqOutApplied,
        s.signalPresent ? "OK" : "ATTENTE");
    }
  }

  // On ne dort pas : yield() implicite via server.handleClient(), mais on
  // garde une boucle courte pour la reactivite du traitement signal.
}
