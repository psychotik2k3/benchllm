/*
  Deye fan tachometer multiplier - Wemos D1 mini / ESP8266

  Board: LOLIN(WEMOS) D1 R2 & mini
  Required ESP8266 Arduino core: 3.x

  Pins:
    D5 / GPIO14 : 9 cm Noctua tach input (through the NPN interface)
    D6 / GPIO12 : 6 cm Noctua tach input (through the NPN interface)
    D1 / GPIO5  : 9 cm simulated tach output (to NPN open collector)
    D2 / GPIO4  : 6 cm simulated tach output (to NPN open collector)
    D4 / GPIO2  : built-in active-low status LED

  The two output waveforms are generated exclusively by hardware Timer1.
  The timer ISR and all data it touches reside in IRAM/RAM; HTTP and Wi-Fi
  code never toggle an output pin. No delay(), digitalWrite(), allocation,
  floating-point operation or flash access occurs in the timer ISR.

  Noctua tach outputs provide two pulses per revolution. The simulated
  output keeps the same 2-PPR convention and multiplies its frequency.
*/

#include <Arduino.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

namespace {

constexpr uint8_t CHANNELS = 2;
constexpr uint8_t TACH_IN_PIN[CHANNELS] = {D5, D6};
constexpr uint8_t TACH_OUT_PIN[CHANNELS] = {D1, D2};
constexpr uint8_t STATUS_LED_PIN = LED_BUILTIN;

constexpr uint32_t CONFIG_MAGIC = 0x44465945UL;  // "DFYE"
constexpr uint16_t CONFIG_VERSION = 1;
constexpr uint32_t SIGNAL_TIMEOUT_US = 2000000UL;
constexpr uint32_t MIN_INPUT_PERIOD_US = 500UL;
constexpr uint32_t MAX_INPUT_PERIOD_US = 1500000UL;
constexpr float MIN_RATIO = 0.10f;
constexpr float MAX_RATIO = 20.0f;

// Timer1 clock: 80 MHz / 16 = 5 ticks/us, 23-bit one-shot counter.
constexpr uint32_t TIMER_TICKS_PER_US = 5UL;
constexpr uint32_t MIN_HALF_PERIOD_TICKS = 250UL;       // 50 us
constexpr uint32_t IDLE_TIMER_TICKS = 50000UL;          // 10 ms
constexpr uint32_t MAX_TIMER_TICKS = 0x7FFFFFUL;

struct PersistedConfig {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  float ratio[CHANNELS];
  char staSsid[33];
  char staPassword[65];
  char apSsid[33];
  char apPassword[65];
  uint32_t crc;
};

struct RuntimeChannel {
  uint32_t processedEdgeCount = 0;
  float filteredPeriodUs = 0.0f;
  float measuredRpm = 0.0f;
  float simulatedRpm = 0.0f;
  bool signalPresent = false;
};

PersistedConfig config;
RuntimeChannel runtimeChannel[CHANNELS];
ESP8266WebServer server(80);

// Written by GPIO ISRs and read by loop(). Values are copied atomically.
volatile uint32_t inputPreviousEdgeUs[CHANNELS] = {0, 0};
volatile uint32_t inputLastValidEdgeUs[CHANNELS] = {0, 0};
volatile uint32_t inputPeriodUs[CHANNELS] = {0, 0};
volatile uint32_t inputEdgeCount[CHANNELS] = {0, 0};

// Main code only writes requestedHalfTicks (atomic aligned 32-bit stores).
// Timer1 ISR exclusively owns all other output timing state.
// outputGpioMask is deliberately non-const so it remains in DRAM: Timer1 must
// never need flash/cache access while Wi-Fi or EEPROM code is using flash.
volatile uint32_t outputGpioMask[CHANNELS] = {
    1UL << 5, 1UL << 4};
volatile uint32_t requestedHalfTicks[CHANNELS] = {0, 0};
volatile uint32_t remainingTicks[CHANNELS] = {
    IDLE_TIMER_TICKS, IDLE_TIMER_TICKS};
volatile bool timerChannelEnabled[CHANNELS] = {false, false};
volatile bool outputPinHigh[CHANNELS] = {false, false};
volatile uint32_t armedTimerTicks = IDLE_TIMER_TICKS;

bool restartPending = false;
uint32_t restartAtMs = 0;

uint32_t crc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xFFFFFFFFUL;
  while (length--) {
    crc ^= *data++;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
  }
  return ~crc;
}

void setDefaultConfig() {
  memset(&config, 0, sizeof(config));
  config.magic = CONFIG_MAGIC;
  config.version = CONFIG_VERSION;
  config.size = sizeof(config);
  config.ratio[0] = 2.0f;
  config.ratio[1] = 2.0f;
  strlcpy(config.apSsid, "Deye-Fan-Simulator", sizeof(config.apSsid));
  strlcpy(config.apPassword, "deye-fan", sizeof(config.apPassword));
}

bool configIsValid() {
  if (config.magic != CONFIG_MAGIC ||
      config.version != CONFIG_VERSION ||
      config.size != sizeof(config)) {
    return false;
  }
  const uint32_t expected =
      crc32(reinterpret_cast<const uint8_t*>(&config),
            offsetof(PersistedConfig, crc));
  if (expected != config.crc) {
    return false;
  }
  for (uint8_t i = 0; i < CHANNELS; ++i) {
    if (!isfinite(config.ratio[i]) ||
        config.ratio[i] < MIN_RATIO ||
        config.ratio[i] > MAX_RATIO) {
      return false;
    }
  }
  const size_t apPasswordLength = strnlen(config.apPassword,
                                           sizeof(config.apPassword));
  return config.apSsid[0] != '\0' &&
         (apPasswordLength == 0 || apPasswordLength >= 8);
}

void saveConfig() {
  config.magic = CONFIG_MAGIC;
  config.version = CONFIG_VERSION;
  config.size = sizeof(config);
  config.crc = crc32(reinterpret_cast<const uint8_t*>(&config),
                     offsetof(PersistedConfig, crc));
  EEPROM.put(0, config);
  EEPROM.commit();
}

void loadConfig() {
  EEPROM.begin(sizeof(PersistedConfig));
  EEPROM.get(0, config);
  if (!configIsValid()) {
    setDefaultConfig();
    saveConfig();
  }
}

// The input interface inverts the fan tach waveform. Either edge gives the
// same complete-period measurement, so RISING is used on the ESP side.
void IRAM_ATTR recordInputEdge(uint8_t channel) {
  const uint32_t now = micros();
  const uint32_t previous = inputPreviousEdgeUs[channel];
  inputPreviousEdgeUs[channel] = now;
  if (previous == 0) {
    return;
  }

  const uint32_t period = now - previous;  // Wrap-safe unsigned subtraction.
  if (period >= MIN_INPUT_PERIOD_US && period <= MAX_INPUT_PERIOD_US) {
    inputPeriodUs[channel] = period;
    inputLastValidEdgeUs[channel] = now;
    ++inputEdgeCount[channel];
  }
}

void IRAM_ATTR tachInput9Isr() {
  recordInputEdge(0);
}

void IRAM_ATTR tachInput6Isr() {
  recordInputEdge(1);
}

inline void IRAM_ATTR releaseOutput(uint8_t channel) {
  // GPIO low switches the external output NPN off: Deye tach is released.
  GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, outputGpioMask[channel]);
  outputPinHigh[channel] = false;
}

inline void IRAM_ATTR toggleOutput(uint8_t channel) {
  const uint32_t mask = outputGpioMask[channel];
  if (outputPinHigh[channel]) {
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, mask);
    outputPinHigh[channel] = false;
  } else {
    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, mask);
    outputPinHigh[channel] = true;
  }
}

void IRAM_ATTR outputTimerIsr() {
  const uint32_t elapsed = armedTimerTicks;
  uint32_t nextInterrupt = IDLE_TIMER_TICKS;

  for (uint8_t channel = 0; channel < CHANNELS; ++channel) {
    const uint32_t targetHalfPeriod = requestedHalfTicks[channel];

    if (targetHalfPeriod == 0) {
      if (timerChannelEnabled[channel]) {
        timerChannelEnabled[channel] = false;
        releaseOutput(channel);
      }
      remainingTicks[channel] = IDLE_TIMER_TICKS;
      continue;
    }

    if (!timerChannelEnabled[channel]) {
      timerChannelEnabled[channel] = true;
      remainingTicks[channel] = MIN_HALF_PERIOD_TICKS;
      releaseOutput(channel);
    } else if (remainingTicks[channel] <= elapsed) {
      toggleOutput(channel);
      // Apply a newly requested period only at a waveform edge.
      remainingTicks[channel] = targetHalfPeriod;
    } else {
      remainingTicks[channel] -= elapsed;
    }

    if (remainingTicks[channel] < nextInterrupt) {
      nextInterrupt = remainingTicks[channel];
    }
  }

  if (nextInterrupt < MIN_HALF_PERIOD_TICKS) {
    nextInterrupt = MIN_HALF_PERIOD_TICKS;
  } else if (nextInterrupt > MAX_TIMER_TICKS) {
    nextInterrupt = MAX_TIMER_TICKS;
  }
  armedTimerTicks = nextInterrupt;
  timer1_write(nextInterrupt);
}

void setupSignalEngine() {
  for (uint8_t channel = 0; channel < CHANNELS; ++channel) {
    pinMode(TACH_IN_PIN[channel], INPUT);
    pinMode(TACH_OUT_PIN[channel], OUTPUT);
    digitalWrite(TACH_OUT_PIN[channel], LOW);
  }
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, HIGH);

  attachInterrupt(digitalPinToInterrupt(TACH_IN_PIN[0]), tachInput9Isr, RISING);
  attachInterrupt(digitalPinToInterrupt(TACH_IN_PIN[1]), tachInput6Isr, RISING);

  timer1_disable();
  timer1_isr_init();
  timer1_attachInterrupt(outputTimerIsr);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
  armedTimerTicks = IDLE_TIMER_TICKS;
  timer1_write(armedTimerTicks);
}

String htmlEscape(const char* input) {
  String result;
  result.reserve(strlen(input) + 8);
  for (const char* p = input; *p; ++p) {
    switch (*p) {
      case '&': result += F("&amp;"); break;
      case '<': result += F("&lt;"); break;
      case '>': result += F("&gt;"); break;
      case '"': result += F("&quot;"); break;
      case '\'': result += F("&#39;"); break;
      default: result += *p;
    }
  }
  return result;
}

String jsonEscape(const String& input) {
  String result;
  result.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); ++i) {
    const char c = input[i];
    if (c == '"' || c == '\\') {
      result += '\\';
      result += c;
    } else if (c == '\n') {
      result += F("\\n");
    } else if (static_cast<uint8_t>(c) >= 0x20) {
      result += c;
    }
  }
  return result;
}

void handleRoot() {
  String page;
  page.reserve(7000);
  page += F(
      "<!doctype html><html lang='fr'><head><meta charset='utf-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Deye Fan Simulator</title><style>"
      "body{font:16px system-ui;max-width:780px;margin:auto;padding:18px;"
      "background:#101820;color:#eef}fieldset{margin:16px 0;padding:14px;"
      "border:1px solid #456;border-radius:8px}label{display:block;margin:9px 0}"
      "input{width:100%;box-sizing:border-box;padding:8px;margin-top:3px}"
      "button{padding:10px 18px;background:#39a;color:#fff;border:0;"
      "border-radius:5px}.cards{display:grid;grid-template-columns:1fr 1fr;"
      "gap:12px}.card{background:#1d2b36;padding:12px;border-radius:8px}"
      ".ok{color:#5eeb8f}.wait{color:#ffca5c}small{color:#abc}"
      "@media(max-width:550px){.cards{grid-template-columns:1fr}}</style>"
      "</head><body><h1>Deye Fan Simulator</h1>"
      "<div class='cards'><div class='card'><b>Ventilateur 9 cm</b>"
      "<p id='c0'>Chargement...</p></div><div class='card'>"
      "<b>Ventilateur 6 cm</b><p id='c1'>Chargement...</p></div></div>"
      "<form id='cfg'><fieldset><legend>Multiplicateurs</legend>"
      "<label>Ratio 9 cm<input name='ratio9' type='number' min='.1' max='20' "
      "step='.01' value='");
  page += String(config.ratio[0], 2);
  page += F("'></label><label>Ratio 6 cm<input name='ratio6' type='number' "
            "min='.1' max='20' step='.01' value='");
  page += String(config.ratio[1], 2);
  page += F(
      "'></label></fieldset><fieldset><legend>Wi-Fi station</legend>"
      "<label>SSID<input name='staSsid' maxlength='32' value='");
  page += htmlEscape(config.staSsid);
  page += F(
      "'></label><label>Nouveau mot de passe (vide = inchangé)"
      "<input name='staPassword' type='password' maxlength='64'></label>"
      "<label><input style='width:auto' name='clearStaPassword' type='checkbox' "
      "value='1'> Réseau ouvert / effacer le mot de passe</label></fieldset>"
      "<fieldset><legend>Point d'accès</legend><label>SSID AP"
      "<input name='apSsid' maxlength='32' value='");
  page += htmlEscape(config.apSsid);
  page += F(
      "'></label><label>Nouveau mot de passe AP (8 caractères minimum ; "
      "vide = inchangé)<input name='apPassword' type='password' maxlength='64'>"
      "</label></fieldset><button>Enregistrer</button> <span id='msg'></span>"
      "</form><p><small>L'adresse AP est 192.168.4.1. Un changement Wi-Fi "
      "redémarre le contrôleur.</small></p><script>"
      "async function status(){try{let s=await(await fetch('/api/status')).json();"
      "for(let i=0;i<2;i++){let c=s.channels[i];"
      "document.getElementById('c'+i).innerHTML="
      "\"<span class='\"+(c.active?'ok':'wait')+\"'>\"+"
      "(c.active?'Simulation active':'En attente du tach')+\"</span><br>\"+"
      "\"Lu : \"+c.inputRpm.toFixed(0)+\" RPM<br>Simulé : \"+"
      "c.outputRpm.toFixed(0)+\" RPM<br>Ratio : ×\"+c.ratio.toFixed(2);}"
      "}catch(e){}setTimeout(status,1000)}status();"
      "document.getElementById('cfg').onsubmit=async e=>{e.preventDefault();"
      "let m=document.getElementById('msg');m.textContent='Enregistrement…';"
      "let r=await fetch('/api/config',{method:'POST',body:new URLSearchParams("
      "new FormData(e.target))});m.textContent=await r.text()};"
      "</script></body></html>");
  server.send(200, F("text/html; charset=utf-8"), page);
}

void handleStatus() {
  String json;
  json.reserve(500);
  json += F("{\"channels\":[");
  for (uint8_t i = 0; i < CHANNELS; ++i) {
    if (i) json += ',';
    json += F("{\"active\":");
    json += runtimeChannel[i].signalPresent ? F("true") : F("false");
    json += F(",\"inputRpm\":");
    json += String(runtimeChannel[i].measuredRpm, 1);
    json += F(",\"outputRpm\":");
    json += String(runtimeChannel[i].simulatedRpm, 1);
    json += F(",\"ratio\":");
    json += String(config.ratio[i], 3);
    json += '}';
  }
  json += F("],\"apIp\":\"");
  json += WiFi.softAPIP().toString();
  json += F("\",\"staIp\":\"");
  json += WiFi.localIP().toString();
  json += F("\",\"staConnected\":");
  json += WiFi.status() == WL_CONNECTED ? F("true") : F("false");
  json += F("}");
  server.send(200, F("application/json"), json);
}

bool copyArgument(const String& name, char* destination, size_t capacity) {
  if (!server.hasArg(name)) return false;
  const String value = server.arg(name);
  if (value.length() >= capacity) return false;
  strlcpy(destination, value.c_str(), capacity);
  return true;
}

void handleConfig() {
  const float ratio9 = server.arg("ratio9").toFloat();
  const float ratio6 = server.arg("ratio6").toFloat();
  if (!isfinite(ratio9) || !isfinite(ratio6) ||
      ratio9 < MIN_RATIO || ratio9 > MAX_RATIO ||
      ratio6 < MIN_RATIO || ratio6 > MAX_RATIO) {
    server.send(400, F("text/plain; charset=utf-8"),
                F("Ratio invalide (plage 0,10 à 20,00)."));
    return;
  }

  PersistedConfig candidate = config;
  candidate.ratio[0] = ratio9;
  candidate.ratio[1] = ratio6;

  if (!copyArgument("staSsid", candidate.staSsid,
                    sizeof(candidate.staSsid)) ||
      !copyArgument("apSsid", candidate.apSsid,
                    sizeof(candidate.apSsid))) {
    server.send(400, F("text/plain; charset=utf-8"), F("SSID invalide."));
    return;
  }
  if (candidate.apSsid[0] == '\0') {
    server.send(400, F("text/plain; charset=utf-8"),
                F("Le SSID du point d'accès ne peut pas être vide."));
    return;
  }

  if (server.hasArg("clearStaPassword")) {
    candidate.staPassword[0] = '\0';
  } else if (server.hasArg("staPassword") &&
             server.arg("staPassword").length() > 0) {
    if (!copyArgument("staPassword", candidate.staPassword,
                      sizeof(candidate.staPassword))) {
      server.send(400, F("text/plain; charset=utf-8"),
                  F("Mot de passe station trop long."));
      return;
    }
  }

  if (server.hasArg("apPassword") &&
      server.arg("apPassword").length() > 0) {
    const size_t length = server.arg("apPassword").length();
    if (length < 8 || length > 63 ||
        !copyArgument("apPassword", candidate.apPassword,
                      sizeof(candidate.apPassword))) {
      server.send(400, F("text/plain; charset=utf-8"),
                  F("Le mot de passe AP doit contenir 8 à 63 caractères."));
      return;
    }
  }

  const bool wifiChanged =
      strcmp(candidate.staSsid, config.staSsid) != 0 ||
      strcmp(candidate.staPassword, config.staPassword) != 0 ||
      strcmp(candidate.apSsid, config.apSsid) != 0 ||
      strcmp(candidate.apPassword, config.apPassword) != 0;

  config = candidate;
  saveConfig();
  server.send(200, F("text/plain; charset=utf-8"),
              wifiChanged ? F("Enregistré. Redémarrage…") : F("Enregistré."));
  if (wifiChanged) {
    restartPending = true;
    restartAtMs = millis() + 750UL;
  }
}

void setupWebAndWifi() {
  WiFi.persistent(false);  // Credentials are owned by our CRC-protected config.
  WiFi.mode(WIFI_AP_STA);
  WiFi.hostname("deye-fan-simulator");

  const bool openAp = strlen(config.apPassword) == 0;
  if (openAp) {
    WiFi.softAP(config.apSsid);
  } else {
    WiFi.softAP(config.apSsid, config.apPassword);
  }

  if (config.staSsid[0] != '\0') {
    WiFi.begin(config.staSsid, config.staPassword);
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/config", HTTP_POST, handleConfig);
  server.onNotFound([]() {
    server.sendHeader(F("Location"), F("/"), true);
    server.send(302, F("text/plain"), "");
  });
  server.begin();
}

void updateChannel(uint8_t channel, uint32_t nowUs) {
  uint32_t lastEdge;
  uint32_t latestPeriod;
  uint32_t edgeCount;

  noInterrupts();
  lastEdge = inputLastValidEdgeUs[channel];
  latestPeriod = inputPeriodUs[channel];
  edgeCount = inputEdgeCount[channel];
  interrupts();

  RuntimeChannel& state = runtimeChannel[channel];
  const bool fresh = lastEdge != 0 &&
                     (nowUs - lastEdge) < SIGNAL_TIMEOUT_US &&
                     latestPeriod != 0;

  if (!fresh) {
    state.signalPresent = false;
    state.measuredRpm = 0.0f;
    state.simulatedRpm = 0.0f;
    state.filteredPeriodUs = 0.0f;
    state.processedEdgeCount = edgeCount;
    requestedHalfTicks[channel] = 0;
    return;
  }

  if (edgeCount != state.processedEdgeCount) {
    state.processedEdgeCount = edgeCount;
    if (state.filteredPeriodUs == 0.0f) {
      state.filteredPeriodUs = static_cast<float>(latestPeriod);
    } else {
      // Light low-pass filtering suppresses Hall sensor period noise while
      // retaining a quick response to a genuine speed change.
      state.filteredPeriodUs =
          state.filteredPeriodUs * 0.75f + latestPeriod * 0.25f;
    }
  }

  state.signalPresent = true;
  state.measuredRpm = 30000000.0f / state.filteredPeriodUs;  // 2 PPR.
  state.simulatedRpm = state.measuredRpm * config.ratio[channel];

  float halfPeriodUs =
      state.filteredPeriodUs / (2.0f * config.ratio[channel]);
  uint32_t ticks =
      static_cast<uint32_t>(halfPeriodUs * TIMER_TICKS_PER_US + 0.5f);
  if (ticks < MIN_HALF_PERIOD_TICKS) ticks = MIN_HALF_PERIOD_TICKS;
  if (ticks > MAX_TIMER_TICKS) ticks = MAX_TIMER_TICKS;
  requestedHalfTicks[channel] = ticks;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  loadConfig();

  // Start deterministic signal handling before the network stack.
  setupSignalEngine();
  setupWebAndWifi();

  Serial.println();
  Serial.println(F("Deye fan simulator ready"));
  Serial.print(F("AP: http://"));
  Serial.println(WiFi.softAPIP());
}

void loop() {
  const uint32_t nowUs = micros();
  updateChannel(0, nowUs);
  updateChannel(1, nowUs);

  const bool active =
      runtimeChannel[0].signalPresent || runtimeChannel[1].signalPresent;
  digitalWrite(STATUS_LED_PIN, active ? LOW : HIGH);

  server.handleClient();

  if (restartPending &&
      static_cast<int32_t>(millis() - restartAtMs) >= 0) {
    ESP.restart();
  }

  // Feeds the Wi-Fi/TCP stack. Output timing remains owned by Timer1.
  yield();
}
