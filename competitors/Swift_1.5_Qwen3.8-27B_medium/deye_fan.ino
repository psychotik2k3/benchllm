/**
 * deye_fan.ino — Simulateur de tachymètre pour onduleur Deye SUN-8K-SG05LP1-EU-AM2-P
 * ================================================================================
 * Remplace les ventilateurs NMB d'origine par 2x Noctua NF-A6x25-FLX + 2x NF-A9-FLX
 * et fait croire à l'onduleur que les RPM sont multipliés par un coefficient
 * configurable par canal (sans recompiler, sans recâbler).
 *
 * Carte   : LOLIN(WEMOS) D1 mini (ESP-12S / ESP8266), 80 MHz
 * IDE     : Arduino, board "Lolin Wemos D1 mini"
 *
 * ----------------------------------------------------------------
 * ARCHITECTURE ANTI-JITTER (point critique de la spec)
 * ----------------------------------------------------------------
 * - La GÉNÉRATION du signal tach de sortie est 100 % pilotée par le
 *   timer matériel (Timer0, tick 100 µs). Le ISR de tick ne fait QUE
 *   décrémenter 2 compteurs et commuter 2 GPIO — aucun WiFi, aucun HTTP,
 *   aucune allocation mémoire, aucun printf dans l'interruption.
 * - La LECTURE des tach entrées se fait par ISR d'edge GPIO qui ne lit
 *   qu'un compteur matériel en microsecondes (pas de micros() qui est
 *   lui-même subject aux interruptions).
 * - Le serveur WiFi/HTTP tourne dans le loop() uniquement. Le WiFi peut
 *   saturer la CPU : la période des sorties n'en est pas affectée car
 *   elle est calculée par le hardware.
 * - Le tick matériel est à 10 kHz ; au pire (WiFi interrupt pending)
 *   le jitter est de l'ordre de quelques µs, imperceptible à ~1-3 kHz
 *   de signal tach (seuil de détection RPM de l'onduleur).
 *
 * ----------------------------------------------------------------
 * CÂBLAGE (voir schema_electronique.txt)
 * ----------------------------------------------------------------
 *  D1 (GPIO5)  : tach IN canal 1 (ventilateur 9 cm)  — via transistors
 *  D2 (GPIO4)  : tach IN canal 2 (ventilateur 6 cm)  — via transistors
 *  D5 (GPIO14) : tach OUT canal 1 (vers Deye)        — collector NPN
 *  D6 (GPIO12) : tach OUT canal 2 (vers Deye)        — collector NPN
 *  D7 (GPIO13) : LED de statut (intégrée Wemos)
 *  Alimentation Wemos : 3V3 issu d'un LDO alimenté depuis les 2
 *  connecteurs ventilateur via diodes Schottky (voir schéma).
 *
 * ----------------------------------------------------------------
 * INTERFACE WEB (AP : "DeyeFanSim", 192.168.4.1 ; STA : SSID enregistré)
 * ----------------------------------------------------------------
 *  GET  /                 → page de statut (auto-refresh 1 s)
 *  GET  /json             → statut JSON
 *  GET  /ratio?ch=1&v=2.5 → ratio canal 1 = 2.5 (0.5 à 10.0)
 *  GET  /wifi?ssid=X&pass=Y → enregistre + connecte le STA
 *
 * Ratios et WiFi persistés en flash (Preferences).
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPServer.h>
#include <Preferences.h>
#include <ESP8266TimerInterrupt.h>

// ------------------------------------------------------------------
// PINS
// ------------------------------------------------------------------
#define CH1_IN_PIN   5    // D1 — tach entrée 9 cm
#define CH2_IN_PIN   4    // D2 — tach entrée 6 cm
#define CH1_OUT_PIN  14   // D5 — tach sortie 9 cm
#define CH2_OUT_PIN  12   // D6 — tach sortie 6 cm
#define LED_PIN      13   // D7 — LED statut

// ------------------------------------------------------------------
// TIMER MATÉRIEL : tick de 100 µs (10 kHz)
// ------------------------------------------------------------------
#define TICK_US      100u
#define TICK_HZ      (1000000UL / TICK_US)

// ------------------------------------------------------------------
// DONNÉES PAR CANAL
// ------------------------------------------------------------------
#define NCH 2

struct Channel {
  uint32_t inEdgeUs[2];      // horodatage des 2 derniers fronts (µs matériel)
  uint32_t lastEdgeTick;     // tick du dernier front (pour l'âge du signal)
  uint32_t inPeriodUs;       // période mesurée entre 2 pulses (µs)
  double   rpmReal;          // RPM mesuré (lissé EMA)
  bool     inputValid;       // signal d'entrée plausible
  double   ratio;            // multiplicateur configuré
  // machine à états de sortie (exécutée DANS le ISR de tick) :
  uint16_t periodTicks;      // période d'une paire de pulses (en ticks)
  uint16_t countDown;        // décrément tick à tick
  uint16_t pulseWidth;       // largeur de pulse (en ticks)
  bool     inPulse;          // pulse en cours ?
  bool     enabled;          // générer ? (fan réel tournant)
};

static Channel ch[NCH];

// ------------------------------------------------------------------
// TIMER0 : tick + horloge µs lisible depuis les ISR
// ------------------------------------------------------------------
static volatile uint32_t g_tickCount = 0;

static void onTick() {
  g_tickCount++;

  for (int i = 0; i < NCH; i++) {
    Channel *c = &ch[i];
    if (!c->enabled) continue;
    if (c->countDown == 0) {
      if (c->inPulse) {
        // fin du pulse → bas, on recommence la période
        digitalWriteFast(i == 0 ? CH1_OUT_PIN : CH2_OUT_PIN, LOW);
        c->inPulse   = false;
        c->countDown = c->periodTicks;
      } else {
        // début de pulse → haut
        digitalWriteFast(i == 0 ? CH1_OUT_PIN : CH2_OUT_PIN, HIGH);
        c->inPulse   = true;
        c->countDown = c->pulseWidth;
      }
    } else {
      c->countDown--;
    }
  }
}

static uint32_t microsTimer() { return g_tickCount * TICK_US; }

// ------------------------------------------------------------------
// ISR d'edge sur les tach d'entrée
// ------------------------------------------------------------------
static void onIn1() {
  uint32_t now = microsTimer();
  uint32_t dt = now - ch[0].inEdgeUs[0];
  ch[0].inEdgeUs[1] = ch[0].inEdgeUs[0];
  ch[0].inEdgeUs[0] = now;
  ch[0].inPeriodUs  = dt;
  ch[0].lastEdgeTick = g_tickCount;
}
static void onIn2() {
  uint32_t now = microsTimer();
  uint32_t dt = now - ch[1].inEdgeUs[0];
  ch[1].inEdgeUs[1] = ch[1].inEdgeUs[0];
  ch[1].inEdgeUs[0] = now;
  ch[1].inPeriodUs  = dt;
  ch[1].lastEdgeTick = g_tickCount;
}

// ------------------------------------------------------------------
// WIFI / WEB
// ------------------------------------------------------------------
#define AP_SSID "DeyeFanSim"
#define AP_PASS "deyefan123"

static WiFiServer   server(80);
static Preferences   prefs;
static String        staSsid, staPass;
static bool          staEnabled = false;

static void saveSettings() {
  prefs.begin("deyefan", false);
  prefs.putFloat("r1", ch[0].ratio);
  prefs.putFloat("r2", ch[1].ratio);
  prefs.putString("ssid", staSsid);
  prefs.putString("pass", staPass);
  prefs.end();
}

static void loadSettings() {
  prefs.begin("deyefan", true);
  ch[0].ratio = prefs.getFloat("r1", 2.0);
  ch[1].ratio = prefs.getFloat("r2", 2.0);
  staSsid     = prefs.getString("ssid", "");
  staPass     = prefs.getString("pass", "");
  staEnabled  = staSsid.length() > 0;
  prefs.end();
}

static String jsonStatus() {
  String s = "{\"ch1\":{\"real\":";
  s += String(ch[0].rpmReal, 1);
  s += ",\"sim\":";
  s += String(ch[0].rpmReal * ch[0].ratio, 1);
  s += ",\"ratio\":";
  s += String(ch[0].ratio, 2);
  s += ",\"ok\":";
  s += (ch[0].inputValid ? "true" : "false");
  s += "},\"ch2\":{\"real\":";
  s += String(ch[1].rpmReal, 1);
  s += ",\"sim\":";
  s += String(ch[1].rpmReal * ch[1].ratio, 1);
  s += ",\"ratio\":";
  s += String(ch[1].ratio, 2);
  s += ",\"ok\":";
  s += (ch[1].inputValid ? "true" : "false");
  s += "},\"ap\":\"192.168.4.1\",\"sta\":\"" + WiFi.SSID() + "\"}";
  return s;
}

static void handleRoot() {
  String h = "<!DOCTYPE html><meta charset='utf-8'>"
             "<title>Deye Fan Sim</title>"
             "<meta http-equiv='refresh' content='1'>"
             "<body style='font-family:monospace'>"
             "<h2>Deye Fan Simulator</h2>"
             "<table><tr><th>Chan</th><th>Real RPM</th><th>Sim RPM</th><th>Ratio</th></tr>";
  for (int i = 0; i < NCH; i++) {
    char b[128];
    snprintf(b, sizeof(b),
             "<tr><td>%d</td><td>%.0f</td><td>%.0f</td><td>%.2f</td></tr>",
             i + 1, ch[i].rpmReal, ch[i].rpmReal * ch[i].ratio, ch[i].ratio);
    h += b;
  }
  h += "</table><p>AP: 192.168.4.1 — STA: " + WiFi.SSID() + "</p>"
       "<p>Ratio ch1: <a href='/ratio?ch=1&v=1.5'>1.5</a> "
       "<a href='/ratio?ch=1&v=2.0'>2.0</a> "
       "<a href='/ratio?ch=1&v=3.0'>3.0</a></p>"
       "<p>Ratio ch2: <a href='/ratio?ch=2&v=1.5'>1.5</a> "
       "<p>WiFi STA: <form action='/wifi' method='get'>"
       "<input name='ssid' placeholder='SSID'> "
       "<input name='pass' placeholder='pass'></form></p>";
  server.send(200, "text/html", h);
}

static void handleJson() {
  server.send(200, "application/json", jsonStatus());
}

static void handleRatio() {
  if (!server.hasArg("ch") || !server.hasArg("v")) {
    server.send(400, "text/plain", "usage: /ratio?ch=1|2&v=2.5");
    return;
  }
  int chn = atoi(server.arg("ch").c_str()) - 1;
  float v = atof(server.arg("v").c_str());
  if (chn < 0 || chn >= NCH || v < 0.5f || v > 10.0f) {
    server.send(400, "text/plain", "ch 1..2, v 0.5..10.0");
    return;
  }
  ch[chn].ratio = v;
  saveSettings();
  server.send(200, "text/plain", "ok ratio ch" + String(chn + 1) + " = " + String(v, 2));
}

static void handleWifi() {
  if (server.hasArg("ssid")) staSsid = server.arg("ssid");
  if (server.hasArg("pass")) staPass = server.arg("pass");
  staEnabled = staSsid.length() > 0;
  saveSettings();
  if (staEnabled) WiFi.connect(staSsid.c_str(), staPass.c_str());
  server.send(200, "text/plain", "ok");
}

// ------------------------------------------------------------------
// SETUP
// ------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(20);
  Serial.println("\n[deye_fan] start");

  pinMode(CH1_IN_PIN,  INPUT_PULLUP);
  pinMode(CH2_IN_PIN,  INPUT_PULLUP);
  pinMode(CH1_OUT_PIN, OUTPUT);
  pinMode(CH2_OUT_PIN, OUTPUT);
  pinMode(LED_PIN,     OUTPUT);
  digitalWriteFast(CH1_OUT_PIN, LOW);
  digitalWriteFast(CH2_OUT_PIN, LOW);

  loadSettings();

  // ---- timer matériel : tick 100 µs ----
  timer0_attachInterrupt(onTick);
  timer0_write(TICK_US);           // premier tick dans 100 µs
  timer0_enable();

  // ---- ISR d'edge sur les entrées (front montant du tach) ----
  attachInterrupt(digitalPinToInterrupt(CH1_IN_PIN), onIn1, RISING);
  attachInterrupt(digitalPinToInterrupt(CH2_IN_PIN), onIn2, RISING);

  // ---- WiFi : AP toujours actif, STA si configuré (AP+STA simultanés) ----
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress apIp(192, 168, 4, 1);
  WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0));
  Serial.print("[deye_fan] AP: "); Serial.println(WiFi.softAPIP().toString());
  if (staEnabled) {
    WiFi.begin(staSsid.c_str(), staPass.c_str());
    Serial.print("[deye_fan] STA -> "); Serial.println(staSsid);
  }

  server.on("/",        handleRoot);
  server.on("/json",    handleJson);
  server.on("/ratio",   handleRatio);
  server.on("/wifi",    handleWifi);
  server.onNotFound([]() { server.send(404, "text/plain", "not found"); });
  server.begin();
  Serial.println("[deye_fan] web ready");
}

// ------------------------------------------------------------------
// LOOP : uniquement les tâches lentes (WiFi, HTTP, filtres, LED)
// ------------------------------------------------------------------
static uint32_t lastSlow = 0;
static uint32_t ledLast = 0;
static bool     ledOn = false;

void loop() {
  server.handleClient();          // bloquant uniquement si client connecté

  uint32_t now = millis();
  if (now - lastSlow < 10) return; // ~100 Hz, largement suffisant
  lastSlow = now;

  // ---- mesure RPM entrée (hors ISR) ----
  for (int i = 0; i < NCH; i++) {
    // période entre 2 pulses ; 2 pulses/tour → RPM = 30e6 / période(µs)
    uint32_t p = ch[i].inPeriodUs;
    bool ok = (p > 500) && (p < 200000);   // 15..6000 RPM plausibles
    if (ok) {
      double rpm = 30000000.0 / (double)p;
      ch[i].rpmReal = (ch[i].rpmReal == 0.0) ? rpm
                                              : ch[i].rpmReal * 0.9 + rpm * 0.1;
    } else if ((g_tickCount - ch[i].lastEdgeTick) * TICK_US > 500000UL) { // silence > 0.5 s
      ch[i].rpmReal = 0.0;
    }
    ch[i].inputValid = ok && (ch[i].rpmReal > 50.0);
  }

  // ---- calcul de la période de sortie (hors ISR, l'ISR ne lise que) ----
  for (int i = 0; i < NCH; i++) {
    double simRpm = ch[i].rpmReal * ch[i].ratio;
    if (simRpm > 50.0) {
      // 2 pulses/tour → fréquence pulse = simRpm*2/60 Hz
      double periodUs = 60000000.0 / simRpm / 2.0;
      uint32_t pt = (uint32_t)(periodUs / TICK_US);
      if (pt < 4) pt = 4;                    // ≥ 400 µs entre pulses
      ch[i].periodTicks = (uint16_t)pt;
      uint32_t pw = periodUs / 20.0;         // pulse ≈ 5 % de la période
      if (pw < 2) pw = 2;
      if (pw > 15) pw = 15;
      ch[i].pulseWidth = (uint16_t)pw;
      if (!ch[i].enabled) {                   // (re)démarrage propre
        ch[i].enabled   = true;
        ch[i].countDown = ch[i].periodTicks;
        ch[i].inPulse   = false;
        digitalWriteFast(i == 0 ? CH1_OUT_PIN : CH2_OUT_PIN, LOW);
      }
    } else if (ch[i].enabled) {
      ch[i].enabled = false;
      digitalWriteFast(i == 0 ? CH1_OUT_PIN : CH2_OUT_PIN, LOW);
    }
  }

  // ---- LED : allumée si simulation active, clignotement lent sinon ----
  bool simActive = ch[0].enabled || ch[1].enabled;
  if (simActive) {
    digitalWriteFast(LED_PIN, HIGH);
    ledOn = true;
  } else {
    if (now - ledLast > 500) {
      ledLast = now;
      ledOn = !ledOn;
      digitalWriteFast(LED_PIN, ledOn ? HIGH : LOW);
    }
  }
}
