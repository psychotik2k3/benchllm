/*
 * deye_fan.ino — Simulateur de tachymètre ventilateurs Deye (ESP8266 / Wemos D1 Mini)
 *
 * Carte cible : LOLIN(WEMOS) D1 mini
 *
 * Lit le signal tach open-collector de 2 ventilateurs Noctua, applique un ratio
 * multiplicateur par canal, et réémet un signal tach simulé vers l'onduleur Deye.
 *
 * Architecture logicielle (séparation stricte temps-réel / best-effort) :
 *   - ISR GPIO (FALLING)  : capture des périodes d'entrée — minimale, en RAM
 *   - Timer1 matériel 50 µs : ordonnanceur de bascules de sortie — indépendant du WiFi
 *   - loop()              : WiFi, serveur web async, LED, persistance LittleFS
 *
 * Formule : RPM = (fréquence impulsions / 2) × 60
 *           Période entre fronts descendants consécutifs = 30 / RPM (secondes)
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// ─── Brochage Wemos D1 Mini ───────────────────────────────────────────────────
// Entrées tach (interruption FALLING) — éviter D0/GPIO16 (pas d'IRQ)
#define PIN_TACH_IN_CH0   14  // D5 — Noctua 9 cm (NF-A9-FLX)
#define PIN_TACH_IN_CH1   12  // D6 — Noctua 6 cm (NF-A6x25-FLX)

// Sorties tach (base transistor NPN → collecteur ouvert vers Deye)
#define PIN_TACH_OUT_CH0  13  // D7 — simulé vers onduleur 9 cm
#define PIN_TACH_OUT_CH1   5  // D1 — simulé vers onduleur 6 cm

// LED d'état (monde best-effort uniquement ; D4 = LED intégrée Wemos, active LOW)
#define PIN_LED_STATUS     2  // D4

// ─── Constantes signal ────────────────────────────────────────────────────────
static const uint8_t  NUM_CHANNELS        = 2;
static const uint32_t TIMER_TICK_US       = 50;       // Résolution ordonnanceur sortie
static const uint32_t INPUT_TIMEOUT_US    = 1500000UL; // 1,5 s sans front → RPM = 0
static const uint32_t MIN_INPUT_PERIOD_US = 2500UL;   // ~7200 RPM max mesuré
static const uint32_t MAX_INPUT_PERIOD_US = 500000UL; // ~60 RPM min mesuré
static const uint8_t  PERIOD_AVG_SAMPLES  = 4;
static const float    RATIO_MIN           = 1.0f;
static const float    RATIO_MAX           = 4.0f;
static const float    RATIO_DEFAULT       = 2.5f;
static const float    PERIOD_SLEW_MAX     = 0.15f;    // Max 15 % variation période / mise à jour

static const char *CONFIG_PATH = "/config.json";

// ─── Configuration persistante ───────────────────────────────────────────────
struct FanConfig {
  float    ratio[NUM_CHANNELS];
  char     staSsid[33];
  char     staPass[65];
  char     apSsid[33];
  char     apPass[65];
};

FanConfig cfg;

// ─── État temps-réel partagé (volatile, accès atomique uint32_t) ───────────────
volatile uint32_t g_lastEdgeUs[NUM_CHANNELS]       = {0, 0};
volatile uint32_t g_rawPeriodUs[NUM_CHANNELS]      = {0, 0};
volatile uint32_t g_edgeCount[NUM_CHANNELS]        = {0, 0};

volatile uint32_t g_outHalfPeriodUs[NUM_CHANNELS]  = {0, 0}; // demi-période sortie (µs)
volatile uint32_t g_nextToggleUs[NUM_CHANNELS]     = {0, 0};
volatile uint8_t  g_outLevel[NUM_CHANNELS]         = {0, 0}; // 0 = GPIO LOW → transistor OFF → ligne haute

// Snapshot pour le monde best-effort (mis à jour dans loop, jamais dans ISR sortie)
uint32_t g_displayInputPeriodUs[NUM_CHANNELS]  = {0, 0};
uint32_t g_displayOutputPeriodUs[NUM_CHANNELS] = {0, 0};
bool     g_channelActive[NUM_CHANNELS]         = {false, false};

// ─── ISR entrée : horodatage minimal ──────────────────────────────────────────
void IRAM_ATTR isrTachInput(uint8_t ch) {
  const uint32_t now = micros();
  const uint32_t last = g_lastEdgeUs[ch];
  g_lastEdgeUs[ch] = now;

  if (last == 0) {
    return;
  }

  uint32_t period = now - last;
  if (period < MIN_INPUT_PERIOD_US || period > MAX_INPUT_PERIOD_US) {
    return;
  }

  g_rawPeriodUs[ch] = period;
  g_edgeCount[ch]++;
}

void IRAM_ATTR isrTachCh0() { isrTachInput(0); }
void IRAM_ATTR isrTachCh1() { isrTachInput(1); }

// ─── ISR Timer1 : ordonnanceur de sortie (pulse scheduler) ────────────────────
void IRAM_ATTR onTimer1Tick() {
  const uint32_t now = micros();

  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    const uint32_t half = g_outHalfPeriodUs[ch];
    if (half == 0) {
      continue;
    }

    if ((int32_t)(now - g_nextToggleUs[ch]) < 0) {
      continue;
    }

    g_outLevel[ch] ^= 1;
    digitalWrite(ch == 0 ? PIN_TACH_OUT_CH0 : PIN_TACH_OUT_CH1, g_outLevel[ch]);
    g_nextToggleUs[ch] = now + half;
  }
}

// ─── Utilitaires configuration ───────────────────────────────────────────────
void setDefaultConfig() {
  cfg.ratio[0] = RATIO_DEFAULT;
  cfg.ratio[1] = RATIO_DEFAULT;
  strncpy(cfg.staSsid, "", sizeof(cfg.staSsid));
  strncpy(cfg.staPass, "", sizeof(cfg.staPass));
  strncpy(cfg.apSsid, "DeyeFanSim", sizeof(cfg.apSsid));
  strncpy(cfg.apPass, "deye1234", sizeof(cfg.apPass));
}

bool loadConfig() {
  if (!LittleFS.exists(CONFIG_PATH)) {
    setDefaultConfig();
    return false;
  }

  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) {
    setDefaultConfig();
    return false;
  }

  StaticJsonDocument<512> doc;
  const DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    setDefaultConfig();
    return false;
  }

  cfg.ratio[0] = doc["ratio0"] | RATIO_DEFAULT;
  cfg.ratio[1] = doc["ratio1"] | RATIO_DEFAULT;
  strncpy(cfg.staSsid, doc["staSsid"] | "", sizeof(cfg.staSsid));
  strncpy(cfg.staPass, doc["staPass"] | "", sizeof(cfg.staPass));
  strncpy(cfg.apSsid, doc["apSsid"] | "DeyeFanSim", sizeof(cfg.apSsid));
  strncpy(cfg.apPass, doc["apPass"] | "deye1234", sizeof(cfg.apPass));

  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    cfg.ratio[ch] = constrain(cfg.ratio[ch], RATIO_MIN, RATIO_MAX);
  }

  return true;
}

bool saveConfig() {
  StaticJsonDocument<512> doc;
  doc["ratio0"]  = cfg.ratio[0];
  doc["ratio1"]  = cfg.ratio[1];
  doc["staSsid"] = cfg.staSsid;
  doc["staPass"] = cfg.staPass;
  doc["apSsid"]  = cfg.apSsid;
  doc["apPass"]  = cfg.apPass;

  File f = LittleFS.open(CONFIG_PATH, "w");
  if (!f) {
    return false;
  }
  const bool ok = serializeJson(doc, f) > 0;
  f.close();
  return ok;
}

// ─── Filtrage / lissage (monde best-effort, appelé depuis loop) ───────────────
uint32_t smoothPeriod(uint8_t ch, uint32_t raw) {
  static uint32_t buf[NUM_CHANNELS][PERIOD_AVG_SAMPLES];
  static uint8_t  idx[NUM_CHANNELS] = {0, 0};
  static uint8_t  count[NUM_CHANNELS] = {0, 0};

  buf[ch][idx[ch]] = raw;
  idx[ch] = (idx[ch] + 1) % PERIOD_AVG_SAMPLES;
  if (count[ch] < PERIOD_AVG_SAMPLES) {
    count[ch]++;
  }

  uint64_t sum = 0;
  for (uint8_t i = 0; i < count[ch]; i++) {
    sum += buf[ch][i];
  }
  return (uint32_t)(sum / count[ch]);
}

uint32_t slewLimit(uint32_t current, uint32_t target) {
  if (current == 0) {
    return target;
  }
  const float maxDelta = (float)current * PERIOD_SLEW_MAX;
  if (target > current) {
    const uint32_t delta = target - current;
    return (delta > (uint32_t)maxDelta) ? current + (uint32_t)maxDelta : target;
  }
  const uint32_t delta = current - target;
  return (delta > (uint32_t)maxDelta) ? current - (uint32_t)maxDelta : target;
}

uint32_t periodToRpm(uint32_t periodUs) {
  if (periodUs == 0) {
    return 0;
  }
  // RPM = (f / 2) × 60 ; T_edge = 30/RPM → RPM = 30e6 / periodUs
  return (uint32_t)(30000000UL / periodUs);
}

void updateRealtimeTargets() {
  const uint32_t now = micros();
  bool anyActive = false;

  noInterrupts();
  const uint32_t snapRaw0 = g_rawPeriodUs[0];
  const uint32_t snapRaw1 = g_rawPeriodUs[1];
  const uint32_t snapLast0 = g_lastEdgeUs[0];
  const uint32_t snapLast1 = g_lastEdgeUs[1];
  interrupts();

  const uint32_t raw[NUM_CHANNELS] = {snapRaw0, snapRaw1};
  const uint32_t lastEdge[NUM_CHANNELS] = {snapLast0, snapLast1};

  static uint32_t smoothed[NUM_CHANNELS] = {0, 0};
  static uint32_t outTarget[NUM_CHANNELS] = {0, 0};

  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    const bool timedOut = (lastEdge[ch] == 0) ||
                          ((now - lastEdge[ch]) > INPUT_TIMEOUT_US);

    if (timedOut || raw[ch] == 0) {
      g_channelActive[ch] = false;
      g_displayInputPeriodUs[ch] = 0;
      g_displayOutputPeriodUs[ch] = 0;

      noInterrupts();
      g_outHalfPeriodUs[ch] = 0;
      g_outLevel[ch] = 0;
      digitalWrite(ch == 0 ? PIN_TACH_OUT_CH0 : PIN_TACH_OUT_CH1, LOW);
      interrupts();
      continue;
    }

    g_channelActive[ch] = true;
    anyActive = true;

    smoothed[ch] = smoothPeriod(ch, raw[ch]);
    g_displayInputPeriodUs[ch] = smoothed[ch];

    const float ratio = cfg.ratio[ch];
    uint32_t targetHalf = (uint32_t)((float)smoothed[ch] / (2.0f * ratio));
    if (targetHalf < (TIMER_TICK_US * 2)) {
      targetHalf = TIMER_TICK_US * 2;
    }

    outTarget[ch] = slewLimit(outTarget[ch], targetHalf);
    g_displayOutputPeriodUs[ch] = outTarget[ch] * 2;

    noInterrupts();
    if (g_outHalfPeriodUs[ch] == 0) {
      g_nextToggleUs[ch] = now + outTarget[ch];
      g_outLevel[ch] = 0;
      digitalWrite(ch == 0 ? PIN_TACH_OUT_CH0 : PIN_TACH_OUT_CH1, LOW);
    }
    g_outHalfPeriodUs[ch] = outTarget[ch];
    interrupts();
  }

  // LED pilotée uniquement depuis loop (best-effort)
  static uint32_t lastLedToggle = 0;
  static bool ledState = false;

  if (anyActive) {
    digitalWrite(PIN_LED_STATUS, LOW); // allumée fixe (active LOW)
  } else {
    if (millis() - lastLedToggle > 500) {
      lastLedToggle = millis();
      ledState = !ledState;
      digitalWrite(PIN_LED_STATUS, ledState ? LOW : HIGH);
    }
  }
}

// ─── WiFi non bloquant ────────────────────────────────────────────────────────
enum WifiState { WIFI_IDLE, WIFI_CONNECTING, WIFI_CONNECTED, WIFI_FAILED };
WifiState wifiState = WIFI_IDLE;
uint32_t wifiConnectStart = 0;

void startWifi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(cfg.apSsid, cfg.apPass);

  if (strlen(cfg.staSsid) > 0) {
    WiFi.begin(cfg.staSsid, cfg.staPass);
    wifiState = WIFI_CONNECTING;
    wifiConnectStart = millis();
  } else {
    wifiState = WIFI_IDLE;
  }
}

void serviceWifi() {
  if (wifiState == WIFI_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiState = WIFI_CONNECTED;
    } else if (millis() - wifiConnectStart > 20000) {
      wifiState = WIFI_FAILED;
    }
  }
}

// ─── Serveur web async ────────────────────────────────────────────────────────
AsyncWebServer server(80);

String buildStatusJson() {
  StaticJsonDocument<384> doc;
  JsonArray inRpm = doc.createNestedArray("inputRpm");
  JsonArray outRpm = doc.createNestedArray("outputRpm");
  JsonArray active = doc.createNestedArray("active");

  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    inRpm.add(periodToRpm(g_displayInputPeriodUs[ch]));
    outRpm.add(periodToRpm(g_displayOutputPeriodUs[ch]));
    active.add(g_channelActive[ch]);
  }

  doc["ratio0"] = cfg.ratio[0];
  doc["ratio1"] = cfg.ratio[1];
  doc["wifiSta"] = (WiFi.status() == WL_CONNECTED) ? cfg.staSsid : "";
  doc["wifiAp"] = cfg.apSsid;
  doc["apIp"] = WiFi.softAPIP().toString();

  String out;
  serializeJson(doc, out);
  return out;
}

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Deye Fan Tach Simulator</title>
<style>
body{font-family:sans-serif;max-width:640px;margin:1rem auto;padding:0 1rem}
table{border-collapse:collapse;width:100%;margin:1rem 0}
td,th{border:1px solid #ccc;padding:.4rem;text-align:center}
label{display:block;margin:.5rem 0}
input[type=number],input[type=text],input[type=password]{width:100%;box-sizing:border-box}
button{margin-top:.5rem;padding:.5rem 1rem}
.status{background:#f4f4f4;padding:.75rem;border-radius:4px;font-family:monospace;font-size:.9rem}
</style>
</head>
<body>
<h1>Simulateur tach Deye</h1>
<div class="status" id="status">Chargement…</div>
<h2>Ratios</h2>
<form id="ratioForm">
<label>Ratio canal 0 (9 cm) <input type="number" id="ratio0" step="0.1" min="1" max="4"></label>
<label>Ratio canal 1 (6 cm) <input type="number" id="ratio1" step="0.1" min="1" max="4"></label>
<button type="submit">Enregistrer ratios</button>
</form>
<h2>WiFi</h2>
<form id="wifiForm">
<label>SSID STA <input type="text" id="staSsid"></label>
<label>Mot de passe STA <input type="password" id="staPass"></label>
<label>SSID AP <input type="text" id="apSsid"></label>
<label>Mot de passe AP <input type="password" id="apPass"></label>
<button type="submit">Enregistrer WiFi (redémarrage)</button>
</form>
<script>
async function refresh(){
  const r=await fetch('/api/status');const d=await r.json();
  document.getElementById('status').innerHTML=
    'Canal 0 — entrée: '+d.inputRpm[0]+' RPM, sortie: '+d.outputRpm[0]+' RPM, actif: '+d.active[0]+'<br>'+
    'Canal 1 — entrée: '+d.inputRpm[1]+' RPM, sortie: '+d.outputRpm[1]+' RPM, actif: '+d.active[1]+'<br>'+
    'AP: '+d.apIp+' | STA: '+(d.wifiSta||'—');
  document.getElementById('ratio0').value=d.ratio0;
  document.getElementById('ratio1').value=d.ratio1;
}
document.getElementById('ratioForm').onsubmit=async e=>{
  e.preventDefault();
  const body=new URLSearchParams({
    ratio0:document.getElementById('ratio0').value,
    ratio1:document.getElementById('ratio1').value
  });
  await fetch('/api/save-ratios',{method:'POST',body});
  refresh();
};
document.getElementById('wifiForm').onsubmit=async e=>{
  e.preventDefault();
  const body=new URLSearchParams({
    staSsid:document.getElementById('staSsid').value,
    staPass:document.getElementById('staPass').value,
    apSsid:document.getElementById('apSsid').value,
    apPass:document.getElementById('apPass').value
  });
  await fetch('/api/save-wifi',{method:'POST',body});
  alert('WiFi enregistré. Redémarrage…');
};
setInterval(refresh,1000);refresh();
</script>
</body>
</html>
)rawliteral";

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send(200, "application/json", buildStatusJson());
  });

  server.on("/api/save-ratios", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (req->hasParam("ratio0", true)) {
      cfg.ratio[0] = constrain(req->getParam("ratio0", true)->value().toFloat(), RATIO_MIN, RATIO_MAX);
    }
    if (req->hasParam("ratio1", true)) {
      cfg.ratio[1] = constrain(req->getParam("ratio1", true)->value().toFloat(), RATIO_MIN, RATIO_MAX);
    }
    saveConfig();
    req->send(200, "text/plain", "OK");
  });

  server.on("/api/save-wifi", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (req->hasParam("staSsid", true)) {
      strncpy(cfg.staSsid, req->getParam("staSsid", true)->value().c_str(), sizeof(cfg.staSsid));
    }
    if (req->hasParam("staPass", true)) {
      strncpy(cfg.staPass, req->getParam("staPass", true)->value().c_str(), sizeof(cfg.staPass));
    }
    if (req->hasParam("apSsid", true)) {
      strncpy(cfg.apSsid, req->getParam("apSsid", true)->value().c_str(), sizeof(cfg.apSsid));
    }
    if (req->hasParam("apPass", true)) {
      strncpy(cfg.apPass, req->getParam("apPass", true)->value().c_str(), sizeof(cfg.apPass));
    }
    saveConfig();
    req->send(200, "text/plain", "OK");
    delay(300);
    ESP.restart();
  });

  server.begin();
}

// ─── Setup / Loop ─────────────────────────────────────────────────────────────
void setup() {
  pinMode(PIN_TACH_IN_CH0, INPUT);
  pinMode(PIN_TACH_IN_CH1, INPUT);
  pinMode(PIN_TACH_OUT_CH0, OUTPUT);
  pinMode(PIN_TACH_OUT_CH1, OUTPUT);
  pinMode(PIN_LED_STATUS, OUTPUT);

  digitalWrite(PIN_TACH_OUT_CH0, LOW);  // transistor OFF → ligne haute (collecteur relâché)
  digitalWrite(PIN_TACH_OUT_CH1, LOW);
  digitalWrite(PIN_LED_STATUS, HIGH);   // LED éteinte au boot

  Serial.begin(115200);
  Serial.println(F("Deye Fan Tach Simulator — démarrage"));

  if (!LittleFS.begin()) {
    Serial.println(F("LittleFS mount failed — format"));
    LittleFS.format();
    LittleFS.begin();
  }
  loadConfig();

  attachInterrupt(digitalPinToInterrupt(PIN_TACH_IN_CH0), isrTachCh0, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_TACH_IN_CH1), isrTachCh1, FALLING);

  // Timer1 : horloge 80 MHz / 16 = 5 MHz → 1 tick = 0,2 µs ; 50 µs = 250 ticks
  timer1_attachInterrupt(onTimer1Tick);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
  timer1_write(TIMER_TICK_US * 5);

  startWifi();
  setupWebServer();

  Serial.println(F("Timer1 + ISR actifs — génération isolée du WiFi"));
}

void loop() {
  serviceWifi();
  updateRealtimeTargets();
  yield(); // laisse le stack TCP/IP respirer sans bloquer le Timer1
}
