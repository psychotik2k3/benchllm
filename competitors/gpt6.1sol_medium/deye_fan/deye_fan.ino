/*
 * Deye / Noctua, deux voies, Wemos D1 mini, ESP8266 Arduino core 3.1.2.
 * IMPORTANT : version ESP8266 seule, sans garantie d'isolation temporelle WiFi.
 * Voir README.md et SCHEMA.md AVANT raccordement. Ce programme ne valide ni
 * le refroidissement ni le protocole de surveillance propre à l'onduleur.
 *
 * D1/GPIO5 <- tach NF-A9, D2/GPIO4 <- tach NF-A6, pull-up externe 3,3 V.
 * D5/GPIO14 -> base NPN voie A9 ; D6/GPIO12 -> base NPN voie A6.
 * D4/GPIO2 : LED intégrée active à LOW. Allumée = au moins une voie active.
 * D0/GPIO16 : bouton facultatif vers GND, maintien 3 s = config par défaut.
 * Aucun signal 12 V ne doit être connecté directement à un GPIO.
 *
 * Capture et synthèse dans les ISR, pas dans loop(). Aucun String, flottant,
 * allocation, réseau ou accès flash depuis les ISR. Timer1 réservé : ne pas
 * ajouter analogWrite(), tone(), Servo ou une autre bibliothèque utilisant ce
 * timer. ISR ordinaires, PAS des NMI : le SDK peut toujours les retarder.
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <stddef.h>
#include <math.h>
#include <stdlib.h>

constexpr uint8_t IN9 = 5, IN6 = 4, OUT9 = 14, OUT6 = 12;
constexpr uint8_t LED = 2, RESET_BUTTON = 16;
constexpr uint32_t MIN_INPUT_US = 2000;    // 15000 RPM à 2 impulsions/tour
constexpr uint32_t MAX_INPUT_US = 500000;  // 60 RPM
constexpr uint32_t MIN_HALF_US = 250;      // sortie plafonnée à 2000 Hz
constexpr uint32_t MAX_WAKE_US = 2000;     // vérification timeout même à l'arrêt
constexpr uint32_t MIN_WAKE_US = 25;
constexpr uint32_t MAGIC = 0x44465932;
constexpr uint16_t VERSION = 1;

// Tous les champs partagés ISR/main sont en RAM et volatiles.
struct Channel {
  volatile uint32_t lastEdge;
  volatile uint32_t period;
  volatile uint32_t inverseQ12; // round(4096 / ratio), ratio dans [1,8]
  volatile uint32_t nextEdge;
  volatile uint32_t halfPeriod;
  volatile uint32_t lateMax;
  volatile uint32_t missed;
  volatile uint8_t good;
  volatile bool seen;
  volatile bool active;
  volatile bool level;
  volatile bool limited;
};
Channel channels[2] = {};

struct Configuration {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  float ratio[2];
  char staSsid[33];
  char staPass[65];
  char apSsid[33];
  char apPass[64];
  uint32_t crc;
};
Configuration config = {};
ESP8266WebServer server(80);
bool pendingSave = false, saveFailed = false, pendingNetwork = false;
uint32_t changedAt = 0, quietAt = 0, networkAt = 0;
String csrfToken;

// Prototypes explicites pour les types du sketch (préprocesseur Arduino).
uint32_t configCrc(const Configuration &c);
bool configValid(const Configuration &c);
void defaults(Configuration &c);

static uint32_t IRAM_ATTR timeoutUs(uint32_t period) {
  uint32_t t = period * 3u;
  return t < 300000u ? 300000u : t; // 0,3 à 1,5 s selon régime
}

static void IRAM_ATTR capture(uint8_t i) {
  Channel &c = channels[i];
  uint32_t now = micros(); // implémentation IRAM du core ESP8266
  if (!c.seen) {
    c.lastEdge = now;
    c.seen = true;
    c.good = 0;
    return;
  }
  uint32_t dt = now - c.lastEdge; // soustraction non signée : wrap micros sûr
  if (dt < MIN_INPUT_US) return;  // rejette le glitch sans déplacer la référence
  c.lastEdge = now;
  if (dt > MAX_INPUT_US) {
    c.good = 0;
    c.period = 0;
    return;
  }
  // Filtre IIR 1/4 ; première période non filtrée. Pas de division flottante.
  c.period = c.good == 0 ? dt : (3u * c.period + dt + 2u) >> 2;
  if (c.good < 3) ++c.good;       // trois périodes valides avant synthèse
}
void IRAM_ATTR tach9() { capture(0); }
void IRAM_ATTR tach6() { capture(1); }

void IRAM_ATTR tachTimer() {
  uint32_t now = micros();
  bool any = false;
  for (uint8_t i = 0; i < 2; ++i) {
    Channel &c = channels[i];
    uint32_t mask = i == 0 ? (1u << OUT9) : (1u << OUT6);
    uint32_t p = c.period;
    bool valid = c.seen && c.good >= 3 && p >= MIN_INPUT_US &&
                 p <= MAX_INPUT_US && (now - c.lastEdge) <= timeoutUs(p);
    if (!valid) {
      GPOC = mask; // base à 0 : NPN bloqué, sortie tach libérée
      c.active = false;
      c.level = false;
      c.halfPeriod = 0;
      c.limited = false;
      if (c.seen && now - c.lastEdge > 1500000u) {
        c.seen = false;
        c.good = 0;
        c.period = 0;
      }
      continue;
    }
    // Produit 32 bits <= 500000 * 4096 + 4096 : aucun débordement.
    uint32_t half = (p * c.inverseQ12 + 4096u) >> 13;
    c.limited = half < MIN_HALF_US;
    if (half < MIN_HALF_US) half = MIN_HALF_US;
    c.halfPeriod = half;
    any = true;
    if (!c.active) {
      c.active = true;
      c.level = false;
      GPOC = mask;
      c.nextEdge = now + half;
    } else if (static_cast<int32_t>(now - c.nextEdge) >= 0) {
      uint32_t late = now - c.nextEdge;
      if (late > c.lateMax) c.lateMax = late;
      c.level = !c.level;
      if (c.level) GPOS = mask; else GPOC = mask;
      // Échéances absolues : pas de dérive accumulée égale au temps de l'ISR.
      // Si retard d'au moins une demi-période, abandon du rattrapage : jamais
      // une rafale d'impulsions fictives pour compenser un blocage logiciel.
      if (late >= half) {
        ++c.missed;
        c.nextEdge = now + half;
      } else {
        c.nextEdge += half;
      }
    }
  }
  if (any) GPOC = 1u << LED; else GPOS = 1u << LED;
  // Réveil sur le prochain front des deux voies, ou sous 2 ms pour le timeout.
  now = micros();
  uint32_t waitUs = MAX_WAKE_US;
  for (uint8_t i = 0; i < 2; ++i) {
    if (!channels[i].active) continue;
    int32_t remaining = static_cast<int32_t>(channels[i].nextEdge - now);
    uint32_t w = remaining < static_cast<int32_t>(MIN_WAKE_US)
                   ? MIN_WAKE_US : static_cast<uint32_t>(remaining);
    if (w < waitUs) waitUs = w;
  }
  timer1_write(waitUs * 5u); // timer1 80 MHz / 16 = 5 ticks par microseconde
}

uint32_t configCrc(const Configuration &c) {
  const uint8_t *p = reinterpret_cast<const uint8_t *>(&c);
  uint32_t crc = 0xffffffffu;
  for (size_t i = 0; i < offsetof(Configuration, crc); ++i) {
    crc ^= p[i];
    for (uint8_t b = 0; b < 8; ++b)
      crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
  }
  return ~crc;
}

void defaults(Configuration &c) {
  memset(&c, 0, sizeof(c)); // padding déterministe pour CRC
  c.magic = MAGIC;
  c.version = VERSION;
  c.size = sizeof(c);
  c.ratio[0] = c.ratio[1] = 1.0f; // aucun ratio NMB supposé sans mesure
  snprintf(c.apSsid, sizeof(c.apSsid), "Deye-Fan-%06X", ESP.getChipId());
  snprintf(c.apPass, sizeof(c.apPass), "Deye%06X", ESP.getChipId());
  c.crc = configCrc(c);
}

bool configValid(const Configuration &c) {
  return c.magic == MAGIC && c.version == VERSION && c.size == sizeof(c) &&
    c.crc == configCrc(c) &&
    memchr(c.staSsid, 0, sizeof(c.staSsid)) &&
    memchr(c.staPass, 0, sizeof(c.staPass)) &&
    memchr(c.apSsid, 0, sizeof(c.apSsid)) &&
    memchr(c.apPass, 0, sizeof(c.apPass)) &&
    strlen(c.apSsid) >= 1 && strlen(c.apPass) >= 8 &&
    isfinite(c.ratio[0]) && isfinite(c.ratio[1]) &&
    c.ratio[0] >= 1 && c.ratio[0] <= 8 &&
    c.ratio[1] >= 1 && c.ratio[1] <= 8;
}

void applyRatios() {
  uint32_t r0 = static_cast<uint32_t>(4096.0f / config.ratio[0] + 0.5f);
  uint32_t r1 = static_cast<uint32_t>(4096.0f / config.ratio[1] + 0.5f);
  noInterrupts();
  channels[0].inverseQ12 = r0;
  channels[1].inverseQ12 = r1;
  interrupts();
}

void dirty() {
  pendingSave = true;
  saveFailed = false;
  changedAt = millis();
  quietAt = millis();
}

void startNetwork() {
  WiFi.persistent(false); // empêche les écritures flash cachées du SDK
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP); // aide la latence, aucune garantie temps réel
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1),
                    IPAddress(255,255,255,0));
  bool ok = WiFi.softAP(config.apSsid, config.apPass);
  Serial.printf("AP %s : %s\n", config.apSsid, ok ? "OK" : "ECHEC");
  WiFi.disconnect(false);
  WiFi.setAutoReconnect(true);
  if (config.staSsid[0]) WiFi.begin(config.staSsid, config.staPass);
  // Pas de boucle d'attente : une STA inaccessible ne bloque pas le système.
}

bool authorized(bool mutation = false) {
  if (!server.authenticate("admin", config.apPass)) {
    server.requestAuthentication();
    return false;
  }
  if (mutation && server.header("X-CSRF-Token") != csrfToken) {
    server.send(403, "text/plain", "Jeton de session invalide. Rechargez la page.");
    return false;
  }
  server.sendHeader("Cache-Control", "no-store");
  return true;
}

String jsonString(const char *s) {
  String out = "\"";
  while (*s) {
    uint8_t ch = *s++;
    if (ch == '\\' || ch == '"') { out += '\\'; out += static_cast<char>(ch); }
    else if (ch < 32) {
      char esc[7]; snprintf(esc, sizeof(esc), "\\u%04x", ch); out += esc;
    } else out += static_cast<char>(ch);
  }
  return out + '"';
}

const char PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="fr"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Deye / Noctua</title>
<style>body{font:16px system-ui;max-width:820px;margin:24px auto;padding:0 16px;background:#f4f6f7;color:#172b37}section{background:white;padding:18px;margin:16px 0;border-radius:10px}label{display:block;margin:12px 0}input{padding:8px;max-width:95%}button{padding:10px;cursor:pointer}table{width:100%;text-align:left}td,th{padding:8px}aside{background:#fff1ce;padding:12px}#msg{white-space:pre-wrap}</style>
<h1>Deye / Noctua</h1><aside>ESP8266 seul : isolation totale du WiFi impossible.
Les RPM de sortie sont calculés, pas mesurés sur le fil. Les ventilateurs doublés non instrumentés ne sont pas surveillés.</aside>
<section><h2>État</h2><p id="net">Connexion…</p><table><thead><tr><th>Canal</th><th>Lu RPM</th><th>Sortie RPM théoriques</th><th>État</th></tr></thead><tbody id="rows"></tbody></table><p id="diag"></p><p id="storage"></p></section>
<section><h2>Ratios</h2><form id="ratios"><label>9 cm <input id="r9" name="r9" type="number" min="1" max="8" step="0.001" required></label><label>6 cm <input id="r6" name="r6" type="number" min="1" max="8" step="0.001" required></label><button>Appliquer</button></form><p>Application immédiate en RAM ; sauvegarde flash différée jusqu'à trois secondes sans aucune impulsion d'entrée. Ne coupez pas l'alimentation avant confirmation.</p></section>
<section><h2>WiFi AP + STA</h2><form id="wifi"><label>SSID du point d'accès <input id="ap" name="ap" maxlength="32" required></label><label>Nouveau mot de passe AP / admin <input name="appass" type="password" minlength="8" maxlength="63" autocomplete="new-password" placeholder="Vide : inchangé"></label><label>SSID du réseau STA (vide : pas de connexion) <input id="sta" name="sta" maxlength="32"></label><label>Nouveau mot de passe STA <input name="stapass" type="password" maxlength="64" autocomplete="new-password" placeholder="Vide : inchangé"></label><label><input type="checkbox" name="opensta" value="1"> STA sans mot de passe / effacer la clé STA</label><button>Appliquer le WiFi</button></form><p>L'AP reste accessible à 192.168.4.1. AP et STA partagent le canal radio. Changer le WiFi peut interrompre cette page ; reconnectez-vous avec les nouveaux identifiants.</p></section><p id="msg" role="status"></p>
<script>
let token='',initialized=false,busy=false;
const $=id=>document.getElementById(id);
async function poll(){if(busy)return;busy=true;try{const r=await fetch('/api/status',{cache:'no-store'});if(!r.ok)throw Error('HTTP '+r.status);const s=await r.json();token=s.token;
if(!initialized){$('r9').value=s.ratio[0];$('r6').value=s.ratio[1];$('ap').value=s.ap;$('sta').value=s.sta;initialized=true;}
$('net').textContent='AP '+s.ap+' · STA '+s.ip+' · '+(s.connected?'connectée':'déconnectée');
$('rows').replaceChildren();s.channels.forEach((c,i)=>{let tr=document.createElement('tr');[i?'6 cm':'9 cm',c.rpm.toFixed(0),c.output.toFixed(0),c.active?(c.limited?'active / plafonnée':'active'):'attente'].forEach(x=>{let td=document.createElement('td');td.textContent=x;tr.append(td)});$('rows').append(tr)});
$('diag').textContent=s.channels.map((c,i)=>(i?'6':'9')+' cm : retard ISR max observé '+c.late+' µs ; resynchronisations '+c.missed).join(' · ');
$('storage').textContent=s.saveFailed?'Échec de sauvegarde : paramètres uniquement en RAM.':s.pending?'Sauvegarde en attente d’arrêt des deux entrées tach.':'Paramètres persistants / configuration chargée.';
}catch(e){$('net').textContent='Communication interrompue : '+e.message}finally{busy=false}}
async function submit(e,path){e.preventDefault();try{const r=await fetch(path,{method:'POST',headers:{'X-CSRF-Token':token},body:new URLSearchParams(new FormData(e.target))});const t=await r.text();if(!r.ok)throw Error(t);$('msg').textContent=t}catch(x){$('msg').textContent=x.message}}
$('ratios').onsubmit=e=>submit(e,'/api/ratios');$('wifi').onsubmit=e=>submit(e,'/api/wifi');poll();setInterval(poll,1000);
</script></html>)HTML";

void statusPage() {
  if (!authorized()) return;
  struct Snapshot { uint32_t p, half, late, missed; bool active, limited; } s[2];
  noInterrupts();
  for (uint8_t i = 0; i < 2; ++i) {
    s[i] = {channels[i].period, channels[i].halfPeriod, channels[i].lateMax,
            channels[i].missed, channels[i].active, channels[i].limited};
  }
  interrupts();
  String j; j.reserve(900);
  j = "{\"token\":" + jsonString(csrfToken.c_str());
  j += ",\"ratio\":[" + String(config.ratio[0], 3) + ',' + String(config.ratio[1], 3) + ']';
  j += ",\"ap\":" + jsonString(config.apSsid) + ",\"sta\":" + jsonString(config.staSsid);
  j += ",\"ip\":" + jsonString(WiFi.localIP().toString().c_str());
  j += ",\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
  j += ",\"pending\":" + String(pendingSave ? "true" : "false");
  j += ",\"saveFailed\":" + String(saveFailed ? "true" : "false");
  j += ",\"channels\":[";
  for (uint8_t i = 0; i < 2; ++i) {
    if (i) j += ',';
    // Hypothèse : 2 impulsions/tour en entrée ET interprétation Deye identique.
    float rpm = s[i].active && s[i].p ? 30000000.0f / s[i].p : 0;
    float output = s[i].active && s[i].half ? 15000000.0f / s[i].half : 0;
    j += "{\"rpm\":" + String(rpm, 2) + ",\"output\":" + String(output, 2);
    j += ",\"active\":" + String(s[i].active ? "true" : "false");
    j += ",\"limited\":" + String(s[i].limited ? "true" : "false");
    j += ",\"late\":" + String(s[i].late) + ",\"missed\":" + String(s[i].missed) + '}';
  }
  server.send(200, "application/json", j + "]}");
}

bool parseRatio(const String &s, float &v) {
  if (s.isEmpty() || s.length() > 16) return false;
  char *end = nullptr;
  v = strtof(s.c_str(), &end);
  return end != s.c_str() && *end == 0 && isfinite(v) && v >= 1 && v <= 8;
}

bool validStaPassword(const String &s) {
  if (s.isEmpty()) return true;
  if (s.length() >= 8 && s.length() <= 63) return true;
  if (s.length() != 64) return false;
  for (unsigned int i = 0; i < s.length(); ++i)
    if (!isxdigit(static_cast<unsigned char>(s[i]))) return false;
  return true;
}

void ratioPage() {
  if (!authorized(true)) return;
  float a, b;
  if (!parseRatio(server.arg("r9"), a) || !parseRatio(server.arg("r6"), b)) {
    server.send(400, "text/plain", "Ratios attendus : nombres finis entre 1 et 8."); return;
  }
  config.ratio[0] = a; config.ratio[1] = b;
  applyRatios(); dirty();
  server.send(200, "text/plain; charset=utf-8", "Ratios appliqués. Sauvegarde différée à l'arrêt des deux entrées.");
}

void wifiPage() {
  if (!authorized(true)) return;
  if (!server.hasArg("ap") || !server.hasArg("sta")) {
    server.send(400, "text/plain", "Champs AP et STA requis."); return;
  }
  String ap = server.arg("ap"), sta = server.arg("sta");
  String app = server.arg("appass"), stap = server.arg("stapass");
  if (ap.length() < 1 || ap.length() > 32 || sta.length() > 32 ||
      (!app.isEmpty() && (app.length() < 8 || app.length() > 63)) ||
      !validStaPassword(stap)) {
    server.send(400, "text/plain", "SSID ou mot de passe invalide (longueurs en octets)."); return;
  }
  ap.toCharArray(config.apSsid, sizeof(config.apSsid));
  sta.toCharArray(config.staSsid, sizeof(config.staSsid));
  if (!app.isEmpty()) app.toCharArray(config.apPass, sizeof(config.apPass));
  if (server.arg("opensta") == "1") config.staPass[0] = 0;
  else if (!stap.isEmpty()) stap.toCharArray(config.staPass, sizeof(config.staPass));
  dirty(); pendingNetwork = true; networkAt = millis();
  server.send(200, "text/plain; charset=utf-8", "WiFi appliqué dans une seconde. Identifiant web : admin ; mot de passe = clé AP. Sauvegarde différée à l'arrêt des entrées.");
}

void configureRoutes() {
  server.collectHeaders("X-CSRF-Token");
  server.on("/", HTTP_GET, []() {
    if (!authorized()) return;
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.send_P(200, "text/html; charset=utf-8", PAGE);
  });
  server.on("/api/status", HTTP_GET, statusPage);
  server.on("/api/ratios", HTTP_POST, ratioPage);
  server.on("/api/wifi", HTTP_POST, wifiPage);
  server.onNotFound([]() { server.send(404, "text/plain", "Introuvable"); });
  server.begin();
}

// Écriture flash délibérément différée : elle peut bloquer les interruptions.
// Cela réduit le risque mais ne protège pas contre un redémarrage des fans
// pendant commit(), ni contre les blocages internes du SDK : voir README.
void servicePersistence() {
  if (!pendingSave || saveFailed) return;
  uint32_t now = millis(), edge0, edge1;
  bool seen0, seen1, active;
  noInterrupts();
  edge0 = channels[0].lastEdge; edge1 = channels[1].lastEdge;
  seen0 = channels[0].seen; seen1 = channels[1].seen;
  active = channels[0].active || channels[1].active;
  interrupts();
  uint32_t usNow = micros(); // après la copie : jamais antérieur au front copié
  if (active || (seen0 && usNow - edge0 < 3000000u) ||
                (seen1 && usNow - edge1 < 3000000u)) {
    quietAt = now; return;
  }
  if (now - changedAt < 3000u || now - quietAt < 3000u) return;
  config.crc = configCrc(config);
  EEPROM.put(0, config);
  if (EEPROM.commit()) pendingSave = false;
  else saveFailed = true; // pas de boucle de tentatives d'effacement flash
}

void serviceResetButton() {
  static bool pressed = false, handled = false;
  static uint32_t since = 0;
  if (digitalRead(RESET_BUTTON) != LOW) { pressed = false; handled = false; return; }
  if (!pressed) { pressed = true; since = millis(); }
  if (!handled && millis() - since >= 3000u) {
    handled = true;
    defaults(config); applyRatios(); dirty();
    pendingNetwork = true; networkAt = millis();
    Serial.println("Configuration par defaut restauree en RAM ; sauvegarde differee.");
  }
}

void setup() {
  // Précharger LOW avant OUTPUT pour ne pas activer les transistors au boot.
  digitalWrite(OUT9, LOW); pinMode(OUT9, OUTPUT);
  digitalWrite(OUT6, LOW); pinMode(OUT6, OUTPUT);
  digitalWrite(LED, HIGH); pinMode(LED, OUTPUT);
  pinMode(IN9, INPUT); pinMode(IN6, INPUT); // résistances externes requises
  pinMode(RESET_BUTTON, INPUT);           // pull-up externe 10 k vers 3,3 V
  Serial.begin(115200);
  EEPROM.begin(512);
  static_assert(sizeof(Configuration) <= 512, "EEPROM trop petite");
  EEPROM.get(0, config);
  if (!configValid(config)) { defaults(config); dirty(); }
  applyRatios();
  attachInterrupt(digitalPinToInterrupt(IN9), tach9, FALLING);
  attachInterrupt(digitalPinToInterrupt(IN6), tach6, FALLING);
  timer1_isr_init();
  timer1_attachInterrupt(tachTimer);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
  timer1_write(MAX_WAKE_US * 5u);
  startNetwork();
  char token[25];
  snprintf(token, sizeof(token), "%08lX%08lX%08lX", os_random(), os_random(), os_random());
  csrfToken = token;
  configureRoutes();
  Serial.printf("Interface : http://192.168.4.1/ ; utilisateur admin\n");
  Serial.printf("Premier demarrage / reset : cle AP = Deye%06X\n", ESP.getChipId());
}

void loop() {
  server.handleClient(); // aucune génération de fronts dans cette boucle
  serviceResetButton();
  if (pendingNetwork && millis() - networkAt >= 1000u) {
    pendingNetwork = false;
    startNetwork();
  }
  servicePersistence();
  yield();
}
