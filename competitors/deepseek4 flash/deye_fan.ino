// =============================================================================
//  deye_fan.ino  -  Simulateur de signal tachymétrique (RPM) pour onduleur Deye
//  Cible        : LOLIN(WEMOS) D1 mini - ESP8266 (ESP-12S)
//  ----------------------------------------------------------------------------
//
//  BUT
//  ===
//  L'onduleur Deye SUN-8K-SG05LP1 lit le signal tach des 2 ventilateurs d'origine
//  (NMB 06025VE-12N-CL = 6 cm et NMB 09225VE-12N-CU = 9 cm, 12 V, 2 impulsions/tour).
//  Ceux-ci sont remplacés par 4 Noctua silencieux (2x NF-A6x25-FLX, 2x NF-A9-FLX),
//  dont la vitesse de rotation est bien plus basse que les NMB d'origine.
//  L'onduleur pourrait déclencher une alarme (ventilation insuffisante). Ce firmware
//  lit le tach réel de chaque ventilateur Noctua, multiplie sa fréquence par un ratio
//  configurable par voie, puis ré-émet un signal tach simulé vers l'onduleur.
//
//  CONTRAINTES COUVERTES
//  =====================
//   1) Lecture des 2 tach      -> interripts GPIO (front montant) + micros()
//   2) Ratio par voie          -> réglable via une interface Web (sans recompiler)
//   3) WiFi AP + STA simulane  -> parametres persistés (EEPROM)
//   4) LED d'etat              -> allumée si simulation active, clignote si attente
//   5) Alimentation            -> depuis les connecteurs ventilo (12V) via 2 diodes
//                                 OR + convertisseur abaisseur 12V->5V
//                                 (schéma complet dans SCHEMA_ELECTRONIQUE.md)
//   6) Tension tach entrante   -> inconnue (3,3/5/12V) : circuit de clamp 3,3V
//   7) SORTIE STABLE           -> générée par le Timer1 MATERIEL FRC1 en mode NMI
// =============================================================================
//  ARCHITECTURE TEMPS REEL  (point primordial du cahier des charges)
//  ---------------------------------------------------------------------------
//  . La GENERATION de la sortie vit DANS le Timer1 materiel FRC1 de l'ESP8266.
//    Par defaut USE_NMI_TIMER=1 : l'ISR est branché sur le vecteur NMI du FRC1.
//    Un NMI ne peut PAS être masqué, pas même par la pile Wi-Fi (qui utilise des
//    sections critiques ETS_INTR_LOCK() pour ses échanges radio). Donc une
//    activité réseau intense ou une requete HTTP ne peut PAS retarder une
//    bascule de sortie.
//  . L'ISR ne fait que : basculer des broches GPIO (W1TS/W1TC) et re-armer le
//    compteur materiel (event-calendar). Aucune fonction de bibliothèque,
//    aucun accès flash, aucun flottant. L'ISR est en IRAM.
//  . La MESURE des entrées utilise des interripts GPIO séparés + micros() (basé
//    sur le compteur de cycles) ; la periode est lissée par un filtre exponentiel.
//  . Le serveur web/TCP ne voit que des "instantanés" volatils (snapshots) : il
//    ne touche jamais au chemin temps réel.
// =============================================================================
// =============================================================================
//  INCLUDES
// =============================================================================
#include <Arduino.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ets_sys.h>          // macros ets_isr_*, NmiTimSetFunc, ETS_FRC_TIMER1_INUM

// =============================================================================
//  AFFECTATION DES BROCHES  (LOLIN Wemos D1 mini)
// =============================================================================
//  Nomenclature Wemos : D1=GPIO5 D2=GPIO4 D4=GPIO2(LED) D5=GPIO14 D6=GPIO12
const uint8_t PIN_IN0   = 5;   // D1  GPIO5  -  tach Noctua 90 mm  -> entrée
const uint8_t PIN_IN1   = 4;   // D2  GPIO4  -  tach Noctua 60 mm  -> entrée
const uint8_t PIN_OUT0  = 14;  // D5  GPIO14 -  tach SIMULEE 90mm  -> onduleur
const uint8_t PIN_OUT1  = 12;  // D6  GPIO12 -  tach SIMULEE 60mm  -> onduleur
const uint8_t PIN_LED   = 2;   // D4  GPIO2  -  LED on-board (actif à l'état bas)

const uint32_t GPIO_MASK_IN0  = (1UL << PIN_IN0);    // 0x00000020
const uint32_t GPIO_MASK_IN1  = (1UL << PIN_IN1);    // 0x00000010
const uint32_t GPIO_MASK_OUT0 = (1UL << PIN_OUT0);   // 0x00004000
const uint32_t GPIO_MASK_OUT1 = (1UL << PIN_OUT1);   // 0x00001000
const uint32_t GPIO_MASK_LED  = (1UL << PIN_LED);    // 0x00000004

// =============================================================================
//  CONSTANTES DE REGLAGE
// =============================================================================
const uint32_t RATIO_DEFAULT_X100[2] = { 200, 260 }; // ch0 (90mm): x2.00 ; ch1 (60mm): x2.60
const uint32_t RATIO_MIN_X100 = 40;                  // 0.40 (borne basse)
const uint32_t RATIO_MAX_X100 = 2000;                // 20.0 (borne haute)

#define PULSES_PER_REV  2         // tach Noctua = 2 impulsions par tour
#define TACH_MIN_US     400UL     // anti-rebond : ignorer les fronts < 0,4 ms
#define TACH_MAX_US     120000000UL  // période tach max acceptée (120 s)
#define TICK_PER_US     5UL       // ticks FRC1 par us (APB 80 MHz / div16)
#define ISR_MIN_DELTA   2UL       // delta d'armement minimal (ticks)
#define ISR_MAX_DELTA   0x7FFFFFUL// compteur FRC1 = 23 bits
#define EMA_SHIFT       3         // filtre période : alpha = 1/2^3
#define STALL_MIN_US    2000000UL // min timeout d'absence de signal : 2 s
#define STALL_MULT      4         // timeout = max(STALL_MIN_US, 4x période)

// Choisir le mode de branchement du Timer1 :
//   1 = NMI (le plus stable, recommandé) ; 0 = ISR "normale" (portable)
#define USE_NMI_TIMER   1

// =============================================================================
//  REGISTRES MATERIELS  (adresses issues du datasheet ESP8266 - section "GPIO"
//  et "FRC1 General Purpose Timer". Le firmware programme directement ces
//  registres pour etre independant du noyau Arduino (pas de timer1_*).
// =============================================================================
#define REG_FRC1_LOAD  0x60000600UL
#define REG_FRC1_COUNT 0x60000604UL
#define REG_FRC1_CTRL  0x60000608UL
#define REG_FRC1_INT   0x6000060CUL
#define REG_GPIO_OUT   0x60000300UL
#define REG_GPIO_W1TS  0x60000304UL   // set bits (write-1-to-set)
#define REG_GPIO_W1TC  0x60000308UL   // clear bits (write-1-to-clear)
#define REG_GPIO_IN    0x60000310UL

#define FRC1_CTRL_DIV_16    (1UL << 8)   // champ diviseur [9:8] = 1 -> /16
#define FRC1_CTRL_EDGE      0UL          // bit6 = 0 -> interrupt de front
#define FRC1_CTRL_NO_RELOAD (0UL << 7)   // pas d'auto-reload (re-armé par l'ISR)
#define FRC1_CTRL_ENABLE    (1UL << 0)   // bit0 = ON

#define REG_WR(a, v) (*((volatile uint32_t *)(a)) = (uint32_t)(v))
#define REG_RD(a)    (*((volatile uint32_t *)(a)))
// =============================================================================
//  ETAT PARTAGE  ISR  <->  BOUCLE
//  (toute écriture depuis la boucle se fait dans noInterrupts() : les accès
//   32 bits alignés du Xtensa sont atomiques, mais on sécurise les groupes)
// =============================================================================
static volatile uint32_t g_nowTicks;             // "now" en ticks FRC1 (0,2 us)
static volatile uint32_t g_lastDelta;            // delta d'armement en cours (= période déjà programmée)
static volatile uint32_t g_halfTicks[2];         // demi-période de SORTIE en ticks ; 0 = voie inactive
static volatile uint32_t g_nextTicks[2];         // date absolue (ticks) de la prochaine bascule
static volatile uint32_t g_outLevel;             // motif GPIO des sorties (bits OUT0/OUT1) - état logique voulu
static volatile uint32_t g_outHW;                // dernier motif réellement écrit sur le port

// --- mesure d'entrée ---------------------------------------------------------
static volatile uint32_t g_inPeriodUs[2];        // dernière période tach fiable (us)
static volatile uint32_t g_inLastUs[2];          // micros() du dernier front reconnu
static volatile uint32_t g_inEdgeCnt[2];         // compteur de fronts pour la fenêtre 1 s

// --- config dynamique (par voie) ----------------------------------------------
static volatile uint32_t g_ratioX100[2];         // ratio réglable (x100), couche web
static volatile uint32_t g_ratioQ16[2];          // ratio converti en Q16 fixe (évite le flottant)

// --- snapshot d'état pour le serveur web --------------------------------------
typedef struct {
  uint32_t inUs[2];    // période tach mesurée (us)
  uint32_t outUs[2];   // période simulée émise (us)
  uint32_t rpmIn[2];   // RPM mesurés (fenêtre 1 s)
  uint32_t rpmOut[2];  // RPM simulés (calculés sur la période réellement émise)
  uint8_t  live[2];    // 1 = signal tach présent depuis moins d'un timeout
} SnapT;
static volatile SnapT  g_snap;
static volatile uint32_t g_snapUptime;

// =============================================================================
//  ISR 1 : GENERATION DES SORTIES  (Timer1 FRC1 - mode NMI de préférence)
// =============================================================================
#if USE_NMI_TIMER
extern "C" void NmiTimSetFunc(void (*)(void));     // ROM : installe le handler NMI FRC1
#endif

// L'ISR doit rester minuscule : registres seulement. NE JAMAIS y appeler
// de fonction de bibliothèque, micros(), Serial, EEPROM, etc.
static void IRAM_ATTR app_timer1_isr(void)
{
    uint32_t now = g_nowTicks + g_lastDelta;       // horloge logique avancée
    uint32_t out = g_outLevel;
    g_nowTicks = now;

    // --- bascule des voies dont l'échéance est atteinte --------------------
    for (uint8_t i = 0; i < 2; i++)
    {
        uint32_t hp = g_halfTicks[i];
        if (hp == 0) continue;                          // voie désactivée
        if ((int32_t)(g_nextTicks[i] - now) <= 0)       // échéance due ?
        {
            out ^= (i == 0) ? GPIO_MASK_OUT0 : GPIO_MASK_OUT1;
            g_nextTicks[i] += hp;                       // prochaine bascule
        }
    }

    // --- application du nouveau motif sur les 2 sorties ----------------------
    uint32_t diff = out ^ g_outHW;
    if (diff)
    {
        REG_WR(REG_GPIO_W1TS, (diff & out));   // bits à passer à 1
        REG_WR(REG_GPIO_W1TC, (diff & ~out));  // bits à passer à 0
        g_outHW = out;
    }
    g_outLevel = out;

    // --- calcul du prochain événement (le plus proche des 2 voies) -----------
    uint32_t nd = 0x7FFFFFFFUL;
    if (g_halfTicks[0]) nd = g_nextTicks[0];
    if (g_halfTicks[1] && (int32_t)(g_nextTicks[1] - nd) < 0) nd = g_nextTicks[1];

    uint32_t delta = nd - now;
    if (delta < ISR_MIN_DELTA) delta = ISR_MIN_DELTA;
    if (delta > ISR_MAX_DELTA) delta = ISR_MAX_DELTA;

    g_lastDelta = delta;
    REG_WR(REG_FRC1_LOAD,  delta);    // écrit LOAD (et COUNT par sécurité) =>
    REG_WR(REG_FRC1_COUNT, delta);    // le compteur 23 bits repart pour delta ticks
}

// =============================================================================
//  ISR 2 : MESURE DES ENTREES  (interrupts GPIO, front montant)
// =============================================================================
static void IRAM_ATTR isr_tach0(void)          // tach Noctua 90 mm (GPIO5/D1)
{
    uint32_t us = micros();
    uint32_t d = us - g_inLastUs[0];
    if ((d >= TACH_MIN_US) && (d <= TACH_MAX_US))
        g_inPeriodUs[0] = d;                    // période fiable = front à front
    g_inLastUs[0] = us;                         // sert aussi au timeout
    if (g_inEdgeCnt[0] < 0xFFFFFFF0UL) g_inEdgeCnt[0]++;
}

static void IRAM_ATTR isr_tach1(void)          // tach Noctua 60 mm (GPIO4/D2)
{
    uint32_t us = micros();
    uint32_t d = us - g_inLastUs[1];
    if ((d >= TACH_MIN_US) && (d <= TACH_MAX_US))
        g_inPeriodUs[1] = d;
    g_inLastUs[1] = us;
    if (g_inEdgeCnt[1] < 0xFFFFFFF0UL) g_inEdgeCnt[1]++;
}
// =============================================================================
//  PERSISTANCE  /  CONFIGURATION  (EEPROM 512 octets)
// =============================================================================
typedef struct {
  uint8_t  magic[4];              // { 'D','F','1','0' }
  uint8_t  ver;
  char     staSsid[33];           // réseau STA (WPA2)
  char     staPw[65];
  char     apSsid[33];            // point d'accès interne
  char     apPw[65];
  uint16_t ratioX100[2];          // ratios x100 (40..2000)
  uint8_t  crc;                   // CRC8 sur tout le reste
} CfgT;

static CfgT g_cfg;

static uint8_t crc8(const uint8_t *p, uint16_t n)
{
    uint8_t c = 0;
    while (n--) c = (uint8_t)(c + *p++);
    return c;
}

static void cfgDefaults(void)
{
    memset(&g_cfg, 0, sizeof(g_cfg));
    g_cfg.magic[0] = 'D'; g_cfg.magic[1] = 'F'; g_cfg.magic[2] = '1'; g_cfg.magic[3] = '0';
    g_cfg.ver = 1;
    strcpy(g_cfg.apSsid, "DeyeFanAP");
    strcpy(g_cfg.apPw,  "deye-fan");
    g_cfg.ratioX100[0] = (uint16_t)RATIO_DEFAULT_X100[0];
    g_cfg.ratioX100[1] = (uint16_t)RATIO_DEFAULT_X100[1];
}

static void cfgLoad(void)
{
    CfgT read;
    EEPROM.get(0, read);
    if (memcmp(read.magic, "DF10", 4) == 0 &&
        read.crc == crc8((const uint8_t *)&read, sizeof(read) - 1))
    {
        g_cfg = read;
        for (uint8_t i = 0; i < 2; i++)          // bornage de sécurité
        {
            if (g_cfg.ratioX100[i] < RATIO_MIN_X100) g_cfg.ratioX100[i] = RATIO_MIN_X100;
            if (g_cfg.ratioX100[i] > RATIO_MAX_X100) g_cfg.ratioX100[i] = RATIO_MAX_X100;
        }
        g_cfg.staSsid[sizeof(g_cfg.staSsid)-1] = 0;  // bornage chaînes
        g_cfg.staPw[sizeof(g_cfg.staPw)-1]     = 0;
        g_cfg.apSsid[sizeof(g_cfg.apSsid)-1]   = 0;
        g_cfg.apPw[sizeof(g_cfg.apPw)-1]       = 0;
    }
    else
    {
        cfgDefaults();
        cfgSave();
    }
}

static void cfgSave(void)
{
    g_cfg.crc = crc8((const uint8_t *)&g_cfg, sizeof(g_cfg) - 1);
    EEPROM.put(0, g_cfg);
    EEPROM.commit();
}

// =============================================================================
//  PETITES AIDEES ARITHMETIQUES
// =============================================================================
static inline uint32_t clampU32(uint32_t v, uint32_t lo, uint32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// ratio x100 (ex: 250 = 2.50) -> Q16 fixe (2.50 -> ~163840)
static inline uint32_t ratioToQ16(uint32_t x100)
{
    return (uint32_t)(((uint64_t)x100 * 65536UL + 50UL) / 100UL);
}

// période tach mesurée (us) -> RPM réels (PULSES_PER_REV = 2)
static inline uint32_t usToRpm(uint32_t us)
{
    if (us == 0) return 0;
    return (60UL * 1000000UL) / ((uint64_t)us * PULSES_PER_REV);
}
// =============================================================================
//  SERVEUR WEB  +  INTERFACE
// =============================================================================
static ESP8266WebServer server(80);
static char g_stateBuf[640];

// ---------------------------------------------------------------------------
//  PAGE HTML  (stockée en flash)
// ---------------------------------------------------------------------------
static const char PAGE_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="fr"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Deye Fan Simulator</title>
<style>
body{font-family:sans-serif;max-width:760px;margin:24px auto;padding:0 12px;background:#f7f7f2;color:#222}
.card{border:1px solid #ccc;border-radius:8px;padding:12px;margin:12px 0;background:#fff}
table{width:100%;border-collapse:collapse} td,th{border:1px solid #ddd;padding:6px;text-align:center}
.live{color:#0a0;font-weight:bold} .dead{color:#c00;font-weight:bold}
input[type=range]{width:92%} .lbl{font-size:.85em;color:#666}
</style></head><body>
<h2>Simulateur tach &mdash; onduleur Deye</h2>
<div class="card"><b>Temps réel</b> (rafraîchissement 1,5 s)
<table><tr><th>Voie</th><th>État</th><th>Tach lue</th><th>RPM r&eacute;els</th><th>Ratio</th><th>P&eacute;riode &eacute;mise</th><th>RPM simul&eacute;s</th></tr>
<tr><td>90&nbsp;mm (NF-A9)</td><td id="st0"></td><td id="iu0"></td><td id="ri0"></td>
<td><input type="range" id="r0" min="40" max="2000" step="5" value="200" oninput="sendR(0,this.value)"><div class="lbl" id="rv0"></div></td>
<td id="ou0"></td><td id="ro0"></td></tr>
<tr><td>60&nbsp;mm (NF-A6x25)</td><td id="st1"></td><td id="iu1"></td><td id="ri1"></td>
<td><input type="range" id="r1" min="40" max="2000" step="5" value="260" oninput="sendR(1,this.value)"><div class="lbl" id="rv1"></div></td>
<td id="ou1"></td><td id="ro1"></td></tr>
</table>
<div class="lbl">Le ratio agit imm&eacute;diatement (non sauvegard&eacute;). Pour le rendre d&eacute;finitif : bouton Enregistrer.</div></div>
<div class="card"><b>Wi-Fi &amp; syst&egrave;me</b>
<table id="wt"></table></div>
<div class="card"><b>R&eacute;glage</b>
<form method="POST" action="/save">
<table><tr><td>Ratio voie 90&nbsp;mm</td><td><input name="r0" type="number" min="0.40" max="20" step="0.05" id="fr0"></td></tr>
<tr><td>Ratio voie 60&nbsp;mm</td><td><input name="r1" type="number" min="0.40" max="20" step="0.05" id="fr1"></td></tr>
<tr><td>SSID STA (maison)</td><td><input name="sta_ssid" id="fs"></td></tr>
<tr><td>Mot de passe STA</td><td><input name="sta_pas" type="password" id="fp"></td></tr>
<tr><td>SSID AP</td><td><input name="ap_ssid" id="fa"></td></tr>
<tr><td>Mot de passe AP</td><td><input name="ap_pas" type="password" id="fap"></td></tr>
</table>
<input type="submit" value="Enregistrer"> <span class="lbl">(les r&eacute;glages Wi-Fi ne sont appliqu&eacute;s qu'apr&egrave;s red&eacute;marrage)</span>
</form></div>
<script>
function fmtUs(u){ if(u>1000){ return (u/1000).toFixed(1)+' ms'; } return u+' \u00b5s'; }
function clampVal(v,a,b){ return Math.max(a, Math.min(b, v)); }
function sendR(ck,v){ v=clampVal(v,40,2000); fetch('/api/set?c='+ck+'&r='+v); document.getElementById('rv'+ck).textContent=(v/100).toFixed(2)+' \u00d7'; }
function esc(s){ return String(s).replace(/[<>&"]/g, function(c){ return {'<':'&lt;','>':'&gt;','&':'&amp;','"':'&quot;'}[c]; }); }
async function refresh(){
 try{
  const j = await (await fetch('/api/state')).json();
  const now = Date.now();
  for(let k=0;k<2;k++){
   const c=j.ch[k];
   document.getElementById('st'+k).textContent = c.live? 'en cours' : 'attente';
   document.getElementById('st'+k).className = c.live? 'live' : 'dead';
   document.getElementById('iu'+k).textContent = c.inUs? fmtUs(c.inUs) : '\u2014';
   document.getElementById('ri'+k).textContent = c.rpm? c.rpm+' RPM' : '\u2014';
   document.getElementById('ro'+k).textContent = c.sim? c.sim+' RPM' : '\u2014';
   document.getElementById('ou'+k).textContent = c.outUs? fmtUs(c.outUs) : '\u2014';
   const rv=(c.r/100).toFixed(2);
   document.getElementById('rv'+k).textContent = rv+' \u00d7';
   document.getElementById('r'+k).value = Math.round(c.r/5)*5;
  }
  document.getElementById('fr0').value = (j.ch[0].r/100).toFixed(2);
  document.getElementById('fr1').value = (j.ch[1].r/100).toFixed(2);
  document.getElementById('fs').value = j.sta.ssid;
  document.getElementById('fp').value = j.sta.pass;
  document.getElementById('fa').value = j.sta.ssidAp;
  document.getElementById('fap').value = j.sta.passAp;
  let w  = '<tr><td>STA</td><td>'+(j.sta.conn? 'connect\u00e9 \u00e0 <b>'+esc(j.sta.ssid)+'</b> \u00b7 IP '+esc(j.sta.ip) : 'pas de liaison')+'</td></tr>';
  w    += '<tr><td>AP</td><td><b>'+esc(j.sta.ssidAp)+'</b> \u00b7 192.168.4.1 \u00b7 '+j.ap.clients+' client(s)</td></tr>';
  w    += '<tr><td>Uptime</td><td>'+Math.round(now/1000)+' s</td></tr>';
  document.getElementById('wt').innerHTML = w;
 }catch(e){ /* silencieux : tentative au prochain polling */ }
}
setInterval(refresh,1500); refresh();
</script></body></html>)HTML";
// ---------------------------------------------------------------------------
//  GESTIONNAIRES HTTP
// ---------------------------------------------------------------------------
static void handleRoot()   { server.send_P(200, "text/html", PAGE_HTML); }

// Conversion "2", "2.5", "2.50" -> 250 (ratio x100)
static long parseRatio100(const char *s)
{
    long ip = 0, fp = 0, mult = 1;
    bool deci = false;
    int  fcnt = 0;
    while (*s == ' ') s++;
    for (; *s; s++)
    {
        char ch = *s;
        if (ch >= '0' && ch <= '9')
        {
            if (!deci) { ip = ip * 10 + (ch - '0'); }
            else if (fcnt < 2) { fp = fp * 10 + (ch - '0'); fcnt++; mult *= 10; }
            else { /* on ignore la 3e décimale */ }
        }
        else if ((ch == '.' || ch == ',') && !deci) { deci = true; }
        else break;
    }
    if (!deci) return ip * 100;
    return ip * 100 + fp * (100L / mult);
}

static void handleState()
{
    uint32_t ratios[2];
    SnapT s;
    noInterrupts();
    s = g_snap;
    ratios[0] = g_ratioX100[0];
    ratios[1] = g_ratioX100[1];
    interrupts();

    bool conn = (WiFi.status() == WL_CONNECTED);
    uint32_t ip = conn ? (uint32_t)WiFi.localIP() : 0;
    int32_t rssi = conn ? WiFi.RSSI() : 0;

    char ipbuf[16];
    snprintf(ipbuf, sizeof(ipbuf), "%u.%u.%u.%u",
             (unsigned)((ip >> 24) & 0xFF), (unsigned)((ip >> 16) & 0xFF),
             (unsigned)((ip >> 8) & 0xFF),  (unsigned)(ip & 0xFF));

    int n = snprintf_P(g_stateBuf, sizeof(g_stateBuf),
        PSTR("{\"ch\":[{\"live\":%u,\"r\":%lu,\"rpm\":%lu,\"sim\":%lu,\"inUs\":%lu,\"outUs\":%lu},"
             "{\"live\":%u,\"r\":%lu,\"rpm\":%lu,\"sim\":%lu,\"inUs\":%lu,\"outUs\":%lu}],"),
        (unsigned)s.live[0], (unsigned long)ratios[0], (unsigned long)s.rpmIn[0], (unsigned long)s.rpmOut[0], (unsigned long)s.inUs[0], (unsigned long)s.outUs[0],
        (unsigned)s.live[1], (unsigned long)ratios[1], (unsigned long)s.rpmIn[1], (unsigned long)s.rpmOut[1], (unsigned long)s.inUs[1], (unsigned long)s.outUs[1]);
    if (n < 0) n = 0;
    if ((size_t)n >= sizeof(g_stateBuf)) n = sizeof(g_stateBuf) - 1;
    snprintf_P(g_stateBuf + n, sizeof(g_stateBuf) - (size_t)n,
        PSTR("\"rssi\":%ld,\"sta\":{\"conn\":%u,\"ssid\":\"%s\",\"pass\":\"%s\",\"ip\":\"%s\",\"ssidAp\":\"%s\",\"passAp\":\"%s\"},\"ap\":{\"clients\":%u}}"),
        (long)rssi, conn ? 1 : 0, g_cfg.staSsid, g_cfg.staPw, ipbuf, g_cfg.apSsid, g_cfg.apPw,
        (unsigned)WiFi.softAPgetStationNum());
    server.send(200, "application/json", g_stateBuf);
}

// Réglage rapide d'un ratio : GET /api/set?c=0&r=250
static void handleSet()
{
    if (!server.hasArg("c") || !server.hasArg("r")) { server.send(400, "text/plain", "parametres c & r requis"); return; }
    int ch = server.arg("c").toInt();
    if (ch < 0 || ch > 1) { server.send(400, "text/plain", "c invalide"); return; }
    long v = strtol(server.arg("r").c_str(), NULL, 10);
    v = (long)clampU32((uint32_t)v, RATIO_MIN_X100, RATIO_MAX_X100);
    g_ratioX100[ch] = (uint32_t)v;
    g_ratioQ16[ch]  = ratioToQ16((uint32_t)v);
    g_cfg.ratioX100[ch] = (uint16_t)v;
    server.send(200, "application/json", "{\"ok\":1}");
}

// Sauvegarde complète : POST /save (formulaire)
static void handleSave()
{
    if (server.hasArg("r0"))
    {
        long v = parseRatio100(server.arg("r0").c_str());
        if (v >= (long)RATIO_MIN_X100 && v <= (long)RATIO_MAX_X100)
        {
            g_cfg.ratioX100[0] = (uint16_t)v;
            g_ratioX100[0] = (uint32_t)v;
            g_ratioQ16[0]  = ratioToQ16((uint32_t)v);
        }
    }
    if (server.hasArg("r1"))
    {
        long v = parseRatio100(server.arg("r1").c_str());
        if (v >= (long)RATIO_MIN_X100 && v <= (long)RATIO_MAX_X100)
        {
            g_cfg.ratioX100[1] = (uint16_t)v;
            g_ratioX100[1] = (uint32_t)v;
            g_ratioQ16[1]  = ratioToQ16((uint32_t)v);
        }
    }
    if (server.hasArg("sta_ssid")) { strncpy(g_cfg.staSsid, server.arg("sta_ssid").c_str(), sizeof(g_cfg.staSsid) - 1); g_cfg.staSsid[sizeof(g_cfg.staSsid) - 1] = 0; }
    if (server.hasArg("sta_pas"))  { strncpy(g_cfg.staPw,  server.arg("sta_pas").c_str(),  sizeof(g_cfg.staPw) - 1);  g_cfg.staPw[sizeof(g_cfg.staPw) - 1]  = 0; }
    if (server.hasArg("ap_ssid"))  { strncpy(g_cfg.apSsid, server.arg("ap_ssid").c_str(), sizeof(g_cfg.apSsid) - 1); g_cfg.apSsid[sizeof(g_cfg.apSsid) - 1] = 0; }
    if (server.hasArg("ap_pas"))   { strncpy(g_cfg.apPw,   server.arg("ap_pas").c_str(),  sizeof(g_cfg.apPw) - 1);  g_cfg.apPw[sizeof(g_cfg.apPw) - 1]   = 0; }
    cfgSave();
    server.sendHeader("Location", "/?saved=1");
    server.send(302, "text/plain", "OK");
}
#if !USE_NMI_TIMER
#include <core_esp8266_timer.h>     // API publique du noyau : timer1_attachInterrupt
#warning "USE_NMI_TIMER=0 : ISR Timer1 'normale' (moins immune aux sections critiques WiFi)."
#endif

// -----------------------------------------------------------------------------
//  INITIALISATION DU TIMER1 MATERIEL (FRC1)  en event-calendar
// -----------------------------------------------------------------------------
static void initTimer(void)
{
    noInterrupts();

    g_nowTicks  = 0;
    g_lastDelta = ISR_MIN_DELTA;
    g_halfTicks[0] = 0;  g_halfTicks[1] = 0;
    g_nextTicks[0] = 0;  g_nextTicks[1] = 0;
    g_outLevel  = 0;
    g_outHW     = 0;

    // Compteur FRC1 : div /16 (0,2 us par tick), IRQ de front, pas d'auto-reload
    REG_WR(REG_FRC1_CTRL, FRC1_CTRL_DIV_16 | FRC1_CTRL_EDGE | FRC1_CTRL_NO_RELOAD);
    REG_WR(REG_FRC1_LOAD,  ISR_MIN_DELTA);
    REG_WR(REG_FRC1_COUNT, ISR_MIN_DELTA);

#if USE_NMI_TIMER
    NmiTimSetFunc(&app_timer1_isr);            // IRQ non masquable : jitter ~0
#else
    timer1_attachInterrupt(&app_timer1_isr);   // ISR "normale" (portabilité)
#endif
    REG_WR(REG_FRC1_CTRL, FRC1_CTRL_DIV_16 | FRC1_CTRL_EDGE | FRC1_CTRL_NO_RELOAD | FRC1_CTRL_ENABLE);
    ets_isr_unmask((1 << ETS_FRC_TIMER1_INUM));   // autoriser la ligne IRQ n°9

    interrupts();
}

// -----------------------------------------------------------------------------
//  WIFI : AP + STA SIMULTANES, continu en boucle
// -----------------------------------------------------------------------------
static void wifiInit(void)
{
    WiFi.mode(WIFI_AP_STA);                    // AP interne + station en même temps
    WiFi.setHostname("deye-fan");

    bool apOk = (g_cfg.apPw[0] != 0)
                ? WiFi.softAP(g_cfg.apSsid, g_cfg.apPw)
                : WiFi.softAP(g_cfg.apSsid);
    if (!apOk) Serial.println(F("[wifi] AP impossible !"));

    if (g_cfg.staSsid[0] != 0)
    {
        WiFi.begin(g_cfg.staSsid, g_cfg.staPw);   // non bloquant (cf. loop)
        WiFi.setAutoReconnect(true);
    }
    else
    {
        Serial.println(F("[wifi] STA non configurée : mode AP seul"));
    }
}

// -----------------------------------------------------------------------------
//  MISE A JOUR D'UNE VOIE  (appelée souvent dans loop)
//   - timeout d'arrêt du ventilateur => sortie à l'arrêt (honnêteté)
//   - période lissée (EMA) => demi-période de sortie = in*(5t/us)/ratio/2
// -----------------------------------------------------------------------------
static uint32_t s_smUs[2];                     // période tach lissée (us)

static void updateOutChannel(uint8_t ch)
{
    uint32_t nowUs = micros();
    uint32_t lastUs, perUs;
    noInterrupts();
    lastUs = g_inLastUs[ch];
    perUs  = g_inPeriodUs[ch];
    interrupts();

    // timeout d'absence de signal (ventilateur arrêté -> pas de pulses simulés)
    uint32_t stall = STALL_MIN_US;
    if (s_smUs[ch] && (s_smUs[ch] * STALL_MULT > stall)) stall = s_smUs[ch] * STALL_MULT;
    if ((nowUs - lastUs) > stall)
    {
        if (g_halfTicks[ch] != 0)
        {
            noInterrupts();
            g_halfTicks[ch] = 0;               // sortie repos (haute) : pas d'impulsion
            interrupts();
        }
        s_smUs[ch] = 0;
        return;
    }

    perUs = g_inPeriodUs[ch];
    if (perUs == 0) return;                    // aucun front valide encore

    // lissage exponentiel de la période mesurée (fonction de EMA_SHIFT)
    if (s_smUs[ch] == 0) s_smUs[ch] = perUs;
    else s_smUs[ch] = (uint32_t)((((uint64_t)s_smUs[ch] * ((1UL << EMA_SHIFT) - 1UL)) + perUs) >> EMA_SHIFT);

    uint64_t rq = g_ratioQ16[ch];
    if (rq == 0) rq = ratioToQ16(RATIO_DEFAULT_X100[ch]);

    // période de sortie (ticks) = inUs * TICK_PER_US * 65536 / ratio ; demi = /2
    uint64_t full = ((uint64_t)s_smUs[ch] * TICK_PER_US * 65536UL) / rq;
    uint32_t half = (uint32_t)(full >> 1);
    half = clampU32(half, 2UL, 0x00400000UL);  // 0,4 us .. 0,84 s

    noInterrupts();
    if (g_halfTicks[ch] == 0)
    {
        g_nextTicks[ch] = g_nowTicks + half;   // première échéance
        REG_WR(REG_FRC1_LOAD, 2);              // "réveille" le compteur endormi
        REG_WR(REG_FRC1_COUNT, 2);
    }
    g_halfTicks[ch] = half;
    interrupts();
}
// -----------------------------------------------------------------------------
//  FENETRE 1 S : RPM + SNAPSHOT POUR LE SERVEUR WEB
// -----------------------------------------------------------------------------
static void updateEverySecond(void)
{
    uint32_t edge[2];
    noInterrupts();
    edge[0] = g_inEdgeCnt[0]; g_inEdgeCnt[0] = 0;
    edge[1] = g_inEdgeCnt[1]; g_inEdgeCnt[1] = 0;
    interrupts();

    for (uint8_t i = 0; i < 2; i++)
    {
        // RPM réels = (impulsions/s) * 60 / impulsions par tour
        g_snap.rpmIn[i] = (uint32_t)((uint64_t)edge[i] * 60UL / PULSES_PER_REV);

        noInterrupts();
        g_snap.inUs[i] = g_inPeriodUs[i];
        if (g_halfTicks[i] != 0)
        {
            uint32_t half  = g_halfTicks[i];
            uint32_t outUs = half * 2UL / TICK_PER_US;           // période émise (us)
            g_snap.outUs[i]  = outUs;
            g_snap.rpmOut[i] = usToRpm(outUs);       // 30e6/outUs (2 impulsions/tour)
            g_snap.live[i]   = 1;
        }
        else
        {
            g_snap.outUs[i]  = 0;
            g_snap.rpmOut[i] = 0;
            g_snap.live[i]   = 0;
        }
        interrupts();
    }

    // corroboration : derniere presence de signal
    uint32_t nowUs = micros();
    for (uint8_t i = 0; i < 2; i++)
    {
        uint32_t lastUs;
        noInterrupts(); lastUs = g_inLastUs[i]; interrupts();
        if ((nowUs - lastUs) > STALL_MIN_US) g_snap.live[i] = 0;
    }
    g_snapUptime = (uint32_t)(millis() / 1000);
}

// -----------------------------------------------------------------------------
//  SETUP
// -----------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    Serial.println(F("[deye_fan] demarrage"));

    // --- broches -------------------------------------------------------------
    pinMode(PIN_IN0,  INPUT_PULLUP);
    pinMode(PIN_IN1,  INPUT_PULLUP);
    pinMode(PIN_OUT0, OUTPUT);   digitalWrite(PIN_OUT0, LOW);  // repos : sortie haute
    pinMode(PIN_OUT1, OUTPUT);   digitalWrite(PIN_OUT1, LOW);
    pinMode(PIN_LED,  OUTPUT);   digitalWrite(PIN_LED,  HIGH); // LED éteinte (active basse)

    // --- configuration persistée -----------------------------------------------
    EEPROM.begin(512);
    cfgLoad();
    for (uint8_t i = 0; i < 2; i++)
    {
        g_ratioX100[i] = g_cfg.ratioX100[i];
        g_ratioQ16[i]  = ratioToQ16(g_cfg.ratioX100[i]);
    }

    // --- temps réel -----------------------------------------------------------------
    initTimer();

    // --- interruptions d'entrée (le n° d'interrupt == n° GPIO sur ESP8266) --------------
    attachInterrupt(PIN_IN0, isr_tach0, RISING);
    attachInterrupt(PIN_IN1, isr_tach1, RISING);

    // --- réseau + web ------------------------------------------------------------------
    wifiInit();
    MDNS.begin("deye-fan");
    MDNS.addService("http", "tcp", 80);

    server.on("/",          handleRoot);
    server.on("/api/state", handleState);
    server.on("/api/set",   handleSet);
    server.on("/save",      HTTP_POST, handleSave);
    server.onNotFound([]() { server.send(404, "text/plain", "not found"); });
    server.begin();

    Serial.print(F("AP_URL=")); Serial.println(F("http://192.168.4.1"));
    Serial.print(F("STA_IP=")); Serial.println(WiFi.localIP());
    Serial.println(F("[deye_fan] pret"));
}

// -----------------------------------------------------------------------------
//  LOOP
// -----------------------------------------------------------------------------
void loop()
{
    static uint32_t tWindow = 0, tLedBlink = 0, tWiFiRetry = 0;
    static bool ledState = false;

    uint32_t now = millis();

    // --- voie par voie : timeout + calcul de la demi-période de sortie -------------
    updateOutChannel(0);
    updateOutChannel(1);

    // --- fenêtre 1 s : RPM réels/simulés => snapshot web ----------------------------
    if ((now - tWindow) >= 1000)
    {
        tWindow = now;
        updateEverySecond();
    }

    // --- LED : allumée = simulation active, clignote = attente de signal -------------
    bool anyLive = (g_snap.live[0] || g_snap.live[1]);
    if (anyLive)
    {
        digitalWrite(PIN_LED, LOW);              // allumée
    }
    else if ((now - tLedBlink) >= 500)
    {
        tLedBlink = now;
        ledState = !ledState;
        digitalWrite(PIN_LED, ledState ? LOW : HIGH);
    }

    // --- serveur web + mDNS -------------------------------------------------------------
    server.handleClient();
    MDNS.update();

    // --- reconnexion STA periodique (l'AP reste actif en permanence) --------------------
    if ((g_cfg.staSsid[0] != 0) && (WiFi.status() != WL_CONNECTED))
    {
        if ((now - tWiFiRetry) >= 20000)
        {
            tWiFiRetry = now;
            WiFi.disconnect();
            WiFi.begin(g_cfg.staSsid, g_cfg.staPw);
            Serial.println(F("[wifi] nouvel essai de connexion STA"));
        }
    }
}

// =============================================================================
//  NOTE D'UTILISATION
//  ----------------------------------------------------------------------------
//  1) Arduino IDE 1.8.x / 2.x ; carte : LOLIN(WEMOS) D1 mini ; noyau esp8266
//     par Espressif (2.7.4 ou plus récent).
//  2) AU DÉMARRAGE : le point d'accès "DeyeFanAP" (mot de passe "deye-fan")
//     est toujours présent. Ouvrir http://192.168.4.1 pour régler les ratios,
//     le WiFi STA (maison) et les identifiants AP. Réglages persistants.
//  3) Les ratios s'appliquent immédiatement sur le sliders, et définitivement
//     via le bouton "Enregistrer". Le changement de réseau WiFi ne s'applique
//     qu'après redémarrage.
//  4) LED : allumée = simulation RPM en cours ; clignote = attente de signal.
// =============================================================================