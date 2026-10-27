/*
  Deye dual fan tachometer simulator
  Target: LOLIN(WEMOS) D1 R2 & mini / ESP8266 Arduino core 3.1.x

  IMPORTANT:
  - The ESP8266 pins are 3.3 V only.
  - Use the input pull-ups and BC337 open-collector output stages documented
    in SCHEMA_ET_MONTAGE.md.
  - Never connect an unknown Deye tach voltage directly to an ESP8266 pin.
*/

#include <Arduino.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <core_esp8266_waveform.h>
#include <stddef.h>

#include "tach_math.h"

namespace {

// Safe ESP8266 boot pins: none of these pins is a boot-strap input.
constexpr uint8_t kTach6InputPin = D1;    // GPIO5
constexpr uint8_t kTach9InputPin = D2;    // GPIO4
constexpr uint8_t kDeye6OutputPin = D5;   // GPIO14 -> BC337 base resistor
constexpr uint8_t kDeye9OutputPin = D6;   // GPIO12 -> BC337 base resistor
constexpr uint8_t kActivityLedPin = D7;   // GPIO13 -> external LED, active high

constexpr float kDefaultRatio6 = 2.50f;
constexpr float kDefaultRatio9 = 2.00f;

// Input periods outside this range are treated as noise or a stopped fan.
constexpr uint32_t kMinInputPeriodUs = 1000UL;     // 30,000 RPM maximum
constexpr uint32_t kMaxInputPeriodUs = 2000000UL;  // 15 RPM minimum
constexpr uint32_t kSignalTimeoutMs = 2000UL;
constexpr uint32_t kChannelUpdateMs = 200UL;
constexpr float kPeriodFilterAlpha = 0.25f;

// Protect the software waveform engine from unreasonable web configuration.
constexpr uint32_t kMinOutputHalfPeriodUs = 250UL;
constexpr uint32_t kMaxOutputHalfPeriodUs = 2000000UL;

constexpr size_t kEepromSize = 512;
constexpr uint32_t kConfigMagic = 0x44465945UL;  // "DFYE"
constexpr uint16_t kConfigVersion = 1;

constexpr char kAccessPointPassword[] = "DeyeFanSetup";

struct TachCapture {
  volatile uint32_t lastEdgeCycles;
  volatile uint32_t periodSumCycles;
  volatile uint16_t sampleCount;
};

struct ChannelRuntime {
  TachCapture* capture;
  uint8_t outputPin;
  float ratio;
  float filteredPeriodUs;
  float measuredRpm;
  float simulatedRpm;
  uint32_t outputHalfPeriodUs;
  uint32_t lastValidPulseMs;
  bool outputRunning;
};

struct PersistedConfig {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  char staSsid[33];
  char staPassword[65];
  float ratio6;
  float ratio9;
  uint32_t crc32;
};

static_assert(sizeof(PersistedConfig) <= kEepromSize,
              "Persisted configuration exceeds EEPROM allocation");

TachCapture gCapture6 = {0, 0, 0};
TachCapture gCapture9 = {0, 0, 0};

ChannelRuntime gChannel6 = {
    &gCapture6, kDeye6OutputPin, kDefaultRatio6, 0.0f, 0.0f, 0.0f, 0, 0, false};
ChannelRuntime gChannel9 = {
    &gCapture9, kDeye9OutputPin, kDefaultRatio9, 0.0f, 0.0f, 0.0f, 0, 0, false};

PersistedConfig gConfig;
ESP8266WebServer gServer(80);

uint32_t gCyclesPerUs = 80;
uint32_t gMinInputPeriodCycles = 0;
uint32_t gMaxInputPeriodCycles = 0;
uint32_t gLastChannelUpdateMs = 0;
uint32_t gApplyWifiAtMs = 0;
bool gWifiApplyPending = false;
char gAccessPointSsid[24] = {};

const char kIndexHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="fr">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Simulateur tach Deye</title>
  <style>
    :root { color-scheme: dark; font-family: system-ui, sans-serif; }
    body { margin: 0; background: #111827; color: #e5e7eb; }
    main { max-width: 780px; margin: auto; padding: 18px; }
    h1 { font-size: 1.55rem; }
    h2 { font-size: 1.1rem; margin-top: 0; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit,minmax(250px,1fr)); gap: 14px; }
    .card { background: #1f2937; border: 1px solid #374151; border-radius: 12px; padding: 16px; }
    .rpm { font-size: 1.7rem; font-weight: 700; color: #67e8f9; }
    .muted { color: #9ca3af; }
    .active { color: #86efac; }
    .waiting { color: #fcd34d; }
    label { display: block; margin: 12px 0 5px; }
    input { box-sizing: border-box; width: 100%; padding: 9px; color: #111827; border-radius: 6px; border: 0; }
    button { margin-top: 16px; padding: 10px 16px; border: 0; border-radius: 7px; background: #0891b2; color: white; font-weight: 700; }
    code { color: #a5f3fc; }
  </style>
</head>
<body>
<main>
  <h1>Simulateur tachymétrique Deye</h1>
  <div class="grid">
    <section class="card">
      <h2>Canal 6 cm</h2>
      <div class="rpm"><span id="in6">0</span> RPM lus</div>
      <div><span id="out6">0</span> RPM simulés · ratio <span id="showRatio6">2.50</span>×</div>
      <p id="state6" class="waiting">En attente du signal</p>
    </section>
    <section class="card">
      <h2>Canal 9 cm</h2>
      <div class="rpm"><span id="in9">0</span> RPM lus</div>
      <div><span id="out9">0</span> RPM simulés · ratio <span id="showRatio9">2.00</span>×</div>
      <p id="state9" class="waiting">En attente du signal</p>
    </section>
  </div>

  <section class="card" style="margin-top:14px">
    <h2>Configuration</h2>
    <form method="post" action="/save">
      <label for="ratio6">Ratio 6 cm (0,10 à 10,00)</label>
      <input id="ratio6" name="ratio6" type="number" min="0.1" max="10" step="0.01" required>
      <label for="ratio9">Ratio 9 cm (0,10 à 10,00)</label>
      <input id="ratio9" name="ratio9" type="number" min="0.1" max="10" step="0.01" required>
      <label for="ssid">SSID Wi-Fi station</label>
      <input id="ssid" name="ssid" maxlength="32" autocomplete="off">
      <label for="password">Mot de passe (vide = conserver si le SSID ne change pas)</label>
      <input id="password" name="password" type="password" maxlength="64" autocomplete="new-password">
      <label><input name="clear_password" type="checkbox" value="1" style="width:auto"> Effacer le mot de passe enregistré</label>
      <button type="submit">Enregistrer et appliquer</button>
    </form>
  </section>

  <section class="card" style="margin-top:14px">
    <h2>Réseau</h2>
    <div>AP : <code id="ap">—</code> · <code id="apIp">—</code></div>
    <div>STA : <span id="staState">—</span> · <code id="staIp">—</code></div>
  </section>
</main>
<script>
let initialized = false;
function channel(prefix, data) {
  document.getElementById('in' + prefix).textContent = data.rpmIn.toFixed(0);
  document.getElementById('out' + prefix).textContent = data.rpmOut.toFixed(0);
  document.getElementById('showRatio' + prefix).textContent = data.ratio.toFixed(2);
  const state = document.getElementById('state' + prefix);
  state.textContent = data.active ? 'Simulation active' : 'En attente du signal';
  state.className = data.active ? 'active' : 'waiting';
}
async function refresh() {
  try {
    const response = await fetch('/api/status', {cache: 'no-store'});
    const data = await response.json();
    channel('6', data.ch6);
    channel('9', data.ch9);
    document.getElementById('ap').textContent = data.wifi.apSsid;
    document.getElementById('apIp').textContent = data.wifi.apIp;
    document.getElementById('staState').textContent = data.wifi.staState;
    document.getElementById('staIp').textContent = data.wifi.staIp || '—';
    if (!initialized) {
      document.getElementById('ratio6').value = data.ch6.ratio.toFixed(2);
      document.getElementById('ratio9').value = data.ch9.ratio.toFixed(2);
      document.getElementById('ssid').value = data.wifi.staSsid;
      initialized = true;
    }
  } catch (_) {}
}
refresh();
setInterval(refresh, 1000);
</script>
</body>
</html>
)HTML";

uint32_t calculateCrc32(const uint8_t* data, size_t length) {
  uint32_t crc = 0xFFFFFFFFUL;
  while (length-- > 0) {
    crc ^= *data++;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1U) ^ (0xEDB88320UL & (0U - (crc & 1U)));
    }
  }
  return ~crc;
}

uint32_t configCrc(const PersistedConfig& config) {
  return calculateCrc32(reinterpret_cast<const uint8_t*>(&config),
                        offsetof(PersistedConfig, crc32));
}

void copyStringToBuffer(const String& source, char* destination,
                        size_t destinationSize) {
  if (destinationSize == 0) {
    return;
  }
  memset(destination, 0, destinationSize);
  source.substring(0, destinationSize - 1).toCharArray(destination,
                                                       destinationSize);
}

void setDefaultConfig() {
  memset(&gConfig, 0, sizeof(gConfig));
  gConfig.magic = kConfigMagic;
  gConfig.version = kConfigVersion;
  gConfig.ratio6 = kDefaultRatio6;
  gConfig.ratio9 = kDefaultRatio9;
  gConfig.crc32 = configCrc(gConfig);
}

void saveConfig() {
  gConfig.magic = kConfigMagic;
  gConfig.version = kConfigVersion;
  gConfig.crc32 = configCrc(gConfig);
  EEPROM.put(0, gConfig);
  EEPROM.commit();
}

void loadConfig() {
  EEPROM.get(0, gConfig);
  const bool valid =
      gConfig.magic == kConfigMagic && gConfig.version == kConfigVersion &&
      gConfig.crc32 == configCrc(gConfig) &&
      TachMath::ratioIsValid(gConfig.ratio6) &&
      TachMath::ratioIsValid(gConfig.ratio9) &&
      memchr(gConfig.staSsid, '\0', sizeof(gConfig.staSsid)) != nullptr &&
      memchr(gConfig.staPassword, '\0', sizeof(gConfig.staPassword)) != nullptr;

  if (!valid) {
    setDefaultConfig();
    saveConfig();
  }

  gChannel6.ratio = gConfig.ratio6;
  gChannel9.ratio = gConfig.ratio9;
}

static inline void IRAM_ATTR captureTachEdge(
    volatile TachCapture* capture) {
  const uint32_t nowCycles = ESP.getCycleCount();
  const uint32_t previousCycles = capture->lastEdgeCycles;

  if (previousCycles == 0) {
    capture->lastEdgeCycles = nowCycles;
    return;
  }

  const uint32_t elapsedCycles = nowCycles - previousCycles;
  if (elapsedCycles < gMinInputPeriodCycles) {
    return;  // Reject a short noise pulse without moving the valid edge.
  }

  capture->lastEdgeCycles = nowCycles;
  if (elapsedCycles > gMaxInputPeriodCycles) {
    capture->periodSumCycles = 0;
    capture->sampleCount = 0;
    return;
  }

  if (capture->sampleCount >= 1000 ||
      capture->periodSumCycles > (0xFFFFFFFFUL - elapsedCycles)) {
    capture->periodSumCycles = elapsedCycles;
    capture->sampleCount = 1;
  } else {
    capture->periodSumCycles += elapsedCycles;
    ++capture->sampleCount;
  }
}

void IRAM_ATTR onTach6Edge() {
  captureTachEdge(&gCapture6);
}

void IRAM_ATTR onTach9Edge() {
  captureTachEdge(&gCapture9);
}

void stopChannelOutput(ChannelRuntime& channel) {
  if (channel.outputRunning) {
    stopWaveform(channel.outputPin);
    channel.outputRunning = false;
  }
  digitalWrite(channel.outputPin, LOW);  // LOW keeps the BC337 switched off.
  channel.outputHalfPeriodUs = 0;
  channel.filteredPeriodUs = 0.0f;
  channel.measuredRpm = 0.0f;
  channel.simulatedRpm = 0.0f;
}

void consumeChannelSamples(ChannelRuntime& channel, uint32_t nowMs) {
  uint32_t periodSumCycles;
  uint16_t sampleCount;

  noInterrupts();
  periodSumCycles = channel.capture->periodSumCycles;
  sampleCount = channel.capture->sampleCount;
  channel.capture->periodSumCycles = 0;
  channel.capture->sampleCount = 0;
  interrupts();

  if (sampleCount > 0) {
    const float averagePeriodUs =
        (static_cast<float>(periodSumCycles) / sampleCount) / gCyclesPerUs;
    if (channel.filteredPeriodUs <= 0.0f) {
      channel.filteredPeriodUs = averagePeriodUs;
    } else {
      channel.filteredPeriodUs +=
          kPeriodFilterAlpha * (averagePeriodUs - channel.filteredPeriodUs);
    }
    channel.lastValidPulseMs = nowMs;
  }

  if (channel.lastValidPulseMs == 0 ||
      (nowMs - channel.lastValidPulseMs) > kSignalTimeoutMs) {
    noInterrupts();
    channel.capture->lastEdgeCycles = 0;
    channel.capture->periodSumCycles = 0;
    channel.capture->sampleCount = 0;
    interrupts();
    stopChannelOutput(channel);
    return;
  }

  channel.measuredRpm =
      TachMath::rpmFromPeriodUs(channel.filteredPeriodUs);
  channel.simulatedRpm = channel.measuredRpm * channel.ratio;

  uint32_t requestedHalfPeriod =
      TachMath::outputHalfPeriodUs(channel.filteredPeriodUs, channel.ratio);
  requestedHalfPeriod =
      constrain(requestedHalfPeriod, kMinOutputHalfPeriodUs,
                kMaxOutputHalfPeriodUs);

  const uint32_t difference =
      requestedHalfPeriod > channel.outputHalfPeriodUs
          ? requestedHalfPeriod - channel.outputHalfPeriodUs
          : channel.outputHalfPeriodUs - requestedHalfPeriod;

  // Two microseconds of hysteresis avoid needless waveform reprogramming.
  if (!channel.outputRunning || difference >= 2U) {
    const bool started =
        startWaveform(channel.outputPin, requestedHalfPeriod,
                      requestedHalfPeriod, 0);
    channel.outputRunning = started;
    if (started) {
      channel.outputHalfPeriodUs = requestedHalfPeriod;
    }
  }
}

void updateTachChannels() {
  const uint32_t nowMs = millis();
  consumeChannelSamples(gChannel6, nowMs);
  consumeChannelSamples(gChannel9, nowMs);
  digitalWrite(kActivityLedPin,
               (gChannel6.outputRunning || gChannel9.outputRunning) ? HIGH
                                                                   : LOW);
}

String jsonEscape(const char* text) {
  String escaped;
  escaped.reserve(strlen(text) + 8);
  for (const char* cursor = text; *cursor != '\0'; ++cursor) {
    const uint8_t value = static_cast<uint8_t>(*cursor);
    if (value == '"' || value == '\\') {
      escaped += '\\';
      escaped += static_cast<char>(value);
    } else if (value >= 0x20) {
      escaped += static_cast<char>(value);
    }
  }
  return escaped;
}

const __FlashStringHelper* stationStateText() {
  switch (WiFi.status()) {
    case WL_CONNECTED:
      return F("connecté");
    case WL_NO_SSID_AVAIL:
      return F("SSID introuvable");
    case WL_CONNECT_FAILED:
      return F("échec d'authentification");
    case WL_CONNECTION_LOST:
      return F("connexion perdue");
    case WL_DISCONNECTED:
      return F("déconnecté");
    default:
      return F("connexion en cours");
  }
}

void handleStatus() {
  String response;
  response.reserve(560);
  response += F("{\"ch6\":{\"rpmIn\":");
  response += String(gChannel6.measuredRpm, 1);
  response += F(",\"rpmOut\":");
  response += String(gChannel6.simulatedRpm, 1);
  response += F(",\"ratio\":");
  response += String(gChannel6.ratio, 3);
  response += F(",\"active\":");
  response += gChannel6.outputRunning ? F("true") : F("false");
  response += F("},\"ch9\":{\"rpmIn\":");
  response += String(gChannel9.measuredRpm, 1);
  response += F(",\"rpmOut\":");
  response += String(gChannel9.simulatedRpm, 1);
  response += F(",\"ratio\":");
  response += String(gChannel9.ratio, 3);
  response += F(",\"active\":");
  response += gChannel9.outputRunning ? F("true") : F("false");
  response += F("},\"wifi\":{\"apSsid\":\"");
  response += jsonEscape(gAccessPointSsid);
  response += F("\",\"apIp\":\"");
  response += WiFi.softAPIP().toString();
  response += F("\",\"staSsid\":\"");
  response += jsonEscape(gConfig.staSsid);
  response += F("\",\"staState\":\"");
  response += stationStateText();
  response += F("\",\"staIp\":\"");
  if (WiFi.status() == WL_CONNECTED) {
    response += WiFi.localIP().toString();
  }
  response += F("\"}}");

  gServer.sendHeader(F("Cache-Control"), F("no-store"));
  gServer.send(200, F("application/json; charset=utf-8"), response);
}

bool parseRatioArgument(const String& value, float& parsed) {
  if (value.length() == 0 || value.length() > 12) {
    return false;
  }
  char* end = nullptr;
  parsed = strtof(value.c_str(), &end);
  return end != value.c_str() && *end == '\0' &&
         TachMath::ratioIsValid(parsed);
}

void scheduleWifiApply() {
  gApplyWifiAtMs = millis() + 750UL;
  gWifiApplyPending = true;
}

void handleSave() {
  float ratio6;
  float ratio9;
  if (!parseRatioArgument(gServer.arg("ratio6"), ratio6) ||
      !parseRatioArgument(gServer.arg("ratio9"), ratio9)) {
    gServer.send(400, F("text/plain; charset=utf-8"),
                 F("Ratio invalide : valeur attendue entre 0,10 et 10,00."));
    return;
  }

  const String newSsid = gServer.arg("ssid");
  const String suppliedPassword = gServer.arg("password");
  if (newSsid.length() > 32 || suppliedPassword.length() > 64) {
    gServer.send(400, F("text/plain; charset=utf-8"),
                 F("SSID ou mot de passe trop long."));
    return;
  }

  const bool ssidChanged = newSsid != String(gConfig.staSsid);
  copyStringToBuffer(newSsid, gConfig.staSsid, sizeof(gConfig.staSsid));

  if (gServer.hasArg("clear_password")) {
    memset(gConfig.staPassword, 0, sizeof(gConfig.staPassword));
  } else if (ssidChanged || suppliedPassword.length() > 0) {
    copyStringToBuffer(suppliedPassword, gConfig.staPassword,
                       sizeof(gConfig.staPassword));
  }

  gConfig.ratio6 = ratio6;
  gConfig.ratio9 = ratio9;
  gChannel6.ratio = ratio6;
  gChannel9.ratio = ratio9;
  saveConfig();
  scheduleWifiApply();

  gServer.sendHeader(F("Location"), F("/"));
  gServer.send(303, F("text/plain"), "");
}

void configureHttpServer() {
  gServer.on("/", HTTP_GET, []() {
    gServer.sendHeader(F("Cache-Control"), F("no-store"));
    gServer.send_P(200, PSTR("text/html; charset=utf-8"), kIndexHtml);
  });
  gServer.on("/api/status", HTTP_GET, handleStatus);
  gServer.on("/save", HTTP_POST, handleSave);
  gServer.onNotFound([]() {
    gServer.send(404, F("text/plain; charset=utf-8"), F("Page introuvable"));
  });
  gServer.begin();
}

void applyStationConfiguration() {
  WiFi.disconnect(false);
  if (gConfig.staSsid[0] != '\0') {
    WiFi.begin(gConfig.staSsid, gConfig.staPassword);
  }
}

void configureWifi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  snprintf(gAccessPointSsid, sizeof(gAccessPointSsid), "DeyeFan-%06X",
           ESP.getChipId());
  WiFi.softAP(gAccessPointSsid, kAccessPointPassword);
  applyStationConfiguration();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("Deye fan tach simulator starting"));

  pinMode(kTach6InputPin, INPUT);
  pinMode(kTach9InputPin, INPUT);
  pinMode(kDeye6OutputPin, OUTPUT);
  pinMode(kDeye9OutputPin, OUTPUT);
  pinMode(kActivityLedPin, OUTPUT);
  digitalWrite(kDeye6OutputPin, LOW);
  digitalWrite(kDeye9OutputPin, LOW);
  digitalWrite(kActivityLedPin, LOW);

  gCyclesPerUs = ESP.getCpuFreqMHz();
  gMinInputPeriodCycles = kMinInputPeriodUs * gCyclesPerUs;
  gMaxInputPeriodCycles = kMaxInputPeriodUs * gCyclesPerUs;

  EEPROM.begin(kEepromSize);
  loadConfig();

  attachInterrupt(digitalPinToInterrupt(kTach6InputPin), onTach6Edge, FALLING);
  attachInterrupt(digitalPinToInterrupt(kTach9InputPin), onTach9Edge, FALLING);

  configureWifi();
  configureHttpServer();

  Serial.print(F("Configuration AP: "));
  Serial.print(gAccessPointSsid);
  Serial.print(F(" / "));
  Serial.println(kAccessPointPassword);
  Serial.print(F("Interface: http://"));
  Serial.println(WiFi.softAPIP());
}

void loop() {
  gServer.handleClient();

  const uint32_t nowMs = millis();
  if (gWifiApplyPending &&
      static_cast<int32_t>(nowMs - gApplyWifiAtMs) >= 0) {
    gWifiApplyPending = false;
    applyStationConfiguration();
  }

  if ((nowMs - gLastChannelUpdateMs) >= kChannelUpdateMs) {
    gLastChannelUpdateMs = nowMs;
    updateTachChannels();
  }

  delay(1);  // Yield to the Wi-Fi stack; tach timing runs in interrupts.
}
