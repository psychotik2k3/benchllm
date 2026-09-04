#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

// ---------------------------------------------------------
// Structures & Constantes
// ---------------------------------------------------------
#define EEPROM_MAGIC 0xDEYE0001

struct Config {
  uint32_t magic;
  char ssid[32];
  char pass[64];
  float ratio9;
  float ratio6;
} cfg;

ESP8266WebServer server(80);

// Broches d'entrée
const int TACH_IN_9CM = D1; // GPIO5
const int TACH_IN_6CM = D2; // GPIO4

// Broches de sortie (vers la base des NPN)
const int PIN_OUT_9 = 14;   // D5 (GPIO14)
const int PIN_OUT_6 = 12;   // D6 (GPIO12)

const int LED_PIN = LED_BUILTIN; // Actif LOW sur Wemos D1 Mini

// Variables volatiles pour les entrées (incrémentées sous interruption)
volatile uint32_t pulses9 = 0;
volatile uint32_t pulses6 = 0;

// Variables volatiles pour la gestion du Timer matériel (génération sans jitter)
// L'unité de temps est le "tick" du Timer1 (ici configuré pour 50 microsecondes)
volatile uint32_t target_ticks_9 = 0;
volatile uint32_t ticks_9 = 0;
volatile bool pin_state_9 = false;

volatile uint32_t target_ticks_6 = 0;
volatile uint32_t ticks_6 = 0;
volatile bool pin_state_6 = false;

// Suivi pour le calcul RPM dans la boucle principale
uint32_t lastCalcTime = 0;
uint32_t rpm9_in = 0, rpm6_in = 0;
uint32_t rpm9_out = 0, rpm6_out = 0;

// ---------------------------------------------------------
// Interface Web (HTML)
// ---------------------------------------------------------
const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <title>Deye Fan Simulator</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="utf-8">
  <style>
    body{font-family:sans-serif;margin:20px;background:#f4f4f4;} 
    .card{background:white;padding:20px;margin-bottom:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);} 
    label{display:inline-block;width:120px;font-weight:bold;} 
    input{margin-bottom:10px;padding:8px;width:calc(100% - 140px);max-width:200px;border:1px solid #ccc;border-radius:4px;}
    input[type=submit]{width:auto;background:#007bff;color:white;border:none;padding:10px 15px;border-radius:4px;cursor:pointer;}
    input[type=submit]:hover{background:#0056b3;}
    .rpm{font-size:1.2em;font-weight:bold;color:#007bff;}
  </style>
  <script>
    setInterval(function(){
      fetch('/status').then(r=>r.json()).then(d=>{
        document.getElementById('r9in').innerText = d.rpm9_in;
        document.getElementById('r9out').innerText = d.rpm9_out;
        document.getElementById('r6in').innerText = d.rpm6_in;
        document.getElementById('r6out').innerText = d.rpm6_out;
      });
    }, 1000);
  </script>
</head>
<body>
  <h2>Deye Fan Simulator</h2>
  <div class="card">
    <h3>Moniteur Temps Réel</h3>
    <p><b>Ventilateur 9cm:</b> Lecture = <span class="rpm" id="r9in">0</span> RPM | Simulé = <span class="rpm" id="r9out">0</span> RPM</p>
    <p><b>Ventilateur 6cm:</b> Lecture = <span class="rpm" id="r6in">0</span> RPM | Simulé = <span class="rpm" id="r6out">0</span> RPM</p>
  </div>
  <div class="card">
    <h3>Configuration des ratios</h3>
    <form action="/save" method="POST">
      <label>WiFi SSID:</label><input type="text" name="ssid" value="%SSID%"><br>
      <label>Mot de passe:</label><input type="password" name="password" value="%PWD%"><br>
      <label>Ratio 9cm:</label><input type="number" step="0.1" min="0.1" name="ratio9" value="%R9%"><br>
      <label>Ratio 6cm:</label><input type="number" step="0.1" min="0.1" name="ratio6" value="%R6%"><br>
      <input type="submit" value="Enregistrer et Redémarrer">
    </form>
  </div>
</body>
</html>
)=====";


// ---------------------------------------------------------
// Interruptions Matérielles (ISR)
// ---------------------------------------------------------

// IRAM_ATTR force la fonction en RAM (vitale pour les performances/stabilité ESP8266)
void IRAM_ATTR isr_in_9() { pulses9++; }
void IRAM_ATTR isr_in_6() { pulses6++; }

// Générateur de fréquence isolé matériellement. 
// Appelé toutes les 50 microsecondes (20 kHz). Indépendant de l'activité WiFi.
void IRAM_ATTR timer1_isr() {
  // Traitement canal 9cm
  if (target_ticks_9 > 0) {
    ticks_9++;
    if (ticks_9 >= target_ticks_9) {
      ticks_9 = 0;
      pin_state_9 = !pin_state_9;
      if (pin_state_9) GPOS = (1 << PIN_OUT_9); // Active NPN (signal vers GND)
      else GPOC = (1 << PIN_OUT_9);             // Désactive NPN (relâché à VCC via pullup Onduleur)
    }
  } else {
    ticks_9 = 0;
    pin_state_9 = false;
    GPOC = (1 << PIN_OUT_9); // Toujours désactivé si 0 RPM
  }
  
  // Traitement canal 6cm
  if (target_ticks_6 > 0) {
    ticks_6++;
    if (ticks_6 >= target_ticks_6) {
      ticks_6 = 0;
      pin_state_6 = !pin_state_6;
      if (pin_state_6) GPOS = (1 << PIN_OUT_6);
      else GPOC = (1 << PIN_OUT_6);
    }
  } else {
    ticks_6 = 0;
    pin_state_6 = false;
    GPOC = (1 << PIN_OUT_6);
  }
}

// ---------------------------------------------------------
// Gestion de la Configuration
// ---------------------------------------------------------
void loadConfig() {
  EEPROM.begin(sizeof(Config));
  EEPROM.get(0, cfg);
  if (cfg.magic != EEPROM_MAGIC) {
    // Initialisation par défaut si l'EEPROM est vierge
    cfg.magic = EEPROM_MAGIC;
    strcpy(cfg.ssid, "");
    strcpy(cfg.pass, "");
    cfg.ratio9 = 2.0;
    cfg.ratio6 = 2.0;
    EEPROM.put(0, cfg);
    EEPROM.commit();
  }
}

void saveConfig(String s, String p, float r9, float r6) {
  strcpy(cfg.ssid, s.c_str());
  strcpy(cfg.pass, p.c_str());
  cfg.ratio9 = r9;
  cfg.ratio6 = r6;
  EEPROM.put(0, cfg);
  EEPROM.commit();
}

// ---------------------------------------------------------
// Gestion des Requêtes Web
// ---------------------------------------------------------
void handleRoot() {
  String html = FPSTR(INDEX_HTML);
  html.replace("%SSID%", cfg.ssid);
  html.replace("%PWD%", cfg.pass);
  html.replace("%R9%", String(cfg.ratio9));
  html.replace("%R6%", String(cfg.ratio6));
  server.send(200, "text/html", html);
}

void handleStatus() {
  String json = "{";
  json += "\"rpm9_in\":" + String(rpm9_in) + ",";
  json += "\"rpm6_in\":" + String(rpm6_in) + ",";
  json += "\"rpm9_out\":" + String(rpm9_out) + ",";
  json += "\"rpm6_out\":" + String(rpm6_out);
  json += "}";
  server.send(200, "application/json", json);
}

void handleSave() {
  if (server.method() == HTTP_POST) {
    String s = server.arg("ssid");
    String p = server.arg("password");
    float r9 = server.arg("ratio9").toFloat();
    float r6 = server.arg("ratio6").toFloat();
    
    // Ratios de sécurité minimum
    if (r9 < 0.1) r9 = 1.0;
    if (r6 < 0.1) r6 = 1.0;
    
    saveConfig(s, p, r9, r6);
    
    server.send(200, "text/html", "<meta charset=\"utf-8\"><h2>Configuration enregistrée ! Redémarrage du module...</h2>");
    delay(1000);
    ESP.restart();
  }
}

// ---------------------------------------------------------
// Setup & Loop
// ---------------------------------------------------------
void setup() {
  Serial.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // Eteint (Actif Bas)
  
  loadConfig();
  
  // Configuration des broches
  pinMode(TACH_IN_9CM, INPUT_PULLUP);
  pinMode(TACH_IN_6CM, INPUT_PULLUP);
  
  pinMode(PIN_OUT_9, OUTPUT);
  pinMode(PIN_OUT_6, OUTPUT);
  digitalWrite(PIN_OUT_9, LOW);
  digitalWrite(PIN_OUT_6, LOW);
  
  // 1. Attachement des interruptions d'entrée (Noctua)
  attachInterrupt(digitalPinToInterrupt(TACH_IN_9CM), isr_in_9, FALLING);
  attachInterrupt(digitalPinToInterrupt(TACH_IN_6CM), isr_in_6, FALLING);
  
  // 2. Configuration du Timer Matériel pour les sorties (Zéro Jitter)
  // Horloge base = 80 MHz, prescaler 16 => tick = 0.2us (5 MHz)
  timer1_attachInterrupt(timer1_isr);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP); 
  // 250 ticks de 0.2us = Interruption toutes les 50 microsecondes
  timer1_write(250); 
  
  // 3. Configuration WiFi (AP + Station simultanés)
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("DeyeFan_Config", "deyesetup"); // Mode Access Point
  
  if (strlen(cfg.ssid) > 0) {
    WiFi.begin(cfg.ssid, cfg.pass); // Mode Client si configuré
  }
  
  // 4. Lancement du Serveur Web
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

void loop() {
  server.handleClient();
  
  uint32_t now = millis();
  if (now - lastCalcTime >= 1000) {
    lastCalcTime = now;
    
    // Récupération atomique des compteurs d'impulsions
    noInterrupts();
    uint32_t p9 = pulses9;
    uint32_t p6 = pulses6;
    pulses9 = 0;
    pulses6 = 0;
    interrupts();
    
    // Calcul des RPM (2 impulsions par tour pour un ventilateur standard)
    // RPM = (impulsions / 2) * 60 = impulsions * 30
    rpm9_in = p9 * 30;
    rpm6_in = p6 * 30;
    
    // Application des ratios
    rpm9_out = rpm9_in * cfg.ratio9;
    rpm6_out = rpm6_in * cfg.ratio6;
    
    // Calcul de la période cible pour le Timer matériel (ticks de 50us)
    // Fréquence cible = RPM / 30. Période complète = 30 / RPM secondes = 30,000,000 / RPM us.
    // Demi-période = 15,000,000 / RPM us.
    // Nombre de ticks de 50us = (15,000,000 / 50) / RPM = 300,000 / RPM.
    uint32_t new_t9 = 0;
    if (rpm9_out > 0) new_t9 = 300000 / rpm9_out;
    
    uint32_t new_t6 = 0;
    if (rpm6_out > 0) new_t6 = 300000 / rpm6_out;
    
    // Mise à jour sécurisée des cibles
    noInterrupts();
    target_ticks_9 = new_t9;
    target_ticks_6 = new_t6;
    interrupts();
    
    // Indication visuelle : LED allumée si le système génère activement un signal
    if (rpm9_out > 0 || rpm6_out > 0) {
      digitalWrite(LED_PIN, LOW);  // LED Allumée
    } else {
      digitalWrite(LED_PIN, HIGH); // LED Eteinte
    }
  }
}