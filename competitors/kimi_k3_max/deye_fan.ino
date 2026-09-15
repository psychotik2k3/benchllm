/*
 ============================================================================
  deye_fan.ino
  ----------------------------------------------------------------------------
  Simulateur de signaux tachymetriques (RPM) pour onduleur Deye
  SUN-8K-SG05LP1-EU-AM2-P, apres remplacement des ventilateurs d'origine
  (NMB 09225VE-12N-CU 9 cm + NMB 06025VE-12N-CL 6 cm) par des Noctua
  silencieux (2x NF-A9 FLX @ 1600 tr/min + 2x NF-A6x25 FLX @ 3000 tr/min).

  Le Deye surveille le signal tach de ses ventilateurs et leve une alarme
  (ou modifie son comportement) si le regime lu est trop bas. Ce montage :
    - lit le tach reel d'un Noctua 9 cm (voie 1) et d'un Noctua 6 cm (voie 2),
    - multiplie chaque frequence par un ratio configurable par voie,
    - regenere vers l'onduleur un signal tach "collecteur ouvert" propre,
    - expose une interface web (AP + STA simultanes) pour regler les ratios,
      configurer le WiFi et visualiser les RPM lus / simules en temps reel,
    - persiste ratios + WiFi en EEPROM (aucune recompilation necessaire),
    - signale par LED : fixe = simulation active, clignotant = attente signal.

  Carte cible : LOLIN(WEMOS) D1 mini (ESP8266 / ESP-12S) @ 80 MHz
  Core Arduino ESP8266 : 2.7+ / 3.x

  ----------------------------------------------------------------------------
  ARCHITECTURE TEMPS REEL — ISOLATION WiFi / SERVEUR WEB
  ----------------------------------------------------------------------------
  Contrainte : une requete HTTP ou une rafale WiFi ne doit avoir AUCUN impact
  mesurable sur le signal envoye a l'onduleur. Choix d'architecture :

  1. Toute la chaine tach (lecture + generation) tourne dans des ISR
     prioritaires, jamais dans loop(). Le serveur web, le WiFi, le JSON et
     l'EEPROM sont cantonnes dans loop(), en contexte cooperatif.
  2. Capture d'entree : interruptions GPIO sur front descendant, horodatees
     par le compteur de cycles CPU (ESP.getCycleCount(), 12,5 ns @ 80 MHz).
     L'ISR ne fait que horodater + stocker : quelques dizaines de cycles.
  3. Generation de sortie : timer MATERIEL timer1 (FRC1), independant du CPU.
     L'ISR timer positionne chaque front sur une ECHEANCE ABSOLUE en cycles
     CPU -> pas de derive cumulative, meme si une interruption est retardee.
  4. Toutes les ISR sont en IRAM (IRAM_ATTR) et n'utilisent que des acces
     registres directs (GPOS/GPOC) : aucun acces flash, heap ou objet Arduino
     -> elles ne peuvent pas etre bloquees par le cache ni par l'allocation.
  5. Les donnees partagees ISR <-> loop sont quelques mots de 32 bits
     volatils (atomiques sur Xtensa) ; les rares mises a jour composees sont
     faites sous noInterrupts() pendant moins d'une microseconde.
  6. WiFi.setSleepMode(WIFI_NONE_SLEEP) et WiFi.persistent(false) : pas de
     latence de reveil modem ni d'ecriture flash imprevue du SDK.

  Budget de gigue (pire cas) :
    - demi-periode de sortie la plus courte utile : 625 us
      (entree 100 Hz = 3000 tr/min x ratio 8 -> 800 Hz) ;
    - latence d'interruption : ~1-3 us typique, < 10 us en rafale WiFi (NMI) ;
    - gigue relative < 1,6 % pire cas, ~0,3 % typique, SANS derive ;
    - le comptage RPM de l'onduleur integre sur ~1 s : l'erreur residuelle
      est tres en dessous de la resolution de sa mesure.
  (Une isolation strictement nulle exigerait un second MCU dedie ; sur
   ESP8266 mono-coeur, cette architecture est le meilleur compromis et son
   impact est non mesurable par l'onduleur.)

  ----------------------------------------------------------------------------
  CABLAGE (detail complet : schema_electronique.md)
  ----------------------------------------------------------------------------
    D5 (GPIO14) <— tach Noctua 9 cm   (pull-up 10k -> 3,3 V, serie 1k)
    D6 (GPIO12) <— tach Noctua 6 cm   (pull-up 10k -> 3,3 V, serie 1k)
    D1 (GPIO5)  —> base Q1 via 2,2k   (collecteur -> tach connecteur Deye 9 cm)
    D2 (GPIO4)  —> base Q2 via 2,2k   (collecteur -> tach connecteur Deye 6 cm)
    D4 (GPIO2)  —> LED integree (active a l'etat bas)
    5V / GND   <— module buck 12 V -> 5 V alimente via diodes OR-ing 1N5819
                  depuis les +12 V des DEUX connecteurs ventilateur.
 ============================================================================
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

#ifndef IRAM_ATTR
#define IRAM_ATTR ICACHE_RAM_ATTR   // compatibilite anciens cores ESP8266
#endif

// ============================ Broches ======================================
static const uint8_t PIN_IN1  = 14;  // D5 : tach Noctua 9 cm (entree)
static const uint8_t PIN_IN2  = 12;  // D6 : tach Noctua 6 cm (entree)
static const uint8_t PIN_OUT1 = 5;   // D1 : tach simule -> Deye, voie 9 cm
static const uint8_t PIN_OUT2 = 4;   // D2 : tach simule -> Deye, voie 6 cm
static const uint8_t PIN_LED  = 2;   // D4 : LED integree (active a l'etat bas)

// ===================== Constantes temporelles ==============================
// L'unite de temps de reference est le CYCLE CPU (compteur materiel libre a
// F_CPU Hz, non affecte par le WiFi ni par les interruptions).
#define CYCLES_PER_SEC     ((uint32_t)F_CPU)
#define US_TO_CYCLES(us)   ((uint32_t)(us) * (F_CPU / 1000000UL))
#define TIMER_TICK_CYCLES  16UL                 // timer1 @ F_CPU/16 (TIM_DIV16)

#define MIN_PULSE_CYCLES   US_TO_CYCLES(400)    // anti-parasite entree : 400 us
#define STALL_CYCLES       (CYCLES_PER_SEC * 3 / 2)  // 1,5 s sans impulsion = arret
#define MIN_HALF_CYCLES    US_TO_CYCLES(250)    // frequence de sortie max : 2 kHz
#define MAX_HALF_CYCLES    (CYCLES_PER_SEC / 2) // frequence de sortie min : 1 Hz
#define MIN_TIMER_TICKS    100                  // 20 us : garde-fou rearmement
#define MAX_TIMER_TICKS    8388607UL            // timer1 = compteur 23 bits

#define RATIO_MIN          0.5f
#define RATIO_MAX          8.0f
#define SERIAL_DEBUG       1                    // 0 pour desactiver les traces

// ============================ Etat des voies ===============================
struct FanChannel {
  uint8_t  pinIn;                 // broche d'entree (tach Noctua)
  uint8_t  pinOut;                // broche de sortie (base NPN)
  uint32_t outMask;               // masque GPIO pour GPOS/GPOC
  // -- partage loop() -> ISR timer (volatile, 32 bits = atomique) --
  volatile uint32_t outHalfCycles;// demi-periode de sortie en cycles ; 0 = OFF
  // -- etat du sequenceur de sortie (ISR timer + sections critiques loop) --
  uint32_t nextEdgeCycle;         // echeance absolue du prochain front
  uint8_t  outLevel;              // niveau logique courant de la broche
  // -- partage ISR entree -> loop() --
  volatile uint32_t lastEdgeCycle;// horodatage de la derniere impulsion
  volatile uint32_t periodCycles; // periode brute entre 2 impulsions
  volatile uint32_t pulseCount;   // compteur total (diagnostic)
};
static FanChannel ch[2];

// Etat "metier" calcule dans loop() (affichage web + LED)
struct ChannelState {
  float smoothPeriod;             // periode lissee (cycles)
  float rpmIn;                    // RPM reel mesure sur le Noctua
  float rpmOut;                   // RPM reellement emis vers le Deye
  bool  active;                   // true = signal d'entree present
};
static ChannelState st[2];

// ============================ Configuration ================================
#define CFG_MAGIC 0x44455945UL    // "DEYE"
struct Config {
  uint32_t magic;
  float    ratio[2];              // multiplicateur par voie (0,5 .. 8,0)
  char     ssid[33];
  char     pass[65];
  uint8_t  staEnable;             // 1 = tenter la connexion STA
  uint16_t crc;                   // CRC16 sur tout le reste de la structure
};
static Config cfg;

static uint16_t crc16(const uint8_t* p, size_t n) {
  uint16_t crc = 0xFFFF;
  while (n--) {
    crc ^= (uint16_t)(*p++) << 8;
    for (uint8_t i = 0; i < 8; i++)
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

static void configDefaults() {
  memset(&cfg, 0, sizeof(cfg));
  cfg.magic = CFG_MAGIC;
  // Ratios de depart (a ajuster via l'interface web) :
  //   voie 1 : NMB 9 cm ~4000 tr/min / Noctua NF-A9 FLX 1600 tr/min  -> ~2,5
  //   voie 2 : NMB 6 cm ~5500 tr/min / Noctua NF-A6x25 FLX 3000 tr/min -> ~2,0
  cfg.ratio[0] = 2.5f;
  cfg.ratio[1] = 2.0f;
  cfg.staEnable = 0;
}

static void saveConfig() {
  cfg.magic = CFG_MAGIC;
  cfg.crc = crc16((const uint8_t*)&cfg, offsetof(Config, crc));
  EEPROM.put(0, cfg);
  EEPROM.commit();
}

static void loadConfig() {
  EEPROM.begin(512);
  EEPROM.get(0, cfg);
  if (cfg.magic != CFG_MAGIC ||
      cfg.crc != crc16((const uint8_t*)&cfg, offsetof(Config, crc))) {
    configDefaults();             // premier boot ou EEPROM corrompue
    saveConfig();
  }
  cfg.ssid[32] = 0;               // securite : chaines forcement terminees
  cfg.pass[64] = 0;
  cfg.ratio[0] = clampRatio(cfg.ratio[0]);
  cfg.ratio[1] = clampRatio(cfg.ratio[1]);
}

static float clampRatio(float r) {
  if (!(r >= RATIO_MIN)) r = RATIO_MIN;   // piege aussi les NaN
  if (r > RATIO_MAX)     r = RATIO_MAX;
  return r;
}

// ============================================================================
//  CAPTURE D'ENTREE — ISR GPIO (IRAM, quelques dizaines de cycles)
//  Front descendant du tach (collecteur ouvert + pull-up 3,3 V).
//  Aucun calcul flottant, aucun acces flash/heap : seulement horodatage
//  materiel + stockage. Le WiFi ne peut pas la bloquer durablement.
// ============================================================================
static inline void IRAM_ATTR captureEdge(FanChannel* c) {
  uint32_t now = ESP.getCycleCount();
  uint32_t dt  = now - c->lastEdgeCycle;      // soustraction 32 bits : OK au debordement
  if (dt < MIN_PULSE_CYCLES) return;          // rejette parasites / double declenchement
  c->periodCycles  = dt;
  c->lastEdgeCycle = now;
  c->pulseCount++;
}
static void IRAM_ATTR isrIn1() { captureEdge(&ch[0]); }
static void IRAM_ATTR isrIn2() { captureEdge(&ch[1]); }

// ============================================================================
//  GENERATION DE SORTIE — ISR timer1 materiel (FRC1)
//  Un seul timer pour les deux voies : on l'arme toujours sur l'echeance la
//  plus proche. Les echeances sont ABSOLUES (compteur de cycles) -> aucune
//  derive cumulative ; la seule erreur possible est la latence d'interruption
//  (~quelques us), sans effet sur la frequence moyenne.
// ============================================================================
static void IRAM_ATTR scheduleNext(uint32_t now) {
  uint32_t nearest = 0xFFFFFFFFUL;
  for (uint8_t i = 0; i < 2; i++) {
    if (ch[i].outHalfCycles == 0) continue;
    uint32_t d = ch[i].nextEdgeCycle - now;
    if (d < nearest) nearest = d;
  }
  if (nearest == 0xFFFFFFFFUL) return;  // plus rien a planifier : timer arrete
  uint32_t ticks = nearest / TIMER_TICK_CYCLES;
  if (ticks < MIN_TIMER_TICKS) ticks = MIN_TIMER_TICKS;
  if (ticks > MAX_TIMER_TICKS) ticks = MAX_TIMER_TICKS;
  timer1_write(ticks);                  // (re)arme le compte a rebours materiel
}

static void IRAM_ATTR isrTimer() {
  uint32_t now = ESP.getCycleCount();
  for (uint8_t i = 0; i < 2; i++) {
    FanChannel* c = &ch[i];
    uint32_t half = c->outHalfCycles;
    if (half == 0) continue;                      // voie inactive
    if ((int32_t)(now - c->nextEdgeCycle) >= 0) { // echeance atteinte ?
      // Bascule de la sortie par ecriture registre directe (sure en ISR) :
      // niveau haut GPIO = transistor sature = ligne tach tiree a GND.
      if (c->outLevel) { GPOC = c->outMask; c->outLevel = 0; }
      else             { GPOS = c->outMask; c->outLevel = 1; }
      c->nextEdgeCycle += half;                   // reference absolue : pas de derive
      if ((int32_t)(now - c->nextEdgeCycle) >= 0) // retard > demi-periode : resync
        c->nextEdgeCycle = now + half;
    }
  }
  scheduleNext(now);
}

// ============================================================================
//  Pilotage d'une voie depuis loop() (section critique < 1 us)
// ============================================================================
static void setOutput(uint8_t i, uint32_t halfCycles) {
  FanChannel* c = &ch[i];
  noInterrupts();
  if (halfCycles == 0) {
    c->outHalfCycles = 0;
    if (c->outLevel) { GPOC = c->outMask; c->outLevel = 0; } // relache la ligne
  } else {
    bool wasOff = (c->outHalfCycles == 0);
    c->outHalfCycles = halfCycles;
    if (wasOff) c->nextEdgeCycle = ESP.getCycleCount() + halfCycles;
    scheduleNext(ESP.getCycleCount());            // (re)arme le timer materiel
  }
  interrupts();
}

// ============================================================================
//  Traitement periodique (loop, toutes les 100 ms) : RPM + lissage + ratio
// ============================================================================
static void updateChannel(uint8_t i, uint32_t nowCycles) {
  FanChannel*   c = &ch[i];
  ChannelState* s = &st[i];

  noInterrupts();                                 // cliché atomique
  uint32_t period   = c->periodCycles;
  uint32_t lastEdge = c->lastEdgeCycle;
  interrupts();

  bool stalled = (lastEdge == 0) ||
                 ((uint32_t)(nowCycles - lastEdge) > STALL_CYCLES);

  if (stalled || period == 0) {   // ventilateur arrete (ou une seule impulsion)
    s->smoothPeriod = 0;
    s->rpmIn  = 0;
    s->rpmOut = 0;
    s->active = false;
    setOutput(i, 0);              // l'onduleur voit 0 tr/min : etat fidele
    return;
  }

  // Lissage exponentiel : le regime d'un ventilateur evolue lentement ; cela
  // supprime le jitter residuel de mesure sans masquer les vraies variations.
  if (s->smoothPeriod == 0) s->smoothPeriod = (float)period;
  else s->smoothPeriod = 0.75f * s->smoothPeriod + 0.25f * (float)period;

  // Tach = 2 impulsions/tour -> RPM = frequence x 30
  s->rpmIn = 30.0f * (float)F_CPU / s->smoothPeriod;

  // Demi-periode de sortie : f_out = f_in x ratio -> T_out/2 = T_in / (2 x ratio)
  float half = s->smoothPeriod / (2.0f * cfg.ratio[i]);
  if (half < MIN_HALF_CYCLES) half = MIN_HALF_CYCLES;
  if (half > MAX_HALF_CYCLES) half = MAX_HALF_CYCLES;

  s->rpmOut = 15.0f * (float)F_CPU / half;   // RPM reellement emis
  s->active = true;
  setOutput(i, (uint32_t)half);
}

// ============================ Serveur web ==================================
static ESP8266WebServer server(80);

static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Simulateur tach Deye</title>
<style>
body{font-family:sans-serif;background:#15171a;color:#e8e8e8;max-width:640px;margin:1em auto;padding:0 1em}
h1{font-size:1.3em}h2{font-size:1.05em;color:#7ec8ff;margin:.2em 0 .5em}
table{width:100%;border-collapse:collapse;margin:.5em 0 1em}
td,th{border:1px solid #3a3f45;padding:.4em .5em;text-align:center}
form{background:#1d2024;border:1px solid #33383e;border-radius:8px;padding:.9em;margin:.9em 0}
input{padding:.35em;margin:.15em 0}
button{background:#2d6cdf;color:#fff;border:0;border-radius:5px;padding:.5em 1.1em;cursor:pointer}
.on{color:#4e5;font-weight:bold}.off{color:#fa5;font-weight:bold}
label{display:inline-block;min-width:9.5em}
small{color:#9aa}
</style></head><body>
<h1>Simulateur tach &mdash; Deye SUN-8K</h1>
<table>
<tr><th>Voie</th><th>RPM lu</th><th>Ratio</th><th>RPM simul&eacute;</th><th>&Eacute;tat</th></tr>
<tr><td>1 (9 cm)</td><td id="r0">-</td><td id="k0">-</td><td id="o0">-</td><td id="a0">-</td></tr>
<tr><td>2 (6 cm)</td><td id="r1">-</td><td id="k1">-</td><td id="o1">-</td><td id="a1">-</td></tr>
</table>
<form method="post" action="/api/ratio">
<h2>Ratios multiplicateurs</h2>
<label>Voie 1 (9 cm)</label><input name="r1" id="f1" type="number" step="0.1" min="0.5" max="8" required><br>
<label>Voie 2 (6 cm)</label><input name="r2" id="f2" type="number" step="0.1" min="0.5" max="8" required><br>
<button>Enregistrer</button> <small>persistant, effet imm&eacute;diat, sans red&eacute;marrage</small>
</form>
<form method="post" action="/api/wifi">
<h2>WiFi station (STA)</h2>
<label>SSID</label><input name="ssid" maxlength="32"><br>
<label>Mot de passe</label><input name="pass" type="password" maxlength="64"><br>
<label>Activer STA</label><input type="checkbox" name="en" value="1"><br>
<button>Enregistrer</button> <small>l'AP reste toujours actif</small>
</form>
<p>AP : <b>DeyeFanSim</b> &mdash; <span id="ap"></span><br>
STA : <span id="sta"></span><br>
Uptime : <span id="up"></span> s</p>
<script>
async function poll(){
 try{
  const d=await (await fetch('/api/status')).json();
  for(let i=0;i<2;i++){
   document.getElementById('r'+i).textContent=d.ch[i].rin;
   document.getElementById('o'+i).textContent=d.ch[i].rout;
   document.getElementById('k'+i).textContent=d.ch[i].ratio;
   const a=document.getElementById('a'+i);
   a.textContent=d.ch[i].act?'ACTIF':'attente';
   a.className=d.ch[i].act?'on':'off';
   const f=document.getElementById('f'+(i+1));
   if(document.activeElement!==f)f.value=d.ch[i].ratio;
  }
  document.getElementById('ap').textContent=d.ap;
  document.getElementById('sta').textContent=d.sta_ok?d.sta:'non connect\u00e9';
  document.getElementById('up').textContent=d.up;
 }catch(e){}
}
setInterval(poll,1000);poll();
</script></body></html>)rawliteral";

static void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

static void handleStatus() {
  char buf[360];
  int r10 = (int)(cfg.ratio[0] * 10.0f + 0.5f);   // ratio x10 en entier :
  int r11 = (int)(cfg.ratio[1] * 10.0f + 0.5f);   // evite le %f dans snprintf
  String staIp = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString()
                                                 : String("-");
  snprintf(buf, sizeof(buf),
    "{\"ch\":[{\"rin\":%u,\"rout\":%u,\"ratio\":%d.%d,\"act\":%u},"
            "{\"rin\":%u,\"rout\":%u,\"ratio\":%d.%d,\"act\":%u}],"
    "\"sta_ok\":%u,\"sta\":\"%s\",\"ap\":\"%s\",\"up\":%lu}",
    (unsigned)(st[0].rpmIn + 0.5f), (unsigned)(st[0].rpmOut + 0.5f),
    r10 / 10, r10 % 10, st[0].active ? 1u : 0u,
    (unsigned)(st[1].rpmIn + 0.5f), (unsigned)(st[1].rpmOut + 0.5f),
    r11 / 10, r11 % 10, st[1].active ? 1u : 0u,
    (WiFi.status() == WL_CONNECTED) ? 1u : 0u,
    staIp.c_str(), WiFi.softAPIP().toString().c_str(),
    (unsigned long)(millis() / 1000UL));
  server.send(200, "application/json", buf);
}

static void handleRatio() {
  if (server.hasArg("r1")) cfg.ratio[0] = clampRatio(server.arg("r1").toFloat());
  if (server.hasArg("r2")) cfg.ratio[1] = clampRatio(server.arg("r2").toFloat());
  saveConfig();                          // effet immediat via updateChannel()
  server.sendHeader("Location", "/");
  server.send(303);
}

static void handleWifi() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  cfg.staEnable = server.hasArg("en") ? 1 : 0;
  strncpy(cfg.ssid, ssid.c_str(), 32); cfg.ssid[32] = 0;
  strncpy(cfg.pass, pass.c_str(), 64); cfg.pass[64] = 0;
  saveConfig();
  if (cfg.staEnable && cfg.ssid[0]) {    // applique sans redemarrer
    WiFi.disconnect();
    WiFi.begin(cfg.ssid, cfg.pass);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// ============================ setup() ======================================
void setup() {
  Serial.begin(115200);
  Serial.println(F("\nSimulateur tach Deye - boot"));

  memset(ch, 0, sizeof(ch));
  memset(st, 0, sizeof(st));
  ch[0].pinIn = PIN_IN1; ch[0].pinOut = PIN_OUT1; ch[0].outMask = 1UL << PIN_OUT1;
  ch[1].pinIn = PIN_IN2; ch[1].pinOut = PIN_OUT2; ch[1].outMask = 1UL << PIN_OUT2;

  // Sorties au repos AVANT le passage en OUTPUT (transistor bloque, ligne
  // tach relachee) : aucun glitch vers l'onduleur au demarrage.
  digitalWrite(PIN_OUT1, LOW); pinMode(PIN_OUT1, OUTPUT);
  digitalWrite(PIN_OUT2, LOW); pinMode(PIN_OUT2, OUTPUT);
  digitalWrite(PIN_LED, HIGH); pinMode(PIN_LED, OUTPUT);   // LED eteinte

  // Pull-up interne en secours ; le pull-up principal est externe (10k/3,3V)
  pinMode(PIN_IN1, INPUT_PULLUP);
  pinMode(PIN_IN2, INPUT_PULLUP);

  loadConfig();

  // ---- WiFi : AP + STA simultanes ----
  WiFi.persistent(false);             // pas d'ecriture flash SDK imprevue
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP); // latence reseau stable
  WiFi.hostname("deye-fan-sim");
  WiFi.softAP("DeyeFanSim", "deye1234");        // toujours : http://192.168.4.1
  if (cfg.staEnable && cfg.ssid[0]) WiFi.begin(cfg.ssid, cfg.pass);
  Serial.print(F("AP IP : ")); Serial.println(WiFi.softAPIP());

  // ---- Serveur web (tout en contexte loop, jamais en ISR) ----
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/ratio", HTTP_POST, handleRatio);
  server.on("/api/wifi", HTTP_POST, handleWifi);
  server.onNotFound([]() { server.send(404, "text/plain", "404"); });
  server.begin();

  // ---- Capture des tach d'entree (interruptions GPIO, front descendant) ----
  attachInterrupt(digitalPinToInterrupt(PIN_IN1), isrIn1, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_IN2), isrIn2, FALLING);

  // ---- timer1 materiel : sequenceur de fronts de sortie ----
  // TIM_SINGLE = one-shot : l'ISR rearme a chaque fois sur l'echeance la
  // plus proche. Pas de timer1_write ici : arme a la 1re voie active.
  timer1_attachInterrupt(isrTimer);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
}

// ============================ loop() =======================================
void loop() {
  server.handleClient();              // web/WiFi : contexte cooperatif uniquement

  uint32_t nowMs = millis();

  // Mesure + regeneration tach : toutes les 100 ms
  static uint32_t lastProcess = 0;
  if (nowMs - lastProcess >= 100) {
    lastProcess = nowMs;
    uint32_t nowCycles = ESP.getCycleCount();
    updateChannel(0, nowCycles);
    updateChannel(1, nowCycles);
  }

  // LED : fixe = au moins une voie simule ; clignotant lent = attente signal
  static uint32_t lastLed = 0;
  static bool ledState = false;
  bool anyActive = st[0].active || st[1].active;
  if (anyActive) {
    digitalWrite(PIN_LED, LOW);       // allumee (active bas)
    ledState = true;
  } else if (nowMs - lastLed >= 500) {
    lastLed = nowMs;
    ledState = !ledState;
    digitalWrite(PIN_LED, ledState ? LOW : HIGH);
  }

  // WiFi STA : reconnexion non bloquante toutes les 15 s si necessaire
  static uint32_t lastWifiCheck = 0;
  if (cfg.staEnable && cfg.ssid[0] && (nowMs - lastWifiCheck >= 15000)) {
    lastWifiCheck = nowMs;
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.disconnect();
      WiFi.begin(cfg.ssid, cfg.pass);
    }
  }

#if SERIAL_DEBUG
  static uint32_t lastDbg = 0;
  if (nowMs - lastDbg >= 5000) {
    lastDbg = nowMs;
    Serial.printf("V1 in=%5.0f out=%5.0f r=%.1f %s | V2 in=%5.0f out=%5.0f r=%.1f %s\n",
                  st[0].rpmIn, st[0].rpmOut, cfg.ratio[0], st[0].active ? "ACTIF" : "attente",
                  st[1].rpmIn, st[1].rpmOut, cfg.ratio[1], st[1].active ? "ACTIF" : "attente");
  }
#endif

  yield();                            // laisse respirer le stack WiFi
}
