/*
 * ============================================================================
 *  deye_fan.ino — Spoofer de tachymétrie ventilateurs pour onduleur Deye
 * ============================================================================
 *
 *  Cible : Wemos / LOLIN D1 mini (ESP8266 / ESP-12S)
 *  IDE   : Arduino IDE + core "ESP8266 by ESP8266 Community"
 *
 *  Rôle :
 *   - Lit les signaux tach (2 impulsions/tour, collecteur ouvert) de deux
 *     ventilateurs Noctua (NF-A9-FLX 9cm et NF-A6x25-FLX 6cm) via un étage
 *     de mise en forme/adaptation de niveau à transistor NPN.
 *   - Recalcule, pour chaque canal, un signal tach simulé dont la fréquence
 *     est celle du ventilateur réel multipliée par un ratio réglable, afin
 *     de faire croire à l'onduleur Deye que les ventilateurs tournent plus
 *     vite qu'en réalité.
 *   - Réémet ce signal simulé vers l'onduleur via un étage de sortie à
 *     collecteur ouvert (compatible avec un pull-up interne du Deye à une
 *     tension inconnue : 3.3V, 5V ou 12V, sans aucune modification matérielle).
 *   - Expose une interface web (AP+STA simultané, réglages persistants en
 *     EEPROM émulée) pour configurer le WiFi et les ratios, et visualiser
 *     en temps réel les RPM mesurés / simulés.
 *   - Pilote une LED d'état : allumée fixe = simulation active sur les 2
 *     canaux, clignotante = au moins un canal attend un signal tach valide.
 *
 *  --------------------------------------------------------------------------
 *  ISOLATION TEMPORELLE VIS-A-VIS DU WIFI / SERVEUR WEB (point critique)
 *  --------------------------------------------------------------------------
 *  L'ESP8266 est mono-coeur : il n'existe pas de véritable parallélisme
 *  matériel comme sur un ESP32. La stratégie retenue pour garantir un signal
 *  de sortie stable malgré le WiFi et le serveur web est la suivante :
 *
 *   1) La génération du signal de sortie ne dépend JAMAIS de loop() ni du
 *      serveur web. Elle est entièrement pilotée par le timer matériel
 *      Timer1, configuré en mode périodique (TIM_LOOP) à 10 kHz (tick de
 *      100 µs). À chaque interruption Timer1, l'ISR se contente de
 *      décrémenter deux compteurs entiers et, le cas échéant, de basculer
 *      une broche GPIO par écriture directe dans les registres GPOS/GPOC
 *      (quelques instructions machine, pas d'appel digitalWrite()).
 *      loop() ne fait qu'écrire, de temps en temps, la valeur cible de
 *      demi-période dans une variable partagée : le timer matériel continue
 *      à tourner de façon totalement autonome pendant qu'une requête HTTP
 *      est traitée, qu'une association WiFi a lieu, etc.
 *
 *   2) La capture des entrées tach utilise des interruptions GPIO
 *      (attachInterrupt) dont le code exécuté est minimal (horodatage
 *      micros() + calcul d'une différence). Le calcul du RPM et la mise à
 *      jour des cibles de sortie sont déportés dans loop(), à faible
 *      fréquence (toutes les ~150 ms), donc hors du chemin temps-réel.
 *
 *   3) Le WiFi "modem sleep" est désactivé (WIFI_NONE_SLEEP) : ce mode
 *      d'économie d'énergie introduit sinon des rafales RF périodiques qui
 *      peuvent retarder de quelques centaines de µs le traitement des
 *      interruptions logicielles. Le CPU est en outre cadencé à 160 MHz
 *      pour maximiser la marge de calcul disponible entre deux ticks du
 *      timer matériel.
 *
 *   4) Le serveur web (ESP8266WebServer, synchrone) ne bloque que pendant
 *      le traitement d'une requête HTTP (quelques ms typiquement) : cela
 *      n'a aucune incidence sur le Timer1 matériel qui continue de générer
 *      le signal de sortie indépendamment, ni sur les interruptions GPIO
 *      d'entrée (matérielles, prioritaires sur le code applicatif).
 *
 *  Limite honnête : sur ESP8266, la pile WiFi peut, dans de rares cas
 *  (réception d'un paquet), masquer les interruptions pendant quelques
 *  microsecondes. Avec une résolution de sortie de 100 µs pour des périodes
 *  de rotation de l'ordre de 10 à 100+ ms, cet effet est totalement
 *  négligeable (<< 1 % d'erreur) et bien en-dessous de ce qu'un onduleur
 *  peut détecter. Une isolation "dure" garantie à 100% nécessiterait un
 *  second coeur (ESP32) ; l'architecture ci-dessus est la meilleure
 *  approche réaliste sur ESP8266.
 *
 * ============================================================================
 *  CABLAGE / BROCHES (voir schéma fourni séparément)
 * ============================================================================
 *   D1 (GPIO5)  -> LED d'état (+ résistance série vers GND)
 *   D2 (GPIO4)  -> Entrée tach adaptée, canal A = Noctua 9 cm
 *   D7 (GPIO13) -> Entrée tach adaptée, canal B = Noctua 6 cm
 *   D5 (GPIO14) -> Sortie tach simulée, canal A, vers onduleur (ex 9 cm)
 *   D6 (GPIO12) -> Sortie tach simulée, canal B, vers onduleur (ex 6 cm)
 *   5V / GND    -> Alimentation depuis le module buck 12V->5V (voir schéma)
 *
 *   Broches volontairement évitées : D0/D3/D4/D8/RX/TX (fonctions de boot
 *   ou de programmation série, à ne pas charger avec de l'électronique
 *   externe pour éviter tout problème de démarrage).
 *
 * ============================================================================
 *  PREMIER DEMARRAGE
 * ============================================================================
 *   - Réseau AP par défaut : SSID "DeyeFanCtrl", mot de passe "changeme123"
 *   - Se connecter à ce réseau, aller sur http://192.168.4.1/
 *   - Configurer le WiFi domestique (STA), les ratios, puis "Enregistrer"
 *     (redémarre automatiquement le module pour appliquer le WiFi).
 * ============================================================================
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

extern "C" {
  #include "user_interface.h"
}

// ============================================================================
//  Configuration matérielle
// ============================================================================
#define PIN_TACH_IN_A   4   // D2 - entrée tach adaptée, Noctua 9 cm
#define PIN_TACH_IN_B   13  // D7 - entrée tach adaptée, Noctua 6 cm
#define PIN_TACH_OUT_A  14  // D5 - sortie tach simulée vers Deye, canal A
#define PIN_TACH_OUT_B  12  // D6 - sortie tach simulée vers Deye, canal B
#define PIN_LED_STATUS  5   // D1 - LED d'état

#define NUM_CHANNELS 2
#define CH_A 0
#define CH_B 1
static const char *CHANNEL_NAME[NUM_CHANNELS] = {"A (9cm)", "B (6cm)"};

// ============================================================================
//  Paramètres tach
// ============================================================================
#define PULSES_PER_REV        2       // NMB et Noctua : 2 impulsions par tour
#define TACH_MIN_PERIOD_US    2000UL  // anti-rebond entrée : ignore < 2 ms (~15000 rpm max)
#define TACH_TIMEOUT_MS       3000UL  // pas d'impulsion depuis 3 s => canal considéré à l'arrêt
#define MIN_OUT_HALF_PERIOD_100US 10  // 10*100µs = 1ms => période 2ms => 500Hz => plafond ~15000 rpm simulés

// ============================================================================
//  Configuration persistante (EEPROM émulée)
// ============================================================================
struct Config {
  uint32_t magic;
  char     sta_ssid[32];
  char     sta_pass[64];
  char     ap_ssid[32];
  char     ap_pass[64];
  float    ratio[NUM_CHANNELS];
  uint32_t checksum;
};

#define CONFIG_MAGIC 0xDEE5FA20UL
#define EEPROM_SIZE  sizeof(Config)

Config config;

uint32_t calcChecksum(const Config &c) {
  const uint8_t *p = (const uint8_t *)&c;
  uint32_t sum = 0;
  for (size_t i = 0; i < sizeof(Config) - sizeof(c.checksum); i++) {
    sum += (uint32_t)p[i] * (i + 1);
  }
  return sum;
}

void saveConfig() {
  config.checksum = calcChecksum(config);
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(0, config);
  EEPROM.commit();
  EEPROM.end();
}

void loadConfig() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, config);
  EEPROM.end();

  if (config.magic != CONFIG_MAGIC || config.checksum != calcChecksum(config)) {
    // Première utilisation ou EEPROM invalide : valeurs par défaut
    memset(&config, 0, sizeof(config));
    config.magic = CONFIG_MAGIC;
    strncpy(config.ap_ssid, "DeyeFanCtrl", sizeof(config.ap_ssid) - 1);
    strncpy(config.ap_pass, "changeme123", sizeof(config.ap_pass) - 1);
    config.sta_ssid[0] = 0;
    config.sta_pass[0] = 0;
    config.ratio[CH_A] = 1.0f;
    config.ratio[CH_B] = 1.0f;
    saveConfig();
  }
}

// ============================================================================
//  Capture des entrées tach (interruptions GPIO — code minimal)
// ============================================================================
volatile uint32_t lastEdgeMicros[NUM_CHANNELS]   = {0, 0};
volatile uint32_t lastPeriodMicros[NUM_CHANNELS] = {0, 0}; // 0 = aucune mesure valide encore
volatile uint32_t lastPulseMillis[NUM_CHANNELS]  = {0, 0};

void ICACHE_RAM_ATTR handleTachEdge(uint8_t ch) {
  uint32_t now = micros();
  uint32_t delta = now - lastEdgeMicros[ch]; // l'arithmétique non signée gère le wrap-around de micros()

  if (delta >= TACH_MIN_PERIOD_US) {
    lastPeriodMicros[ch] = delta;
    lastEdgeMicros[ch]   = now;
    lastPulseMillis[ch]  = millis();
  }
  // sinon : rebond / bruit -> on ignore complètement cette transition
  // (on ne met pas à jour lastEdgeMicros pour ne pas fausser la mesure suivante)
}

void ICACHE_RAM_ATTR isrTachA() { handleTachEdge(CH_A); }
void ICACHE_RAM_ATTR isrTachB() { handleTachEdge(CH_B); }

// ============================================================================
//  Génération des sorties tach — pilotée entièrement par Timer1 matériel
// ============================================================================
// Timer1 configuré en TIM_DIV16 (5 MHz, soit 0.2µs/tick) + TIM_LOOP
// (rechargement automatique) à 500 ticks => interruption toutes les 100µs.
#define TIMER1_TICKS_PER_ISR 500

volatile int32_t  chCountdown100us[NUM_CHANNELS]  = {0, 0};
volatile uint32_t chHalfPeriod100us[NUM_CHANNELS] = {0, 0}; // 0 = canal inactif
volatile uint8_t  chPinState[NUM_CHANNELS]        = {0, 0};
volatile bool     chOutputActive[NUM_CHANNELS]    = {false, false};

void ICACHE_RAM_ATTR onTimer1ISR() {
  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    if (!chOutputActive[ch]) continue;

    if (chCountdown100us[ch] <= 0) {
      uint8_t pin = (ch == CH_A) ? PIN_TACH_OUT_A : PIN_TACH_OUT_B;
      chPinState[ch] ^= 1;
      if (chPinState[ch]) {
        GPOS = (1UL << pin); // set (HIGH -> transistor de sortie ON -> ligne tirée au bas niveau côté Deye)
      } else {
        GPOC = (1UL << pin); // clear
      }
      chCountdown100us[ch] = (int32_t)chHalfPeriod100us[ch];
    } else {
      chCountdown100us[ch]--;
    }
  }
}

// Active ou désactive proprement un canal de sortie (appelé depuis loop(), jamais depuis l'ISR)
void setChannelOutput(uint8_t ch, bool active, uint32_t halfPeriod100us) {
  uint8_t pin = (ch == CH_A) ? PIN_TACH_OUT_A : PIN_TACH_OUT_B;

  noInterrupts();
  if (active) {
    chHalfPeriod100us[ch] = halfPeriod100us;
    if (!chOutputActive[ch]) {
      // (ré)activation : on force un démarrage rapide et propre
      chPinState[ch] = 0;
      chCountdown100us[ch] = 1;
      chOutputActive[ch] = true;
    }
  } else {
    chOutputActive[ch] = false;
    chPinState[ch] = 0;
  }
  interrupts();

  if (!active) {
    GPOC = (1UL << pin); // ligne au repos = niveau bas (transistor OFF, tirée au niveau haut côté Deye)
  }
}

// ============================================================================
//  RPM mesurés / simulés (pour l'affichage web)
// ============================================================================
float    rpmIn[NUM_CHANNELS]  = {0, 0};
float    rpmOut[NUM_CHANNELS] = {0, 0};
bool     channelHasSignal[NUM_CHANNELS] = {false, false};

void updateChannel(uint8_t ch) {
  noInterrupts();
  uint32_t period    = lastPeriodMicros[ch];
  uint32_t lastPulse = lastPulseMillis[ch];
  interrupts();

  uint32_t nowMs = millis();
  bool haveSignal = (period > 0) && ((nowMs - lastPulse) < TACH_TIMEOUT_MS);
  channelHasSignal[ch] = haveSignal;

  if (haveSignal) {
    // RPM = (1e6 / période_us) * 60 / PULSES_PER_REV  =>  30 000 000 / période_us (pour PULSES_PER_REV=2)
    rpmIn[ch] = (60000000.0f / (float)period) / PULSES_PER_REV;

    float ratio = config.ratio[ch];
    if (ratio < 0.1f) ratio = 0.1f;
    if (ratio > 10.0f) ratio = 10.0f;

    // période de sortie = période d'entrée / ratio (directement, sans repasser par le RPM
    // pour limiter les erreurs d'arrondi en chaîne)
    float periodOutUs = (float)period / ratio;

    uint32_t halfPeriod100us = (uint32_t)((periodOutUs / 2.0f) / 100.0f);
    if (halfPeriod100us < MIN_OUT_HALF_PERIOD_100US) halfPeriod100us = MIN_OUT_HALF_PERIOD_100US;

    rpmOut[ch] = (60000000.0f / (float)(halfPeriod100us * 2 * 100)) / PULSES_PER_REV;

    setChannelOutput(ch, true, halfPeriod100us);
  } else {
    rpmIn[ch]  = 0;
    rpmOut[ch] = 0;
    setChannelOutput(ch, false, 0);
  }
}

// ============================================================================
//  LED d'état
// ============================================================================
uint32_t ledBlinkTs = 0;
bool     ledState = false;

void updateLed() {
  bool bothActive = chOutputActive[CH_A] && chOutputActive[CH_B];

  if (bothActive) {
    digitalWrite(PIN_LED_STATUS, HIGH); // fixe = simulation active sur les 2 canaux
  } else {
    uint32_t now = millis();
    if (now - ledBlinkTs > 250) {
      ledBlinkTs = now;
      ledState = !ledState;
      digitalWrite(PIN_LED_STATUS, ledState ? HIGH : LOW); // clignote = au moins un canal en attente
    }
  }
}

// ============================================================================
//  Serveur Web
// ============================================================================
ESP8266WebServer server(80);
bool     needRestart = false;
uint32_t restartAtMillis = 0;

const char PAGE_MAIN[] PROGMEM = R"HTMLPAGE(
<!DOCTYPE html><html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Deye Fan Spoofer</title>
<style>
body{font-family:sans-serif;max-width:640px;margin:1em auto;padding:0 1em;background:#111;color:#eee}
h1{font-size:1.3em} h2{font-size:1.05em;border-bottom:1px solid #444;padding-bottom:.2em;margin-top:1.5em}
table{width:100%;border-collapse:collapse;margin:.5em 0}
td,th{padding:.35em .5em;text-align:left;border-bottom:1px solid #333}
input{width:100%;box-sizing:border-box;padding:.4em;margin:.2em 0;background:#222;color:#eee;border:1px solid #444;border-radius:4px}
button{padding:.6em 1.2em;margin-top:.8em;background:#2a7;color:#fff;border:none;border-radius:4px;cursor:pointer}
button:hover{background:#3b8}
.badge{padding:.15em .5em;border-radius:4px;font-size:.85em}
.ok{background:#264}
.wait{background:#742}
label{font-size:.9em;color:#aaa}
</style></head><body>
<h1>Deye Fan Spoofer</h1>

<h2>Etat en temps reel</h2>
<table id="status">
<tr><th></th><th>RPM mesure</th><th>RPM simule</th><th>Etat</th></tr>
<tr><td>Canal A (9cm)</td><td id="ria">-</td><td id="roa">-</td><td id="sa">-</td></tr>
<tr><td>Canal B (6cm)</td><td id="rib">-</td><td id="rob">-</td><td id="sb">-</td></tr>
</table>

<h2>Configuration</h2>
<form id="cfgform">
<label>Ratio canal A (9cm)</label>
<input type="number" step="0.05" min="0.1" max="10" name="ratio_a" id="ratio_a">
<label>Ratio canal B (6cm)</label>
<input type="number" step="0.05" min="0.1" max="10" name="ratio_b" id="ratio_b">

<h2>WiFi domestique (STA)</h2>
<label>SSID</label>
<input type="text" name="sta_ssid" id="sta_ssid">
<label>Mot de passe (laisser vide pour ne pas changer)</label>
<input type="password" name="sta_pass" id="sta_pass" placeholder="********">

<h2>Point d'acces (AP)</h2>
<label>SSID</label>
<input type="text" name="ap_ssid" id="ap_ssid">
<label>Mot de passe (laisser vide pour ne pas changer, min 8 caracteres)</label>
<input type="password" name="ap_pass" id="ap_pass" placeholder="********">

<button type="submit">Enregistrer et redemarrer</button>
</form>
<p id="msg"></p>

<script>
function refreshStatus(){
  fetch('/status').then(r=>r.json()).then(d=>{
    document.getElementById('ria').textContent = d.rpm_in_a.toFixed(0);
    document.getElementById('roa').textContent = d.rpm_out_a.toFixed(0);
    document.getElementById('sa').innerHTML = d.active_a ? '<span class="badge ok">simulation</span>' : '<span class="badge wait">attente</span>';
    document.getElementById('rib').textContent = d.rpm_in_b.toFixed(0);
    document.getElementById('rob').textContent = d.rpm_out_b.toFixed(0);
    document.getElementById('sb').innerHTML = d.active_b ? '<span class="badge ok">simulation</span>' : '<span class="badge wait">attente</span>';
  }).catch(()=>{});
}
setInterval(refreshStatus, 1000);
refreshStatus();

function loadConfig(){
  fetch('/config').then(r=>r.json()).then(d=>{
    document.getElementById('ratio_a').value = d.ratio_a;
    document.getElementById('ratio_b').value = d.ratio_b;
    document.getElementById('sta_ssid').value = d.sta_ssid;
    document.getElementById('ap_ssid').value = d.ap_ssid;
  });
}
loadConfig();

document.getElementById('cfgform').addEventListener('submit', function(e){
  e.preventDefault();
  const data = new URLSearchParams(new FormData(this));
  fetch('/save', {method:'POST', body:data}).then(r=>r.text()).then(t=>{
    document.getElementById('msg').textContent = t;
  });
});
</script>
</body></html>
)HTMLPAGE";

void handleRoot() {
  server.send_P(200, "text/html", PAGE_MAIN);
}

void handleStatus() {
  char json[220];
  snprintf(json, sizeof(json),
    "{\"rpm_in_a\":%.0f,\"rpm_out_a\":%.0f,\"active_a\":%s,"
    "\"rpm_in_b\":%.0f,\"rpm_out_b\":%.0f,\"active_b\":%s,"
    "\"uptime_s\":%lu}",
    rpmIn[CH_A], rpmOut[CH_A], chOutputActive[CH_A] ? "true" : "false",
    rpmIn[CH_B], rpmOut[CH_B], chOutputActive[CH_B] ? "true" : "false",
    (unsigned long)(millis() / 1000));
  server.send(200, "application/json", json);
}

void handleGetConfig() {
  char json[256];
  snprintf(json, sizeof(json),
    "{\"ratio_a\":%.2f,\"ratio_b\":%.2f,\"sta_ssid\":\"%s\",\"ap_ssid\":\"%s\"}",
    config.ratio[CH_A], config.ratio[CH_B], config.sta_ssid, config.ap_ssid);
  server.send(200, "application/json", json);
}

void handleSave() {
  if (server.hasArg("ratio_a")) {
    float r = server.arg("ratio_a").toFloat();
    if (r >= 0.1f && r <= 10.0f) config.ratio[CH_A] = r;
  }
  if (server.hasArg("ratio_b")) {
    float r = server.arg("ratio_b").toFloat();
    if (r >= 0.1f && r <= 10.0f) config.ratio[CH_B] = r;
  }
  if (server.hasArg("sta_ssid")) {
    strncpy(config.sta_ssid, server.arg("sta_ssid").c_str(), sizeof(config.sta_ssid) - 1);
  }
  if (server.hasArg("sta_pass") && server.arg("sta_pass").length() > 0) {
    strncpy(config.sta_pass, server.arg("sta_pass").c_str(), sizeof(config.sta_pass) - 1);
  }
  if (server.hasArg("ap_ssid") && server.arg("ap_ssid").length() > 0) {
    strncpy(config.ap_ssid, server.arg("ap_ssid").c_str(), sizeof(config.ap_ssid) - 1);
  }
  if (server.hasArg("ap_pass") && server.arg("ap_pass").length() >= 8) {
    strncpy(config.ap_pass, server.arg("ap_pass").c_str(), sizeof(config.ap_pass) - 1);
  }

  saveConfig();
  server.send(200, "text/plain", "Enregistre. Redemarrage dans 2 secondes...");

  needRestart = true;
  restartAtMillis = millis() + 2000;
}

// ============================================================================
//  setup() / loop()
// ============================================================================
void setup() {
  system_update_cpu_freq(SYS_CPU_160MHZ); // marge de calcul maximale

  pinMode(PIN_LED_STATUS, OUTPUT);
  digitalWrite(PIN_LED_STATUS, LOW);

  pinMode(PIN_TACH_OUT_A, OUTPUT);
  pinMode(PIN_TACH_OUT_B, OUTPUT);
  digitalWrite(PIN_TACH_OUT_A, LOW);
  digitalWrite(PIN_TACH_OUT_B, LOW);

  pinMode(PIN_TACH_IN_A, INPUT);
  pinMode(PIN_TACH_IN_B, INPUT);

  loadConfig();

  // --- Timer1 matériel : génération des sorties, isolé du reste du firmware ---
  timer1_isr_init();
  timer1_attachInterrupt(onTimer1ISR);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
  timer1_write(TIMER1_TICKS_PER_ISR);

  // --- Interruptions d'entrée tach ---
  attachInterrupt(digitalPinToInterrupt(PIN_TACH_IN_A), isrTachA, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_TACH_IN_B), isrTachB, RISING);

  // --- WiFi : AP + STA simultanés, sans veille modem (réduit la gigue) ---
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.softAP(config.ap_ssid, config.ap_pass);
  if (strlen(config.sta_ssid) > 0) {
    WiFi.begin(config.sta_ssid, config.sta_pass);
  }

  // --- Serveur web ---
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/config", HTTP_GET, handleGetConfig);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

void loop() {
  server.handleClient();

  static uint32_t lastUpdate = 0;
  uint32_t now = millis();
  if (now - lastUpdate >= 150) {
    lastUpdate = now;
    updateChannel(CH_A);
    updateChannel(CH_B);
  }

  updateLed();

  if (needRestart && millis() > restartAtMillis) {
    ESP.restart();
  }
}
