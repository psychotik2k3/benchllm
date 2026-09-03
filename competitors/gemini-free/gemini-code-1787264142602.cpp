#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// --- PINOUT DEFINITIONS ---
#define PIN_TACH_IN_1   14  // D5 (GPIO14) - Entrée Tach Canal 1 (9cm)
#define PIN_TACH_IN_2   12  // D6 (GPIO12) - Entrée Tach Canal 2 (6cm)
#define PIN_TACH_OUT_1  5   // D1 (GPIO5)  - Sortie Simulée Canal 1
#define PIN_TACH_OUT_2  4   // D2 (GPIO4)  - Sortie Simulée Canal 2
#define PIN_LED_STATUS  13  // D7 (GPIO13) - LED d'état

// --- PARAMÈTRES PAR DÉFAUT ---
float ratioCh1 = 2.0;
float ratioCh2 = 2.0;

char wifiSSID[32] = "";
char wifiPass[64] = "";

// --- VARIABLES DE MESURE & INTERRUPTIONS ---
volatile unsigned long lastPulseTime1 = 0;
volatile unsigned long pulseInterval1 = 0;
volatile unsigned long lastPulseTime2 = 0;
volatile unsigned long pulseInterval2 = 0;

volatile unsigned long targetOutHalfPeriod1 = 0; // en microsecondes
volatile unsigned long targetOutHalfPeriod2 = 0; // en microsecondes

volatile bool outState1 = LOW;
volatile bool outState2 = LOW;

unsigned long lastTick1 = 0;
unsigned long lastTick2 = 0;

// Variables pour le calcul des RPM réels
float rpmIn1 = 0, rpmOut1 = 0;
float rpmIn2 = 0, rpmOut2 = 0;

AsyncWebServer server(80);

// --- INTERRUPTIONS SUR ENTRÉES TACH ---
void IRAM_ATTR handleTachIn1() {
  unsigned long now = micros();
  unsigned long diff = now - lastPulseTime1;
  if (diff > 5000) { // Anti-rebond (Max ~12000 RPM)
    pulseInterval1 = diff;
    lastPulseTime1 = now;
  }
}

void IRAM_ATTR handleTachIn2() {
  unsigned long now = micros();
  unsigned long diff = now - lastPulseTime2;
  if (diff > 5000) {
    pulseInterval2 = diff;
    lastPulseTime2 = now;
  }
}

// --- INTERRUPTION TIMER (Génération dynamique du signal de sortie) ---
void IRAM_ATTR onTimerISR() {
  static unsigned long counter1 = 0;
  static unsigned long counter2 = 0;
  const unsigned long timerStepUs = 50; // Résolution timer = 50µs

  // Canal 1
  if (targetOutHalfPeriod1 > 0) {
    counter1 += timerStepUs;
    if (counter1 >= targetOutHalfPeriod1) {
      counter1 = 0;
      outState1 = !outState1;
      digitalWrite(PIN_TACH_OUT_1, outState1);
    }
  } else {
    digitalWrite(PIN_TACH_OUT_1, LOW);
  }

  // Canal 2
  if (targetOutHalfPeriod2 > 0) {
    counter2 += timerStepUs;
    if (counter2 >= targetOutHalfPeriod2) {
      counter2 = 0;
      outState2 = !outState2;
      digitalWrite(PIN_TACH_OUT_2, outState2);
    }
  } else {
    digitalWrite(PIN_TACH_OUT_2, LOW);
  }

  timer1_write(80 * timerStepUs); // Recharger le timer (80 ticks/µs à 80MHz)
}

// --- CHARGEMENT & SAUVEGARDE CONFIGURATION ---
void loadConfig() {
  if (LittleFS.exists("/config.json")) {
    File configFile = LittleFS.open("/config.json", "r");
    if (configFile) {
      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, configFile);
      if (!error) {
        ratioCh1 = doc["ratio1"] | 2.0;
        ratioCh2 = doc["ratio2"] | 2.0;
        strlcpy(wifiSSID, doc["ssid"] | "", sizeof(wifiSSID));
        strlcpy(wifiPass, doc["pass"] | "", sizeof(wifiPass));
      }
      configFile.close();
    }
  }
}

void saveConfig() {
  StaticJsonDocument<512> doc;
  doc["ratio1"] = ratioCh1;
  doc["ratio2"] = ratioCh2;
  doc["ssid"] = wifiSSID;
  doc["pass"] = wifiPass;

  File configFile = LittleFS.open("/config.json", "w");
  if (configFile) {
    serializeJson(doc, configFile);
    configFile.close();
  }
}

// --- INTERFACE WEB HTML/JS ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta charset="UTF-8">
  <title>Deye Fan Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; background: #121212; color: #e0e0e0; }
    .card { background: #1e1e1e; padding: 20px; border-radius: 8px; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
    h2 { color: #00bcd4; margin-top: 0; }
    label { display: block; margin: 10px 0 5px; }
    input[type=number], input[type=text], input[type=password] { width: 100%; padding: 8px; box-sizing: border-box; background: #2c2c2c; border: 1px solid #444; color: #fff; border-radius: 4px; }
    button { background: #00bcd4; border: none; color: white; padding: 10px 15px; margin-top: 15px; border-radius: 4px; cursor: pointer; font-weight: bold; }
    button:hover { background: #008ba3; }
    .val { font-size: 1.2em; font-weight: bold; color: #4caf50; }
  </style>
</head>
<body>
  <div class="card">
    <h2>Moniteur de Ventilation</h2>
    <p>Canal 1 (9cm) : Entrée <span id="in1" class="val">0</span> RPM | Simulés <span id="out1" class="val">0</span> RPM</p>
    <p>Canal 2 (6cm) : Entrée <span id="in2" class="val">0</span> RPM | Simulés <span id="out2" class="val">0</span> RPM</p>
  </div>

  <div class="card">
    <h2>Ratios de Multiplications</h2>
    <form action="/set-ratios" method="POST">
      <label>Ratio Canal 1 (9cm) :</label>
      <input type="number" step="0.1" name="r1" id="r1" value="2.0">
      <label>Ratio Canal 2 (6cm) :</label>
      <input type="number" step="0.1" name="r2" id="r2" value="2.0">
      <button type="submit">Enregistrer Ratios</button>
    </form>
  </div>

  <div class="card">
    <h2>Configuration WiFi</h2>
    <form action="/set-wifi" method="POST">
      <label>SSID WiFi :</label>
      <input type="text" name="ssid" id="ssid">
      <label>Mot de passe :</label>
      <input type="password" name="pass">
      <button type="submit">Enregistrer & Redémarrer</button>
    </form>
  </div>

<script>
setInterval(() => {
  fetch('/data').then(r => r.json()).then(d => {
    document.getElementById('in1').innerText = d.in1;
    document.getElementById('out1').innerText = d.out1;
    document.getElementById('in2').innerText = d.in2;
    document.getElementById('out2').innerText = d.out2;
  });
}, 1000);

fetch('/config').then(r => r.json()).then(d => {
  document.getElementById('r1').value = d.r1;
  document.getElementById('r2').value = d.r2;
  document.getElementById('ssid').value = d.ssid;
});
</script>
</body>
</html>
)rawliteral";

void setup() {
  pinMode(PIN_TACH_IN_1, INPUT_PULLUP);
  pinMode(PIN_TACH_IN_2, INPUT_PULLUP);
  pinMode(PIN_TACH_OUT_1, OUTPUT);
  pinMode(PIN_TACH_OUT_2, OUTPUT);
  pinMode(PIN_LED_STATUS, OUTPUT);

  digitalWrite(PIN_TACH_OUT_1, LOW);
  digitalWrite(PIN_TACH_OUT_2, LOW);
  digitalWrite(PIN_LED_STATUS, LOW);

  LittleFS.begin();
  loadConfig();

  // Config WiFi AP + STA
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("Deye-Fan-Simulator", "12345678");

  if (strlen(wifiSSID) > 0) {
    WiFi.begin(wifiSSID, wifiPass);
  }

  // Interruption Entrées
  attachInterrupt(digitalPinToInterrupt(PIN_TACH_IN_1), handleTachIn1, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_TACH_IN_2), handleTachIn2, FALLING);

  // Configuration Timer1 Matériel pour la sortie
  timer1_attachInterrupt(onTimerISR);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
  timer1_write(4000); // Démarre le timer

  // --- ROUTES WEBSERVER ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<256> doc;
    doc["in1"] = round(rpmIn1);
    doc["out1"] = round(rpmOut1);
    doc["in2"] = round(rpmIn2);
    doc["out2"] = round(rpmOut2);
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<256> doc;
    doc["r1"] = ratioCh1;
    doc["r2"] = ratioCh2;
    doc["ssid"] = wifiSSID;
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  server.on("/set-ratios", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("r1", true)) ratioCh1 = request->getParam("r1", true)->value().toFloat();
    if (request->hasParam("r2", true)) ratioCh2 = request->getParam("r2", true)->value().toFloat();
    saveConfig();
    request->redirect("/");
  });

  server.on("/set-wifi", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("ssid", true)) strlcpy(wifiSSID, request->getParam("ssid", true)->value().c_str(), sizeof(wifiSSID));
    if (request->hasParam("pass", true)) strlcpy(wifiPass, request->getParam("pass", true)->value().c_str(), sizeof(wifiPass));
    saveConfig();
    request->send(200, "text/plain", "WiFi enregistre. Redemarrage du Wemos...");
    delay(1000);
    ESP.restart();
  });

  server.begin();
}

void loop() {
  unsigned long now = micros();

  // --- TRAITEMENT CANAL 1 ---
  if (now - lastPulseTime1 > 1000000) { // Pas de pulse depuis 1s = arrêt
    pulseInterval1 = 0;
    rpmIn1 = 0;
    rpmOut1 = 0;
    targetOutHalfPeriod1 = 0;
  } else if (pulseInterval1 > 0) {
    rpmIn1 = (60.0 * 1000000.0) / (pulseInterval1 * 2.0); // 2 pulses par tour
    rpmOut1 = rpmIn1 * ratioCh1;
    if (rpmOut1 > 0) {
      // Demi-période en µs = (60 * 1000000) / (RPM_out * 2 pulses * 2 transitions)
      targetOutHalfPeriod1 = (15000000.0 / rpmOut1);
    }
  }

  // --- TRAITEMENT CANAL 2 ---
  if (now - lastPulseTime2 > 1000000) {
    pulseInterval2 = 0;
    rpmIn2 = 0;
    rpmOut2 = 0;
    targetOutHalfPeriod2 = 0;
  } else if (pulseInterval2 > 0) {
    rpmIn2 = (60.0 * 1000000.0) / (pulseInterval2 * 2.0);
    rpmOut2 = rpmIn2 * ratioCh2;
    if (rpmOut2 > 0) {
      targetOutHalfPeriod2 = (15000000.0 / rpmOut2);
    }
  }

  // --- GESTION LED D'ÉTAT ---
  // Clignote ou s'allume en continu si au moins un ventilateur tourne
  if (rpmOut1 > 0 || rpmOut2 > 0) {
    digitalWrite(PIN_LED_STATUS, (millis() / 250) % 2); // Clignotement rapide
  } else {
    digitalWrite(PIN_LED_STATUS, LOW); // Éteint si aucun ventilateur ne tourne
  }

  delay(50); // Soulage la boucle principale
}