/*
 * ================================================================
 * DEYE FAN TACH MULTIPLIER
 * ================================================================
 *
 * Cible :
 *   LOLIN(WEMOS) D1 mini
 *   ESP8266
 *
 * Fonction :
 *   - mesure le tach d'un Noctua 92 mm
 *   - mesure le tach d'un Noctua 60 mm
 *   - multiplie indépendamment leurs RPM
 *   - génère deux tach simulés vers le Deye
 *   - WiFi AP + STA simultanément
 *   - configuration via interface Web
 *   - ratios persistants en EEPROM
 *   - génération tach indépendante du loop()/WiFi
 *
 * ================================================================
 *
 * BROCHAGE
 *
 * Canal 92 mm :
 *   Noctua TACH -> D1 / GPIO5
 *   Deye TACH   <- D5 / GPIO14 via transistor NPN
 *
 * Canal 60 mm :
 *   Noctua TACH -> D2 / GPIO4
 *   Deye TACH   <- D6 / GPIO12 via transistor NPN
 *
 * LED :
 *   D7 / GPIO13
 *
 * ================================================================
 *
 * IMPORTANT
 *
 * Les entrées ESP8266 ne doivent JAMAIS recevoir directement
 * 5 V ou 12 V.
 *
 * Utiliser le diviseur :
 *
 *       TACH NOCTUA
 *            |
 *          10 k
 *            |
 *            +------ 1 k ------ GPIO
 *            |
 *          3.3 k
 *            |
 *           GND
 *
 * Le Deye TACH est connecté via un transistor NPN en
 * collecteur ouvert.
 *
 * ================================================================
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

#include "core_esp8266_waveform.h"


// =================================================================
// BROCHES
// =================================================================

constexpr uint8_t PIN_TACH_92_IN  = D1;   // GPIO5
constexpr uint8_t PIN_TACH_60_IN  = D2;   // GPIO4

constexpr uint8_t PIN_TACH_92_OUT = D5;   // GPIO14
constexpr uint8_t PIN_TACH_60_OUT = D6;   // GPIO12

constexpr uint8_t PIN_LED         = D7;   // GPIO13


// =================================================================
// PARAMÈTRES VENTILATEURS
// =================================================================

/*
 * Les ventilateurs 3 fils PC utilisent normalement deux impulsions
 * tach par révolution.
 */
constexpr uint8_t TACH_PULSES_PER_REV = 2;


// =================================================================
// LIMITES
// =================================================================

constexpr float RATIO_MIN = 0.10f;
constexpr float RATIO_MAX = 10.00f;

/*
 * Timeout après lequel on considère que le ventilateur ne fournit
 * plus de tach.
 */
constexpr uint32_t TACH_TIMEOUT_US = 500000UL;

/*
 * Filtrage anti-glitch.
 *
 * Avec 160 MHz CPU, 40000 cycles = 250 us.
 *
 * Cela correspond à une fréquence tach maximale de 4 kHz,
 * largement supérieure à ce qui est attendu ici.
 */
constexpr uint32_t MIN_PERIOD_CYCLES = 40000UL;

/*
 * 320 millions de cycles à 160 MHz = 2 secondes.
 */
constexpr uint32_t MAX_PERIOD_CYCLES = 320000000UL;


// =================================================================
// EEPROM
// =================================================================

constexpr uint32_t EEPROM_MAGIC = 0x44594631UL;
constexpr size_t EEPROM_SIZE = 512;


struct Config
{
  uint32_t magic;

  float ratio92;
  float ratio60;

  char staSSID[64];
  char staPassword[64];

  char apSSID[33];
  char apPassword[65];
};


Config config;


// =================================================================
// ÉTAT TACH
// =================================================================

struct TachChannel
{
  /*
   * Variables écrites par ISR.
   */
  volatile uint32_t lastEdgeCycles;
  volatile uint32_t periodCycles;
  volatile bool newPeriod;

  /*
   * Variables utilisées hors ISR.
   */
  uint32_t filteredPeriodCycles;

  uint32_t lastValidEdgeUs;

  float rpm;
  float simulatedRPM;

  bool signalPresent;
};


TachChannel tach92 = {};
TachChannel tach60 = {};


// =================================================================
// SERVEUR WEB
// =================================================================

ESP8266WebServer server(80);


// =================================================================
// CONFIGURATION PAR DÉFAUT
// =================================================================

void setDefaultConfig()
{
  memset(&config, 0, sizeof(config));

  config.magic = EEPROM_MAGIC;

  /*
   * Valeurs initiales.
   *
   * Elles sont volontairement faciles à modifier depuis
   * l'interface Web.
   */
  config.ratio92 = 2.00f;
  config.ratio60 = 2.00f;

  /*
   * Pas de réseau STA par défaut.
   *
   * Le système démarre uniquement avec son AP.
   */
  config.staSSID[0] = '\0';
  config.staPassword[0] = '\0';

  strcpy(config.apSSID, "DeyeFan");

  /*
   * WPA2 exige normalement au minimum 8 caractères.
   */
  strcpy(config.apPassword, "DeyeFan123");
}


// =================================================================
// EEPROM - LECTURE
// =================================================================

void loadConfig()
{
  EEPROM.begin(EEPROM_SIZE);

  EEPROM.get(0, config);

  if (config.magic != EEPROM_MAGIC)
  {
    setDefaultConfig();

    EEPROM.put(0, config);
    EEPROM.commit();
  }

  /*
   * Sécurisation des ratios.
   */
  config.ratio92 = constrain(
    config.ratio92,
    RATIO_MIN,
    RATIO_MAX
  );

  config.ratio60 = constrain(
    config.ratio60,
    RATIO_MIN,
    RATIO_MAX
  );
}


// =================================================================
// EEPROM - ÉCRITURE
// =================================================================

void saveConfig()
{
  config.magic = EEPROM_MAGIC;

  EEPROM.put(0, config);

  EEPROM.commit();
}


// =================================================================
// COMPTEUR DE CYCLES ESP8266
// =================================================================

static inline uint32_t IRAM_ATTR getCycleCounter()
{
  uint32_t cycles;

  __asm__ __volatile__(
    "rsr %0,ccount"
    : "=a"(cycles)
  );

  return cycles;
}


// =================================================================
// ISR TACH 92 mm
// =================================================================

void IRAM_ATTR tach92ISR()
{
  uint32_t now = getCycleCounter();

  uint32_t previous = tach92.lastEdgeCycles;

  tach92.lastEdgeCycles = now;

  if (previous == 0)
  {
    return;
  }

  /*
   * unsigned subtraction fonctionne également lorsque CCOUNT
   * repasse de 0xffffffff à 0.
   */
  uint32_t period = now - previous;

  /*
   * Élimination des glitches électriques.
   */
  if (period >= MIN_PERIOD_CYCLES &&
      period <= MAX_PERIOD_CYCLES)
  {
    tach92.periodCycles = period;
    tach92.newPeriod = true;
  }
}


// =================================================================
// ISR TACH 60 mm
// =================================================================

void IRAM_ATTR tach60ISR()
{
  uint32_t now = getCycleCounter();

  uint32_t previous = tach60.lastEdgeCycles;

  tach60.lastEdgeCycles = now;

  if (previous == 0)
  {
    return;
  }

  uint32_t period = now - previous;

  if (period >= MIN_PERIOD_CYCLES &&
      period <= MAX_PERIOD_CYCLES)
  {
    tach60.periodCycles = period;
    tach60.newPeriod = true;
  }
}


// =================================================================
// CYCLES -> RPM
// =================================================================

float cyclesToRPM(uint32_t cycles)
{
  if (cycles == 0)
  {
    return 0.0f;
  }

  /*
   * fréquence tach :
   *
   *       F_CPU / période_cycles
   *
   * RPM :
   *
   *       fréquence × 60 / impulsions_par_tour
   */
  double tachFrequency =
    (double)F_CPU / (double)cycles;

  double rpm =
    tachFrequency *
    60.0 /
    (double)TACH_PULSES_PER_REV;

  return (float)rpm;
}


// =================================================================
// ARRÊT DU SIGNAL DE SORTIE
// =================================================================

void stopTachOutput(uint8_t pin)
{
  /*
   * stopWaveform() arrête le générateur du core ESP8266.
   */
  stopWaveform(pin);

  /*
   * Le transistor NPN doit être éteint lorsque le signal
   * n'est plus généré.
   */
  digitalWrite(pin, LOW);
}


// =================================================================
// CALCUL DU SIGNAL TACH
// =================================================================

void setTachOutputRPM(uint8_t pin, float rpm)
{
  if (rpm <= 0.1f)
  {
    stopTachOutput(pin);
    return;
  }

  /*
   * Tach :
   *
   *        2 impulsions / tour
   *
   * fréquence :
   *
   *        RPM / 60 × 2
   *
   * période :
   *
   *        60 / (RPM × 2)
   */

  double periodSeconds =
    60.0 /
    ((double)rpm *
     (double)TACH_PULSES_PER_REV);

  /*
   * Conversion en cycles CPU.
   *
   * F_CPU vaut généralement :
   *
   *   80 MHz
   *   ou
   *   160 MHz
   */
  double periodCycles =
    periodSeconds *
    (double)F_CPU;

  if (periodCycles < 4.0)
  {
    periodCycles = 4.0;
  }

  /*
   * Signal 50 % :
   *
   *      HIGH | LOW | HIGH | LOW
   */
  uint32_t halfCycles =
    (uint32_t)(periodCycles / 2.0);

  if (halfCycles < 2)
  {
    halfCycles = 2;
  }

  /*
   * IMPORTANT :
   *
   * runTimeCycles = 0
   *       => fonctionnement continu
   *
   * alignPhase = -1
   *       => pas de synchronisation de phase
   *
   * phaseOffsetUS = 0
   *
   * autoPwm = false
   *
   * Le générateur de waveform du core ESP8266 utilise Timer1/NMI
   * et le compteur de cycles CPU pour générer le signal.
   */
  startWaveformClockCycles(
    pin,
    halfCycles,
    halfCycles,
    0,
    -1,
    0,
    false
  );
}


// =================================================================
// TRAITEMENT D'UN CANAL
// =================================================================

void processChannel(
  TachChannel &channel,
  uint8_t outputPin,
  float ratio
)
{
  uint32_t period = 0;
  bool newMeasurement = false;

  /*
   * --------------------------------------------------------------
   * SECTION CRITIQUE
   * --------------------------------------------------------------
   *
   * On copie simplement la mesure fournie par l'ISR.
   *
   * Aucune opération WiFi, String, calcul flottant ou HTTP
   * n'est effectuée dans l'ISR.
   */
  noInterrupts();

  if (channel.newPeriod)
  {
    period = channel.periodCycles;

    channel.newPeriod = false;

    newMeasurement = true;
  }

  interrupts();


  /*
   * --------------------------------------------------------------
   * NOUVELLE MESURE
   * --------------------------------------------------------------
   */

  if (newMeasurement)
  {
    /*
     * Filtre IIR :
     *
     *     nouvelle = 7/8 ancienne + 1/8 mesure
     *
     * Cela évite que de petites variations du tach réel
     * se retrouvent directement sur le tach simulé.
     */
    if (channel.filteredPeriodCycles == 0)
    {
      channel.filteredPeriodCycles = period;
    }
    else
    {
      uint64_t filtered =
        ((uint64_t)channel.filteredPeriodCycles * 7ULL +
         (uint64_t)period)
        / 8ULL;

      channel.filteredPeriodCycles =
        (uint32_t)filtered;
    }


    /*
     * Calcul RPM réel.
     */
    channel.rpm =
      cyclesToRPM(
        channel.filteredPeriodCycles
      );


    /*
     * Application du ratio.
     */
    channel.simulatedRPM =
      channel.rpm * ratio;


    /*
     * Signal reçu récemment.
     */
    channel.lastValidEdgeUs = micros();

    channel.signalPresent = true;


    /*
     * ------------------------------------------------------------
     * MISE À JOUR DU GÉNÉRATEUR
     * ------------------------------------------------------------
     *
     * Le waveform generator continue à générer le signal pendant
     * que le WiFi fonctionne.
     *
     * Cette fonction ne produit pas elle-même les fronts.
     */
    setTachOutputRPM(
      outputPin,
      channel.simulatedRPM
    );
  }


  /*
   * --------------------------------------------------------------
   * TIMEOUT
   * --------------------------------------------------------------
   */

  uint32_t nowUs = micros();

  if (channel.signalPresent)
  {
    if ((uint32_t)(nowUs - channel.lastValidEdgeUs)
        > TACH_TIMEOUT_US)
    {
      channel.signalPresent = false;

      channel.rpm = 0.0f;

      channel.simulatedRPM = 0.0f;

      stopTachOutput(outputPin);
    }
  }
}


// =================================================================
// LED
// =================================================================

void updateLED()
{
  bool active =
    tach92.signalPresent ||
    tach60.signalPresent;

  digitalWrite(
    PIN_LED,
    active ? HIGH : LOW
  );
}


// =================================================================
// PAGE WEB
// =================================================================

String makeWebPage()
{
  String html;

  html.reserve(9000);

  html += F(
    "<!DOCTYPE html>"
    "<html lang='fr'>"
    "<head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' "
    "content='width=device-width,initial-scale=1'>"

    "<title>Deye Fan Tach Multiplier</title>"

    "<style>"

    "body{"
    "font-family:Arial,sans-serif;"
    "background:#101010;"
    "color:#eee;"
    "margin:0;"
    "padding:20px;"
    "}"

    ".container{"
    "max-width:700px;"
    "margin:auto;"
    "}"

    ".card{"
    "background:#202020;"
    "border-radius:12px;"
    "padding:20px;"
    "margin-bottom:20px;"
    "box-shadow:0 2px 10px #000;"
    "}"

    "h1{color:#55aaff;}"

    "h2{color:#ddd;}"

    ".rpm{"
    "font-size:30px;"
    "font-weight:bold;"
    "color:#55ff88;"
    "}"

    ".inactive{"
    "color:#ff5555;"
    "}"

    "label{"
    "display:block;"
    "margin-top:12px;"
    "margin-bottom:4px;"
    "}"

    "input{"
    "box-sizing:border-box;"
    "width:100%;"
    "padding:10px;"
    "font-size:18px;"
    "border-radius:6px;"
    "border:1px solid #555;"
    "background:#111;"
    "color:#fff;"
    "}"

    "button{"
    "margin-top:20px;"
    "padding:12px 20px;"
    "font-size:18px;"
    "border:0;"
    "border-radius:7px;"
    "background:#1677ff;"
    "color:white;"
    "cursor:pointer;"
    "}"

    "button:hover{"
    "background:#3390ff;"
    "}"

    ".status{"
    "font-weight:bold;"
    "}"

    "</style>"
    "</head>"

    "<body>"

    "<div class='container'>"

    "<h1>Deye Fan Tach Multiplier</h1>"

    "<div class='card'>"

    "<h2>Ventilateur 92 mm</h2>"

    "<p>"
    "RPM réel : "
    "<span id='rpm92' class='rpm'>---</span>"
    "</p>"

    "<p>"
    "RPM simulé : "
    "<span id='sim92' class='rpm'>---</span>"
    "</p>"

    "<p>"
    "État : "
    "<span id='state92' class='status'>---</span>"
    "</p>"

    "<label for='ratio92'>Ratio multiplicateur</label>"

    "<input "
    "id='ratio92' "
    "type='number' "
    "step='0.01' "
    "min='0.10' "
    "max='10.00'>"

    "</div>"


    "<div class='card'>"

    "<h2>Ventilateur 60 mm</h2>"

    "<p>"
    "RPM réel : "
    "<span id='rpm60' class='rpm'>---</span>"
    "</p>"

    "<p>"
    "RPM simulé : "
    "<span id='sim60' class='rpm'>---</span>"
    "</p>"

    "<p>"
    "État : "
    "<span id='state60' class='status'>---</span>"
    "</p>"

    "<label for='ratio60'>Ratio multiplicateur</label>"

    "<input "
    "id='ratio60' "
    "type='number' "
    "step='0.01' "
    "min='0.10' "
    "max='10.00'>"

    "</div>"


    "<div class='card'>"

    "<h2>WiFi STA</h2>"

    "<label>SSID</label>"

    "<input "
    "id='staSSID' "
    "maxlength='63'>"

    "<label>Mot de passe</label>"

    "<input "
    "id='staPassword' "
    "type='password' "
    "maxlength='63'>"

    "</div>"


    "<div class='card'>"

    "<h2>Point d'accès ESP8266</h2>"

    "<label>SSID AP</label>"

    "<input "
    "id='apSSID' "
    "maxlength='32'>"

    "<label>Mot de passe AP</label>"

    "<input "
    "id='apPassword' "
    "type='password' "
    "maxlength='64'>"

    "</div>"


    "<div class='card'>"

    "<button onclick='saveConfig()'>"
    "Enregistrer"
    "</button>"

    "<p id='message'></p>"

    "</div>"


    "</div>"


    "<script>"

    "function updateStatus(){"

    "fetch('/api/status')"

    ".then(r=>r.json())"

    ".then(d=>{"

    "document.getElementById('rpm92').textContent="
    "d.rpm92.toFixed(0);"

    "document.getElementById('sim92').textContent="
    "d.sim92.toFixed(0);"

    "document.getElementById('rpm60').textContent="
    "d.rpm60.toFixed(0);"

    "document.getElementById('sim60').textContent="
    "d.sim60.toFixed(0);"

    "document.getElementById('state92').textContent="
    "d.active92?'ACTIF':'ATTENTE';"

    "document.getElementById('state60').textContent="
    "d.active60?'ACTIF':'ATTENTE';"

    "});"

    "}"


    "function saveConfig(){"

    "let body="

    "'ratio92='+encodeURIComponent("
    "document.getElementById('ratio92').value)"

    "+'&ratio60='+encodeURIComponent("
    "document.getElementById('ratio60').value)"

    "+'&staSSID='+encodeURIComponent("
    "document.getElementById('staSSID').value)"

    "+'&staPassword='+encodeURIComponent("
    "document.getElementById('staPassword').value)"

    "+'&apSSID='+encodeURIComponent("
    "document.getElementById('apSSID').value)"

    "+'&apPassword='+encodeURIComponent("
    "document.getElementById('apPassword').value);"


    "fetch('/api/save',{"
    "method:'POST',"
    "headers:{"
    "'Content-Type':"
    "'application/x-www-form-urlencoded'"
    "},"
    "body:body"
    "})"

    ".then(r=>r.text())"

    ".then(t=>{"

    "document.getElementById('message').textContent="
    "'Configuration enregistrée.';"

    "});"

    "}"


    "function loadConfig(){"

    "fetch('/api/config')"

    ".then(r=>r.json())"

    ".then(d=>{"

    "document.getElementById('ratio92').value="
    "d.ratio92;"

    "document.getElementById('ratio60').value="
    "d.ratio60;"

    "document.getElementById('staSSID').value="
    "d.staSSID;"

    "document.getElementById('staPassword').value="
    "d.staPassword;"

    "document.getElementById('apSSID').value="
    "d.apSSID;"

    "document.getElementById('apPassword').value="
    "d.apPassword;"

    "});"

    "}"


    "window.addEventListener('load',()=>{"

    "loadConfig();"

    "updateStatus();"

    "setInterval(updateStatus,500);"

    "});"

    "</script>"

    "</body>"
    "</html>"
  );

  return html;
}


// =================================================================
// HTTP : PAGE PRINCIPALE
// =================================================================

void handleRoot()
{
  server.send(
    200,
    "text/html; charset=utf-8",
    makeWebPage()
  );
}


// =================================================================
// HTTP : STATUS
// =================================================================

void handleStatus()
{
  String json;

  json.reserve(300);

  json += "{";

  json += "\"rpm92\":";
  json += String(tach92.rpm, 1);

  json += ",\"sim92\":";
  json += String(tach92.simulatedRPM, 1);

  json += ",\"rpm60\":";
  json += String(tach60.rpm, 1);

  json += ",\"sim60\":";
  json += String(tach60.simulatedRPM, 1);

  json += ",\"active92\":";
  json += tach92.signalPresent ? "true" : "false";

  json += ",\"active60\":";
  json += tach60.signalPresent ? "true" : "false";

  json += "}";

  server.send(
    200,
    "application/json",
    json
  );
}


// =================================================================
// HTTP : CONFIGURATION
// =================================================================

void handleGetConfig()
{
  String json;

  json.reserve(500);

  json += "{";

  json += "\"ratio92\":";
  json += String(config.ratio92, 2);

  json += ",\"ratio60\":";
  json += String(config.ratio60, 2);

  json += ",\"staSSID\":\"";
  json += String(config.staSSID);
  json += "\"";

  json += ",\"staPassword\":\"";
  json += String(config.staPassword);
  json += "\"";

  json += ",\"apSSID\":\"";
  json += String(config.apSSID);
  json += "\"";

  json += ",\"apPassword\":\"";
  json += String(config.apPassword);
  json += "\"";

  json += "}";

  server.send(
    200,
    "application/json",
    json
  );
}


// =================================================================
// HTTP : SAUVEGARDE
// =================================================================

void handleSaveConfig()
{
  /*
   * Ratio 92 mm.
   */
  if (server.hasArg("ratio92"))
  {
    config.ratio92 =
      constrain(
        server.arg("ratio92").toFloat(),
        RATIO_MIN,
        RATIO_MAX
      );
  }


  /*
   * Ratio 60 mm.
   */
  if (server.hasArg("ratio60"))
  {
    config.ratio60 =
      constrain(
        server.arg("ratio60").toFloat(),
        RATIO_MIN,
        RATIO_MAX
      );
  }


  /*
   * STA SSID.
   */
  if (server.hasArg("staSSID"))
  {
    server.arg("staSSID").toCharArray(
      config.staSSID,
      sizeof(config.staSSID)
    );
  }


  /*
   * STA password.
   */
  if (server.hasArg("staPassword"))
  {
    server.arg("staPassword").toCharArray(
      config.staPassword,
      sizeof(config.staPassword)
    );
  }


  /*
   * AP SSID.
   */
  if (server.hasArg("apSSID"))
  {
    server.arg("apSSID").toCharArray(
      config.apSSID,
      sizeof(config.apSSID)
    );
  }


  /*
   * AP password.
   */
  if (server.hasArg("apPassword"))
  {
    server.arg("apPassword").toCharArray(
      config.apPassword,
      sizeof(config.apPassword)
    );
  }


  /*
   * Sécurité AP.
   */
  if (strlen(config.apPassword) < 8)
  {
    strcpy(
      config.apPassword,
      "DeyeFan123"
    );
  }


  saveConfig();


  /*
   * Le ratio est appliqué immédiatement.
   */
  if (tach92.signalPresent)
  {
    tach92.simulatedRPM =
      tach92.rpm * config.ratio92;

    setTachOutputRPM(
      PIN_TACH_92_OUT,
      tach92.simulatedRPM
    );
  }


  if (tach60.signalPresent)
  {
    tach60.simulatedRPM =
      tach60.rpm * config.ratio60;

    setTachOutputRPM(
      PIN_TACH_60_OUT,
      tach60.simulatedRPM
    );
  }


  server.send(
    200,
    "text/plain",
    "OK"
  );
}


// =================================================================
// WIFI
// =================================================================

void startWiFi()
{
  /*
   * AP + STA simultanément.
   */
  WiFi.mode(WIFI_AP_STA);


  /*
   * --------------------------------------------------------------
   * AP
   * --------------------------------------------------------------
   */

  String apSSID =
    String(config.apSSID);

  String apPassword =
    String(config.apPassword);

  if (apPassword.length() < 8)
  {
    apPassword = "DeyeFan123";
  }

  WiFi.softAP(
    apSSID.c_str(),
    apPassword.c_str()
  );


  /*
   * --------------------------------------------------------------
   * STA
   * --------------------------------------------------------------
   *
   * Si aucun SSID n'est configuré, le système reste simplement
   * en AP.
   */

  if (strlen(config.staSSID) > 0)
  {
    WiFi.begin(
      config.staSSID,
      config.staPassword
    );
  }
}


// =================================================================
// SERVEUR WEB
// =================================================================

void startWebServer()
{
  server.on(
    "/",
    HTTP_GET,
    handleRoot
  );

  server.on(
    "/api/status",
    HTTP_GET,
    handleStatus
  );

  server.on(
    "/api/config",
    HTTP_GET,
    handleGetConfig
  );

  server.on(
    "/api/save",
    HTTP_POST,
    handleSaveConfig
  );

  server.begin();
}


// =================================================================
// SETUP
// =================================================================

void setup()
{
  /*
   * --------------------------------------------------------------
   * GPIO
   * --------------------------------------------------------------
   */

  /*
   * Entrées tach.
   *
   * Elles sont alimentées par le diviseur externe.
   */
  pinMode(
    PIN_TACH_92_IN,
    INPUT
  );

  pinMode(
    PIN_TACH_60_IN,
    INPUT
  );


  /*
   * Sorties vers les bases des NPN.
   */
  pinMode(
    PIN_TACH_92_OUT,
    OUTPUT
  );

  pinMode(
    PIN_TACH_60_OUT,
    OUTPUT
  );


  /*
   * LED.
   */
  pinMode(
    PIN_LED,
    OUTPUT
  );


  /*
   * Au démarrage, les NPN sont éteints.
   */
  digitalWrite(
    PIN_TACH_92_OUT,
    LOW
  );

  digitalWrite(
    PIN_TACH_60_OUT,
    LOW
  );

  digitalWrite(
    PIN_LED,
    LOW
  );


  /*
   * --------------------------------------------------------------
   * SERIAL
   * --------------------------------------------------------------
   */

  Serial.begin(115200);

  delay(100);

  Serial.println();
  Serial.println();
  Serial.println(
    "======================================"
  );

  Serial.println(
    " Deye Fan Tach Multiplier"
  );

  Serial.println(
    " ESP8266 / Wemos D1 mini"
  );

  Serial.println(
    "======================================"
  );


  /*
   * --------------------------------------------------------------
   * CONFIGURATION
   * --------------------------------------------------------------
   */

  loadConfig();


  Serial.print(
    "Ratio 92 mm : "
  );

  Serial.println(
    config.ratio92
  );


  Serial.print(
    "Ratio 60 mm : "
  );

  Serial.println(
    config.ratio60
  );


  /*
   * --------------------------------------------------------------
   * INTERRUPTIONS TACH
   * --------------------------------------------------------------
   *
   * On ne mesure que le front montant.
   *
   * Cela donne une mesure indépendante du duty-cycle du tach.
   */

  attachInterrupt(
    digitalPinToInterrupt(PIN_TACH_92_IN),
    tach92ISR,
    RISING
  );


  attachInterrupt(
    digitalPinToInterrupt(PIN_TACH_60_IN),
    tach60ISR,
    RISING
  );


  /*
   * --------------------------------------------------------------
   * WIFI
   * --------------------------------------------------------------
   */

  startWiFi();


  /*
   * --------------------------------------------------------------
   * SERVEUR
   * --------------------------------------------------------------
   */

  startWebServer();


  /*
   * --------------------------------------------------------------
   * INFORMATIONS
   * --------------------------------------------------------------
   */

  Serial.print(
    "AP SSID : "
  );

  Serial.println(
    config.apSSID
  );


  Serial.print(
    "AP IP : "
  );

  Serial.println(
    WiFi.softAPIP()
  );


  Serial.print(
    "CPU : "
  );

  Serial.print(
    F_CPU / 1000000
  );

  Serial.println(
    " MHz"
  );


  Serial.println(
    "System ready."
  );
}


// =================================================================
// LOOP
// =================================================================

void loop()
{
  /*
   * --------------------------------------------------------------
   * IMPORTANT ARCHITECTURE
   * --------------------------------------------------------------
   *
   * Le loop n'est PAS responsable de générer les impulsions.
   *
   * Les fronts sont générés par le waveform generator du core
   * ESP8266.
   *
   * Le WiFi et le serveur HTTP peuvent donc fonctionner ici sans
   * utiliser delayMicroseconds() pour fabriquer le tach.
   */


  /*
   * HTTP.
   */
  server.handleClient();


  /*
   * Traitement du canal 92 mm.
   */
  processChannel(
    tach92,
    PIN_TACH_92_OUT,
    config.ratio92
  );


  /*
   * Traitement du canal 60 mm.
   */
  processChannel(
    tach60,
    PIN_TACH_60_OUT,
    config.ratio60
  );


  /*
   * LED.
   */
  updateLED();


  /*
   * Permet au stack WiFi de fonctionner.
   */
  yield();
}