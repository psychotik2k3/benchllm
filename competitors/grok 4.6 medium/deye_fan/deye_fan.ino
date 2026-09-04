/*
 * deye_fan.ino
 * Simulateur de tachymètre pour onduleur Deye SUN-8K-SG05LP1-EU-AM2-P
 * Carte : LOLIN (WEMOS) D1 mini  (ESP-12S / ESP8266)
 *
 * Architecture de timing (isolation WiFi / HTTP) :
 *   - Lecture tach  : ISR GPIO IRAM, horodatage par CCOUNT (80 MHz).
 *   - Génération    : Timer1 en NMI (priorité > WiFi, > GPIO, > SDK).
 *     L'ISR NMI ne fait que décompter et basculer des GPIO via GPOS/GPOC.
 *     Aucun appel flash, aucun digitalWrite, aucun code réseau.
 *   - Le loop() et le serveur web ne font que publier une période déjà
 *     calculée (uint32 atomique). Ils ne produisent jamais le signal.
 *
 * Ne pas utiliser analogWrite() / tone() / Servo : ils monopolisent Timer1.
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <user_interface.h>

extern "C" {
#include "ets_sys.h"
}

/* -------------------------------------------------------------------------- */
/* Brochage Wemos D1 Mini                                                     */
/* -------------------------------------------------------------------------- */
/* D5 / GPIO14 : tach IN  9 cm (Noctua, collecteur ouvert)                    */
/* D6 / GPIO12 : tach IN  6 cm                                                */
/* D1 / GPIO5  : base NPN  → tach OUT onduleur 9 cm                           */
/* D2 / GPIO4  : base NPN  → tach OUT onduleur 6 cm                           */
/* D4 / GPIO2  : LED onboard (active LOW)                                     */
/* GPIO0 / GPIO15 / GPIO16 évités (boot / pas d'IRQ)                          */

static const uint8_t PIN_TACH_IN_9  = 14;   /* D5 */
static const uint8_t PIN_TACH_IN_6  = 12;   /* D6 */
static const uint8_t PIN_TACH_OUT_9 = 5;    /* D1 */
static const uint8_t PIN_TACH_OUT_6 = 4;    /* D2 */
static const uint8_t PIN_LED        = 2;    /* D4, active LOW */

static const uint32_t MASK_OUT_9 = (1u << PIN_TACH_OUT_9);
static const uint32_t MASK_OUT_6 = (1u << PIN_TACH_OUT_6);

/* Timer1 NMI : DIV16 → 5 MHz. Espressif impose ≥ 100 ticks en NMI autoload. */
static const uint32_t TMR_DIV16_HZ     = 5000000UL;
static const uint32_t NMI_PERIOD_TICKS = 100UL;                 /* 20 µs */
static const uint32_t NMI_HZ           = TMR_DIV16_HZ / NMI_PERIOD_TICKS;

static const uint32_t CPU_HZ           = 80000000UL;
static const uint32_t MIN_EDGE_CYCLES  = CPU_HZ / 5000UL;       /* 200 µs anti-rebond */
static const uint32_t STALE_US         = 400000UL;              /* 400 ms sans pulse → idle */
static const uint32_t PULSES_PER_REV   = 2UL;                   /* spec Noctua / Intel */

static const float    RATIO_MIN        = 0.50f;
static const float    RATIO_MAX        = 8.00f;
static const uint16_t RATIO_DEF_X100   = 200;                   /* 2.00 */

static const uint32_t EEPROM_MAGIC     = 0xDE4EFA01UL;
static const int      EEPROM_SIZE      = 512;

enum { CH9 = 0, CH6 = 1, NCH = 2 };

/* -------------------------------------------------------------------------- */
/* État partagé ISR NMI / ISR GPIO / loop  (accès 32-bit aligné = atomique)   */
/* -------------------------------------------------------------------------- */

struct TachChannel {
  volatile uint32_t nmiReload;     /* demi-période en ticks NMI (20 µs) */
  volatile uint32_t nmiCount;
  volatile uint8_t  nmiEnabled;    /* 1 = générer, 0 = transistor OFF (ligne relâchée) */
  volatile uint8_t  nmiLevel;      /* 0/1 niveau GPIO actuel */
  volatile uint32_t lastCycle;     /* CCOUNT du dernier front */
  volatile uint32_t periodCycles;  /* cycles CPU entre deux fronts */
  volatile uint32_t lastEdgeUs;
  volatile uint32_t pulseCount;
  volatile uint8_t  havePeriod;
  uint16_t          ratioX100;     /* 200 = 2.00  (écrit depuis loop uniquement) */
};

static TachChannel g_ch[NCH];

/* -------------------------------------------------------------------------- */
/* NMI : génération tach — zéro flash, zéro libc, registres GPIO uniquement   */
/* -------------------------------------------------------------------------- */

static void IRAM_ATTR nmiTachIsr(void) {
  /* Canal 9 cm */
  if (g_ch[CH9].nmiEnabled) {
    uint32_t c = g_ch[CH9].nmiCount;
    if (c <= 1) {
      g_ch[CH9].nmiCount = g_ch[CH9].nmiReload;
      uint8_t lvl = g_ch[CH9].nmiLevel ^ 1u;
      g_ch[CH9].nmiLevel = lvl;
      if (lvl) {
        GPOS = MASK_OUT_9;          /* base NPN HIGH → collecteur tire la tach à GND */
      } else {
        GPOC = MASK_OUT_9;          /* base LOW → collecteur ouvert (pull-up onduleur) */
      }
    } else {
      g_ch[CH9].nmiCount = c - 1;
    }
  } else {
    GPOC = MASK_OUT_9;
    g_ch[CH9].nmiLevel = 0;
  }

  /* Canal 6 cm */
  if (g_ch[CH6].nmiEnabled) {
    uint32_t c = g_ch[CH6].nmiCount;
    if (c <= 1) {
      g_ch[CH6].nmiCount = g_ch[CH6].nmiReload;
      uint8_t lvl = g_ch[CH6].nmiLevel ^ 1u;
      g_ch[CH6].nmiLevel = lvl;
      if (lvl) {
        GPOS = MASK_OUT_6;
      } else {
        GPOC = MASK_OUT_6;
      }
    } else {
      g_ch[CH6].nmiCount = c - 1;
    }
  } else {
    GPOC = MASK_OUT_6;
    g_ch[CH6].nmiLevel = 0;
  }
}

/* -------------------------------------------------------------------------- */
/* ISR GPIO : capture de période (IRAM). Uniquement CCOUNT / micros.          */
/* -------------------------------------------------------------------------- */

static void IRAM_ATTR tachCapture(TachChannel *ch) {
  const uint32_t now = ESP.getCycleCount();
  const uint32_t prev = ch->lastCycle;
  ch->lastCycle = now;
  ch->lastEdgeUs = micros();
  ch->pulseCount = ch->pulseCount + 1;

  if (prev == 0) {
    return;
  }
  const uint32_t dt = now - prev;   /* wrap 32-bit OK */
  if (dt < MIN_EDGE_CYCLES) {
    return;
  }
  ch->periodCycles = dt;
  ch->havePeriod = 1;
}

static void IRAM_ATTR isrTach9(void) { tachCapture(&g_ch[CH9]); }
static void IRAM_ATTR isrTach6(void) { tachCapture(&g_ch[CH6]); }

/* -------------------------------------------------------------------------- */
/* EEPROM / WiFi persistants                                                  */
/* -------------------------------------------------------------------------- */

struct PersistCfg {
  uint32_t magic;
  uint16_t ratio9X100;
  uint16_t ratio6X100;
  char     staSsid[33];
  char     staPass[65];
  char     apSsid[33];
  char     apPass[65];
  uint32_t crc;
};

static PersistCfg g_cfg;
static ESP8266WebServer g_http(80);

static uint32_t crc32(const uint8_t *data, size_t n) {
  uint32_t c = 0xFFFFFFFFUL;
  for (size_t i = 0; i < n; i++) {
    c ^= data[i];
    for (int b = 0; b < 8; b++) {
      c = (c & 1u) ? (c >> 1) ^ 0xEDB88320UL : (c >> 1);
    }
  }
  return ~c;
}

static uint32_t cfgCrc(const PersistCfg *c) {
  return crc32(reinterpret_cast<const uint8_t *>(c), offsetof(PersistCfg, crc));
}

static void defaultCfg(PersistCfg *c) {
  memset(c, 0, sizeof(*c));
  c->magic = EEPROM_MAGIC;
  c->ratio9X100 = RATIO_DEF_X100;
  c->ratio6X100 = RATIO_DEF_X100;
  snprintf(c->apSsid, sizeof(c->apSsid), "DeyeFan-%04X", (uint16_t)(ESP.getChipId() & 0xFFFF));
  strncpy(c->apPass, "deyefan12", sizeof(c->apPass) - 1);
  c->crc = cfgCrc(c);
}

static void loadCfg() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, g_cfg);
  if (g_cfg.magic != EEPROM_MAGIC || g_cfg.crc != cfgCrc(&g_cfg)) {
    defaultCfg(&g_cfg);
    EEPROM.put(0, g_cfg);
    EEPROM.commit();
  }
  g_ch[CH9].ratioX100 = constrain(g_cfg.ratio9X100, (uint16_t)(RATIO_MIN * 100), (uint16_t)(RATIO_MAX * 100));
  g_ch[CH6].ratioX100 = constrain(g_cfg.ratio6X100, (uint16_t)(RATIO_MIN * 100), (uint16_t)(RATIO_MAX * 100));
}

static void saveCfg() {
  g_cfg.ratio9X100 = g_ch[CH9].ratioX100;
  g_cfg.ratio6X100 = g_ch[CH6].ratioX100;
  g_cfg.magic = EEPROM_MAGIC;
  g_cfg.crc = cfgCrc(&g_cfg);
  EEPROM.put(0, g_cfg);
  EEPROM.commit();
}

/* -------------------------------------------------------------------------- */
/* Conversion période d'entrée → reload NMI (faite dans loop, pas dans l'ISR) */
/* -------------------------------------------------------------------------- */
/*
 * 2 impulsions / tour : RPM = f_Hz * 30.
 * f_out = f_in * ratio  ⇒  T_out = T_in / ratio
 * Demi-période (carré 50 %) en ticks NMI de 20 µs :
 *   half_nmi = (periodCycles / CPU_HZ) / ratio / 2 * NMI_HZ
 *            = periodCycles * NMI_HZ / (CPU_HZ * ratio * 2)
 * ratio = ratioX100 / 100
 */

static uint32_t periodToNmiHalf(uint32_t periodCycles, uint16_t ratioX100) {
  if (periodCycles == 0 || ratioX100 == 0) {
    return 0;
  }
  /* half_nmi = periodCycles * NMI_HZ * 50 / (CPU_HZ * ratioX100)
   * NMI_HZ = 50000, CPU = 80e6  →  50000*50 / 80e6 = 1/32
   * half_nmi = periodCycles / (32 * ratioX100 / 100) wait:
   * periodCycles * 50000 * 50 / (80e6 * ratioX100)
   * = periodCycles * 2_500_000 / (80_000_000 * ratioX100)
   * = periodCycles * 25 / (800 * ratioX100)
   * = periodCycles * 5 / (160 * ratioX100)
   */
  uint64_t num = (uint64_t)periodCycles * 5ULL;
  uint64_t den = 160ULL * (uint64_t)ratioX100;
  uint32_t half = (uint32_t)(num / den);
  if (half < 2) {
    half = 2;                       /* plancher : 40 µs demi-période */
  }
  if (half > (NMI_HZ / 2)) {        /* plafond ~ 1 Hz de carré */
    half = NMI_HZ / 2;
  }
  return half;
}

static uint32_t rpmFromPeriod(uint32_t periodCycles) {
  if (periodCycles == 0) {
    return 0;
  }
  /* f_Hz = CPU_HZ / periodCycles ; RPM = f * 60 / PULSES_PER_REV */
  return (uint32_t)((uint64_t)CPU_HZ * 60ULL / ((uint64_t)periodCycles * PULSES_PER_REV));
}

static void updateChannelFromInput(uint8_t id) {
  TachChannel *ch = &g_ch[id];
  const uint32_t now = micros();
  const uint32_t edge = ch->lastEdgeUs;
  const bool stale = (edge == 0) || ((uint32_t)(now - edge) > STALE_US);

  if (stale || !ch->havePeriod) {
    ch->nmiEnabled = 0;
    return;
  }

  const uint32_t half = periodToNmiHalf(ch->periodCycles, ch->ratioX100);
  if (half < 2) {
    ch->nmiEnabled = 0;
    return;
  }
  ch->nmiReload = half;
  if (!ch->nmiEnabled) {
    ch->nmiCount = half;
    ch->nmiEnabled = 1;
  }
}

/* -------------------------------------------------------------------------- */
/* LED d'état : fixe = simulation active, clignote = attente de signal        */
/* -------------------------------------------------------------------------- */

static void updateLed(bool simulating) {
  if (simulating) {
    digitalWrite(PIN_LED, LOW);     /* ON */
  } else {
    digitalWrite(PIN_LED, ((millis() / 250) & 1) ? LOW : HIGH);
  }
}

/* -------------------------------------------------------------------------- */
/* Wi-Fi AP + STA                                                             */
/* -------------------------------------------------------------------------- */

static void startWifi() {
  WiFi.persistent(false);           /* on gère la NVRAM nous-mêmes */
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(g_cfg.apSsid, g_cfg.apPass, 6, 0, 4);

  if (g_cfg.staSsid[0] != '\0') {
    WiFi.begin(g_cfg.staSsid, g_cfg.staPass);
  }
}

static uint32_t g_lastStaAttempt = 0;

static void wifiMaintain() {
  if (g_cfg.staSsid[0] == '\0') {
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  if (millis() - g_lastStaAttempt < 15000UL) {
    return;
  }
  g_lastStaAttempt = millis();
  WiFi.begin(g_cfg.staSsid, g_cfg.staPass);
}

/* -------------------------------------------------------------------------- */
/* Interface web                                                              */
/* -------------------------------------------------------------------------- */

static const char PAGE_INDEX[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Deye Fan Tach</title>
<style>
  :root { font-family: system-ui, sans-serif; background:#0f1419; color:#e7ecf3; }
  body { max-width: 720px; margin: 1.2rem auto; padding: 0 1rem; }
  h1 { font-size: 1.25rem; font-weight: 650; }
  .grid { display:grid; grid-template-columns:1fr 1fr; gap:.8rem; }
  .card { background:#1a2330; border-radius:12px; padding:1rem; }
  .rpm { font-size:1.6rem; font-variant-numeric: tabular-nums; }
  .muted { color:#8b9bb4; font-size:.85rem; }
  label { display:block; margin:.55rem 0 .2rem; font-size:.85rem; }
  input { width:100%; box-sizing:border-box; padding:.45rem .55rem; border-radius:8px;
          border:1px solid #334155; background:#0f1419; color:#e7ecf3; }
  button { margin-top:.8rem; padding:.55rem 1rem; border:0; border-radius:8px;
           background:#3b82f6; color:#fff; font-weight:600; cursor:pointer; }
  .ok { color:#34d399; } .wait { color:#fbbf24; }
  @media (max-width:600px){ .grid { grid-template-columns:1fr; } }
</style>
</head>
<body>
<h1>Simulateur tach Deye</h1>
<p class="muted" id="net">Wi-Fi…</p>
<div class="grid">
  <div class="card">
    <div class="muted">Canal 9 cm</div>
    <div>Lu <span class="rpm" id="r9">—</span> <span class="muted">tr/min</span></div>
    <div>Simulé <span class="rpm" id="s9">—</span> <span class="muted">tr/min</span></div>
    <div id="a9" class="muted">—</div>
  </div>
  <div class="card">
    <div class="muted">Canal 6 cm</div>
    <div>Lu <span class="rpm" id="r6">—</span> <span class="muted">tr/min</span></div>
    <div>Simulé <span class="rpm" id="s6">—</span> <span class="muted">tr/min</span></div>
    <div id="a6" class="muted">—</div>
  </div>
</div>
<div class="card" style="margin-top:.8rem">
  <form id="f">
    <label>Ratio 9 cm (0,50 – 8,00)</label>
    <input name="ratio9" id="ratio9" type="number" step="0.01" min="0.5" max="8">
    <label>Ratio 6 cm (0,50 – 8,00)</label>
    <input name="ratio6" id="ratio6" type="number" step="0.01" min="0.5" max="8">
    <label>Wi-Fi STA — SSID</label>
    <input name="staSsid" id="staSsid" maxlength="32" autocomplete="off">
    <label>Wi-Fi STA — mot de passe</label>
    <input name="staPass" id="staPass" type="password" maxlength="64">
    <label>AP — SSID</label>
    <input name="apSsid" id="apSsid" maxlength="32">
    <label>AP — mot de passe (≥ 8 car.)</label>
    <input name="apPass" id="apPass" type="password" maxlength="64">
    <button type="submit">Enregistrer</button>
    <span class="muted" id="msg"></span>
  </form>
</div>
<script>
function q(id){return document.getElementById(id)}
function tick(){
  fetch('/api/status').then(r=>r.json()).then(d=>{
    q('r9').textContent=d.ch9.rpm; q('s9').textContent=d.ch9.sim;
    q('r6').textContent=d.ch6.rpm; q('s6').textContent=d.ch6.sim;
    q('a9').innerHTML=d.ch9.alive?'<span class="ok">simulation active</span>':'<span class="wait">attente signal</span>';
    q('a6').innerHTML=d.ch6.alive?'<span class="ok">simulation active</span>':'<span class="wait">attente signal</span>';
    q('net').textContent='AP '+d.wifi.apIp+'  ·  STA '+(d.wifi.sta?'connecté '+d.wifi.staIp:'hors ligne');
    if(!q('ratio9').dataset.init){
      q('ratio9').value=d.ch9.ratio; q('ratio6').value=d.ch6.ratio;
      q('staSsid').value=d.wifi.staSsid; q('apSsid').value=d.wifi.apSsid;
      q('ratio9').dataset.init='1';
    }
  }).catch(()=>{});
}
setInterval(tick, 500); tick();
q('f').onsubmit=function(e){
  e.preventDefault();
  const b=new URLSearchParams(new FormData(q('f')));
  fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b})
    .then(r=>r.text()).then(t=>{q('msg').textContent=t; setTimeout(()=>location.reload(),800);});
};
</script>
</body>
</html>
)HTML";

static uint16_t parseRatioX100(const String &s, uint16_t fallback) {
  float v = s.toFloat();
  if (v < RATIO_MIN || v > RATIO_MAX) {
    return fallback;
  }
  return (uint16_t)(v * 100.0f + 0.5f);
}

static void handleRoot() {
  g_http.send_P(200, "text/html", PAGE_INDEX);
}

static void handleStatus() {
  const bool a9 = g_ch[CH9].nmiEnabled;
  const bool a6 = g_ch[CH6].nmiEnabled;
  const uint32_t rpm9 = a9 ? rpmFromPeriod(g_ch[CH9].periodCycles) : 0;
  const uint32_t rpm6 = a6 ? rpmFromPeriod(g_ch[CH6].periodCycles) : 0;
  const uint32_t sim9 = (rpm9 * g_ch[CH9].ratioX100 + 50) / 100;
  const uint32_t sim6 = (rpm6 * g_ch[CH6].ratioX100 + 50) / 100;

  String json;
  json.reserve(420);
  json += F("{\"ch9\":{");
  json += F("\"rpm\":"); json += rpm9;
  json += F(",\"sim\":"); json += sim9;
  json += F(",\"ratio\":"); json += String(g_ch[CH9].ratioX100 / 100.0f, 2);
  json += F(",\"alive\":"); json += a9 ? F("true") : F("false");
  json += F("},\"ch6\":{");
  json += F("\"rpm\":"); json += rpm6;
  json += F(",\"sim\":"); json += sim6;
  json += F(",\"ratio\":"); json += String(g_ch[CH6].ratioX100 / 100.0f, 2);
  json += F(",\"alive\":"); json += a6 ? F("true") : F("false");
  json += F("},\"wifi\":{");
  json += F("\"sta\":"); json += (WiFi.status() == WL_CONNECTED) ? F("true") : F("false");
  json += F(",\"staIp\":\""); json += WiFi.localIP().toString();
  json += F("\",\"apIp\":\""); json += WiFi.softAPIP().toString();
  json += F("\",\"staSsid\":\""); json += g_cfg.staSsid;
  json += F("\",\"apSsid\":\""); json += g_cfg.apSsid;
  json += F("\"}}");
  g_http.send(200, "application/json", json);
}

static void handleConfig() {
  if (g_http.hasArg("ratio9")) {
    g_ch[CH9].ratioX100 = parseRatioX100(g_http.arg("ratio9"), g_ch[CH9].ratioX100);
  }
  if (g_http.hasArg("ratio6")) {
    g_ch[CH6].ratioX100 = parseRatioX100(g_http.arg("ratio6"), g_ch[CH6].ratioX100);
  }
  if (g_http.hasArg("staSsid")) {
    strncpy(g_cfg.staSsid, g_http.arg("staSsid").c_str(), sizeof(g_cfg.staSsid) - 1);
  }
  if (g_http.hasArg("staPass") && g_http.arg("staPass").length() > 0) {
    strncpy(g_cfg.staPass, g_http.arg("staPass").c_str(), sizeof(g_cfg.staPass) - 1);
  }
  if (g_http.hasArg("apSsid") && g_http.arg("apSsid").length() >= 1) {
    strncpy(g_cfg.apSsid, g_http.arg("apSsid").c_str(), sizeof(g_cfg.apSsid) - 1);
  }
  if (g_http.hasArg("apPass") && g_http.arg("apPass").length() >= 8) {
    strncpy(g_cfg.apPass, g_http.arg("apPass").c_str(), sizeof(g_cfg.apPass) - 1);
  }
  saveCfg();
  startWifi();
  g_http.send(200, "text/plain", "OK — paramètres enregistrés");
}

/* -------------------------------------------------------------------------- */

void setup() {
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  pinMode(PIN_TACH_OUT_9, OUTPUT);
  pinMode(PIN_TACH_OUT_6, OUTPUT);
  GPOC = MASK_OUT_9 | MASK_OUT_6;

  /* Pull-up 3,3 V : le tach Noctua est collecteur ouvert (ne sort jamais 12 V). */
  pinMode(PIN_TACH_IN_9, INPUT_PULLUP);
  pinMode(PIN_TACH_IN_6, INPUT_PULLUP);

  memset(g_ch, 0, sizeof(g_ch));
  g_ch[CH9].nmiReload = 2500;
  g_ch[CH6].nmiReload = 2500;
  g_ch[CH9].nmiCount = 2500;
  g_ch[CH6].nmiCount = 2500;

  loadCfg();

  /* Timer1 NMI AVANT le Wi-Fi : le signal existe même pendant l'association. */
  timer1_isr_init();
  timer1_disable();
  ETS_FRC_TIMER1_NMI_INTR_ATTACH(nmiTachIsr);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
  timer1_write(NMI_PERIOD_TICKS);

  attachInterrupt(digitalPinToInterrupt(PIN_TACH_IN_9), isrTach9, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_TACH_IN_6), isrTach6, FALLING);

  startWifi();

  g_http.on("/", HTTP_GET, handleRoot);
  g_http.on("/api/status", HTTP_GET, handleStatus);
  g_http.on("/api/config", HTTP_POST, handleConfig);
  g_http.begin();
}

void loop() {
  updateChannelFromInput(CH9);
  updateChannelFromInput(CH6);
  updateLed(g_ch[CH9].nmiEnabled || g_ch[CH6].nmiEnabled);

  g_http.handleClient();
  wifiMaintain();
}
