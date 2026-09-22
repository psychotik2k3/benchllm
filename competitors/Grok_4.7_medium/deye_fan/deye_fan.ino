/*
 * deye_fan.ino — simulateur de tachymètre pour onduleur Deye
 *
 * Carte : LOLIN(WEMOS) D1 mini (ESP8266), CPU 80 MHz, flash 4 Mo.
 * Core  : esp8266/Arduino 3.x (Board Manager). Aucune bibliothèque externe.
 *
 * Deux canaux indépendants :
 *   D1 / GPIO5  entrée tach Noctua 6 cm (NF-A6x25 FLX)
 *   D2 / GPIO4  entrée tach Noctua 9 cm (NF-A9 FLX)
 *   D5 / GPIO14 sortie open-collector vers l'onduleur, canal 6 cm
 *   D6 / GPIO12 sortie open-collector vers l'onduleur, canal 9 cm
 *   D4 / GPIO2  LED carte, active à l'état bas
 *
 * Pourquoi le signal ne bouge pas quand le WiFi travaille
 * -------------------------------------------------------
 * L'ESP8266 n'a qu'un cœur. Le WiFi masque les interruptions ordinaires
 * pendant des centaines de microsecondes (piles réseau, écriture flash).
 * Une interruption GPIO classique daterait donc les fronts trop tard, et
 * un timer logiciel (os_timer, millis, analogWrite) serait tout aussi
 * décalé. analogWrite / tone / Servo sont interdits ici : ils s'approprient
 * le timer1.
 *
 * Le timer1 matériel est attaché en NMI (interruption non masquable).
 * Le WiFi ne peut pas la retarder. Cette NMI est la seule tâche qui lit
 * les tach et qui écrit les sorties. Le serveur web, l'EEPROM et la LED
 * tournent dans loop() et n'ont aucun accès aux broches de sortie.
 *
 * Les sorties sont un carré synthétisé, calé sur le compteur de cycles
 * (80 MHz), pas une recopie impulsion par impulsion. La NMI ne fait que
 * poser le front à l'instant prévu : le jitter restant est celui de
 * l'entrée en NMI, bien en dessous de 1 µs. L'entrée est échantillonnée
 * au plus toutes les 32 µs par la même base de temps, puis filtrée.
 * Une requête HTTP ne change ni la date des fronts ni la période émise.
 *
 * Formule (2 impulsions tach par tour, Noctua et NMB) :
 *   RPM = fréquence_Hz × 30
 *   période_sortie = période_entrée / ratio
 * Vérification : 3000 RPM → 100 Hz → 800000 cycles à 80 MHz.
 * ratio 2,35 (q10 = 2406) → demi-période 170241 cycles → 7050 RPM.
 *
 * Ratios par défaut, à affiner dans la page web :
 *   6 cm : 2,35 car NMB 06025VE-12N = 7000 RPM, NF-A6x25 FLX = 3000 RPM
 *   9 cm : 2,80 car NF-A9 FLX = 1600 RPM et la classe N des 09225
 *          proches du 09225VE-12N-CU est à 4500 RPM. Le débit exact du
 *          09225VE-12N-CU n'est pas publié sous ce suffixe : régler au banc.
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <cstddef>
#include <ets_sys.h>
#include <user_interface.h>

static const uint8_t PIN_IN6 = D1;          // GPIO5
static const uint8_t PIN_IN9 = D2;          // GPIO4
static const uint8_t PIN_OUT6 = D5;         // GPIO14
static const uint8_t PIN_OUT9 = D6;         // GPIO12
static const uint8_t PIN_LED = LED_BUILTIN; // GPIO2, LED allumée à LOW

static const uint32_t CPU_HZ = 80000000UL;
static const uint32_t POLL_CYC = 32UL * 80UL;             // grille d'échantillonnage entrée
static const uint32_t MIN_ARM_CYC = 12UL * 80UL;          // réarmement minimum du timer1
static const uint32_t MIN_IN_CYC = 2UL * CPU_HZ / 1000UL; // 2 ms  → 15 000 RPM max lus
static const uint32_t MAX_IN_CYC = 300UL * CPU_HZ / 1000UL;
static const uint32_t LOST_CYC = 350UL * CPU_HZ / 1000UL; // plus de front = ventilateur arrêté
static const uint32_t MIN_HALF_CYC = 100000UL;            // 1,25 ms → plafond 12 000 RPM simulés
static const uint32_t RATIO_MIN_Q10 = 512;                // 0,50 × 1024
static const uint32_t RATIO_MAX_Q10 = 6144;               // 6,00 × 1024
static const uint16_t RATIO6_DEFAULT_Q10 = 2406;          // 2,35
static const uint16_t RATIO9_DEFAULT_Q10 = 2867;          // 2,80
static const uint32_t CFG_MAGIC = 0xD6E7FAu;
static const uint16_t CFG_VERSION = 1;

struct Chan {
  uint32_t inMask;
  uint32_t outMask;
  uint32_t lastEdge;
  uint32_t lastGood;
  uint32_t filt;
  uint32_t half;
  uint32_t nextToggle;
  uint8_t lastLevel;
  uint8_t haveEdge;
  uint8_t level;
  uint8_t running;
};

static Chan ch[2];

// Boîte aux lettres loop → NMI (un uint32 aligné est atomique sur l'ESP8266).
static volatile uint32_t g_ratio_q10[2] = {RATIO6_DEFAULT_Q10, RATIO9_DEFAULT_Q10};

// Boîte aux lettres NMI → loop, protégée par un compteur de séquence pair/impair.
static volatile uint32_t g_seq[2];
static volatile uint32_t g_period[2];
static volatile uint32_t g_half[2];
static volatile uint8_t g_run[2];

struct Config {
  uint32_t magic;
  uint16_t version;
  uint16_t ratio6_q10;
  uint16_t ratio9_q10;
  char sta_ssid[33];
  char sta_pass[65];
  char ap_pass[65];
  uint32_t crc;
};

static Config cfg;
static ESP8266WebServer server(80);
static char apSsid[20];

static inline uint32_t IRAM_ATTR cycles() {
  uint32_t c;
  __asm__ __volatile__("rsr %0,ccount" : "=a"(c));
  return c;
}

static inline void IRAM_ATTR pinHigh(uint32_t mask) { GPOS = mask; }
static inline void IRAM_ATTR pinLow(uint32_t mask) { GPOC = mask; }

static void IRAM_ATTR publish(uint8_t i) {
  g_seq[i]++;
  g_period[i] = ch[i].filt;
  g_half[i] = ch[i].half;
  g_run[i] = ch[i].running;
  g_seq[i]++;
}

static void IRAM_ATTR releaseOutput(uint8_t i) {
  ch[i].running = 0;
  ch[i].level = 0;
  ch[i].half = 0;
  ch[i].filt = 0;
  ch[i].haveEdge = 0;
  pinLow(ch[i].outMask);
}

static void IRAM_ATTR acceptPeriod(uint8_t i, uint32_t period, uint32_t now) {
  if (ch[i].filt == 0) {
    ch[i].filt = period;
  } else {
    int32_t err = (int32_t)period - (int32_t)ch[i].filt;
    ch[i].filt = (uint32_t)((int32_t)ch[i].filt + (err >> 3));
  }

  uint32_t ratio = g_ratio_q10[i];
  if (ratio < RATIO_MIN_Q10) {
    ratio = RATIO_MIN_Q10;
  } else if (ratio > RATIO_MAX_Q10) {
    ratio = RATIO_MAX_Q10;
  }

  // Division 32 bits seulement : la routine 64 bits de libgcc est en flash,
  // et une NMI n'a pas le droit d'y aller.
  // half = filt × 512 / ratio, sans déborder (filt ≤ 24e6, ratio ≥ 512).
  uint32_t q = ch[i].filt / ratio;
  uint32_t r = ch[i].filt % ratio;
  uint32_t half = q * 512u + (r * 512u) / ratio;
  if (half < MIN_HALF_CYC) {
    half = MIN_HALF_CYC;
  }
  ch[i].half = half;
  ch[i].lastGood = now;

  if (!ch[i].running) {
    ch[i].running = 1;
    ch[i].level = 0;
    pinLow(ch[i].outMask);
    ch[i].nextToggle = now + half;
  }
}

// Seule routine qui touche aux tach. Entièrement en IRAM : aucun accès flash.
static void IRAM_ATTR tachIsr() {
  uint32_t now = cycles();
  uint32_t pins = GPI;

  for (uint8_t i = 0; i < 2; i++) {
    if (!ch[i].running) {
      continue;
    }
    if ((int32_t)(now - ch[i].nextToggle) >= 0) {
      ch[i].level ^= 1;
      if (ch[i].level) {
        pinHigh(ch[i].outMask);
      } else {
        pinLow(ch[i].outMask);
      }
      ch[i].nextToggle += ch[i].half;
      // Un réveil en retard ne doit pas émettre une rafale de fronts :
      // l'onduleur compterait des tours qui n'existent pas.
      if ((int32_t)(now - ch[i].nextToggle) >= 0) {
        ch[i].nextToggle = now + ch[i].half;
      }
    }
  }

  for (uint8_t i = 0; i < 2; i++) {
    uint8_t level = (pins & ch[i].inMask) ? 1 : 0;
    // Le délai part du dernier *vrai* tour, pas d'un parasite : des pics
    // plus courts que 2 ms ne maintiennent pas la simulation en vie.
    if (ch[i].running && (int32_t)(now - ch[i].lastGood) > (int32_t)LOST_CYC) {
      releaseOutput(i);
    }

    if (level != ch[i].lastLevel) {
      ch[i].lastLevel = level;
      if (level == 0) {
        // Front descendant GPIO = front montant du tach Noctua (entrée inversée).
        // Un front par impulsion tach, donc par demi-tour.
        if (ch[i].haveEdge) {
          uint32_t dt = now - ch[i].lastEdge;
          if (dt >= MIN_IN_CYC && dt <= MAX_IN_CYC) {
            acceptPeriod(i, dt, now);
          }
        } else {
          ch[i].haveEdge = 1;
        }
        ch[i].lastEdge = now;
      }
    }
    publish(i);
  }

  uint32_t next = now + POLL_CYC;
  for (uint8_t i = 0; i < 2; i++) {
    if (!ch[i].running) {
      continue;
    }
    if ((int32_t)(ch[i].nextToggle - now) < (int32_t)(next - now)) {
      next = ch[i].nextToggle;
    }
  }

  int32_t dt = (int32_t)(next - cycles());
  if (dt < (int32_t)MIN_ARM_CYC) {
    dt = (int32_t)MIN_ARM_CYC;
  }
  timer1_write((uint32_t)dt);
}

static uint32_t crc32Bytes(const uint8_t *data, size_t len) {
  uint32_t c = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) {
    c ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      uint32_t mask = -(c & 1u);
      c = (c >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return ~c;
}

static uint16_t clampRatio(uint16_t q) {
  if (q < RATIO_MIN_Q10) {
    return RATIO_MIN_Q10;
  }
  if (q > RATIO_MAX_Q10) {
    return RATIO_MAX_Q10;
  }
  return q;
}

static void applyRatios() {
  g_ratio_q10[0] = cfg.ratio6_q10;
  g_ratio_q10[1] = cfg.ratio9_q10;
  __asm__ __volatile__("" ::: "memory");
}

static void setDefaults() {
  memset(&cfg, 0, sizeof(cfg));
  cfg.magic = CFG_MAGIC;
  cfg.version = CFG_VERSION;
  cfg.ratio6_q10 = RATIO6_DEFAULT_Q10;
  cfg.ratio9_q10 = RATIO9_DEFAULT_Q10;
  strncpy(cfg.ap_pass, "deyefan1", sizeof(cfg.ap_pass) - 1);
}

static void loadConfig() {
  EEPROM.begin(512);
  EEPROM.get(0, cfg);
  uint32_t expect = crc32Bytes(reinterpret_cast<uint8_t *>(&cfg), offsetof(Config, crc));
  if (cfg.magic != CFG_MAGIC || cfg.version != CFG_VERSION || cfg.crc != expect) {
    setDefaults();
  }
  cfg.ratio6_q10 = clampRatio(cfg.ratio6_q10);
  cfg.ratio9_q10 = clampRatio(cfg.ratio9_q10);
  cfg.sta_ssid[sizeof(cfg.sta_ssid) - 1] = 0;
  cfg.sta_pass[sizeof(cfg.sta_pass) - 1] = 0;
  cfg.ap_pass[sizeof(cfg.ap_pass) - 1] = 0;
  if (strlen(cfg.ap_pass) < 8) {
    strncpy(cfg.ap_pass, "deyefan1", sizeof(cfg.ap_pass) - 1);
  }
  applyRatios();
}

static void saveConfig() {
  cfg.magic = CFG_MAGIC;
  cfg.version = CFG_VERSION;
  cfg.crc = crc32Bytes(reinterpret_cast<uint8_t *>(&cfg), offsetof(Config, crc));
  EEPROM.put(0, cfg);
  EEPROM.commit();
}

struct Snap {
  uint32_t period;
  uint32_t half;
  uint8_t run;
};

static Snap readSnap(uint8_t i) {
  Snap s;
  uint32_t a;
  uint32_t b;
  do {
    a = g_seq[i];
    s.period = g_period[i];
    s.half = g_half[i];
    s.run = g_run[i];
    b = g_seq[i];
  } while (a != b || (a & 1u));
  return s;
}

static uint32_t rpmFromPeriod(uint32_t period) {
  if (period == 0) {
    return 0;
  }
  return 2400000000UL / period;
}

static uint32_t rpmFromHalf(uint32_t half) {
  if (half == 0) {
    return 0;
  }
  return 1200000000UL / half;
}

static void copyField(char *dst, size_t n, const String &src) {
  size_t i = 0;
  size_t m = src.length();
  if (m >= n) {
    m = n - 1;
  }
  for (; i < m; i++) {
    dst[i] = src[i];
  }
  dst[i] = 0;
}

static uint16_t parseRatio(const String &raw) {
  String s = raw;
  s.replace(',', '.');
  s.trim();
  float v = s.toFloat();
  if (v < 0.50f) {
    v = 0.50f;
  }
  if (v > 6.00f) {
    v = 6.00f;
  }
  return clampRatio((uint16_t)(v * 1024.0f + 0.5f));
}

static void appendJsonStr(char *dst, size_t n, const char *s) {
  size_t j = strlen(dst);
  for (; *s && j + 2 < n; s++) {
    if (*s == '"' || *s == '\\') {
      dst[j++] = '\\';
    }
    dst[j++] = *s;
  }
  dst[j] = 0;
}

static void updateLed() {
  uint8_t n = (g_run[0] ? 1 : 0) + (g_run[1] ? 1 : 0);
  if (n == 2) {
    digitalWrite(PIN_LED, LOW);
    return;
  }
  uint32_t t = millis();
  bool on;
  if (n == 1) {
    on = ((t / 150UL) % 2UL) == 0;
  } else {
    on = (t % 1000UL) < 150UL;
  }
  digitalWrite(PIN_LED, on ? LOW : HIGH);
}

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Deye tach</title>
<style>
body{margin:0;font:16px/1.4 sans-serif;background:#121418;color:#e8e6e3}
main{max-width:40rem;margin:0 auto;padding:1rem}
h1{font-size:1.25rem;margin:0 0 .25rem}
p{color:#b7b3ac}
.card{background:#1c2128;border-radius:10px;padding:1rem;margin:.75rem 0}
.rpm{font:600 1.8rem/1.1 ui-monospace,monospace}
label{display:block;margin:.6rem 0 .2rem}
input{width:100%;box-sizing:border-box;padding:.55rem;font-size:1rem;border-radius:8px;border:1px solid #3a414c;background:#0e1116;color:#fff}
button{margin-top:.8rem;padding:.7rem 1rem;font-size:1rem;border:0;border-radius:8px;background:#e7a23a;color:#1a1203}
.ok{color:#8fdf9a}.wait{color:#e0b15a}
#msg{min-height:1.2em}
</style>
</head>
<body>
<main>
<h1>Simulateur tach Deye</h1>
<p id="net">connexion…</p>
<section class="card" id="c0">
<h2>Canal 6 cm</h2>
<div class="rpm"><span id="in0">–</span> → <span id="out0">–</span> RPM</div>
<div id="st0" class="wait">attente</div>
</section>
<section class="card" id="c1">
<h2>Canal 9 cm</h2>
<div class="rpm"><span id="in1">–</span> → <span id="out1">–</span> RPM</div>
<div id="st1" class="wait">attente</div>
</section>
<form class="card" id="f">
<h2>Réglages</h2>
<label for="r6">Ratio 6 cm (0,50 – 6,00)</label>
<input id="r6" name="r6" inputmode="decimal" required>
<label for="r9">Ratio 9 cm (0,50 – 6,00)</label>
<input id="r9" name="r9" inputmode="decimal" required>
<label for="ssid">WiFi de la maison (laisser vide pour couper)</label>
<input id="ssid" name="ssid" maxlength="32" autocomplete="off">
<label for="pass">Mot de passe WiFi (vide = inchangé)</label>
<input id="pass" name="pass" type="password" maxlength="64" autocomplete="new-password">
<label for="appass">Mot de passe du point d'accès (8 caractères min., vide = inchangé)</label>
<input id="appass" name="appass" type="password" maxlength="64" autocomplete="new-password">
<button type="submit">Enregistrer</button>
<p id="msg"></p>
<p>Défauts : 2,35 sur le 6 cm (7000/3000) et 2,80 sur le 9 cm (4500/1600). La LED de la carte est fixe si les deux canaux simulent, clignote vite si un seul est actif, et fait un bref éclair si aucun tach n'est reçu.</p>
</form>
</main>
<script>
const fmt = q => (q / 1024).toFixed(2).replace('.', ',');
const $ = id => document.getElementById(id);
async function loadCfg() {
  const c = await (await fetch('/api/config')).json();
  $('r6').value = fmt(c.r6);
  $('r9').value = fmt(c.r9);
  $('ssid').value = c.ssid;
}
async function poll() {
  try {
    const s = await (await fetch('/api/status')).json();
    $('net').textContent = s.sta
      ? ('point d\'accès ' + s.ap + '  ·  maison ' + s.ip)
      : ('point d\'accès ' + s.ap + '  ·  maison non connectée');
    for (let i = 0; i < 2; i++) {
      const ch = s.c[i];
      document.getElementById('in' + i).textContent = ch.run ? ch.in : '–';
      document.getElementById('out' + i).textContent = ch.run ? ch.out : '–';
      const st = document.getElementById('st' + i);
      st.textContent = ch.run ? 'simulation active' : 'attente de signal';
      st.className = ch.run ? 'ok' : 'wait';
    }
  } catch (e) {
    $('net').textContent = 'page injoignable';
  }
}
$('f').addEventListener('submit', async (ev) => {
  ev.preventDefault();
  $('msg').textContent = 'enregistrement…';
  const body = new URLSearchParams(new FormData($('f')));
  const r = await fetch('/api/save', {
    method: 'POST',
    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
    body
  });
  $('msg').textContent = r.ok ? 'enregistré, ratios appliqués' : 'refus';
  $('pass').value = '';
  $('appass').value = '';
});
loadCfg().catch(() => { $('msg').textContent = 'réglages illisibles'; });
poll();
setInterval(poll, 500);
</script>
</body>
</html>
)HTML";

static void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

static void handleConfig() {
  char buf[192];
  snprintf(buf, sizeof(buf), "{\"r6\":%u,\"r9\":%u,\"ssid\":\"", cfg.ratio6_q10, cfg.ratio9_q10);
  appendJsonStr(buf, sizeof(buf), cfg.sta_ssid);
  strlcat(buf, "\"}", sizeof(buf));
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", buf);
}

static void handleStatus() {
  Snap a = readSnap(0);
  Snap b = readSnap(1);
  bool sta = WiFi.status() == WL_CONNECTED;
  char ip[20];
  if (sta) {
    strncpy(ip, WiFi.localIP().toString().c_str(), sizeof(ip) - 1);
    ip[sizeof(ip) - 1] = 0;
  } else {
    ip[0] = 0;
  }
  char buf[320];
  snprintf(buf, sizeof(buf),
           "{\"ap\":\"%s\",\"sta\":%d,\"ip\":\"%s\",\"c\":["
           "{\"run\":%u,\"in\":%lu,\"out\":%lu},"
           "{\"run\":%u,\"in\":%lu,\"out\":%lu}]}",
           WiFi.softAPIP().toString().c_str(), sta ? 1 : 0, ip, a.run,
           (unsigned long)rpmFromPeriod(a.run ? a.period : 0),
           (unsigned long)rpmFromHalf(a.run ? a.half : 0), b.run,
           (unsigned long)rpmFromPeriod(b.run ? b.period : 0),
           (unsigned long)rpmFromHalf(b.run ? b.half : 0));
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", buf);
}

static void handleSave() {
  if (!server.hasArg("r6") || !server.hasArg("r9")) {
    server.send(400, "text/plain", "ratio manquant");
    return;
  }
  if (server.hasArg("appass") && server.arg("appass").length() > 0 && server.arg("appass").length() < 8) {
    server.send(400, "text/plain", "mot de passe AP trop court");
    return;
  }

  cfg.ratio6_q10 = parseRatio(server.arg("r6"));
  cfg.ratio9_q10 = parseRatio(server.arg("r9"));
  applyRatios();

  if (server.hasArg("ssid")) {
    copyField(cfg.sta_ssid, sizeof(cfg.sta_ssid), server.arg("ssid"));
  }
  if (server.hasArg("pass") && server.arg("pass").length() > 0) {
    copyField(cfg.sta_pass, sizeof(cfg.sta_pass), server.arg("pass"));
  }
  if (server.hasArg("appass") && server.arg("appass").length() >= 8) {
    copyField(cfg.ap_pass, sizeof(cfg.ap_pass), server.arg("appass"));
  }

  saveConfig();

  WiFi.softAP(apSsid, cfg.ap_pass);
  if (cfg.sta_ssid[0]) {
    WiFi.begin(cfg.sta_ssid, cfg.sta_pass);
  } else {
    WiFi.disconnect();
  }
  server.send(200, "text/plain", "ok");
}

static void handleNotFound() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "redirect");
}

static void startRadio() {
  snprintf(apSsid, sizeof(apSsid), "DeyeFan-%04X", ESP.getChipId() & 0xFFFF);
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.hostname("deye-fan");
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(apSsid, cfg.ap_pass);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  if (cfg.sta_ssid[0]) {
    WiFi.begin(cfg.sta_ssid, cfg.sta_pass);
  }
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/config", HTTP_GET, handleConfig);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/save", HTTP_POST, handleSave);
  server.onNotFound(handleNotFound);
  server.begin();
}

static void startTach() {
  uint32_t pins = GPI;
  ch[0].inMask = 1UL << PIN_IN6;
  ch[0].outMask = 1UL << PIN_OUT6;
  ch[1].inMask = 1UL << PIN_IN9;
  ch[1].outMask = 1UL << PIN_OUT9;
  for (uint8_t i = 0; i < 2; i++) {
    ch[i].lastLevel = (pins & ch[i].inMask) ? 1 : 0;
    pinLow(ch[i].outMask);
    publish(i);
  }

  timer1_disable();
  ETS_FRC_TIMER1_INTR_ATTACH(NULL, NULL);
  ETS_FRC_TIMER1_NMI_INTR_ATTACH(tachIsr);
  timer1_enable(TIM_DIV1, TIM_EDGE, TIM_SINGLE);
  timer1_write(POLL_CYC);
}

void setup() {
  Serial.begin(115200);
  // Le compteur de cycles et le timer1 doivent rester au même rythme.
  // À 160 MHz le compteur va deux fois plus vite que le timer1 (80 MHz).
  system_update_cpu_freq(80);

  // LOW avant pinMode : le transistor de sortie reste bloqué au démarrage,
  // donc la ligne tach de l'onduleur n'est pas tirée par accident.
  digitalWrite(PIN_OUT6, LOW);
  digitalWrite(PIN_OUT9, LOW);
  digitalWrite(PIN_LED, HIGH);
  pinMode(PIN_IN6, INPUT_PULLUP);
  pinMode(PIN_IN9, INPUT_PULLUP);
  pinMode(PIN_OUT6, OUTPUT);
  pinMode(PIN_OUT9, OUTPUT);
  pinMode(PIN_LED, OUTPUT);

  loadConfig();
  // NMI lancée avant le WiFi : la synthèse existe déjà quand la radio démarre.
  startTach();
  startRadio();

  Serial.printf("AP %s  http://192.168.4.1  mdp %s\n", apSsid, cfg.ap_pass);
}

void loop() {
  server.handleClient();
  updateLed();

  static uint32_t lastLog;
  uint32_t nowMs = millis();
  if (nowMs - lastLog >= 2000) {
    lastLog = nowMs;
    Snap a = readSnap(0);
    Snap b = readSnap(1);
    Serial.printf("6cm %s %lu -> %lu   9cm %s %lu -> %lu\n",
                  a.run ? "actif" : "attente",
                  (unsigned long)rpmFromPeriod(a.run ? a.period : 0),
                  (unsigned long)rpmFromHalf(a.run ? a.half : 0),
                  b.run ? "actif" : "attente",
                  (unsigned long)rpmFromPeriod(b.run ? b.period : 0),
                  (unsigned long)rpmFromHalf(b.run ? b.half : 0));
  }
  yield();
}
