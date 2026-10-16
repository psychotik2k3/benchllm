/*
 *******************************************************************************
 * deye_fan.ino — Simulateur de signal tachymétrique (RPM) pour onduleur
 *                Deye SUN-8K-SG05LP1-EU-AM2-P + ventilateurs Noctua
 *
 * Carte cible : LOLIN (WEMOS) D1 mini — ESP8266 / ESP-12S
 *               (Arduino IDE : "LOLIN(WEMOS) D1 R2 & mini", CPU 80 MHz)
 * Core requis : esp8266 by ESP8266 Community (>= 3.x). Aucune bibliothèque
 *               tierce : seuls WiFi / WebServer / EEPROM du core sont utilisés.
 *
 * ---------------------------------------------------------------------------
 * RÔLE
 * ---------------------------------------------------------------------------
 * Les ventilateurs d'origine (NMB, ~5 600 tr/min en 92 mm et ~7 000 tr/min
 * en 60 mm) sont remplacés par des Noctua beaucoup plus lents
 * (NF-A9 FLX : 1 600 tr/min max ; NF-A6x25 FLX : 3 000 tr/min max).
 * Ce montage lit le signal tach de CHAQUE canal Noctua (1 seul ventilateur
 * "maître" par canal, voir le schéma), multiplie sa fréquence par un ratio
 * réglable par canal, et ré-émet vers l'onduleur un signal tach simulé,
 * au standard PC : collecteur ouvert, 2 impulsions par tour.
 *
 *   RPM_simulé = RPM_réel × ratio          (par canal, réglable par le web)
 *   RPM        = fréquence_tach [Hz] × 60 / 2 impulsions-par-tour
 *
 * ---------------------------------------------------------------------------
 * ARCHITECTURE D'ISOLATION TEMPORELLE (point critique du cahier des charges)
 * ---------------------------------------------------------------------------
 * Sur ESP8266, la pile WiFi peut masquer les interruptions classiques
 * (niveau 1) pendant des durées importantes et le serveur web monopolise
 * la boucle principale. Générer le tach de sortie avec delay(), millis(),
 * analogWrite()/tone() ou une interruption ordinaire produirait donc un
 * jitter visible côté onduleur dès qu'il y a du trafic réseau.
 *
 * Ce code sépare donc strictement trois plans :
 *
 *  1) GÉNÉRATION DE SORTIE — Timer1 matériel en interruption NMI
 *     (ETS_FRC_TIMER1_NMI_INTR_ATTACH). La NMI n'est masquable NI par la
 *     pile WiFi, NI par le code Arduino, NI par les sections critiques.
 *     Son handler (en IRAM, accès directs aux registres GPIO/Timer1,
 *     arithmétique entière uniquement, aucun appel en Flash) bascule les
 *     sorties au tick près (Timer1 à 5 MHz → résolution 0,2 µs) et se
 *     reprogramme tout seul pour la prochaine échéance. Il ne lit que deux
 *     mots 32 bits de configuration écrits atomiquement par le reste du
 *     programme. Le WiFi, le web et même un blocage total de loop()
 *     n'ont donc AUCUN effet mesurable sur le signal émis, tant que les
 *     périodes de consigne ne changent pas.
 *     Conséquence : ne JAMAIS appeler analogWrite(), tone() ou Servo dans
 *     ce sketch — ils réquisitionnent le Timer1/NMI pour leur propre usage.
 *
 *  2) CAPTURE D'ENTRÉE — interruptions GPIO ordinaires, réduites au strict
 *     minimum : lire le compteur de cycles CPU (rsr ccount) et pousser
 *     l'horodatage dans un anneau (ring buffer) en RAM. Aucun calcul.
 *     Si le WiFi retarde une interruption d'entrée, la mesure de CETTE
 *     période est entachée de jitter… mais elle ne pollue pas la sortie :
 *     voir plan 3.
 *
 *  3) ESTIMATION / PLL LOGICIELLE — dans loop(), jamais en interruption :
 *     médiane glissante + lissage exponentiel des périodes d'entrée,
 *     rejet des périodes aberrantes (rebonds, parasites), limitation de la
 *     vitesse de variation de la période de sortie (slew-rate ≈ 2 % par
 *     impulsion reçue). La sortie est un oscillateur libre discipliné
 *     lentement par l'entrée : le jitter d'entrée est filtré, il n'est
 *     jamais recopié tel quel sur la sortie en re-déclenchant les fronts.
 *
 * En résumé : l'entrée mesure, loop() estime, la NMI émet — seule la NMI
 * touche les broches de sortie, et rien ne peut la retarder.
 *
 * ---------------------------------------------------------------------------
 * BROCHAGE (Wemos D1 mini)
 * ---------------------------------------------------------------------------
 *   D1 (GPIO5)  Entrée tach canal 9 cm  (NF-A9 FLX maître)
 *   D2 (GPIO4)  Entrée tach canal 6 cm  (NF-A6x25 FLX maître)
 *   D5 (GPIO14) Sortie tach simulée → transistor NPN → onduleur (canal 9 cm)
 *   D6 (GPIO12) Sortie tach simulée → transistor NPN → onduleur (canal 6 cm)
 *   D4 (GPIO2)  LED bleue embarquée (active à l'état BAS) : état système
 *   5V / G      Alimentation 5 V issue du buck 12 V → 5 V (voir schéma)
 *
 * GPIO14/GPIO12 ne sont pas des broches de strap de démarrage : pendant le
 * reset, les transistors de sortie sont bloqués par leur résistance
 * base-émetteur de 10 kΩ, la ligne tach de l'onduleur reste donc relâchée
 * (niveau haut par le pull-up interne de l'onduleur) — état sûr au boot.
 *
 * ---------------------------------------------------------------------------
 * LED D'ÉTAT (D4, LED embarquée)
 * ---------------------------------------------------------------------------
 *   Allumée fixe      : simulation active sur les 2 canaux
 *   Clignotement lent (1 Hz, 50 %) : aucun signal tach reçu (attente)
 *   Double flash / s  : simulation active sur 1 seul canal sur 2
 *
 * ---------------------------------------------------------------------------
 * INTERFACE WEB (AP + STA simultanés, paramètres persistants en EEPROM)
 * ---------------------------------------------------------------------------
 *   AP par défaut : SSID "DeyeFan-xxxx" / mot de passe "deyefan123"
 *                   (à personnaliser dans la page), IP 192.168.4.1
 *   STA           : se connecte en plus au réseau WiFi renseigné, si présent.
 *   http://192.168.4.1/  (ou http://deye-fan.local/ , ou l'IP STA)
 *     - RPM réels / simulés des 2 canaux, rafraîchis toutes les 500 ms
 *     - réglage des 2 ratios (0,50 … 5,00) sans recompiler ni recâbler
 *     - configuration WiFi STA + AP, redémarrage à distance
 *
 * Ratios par défaut : 3,00 (canal 9 cm) et 2,30 (canal 6 cm). Ce sont des
 * points de départ (1 600 × 3 ≈ 4 800 ; 3 000 × 2,3 ≈ 6 900 tr/min simulés),
 * à ajuster selon le seuil d'alarme réel de VOTRE onduleur.
 *
 * SÉCURITÉ : n'intervenir que sur les connecteurs basse tension 12 V des
 * ventilateurs, onduleur hors tension et débranché. Ne jamais ouvrir le
 * compartiment puissance. Le remplacement des ventilateurs peut affecter
 * la garantie et le refroidissement : surveiller les températures internes
 * de l'onduleur après modification.
 *******************************************************************************
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <ets_sys.h>          // ETS_FRC_TIMER1_NMI_INTR_ATTACH

// =============================================================================
// Constantes
// =============================================================================

static const uint8_t  PIN_TACH_IN_9CM   = D1;   // GPIO5
static const uint8_t  PIN_TACH_IN_6CM   = D2;   // GPIO4
static const uint8_t  PIN_TACH_OUT_9CM  = D5;   // GPIO14
static const uint8_t  PIN_TACH_OUT_6CM  = D6;   // GPIO12
static const uint8_t  PIN_LED           = D4;   // GPIO2, LED active LOW

static const uint8_t  CH_9CM = 0;
static const uint8_t  CH_6CM = 1;
static const uint8_t  NB_CH  = 2;

static const uint8_t  PULSES_PER_REV = 2;       // standard PC / Noctua & NMB
static const float    CPU_MHZ        = 80.0f;   // carte réglée à 80 MHz

// Timer1 cadencé à 80 MHz / 16 = 5 MHz → 1 tick = 0,2 µs, compteur 23 bits.
static const float    TIMER_MHZ      = 5.0f;
static const uint32_t NMI_POLL_TICKS = 25000;   // réveil de contrôle : 5 ms
static const uint32_t NMI_MIN_TICKS  = 100;     // garde-fou anti-échéance passée

static const uint32_t INPUT_TIMEOUT_MS = 1200;  // sans impulsion → arrêt sortie
static const float    RATIO_MIN = 0.50f;
static const float    RATIO_MAX = 5.00f;

// =============================================================================
// Configuration persistante (EEPROM)
// =============================================================================

struct Config {
  uint32_t magic;              // 0xDE7EFAN0 (voir CONFIG_MAGIC)
  uint16_t version;
  float    ratio[NB_CH];       // multiplicateur par canal
  char     staSsid[33];
  char     staPass[65];
  char     apSsid[33];
  char     apPass[65];
  uint16_t crc;                // CRC16 calculé sur tout ce qui précède
};

static const uint32_t CONFIG_MAGIC   = 0xDE7EFA10UL;
static const uint16_t CONFIG_VERSION = 1;
static Config cfg;

static uint16_t crc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  while (len--) {
    crc ^= (uint16_t)(*data++) << 8;
    for (uint8_t i = 0; i < 8; i++)
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

static void configSetDefaults() {
  memset(&cfg, 0, sizeof(cfg));
  cfg.magic   = CONFIG_MAGIC;
  cfg.version = CONFIG_VERSION;
  cfg.ratio[CH_9CM] = 3.00f;   // 1 600 tr/min réels → ~4 800 simulés
  cfg.ratio[CH_6CM] = 2.30f;   // 3 000 tr/min réels → ~6 900 simulés
  String ap = "DeyeFan-" + String(ESP.getChipId(), HEX);
  ap.toCharArray(cfg.apSsid, sizeof(cfg.apSsid));
  strncpy(cfg.apPass, "deyefan123", sizeof(cfg.apPass) - 1);
  // staSsid / staPass laissés vides : pas de tentative STA tant que non renseignés.
}

static bool configLoad() {
  EEPROM.begin(1024);
  EEPROM.get(0, cfg);
  if (cfg.magic != CONFIG_MAGIC || cfg.version != CONFIG_VERSION) return false;
  uint16_t expected = crc16((const uint8_t *)&cfg, sizeof(cfg) - sizeof(cfg.crc));
  if (expected != cfg.crc) return false;
  for (uint8_t i = 0; i < NB_CH; i++)
    if (cfg.ratio[i] < RATIO_MIN || cfg.ratio[i] > RATIO_MAX) return false;
  cfg.staSsid[sizeof(cfg.staSsid) - 1] = '\0';
  cfg.staPass[sizeof(cfg.staPass) - 1] = '\0';
  cfg.apSsid[sizeof(cfg.apSsid) - 1]   = '\0';
  cfg.apPass[sizeof(cfg.apPass) - 1]   = '\0';
  return true;
}

static void configSave() {
  cfg.magic   = CONFIG_MAGIC;
  cfg.version = CONFIG_VERSION;
  cfg.crc = crc16((const uint8_t *)&cfg, sizeof(cfg) - sizeof(cfg.crc));
  EEPROM.put(0, cfg);
  EEPROM.commit();   // peut prendre quelques ms : sans effet sur la sortie (NMI)
}

// =============================================================================
// Plan 2 — Capture d'entrée : interruptions GPIO minimales
// =============================================================================

struct InputChannel {
  volatile uint32_t ring[16];     // horodatages ccount poussés par l'ISR
  volatile uint8_t  isrHead;      // écrit par l'ISR uniquement
  uint8_t           procTail;     // lu/écrit par loop() uniquement
  uint32_t lastTimestamp;         // dernier horodatage traité (ccount)
  bool     hasLast;
  float    periodUs;              // période filtrée entre impulsions (µs)
  float    rpm;                   // RPM réel estimé
  uint32_t lastPulseMs;           // millis() de la dernière impulsion valide
  uint32_t pulseCount;            // compteur total (diagnostic)
};

static InputChannel inCh[NB_CH];

static inline uint32_t IRAM_ATTR cycleCountNow() {
  uint32_t c;
  __asm__ volatile("rsr %0, ccount" : "=a"(c));
  return c;
}

// L'ISR ne fait QUE horodater et empiler. Aucun calcul, aucun appel en Flash.
static void IRAM_ATTR tachIsr9cm() {
  uint8_t next = (uint8_t)((inCh[CH_9CM].isrHead + 1) & 0x0F);
  inCh[CH_9CM].ring[inCh[CH_9CM].isrHead] = cycleCountNow();
  inCh[CH_9CM].isrHead = next;
}
static void IRAM_ATTR tachIsr6cm() {
  uint8_t next = (uint8_t)((inCh[CH_6CM].isrHead + 1) & 0x0F);
  inCh[CH_6CM].ring[inCh[CH_6CM].isrHead] = cycleCountNow();
  inCh[CH_6CM].isrHead = next;
}

static uint8_t suspectCount[NB_CH] = {0, 0};  // écarts répétés (par canal)

// Traitement d'une période brute mesurée : rejet d'aberrations + lissage.
// Appelé depuis loop() uniquement.
static void feedPeriod(InputChannel &ch, float periodUs) {
  // Fenêtre physique plausible : 200 … 30 000 tr/min
  // période = 30 000 000 / RPM µs (2 impulsions/tour) → 100 µs … 150 ms.
  if (periodUs < 100.0f || periodUs > 150000.0f) return;

  if (ch.periodUs <= 0.0f) {
    ch.periodUs = periodUs;                       // première mesure : directe
  } else {
    float ratioMeas = periodUs / ch.periodUs;
    if (ratioMeas < 0.70f || ratioMeas > 1.43f) {
      // Écart > ±30/43 % : parasite ou rebond probable → ignoré, sauf s'il
      // se répète (vrai changement brutal) — géré par le compteur ci-dessous.
      uint8_t idx = (&ch == &inCh[CH_9CM]) ? CH_9CM : CH_6CM;
      if (++suspectCount[idx] >= 4) { ch.periodUs = periodUs; suspectCount[idx] = 0; }
      return;
    }
    suspectCount[(&ch == &inCh[CH_9CM]) ? CH_9CM : CH_6CM] = 0;
    // Lissage exponentiel : constante ≈ 4 impulsions. C'est CE filtre qui
    // empêche le jitter de capture (WiFi) de se propager vers la sortie.
    ch.periodUs += 0.25f * (periodUs - ch.periodUs);
  }
  ch.rpm = 30000000.0f / ch.periodUs;             // 60e6 / (période × 2 imp/tr)
  ch.lastPulseMs = millis();
}

static void processInputChannel(uint8_t idx) {
  InputChannel &ch = inCh[idx];
  while (ch.procTail != ch.isrHead) {
    uint32_t ts = ch.ring[ch.procTail];
    ch.procTail = (uint8_t)((ch.procTail + 1) & 0x0F);
    if (ch.hasLast) {
      // Soustraction non signée : correcte malgré le débordement du ccount
      // (toutes les ~53 s à 80 MHz).
      float periodUs = (float)(uint32_t)(ts - ch.lastTimestamp) / CPU_MHZ;
      feedPeriod(ch, periodUs);
      ch.pulseCount++;
    }
    ch.lastTimestamp = ts;
    ch.hasLast = true;
  }
  if (ch.periodUs > 0.0f && (millis() - ch.lastPulseMs) > INPUT_TIMEOUT_MS) {
    ch.periodUs = 0.0f;                            // ventilateur arrêté
    ch.rpm = 0.0f;
    ch.hasLast = false;
  }
}

// =============================================================================
// Plan 1 — Génération de sortie : Timer1 en NMI, autonome et non masquable
// =============================================================================

// Consignes écrites par loop(), lues par la NMI. Sur Xtensa, un accès 32 bits
// aligné est atomique : aucun mutex nécessaire, aucun risque de valeur déchirée.
static volatile uint32_t outHalfTicks[NB_CH] = {0, 0};  // demi-période (ticks 5 MHz)
static volatile uint8_t  outEnabled[NB_CH]   = {0, 0};

// État privé de l'ISR NMI (touché par elle seule).
static uint32_t nmiRemain[NB_CH]   = {0, 0};
static uint32_t nmiProgrammed      = NMI_POLL_TICKS;
static bool     nmiRunning[NB_CH]  = {false, false};
static bool     nmiPinHigh[NB_CH]  = {false, false};
static const uint32_t OUT_MASK[NB_CH] = {(1UL << 14), (1UL << 12)};  // GPIO14, GPIO12

// Handler NMI : IRAM obligatoire (la Flash peut être injoignable pendant un
// accès EEPROM/WiFi). Registres directs uniquement, zéro appel externe.
static void IRAM_ATTR timer1NmiIsr() {
  // 1) Comptabiliser le temps écoulé depuis le dernier réveil.
  for (uint8_t i = 0; i < NB_CH; i++) {
    if (nmiRunning[i]) {
      nmiRemain[i] = (nmiRemain[i] > nmiProgrammed) ? (nmiRemain[i] - nmiProgrammed) : 0;
    }
  }

  // 2) Synchroniser l'état marche/arrêt avec les consignes de loop().
  for (uint8_t i = 0; i < NB_CH; i++) {
    if (outEnabled[i] && !nmiRunning[i]) {
      nmiRunning[i] = true;
      nmiPinHigh[i] = true;               // transistor passant → tach tiré au BAS
      GPOS = OUT_MASK[i];
      nmiRemain[i] = outHalfTicks[i];
    } else if (!outEnabled[i] && nmiRunning[i]) {
      nmiRunning[i] = false;
      nmiPinHigh[i] = false;              // transistor bloqué → ligne relâchée
      GPOC = OUT_MASK[i];
      nmiRemain[i] = 0;
    }
  }

  // 3) Basculer les canaux arrivés à échéance (rapport cyclique 50 %).
  for (uint8_t i = 0; i < NB_CH; i++) {
    if (nmiRunning[i] && nmiRemain[i] == 0) {
      nmiPinHigh[i] = !nmiPinHigh[i];
      if (nmiPinHigh[i]) GPOS = OUT_MASK[i]; else GPOC = OUT_MASK[i];
      uint32_t half = outHalfTicks[i];    // relit la consigne la plus fraîche
      nmiRemain[i] = (half >= NMI_MIN_TICKS) ? half : NMI_MIN_TICKS;
    }
  }

  // 4) Programmer le prochain réveil sur l'échéance la plus proche.
  uint32_t next = NMI_POLL_TICKS;         // aucun canal actif : simple contrôle
  for (uint8_t i = 0; i < NB_CH; i++)
    if (nmiRunning[i] && nmiRemain[i] < next) next = nmiRemain[i];
  if (next < NMI_MIN_TICKS) next = NMI_MIN_TICKS;
  nmiProgrammed = next;
  T1L = next & 0x7FFFFF;                  // rechargement Timer1 (mode single)
}

// Traduit l'estimation filtrée en consigne pour la NMI, avec limitation de
// pente : la période de sortie ne dérive que progressivement vers sa cible,
// ce qui lisse encore les à-coups résiduels de la mesure d'entrée.
static float appliedPeriodUs[NB_CH] = {0.0f, 0.0f};

static void updateOutputConsigne(uint8_t idx) {
  InputChannel &ch = inCh[idx];
  bool active = (ch.periodUs > 0.0f) &&
                ((millis() - ch.lastPulseMs) <= INPUT_TIMEOUT_MS);

  if (!active) {
    appliedPeriodUs[idx] = 0.0f;
    outEnabled[idx] = 0;
    return;
  }
  float targetPeriodUs = ch.periodUs / cfg.ratio[idx];

  if (appliedPeriodUs[idx] <= 0.0f) {
    appliedPeriodUs[idx] = targetPeriodUs;         // démarrage : immédiat
  } else {
    float maxStep = appliedPeriodUs[idx] * 0.02f;  // ±2 % par mise à jour
    float delta = targetPeriodUs - appliedPeriodUs[idx];
    if (delta >  maxStep) delta =  maxStep;
    if (delta < -maxStep) delta = -maxStep;
    appliedPeriodUs[idx] += delta;
  }

  // Demi-période en ticks de 5 MHz : période[µs] × 5 / 2.
  float half = appliedPeriodUs[idx] * (TIMER_MHZ / 2.0f);
  if (half < 2500.0f)    half = 2500.0f;           // plafond : 30 000 tr/min sim.
  if (half > 2500000.0f) half = 2500000.0f;        // plancher : ~60 tr/min sim.
  outHalfTicks[idx] = (uint32_t)half;              // écriture atomique → NMI
  outEnabled[idx] = 1;
}

static float simulatedRpm(uint8_t idx) {
  if (!outEnabled[idx] || inCh[idx].periodUs <= 0.0f) return 0.0f;
  return inCh[idx].rpm * cfg.ratio[idx];
}

// =============================================================================
// Plan 3 — Serveur web (tourne dans loop(), sans aucun rôle temporel critique)
// =============================================================================

static ESP8266WebServer server(80);
static uint32_t rebootAtMs = 0;   // >0 : redémarrage programmé (après réponse HTTP)

static const char INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Deye Fan Simulator</title>
<style>
body{font-family:system-ui,Arial,sans-serif;margin:16px;background:#111;color:#eee}
.card{background:#1d1d1f;border-radius:10px;padding:14px;margin:12px 0}
h1{font-size:20px}h2{font-size:16px;margin:4px 0 10px}
table{width:100%;border-collapse:collapse}td,th{padding:6px;text-align:left;border-bottom:1px solid #333}
input{width:100%;padding:8px;margin:4px 0 10px;background:#2a2a2d;color:#fff;border:1px solid #444;border-radius:6px;box-sizing:border-box}
button{background:#0a84ff;color:#fff;border:0;padding:10px 16px;border-radius:8px;cursor:pointer}
.big{font-size:22px;font-weight:700}.ok{color:#30d158}.warn{color:#ffd60a}
</style></head><body>
<h1>Simulateur tach Deye — Noctua</h1>

<div class="card"><h2>Temps réel</h2>
<table><tr><th>Canal</th><th>RPM réel</th><th>Ratio</th><th>RPM simulé</th><th>État</th></tr>
<tr><td>9 cm (NF-A9)</td><td class="big" id="r0">—</td><td id="x0">—</td><td class="big" id="s0">—</td><td id="e0">—</td></tr>
<tr><td>6 cm (NF-A6x25)</td><td class="big" id="r1">—</td><td id="x1">—</td><td class="big" id="s1">—</td><td id="e1">—</td></tr>
</table><p id="wifi">—</p></div>

<div class="card"><h2>Ratios de multiplication</h2>
<form method="POST" action="/api/config">
<label>Ratio canal 9 cm (0,50 – 5,00)</label><input name="ratio0" id="ratio0" type="number" step="0.05" min="0.5" max="5" required>
<label>Ratio canal 6 cm (0,50 – 5,00)</label><input name="ratio1" id="ratio1" type="number" step="0.05" min="0.5" max="5" required>
<button type="submit">Enregistrer les ratios</button></form></div>

<div class="card"><h2>WiFi</h2>
<form method="POST" action="/api/wifi">
<label>SSID réseau existant (STA, vide = inchangé/désactivé selon saisie)</label><input name="sta_ssid" id="sta_ssid" maxlength="32">
<label>Mot de passe STA (vide = inchangé)</label><input name="sta_pass" type="password" maxlength="64">
<label>SSID point d'accès (AP)</label><input name="ap_ssid" id="ap_ssid" maxlength="32">
<label>Mot de passe AP, 8 caractères min. (vide = inchangé)</label><input name="ap_pass" type="password" maxlength="64">
<button type="submit">Enregistrer et redémarrer</button></form>
<form method="POST" action="/api/reboot" onsubmit="return confirm('Redémarrer ?')">
<button type="submit">Redémarrer le module</button></form></div>

<script>
async function refresh(){
 try{
  let s=await (await fetch('/api/status')).json();
  for(let i=0;i<2;i++){document.getElementById('r'+i).textContent=Math.round(s.rpm_in[i]);
   document.getElementById('s'+i).textContent=Math.round(s.rpm_out[i]);
   document.getElementById('x'+i).textContent=s.ratio[i].toFixed(2);
   let e=document.getElementById('e'+i);e.textContent=s.active[i]?'Simulation active':'En attente de signal';
   e.className=s.active[i]?'ok':'warn';}
  document.getElementById('wifi').textContent='STA : '+(s.sta_connected?('connecté, IP '+s.sta_ip):'non connecté')+' — AP : '+s.ap_ip;
 }catch(e){}
}
async function loadCfg(){
 try{let c=await (await fetch('/api/config')).json();
  ratio0.value=c.ratio[0].toFixed(2);ratio1.value=c.ratio[1].toFixed(2);
  sta_ssid.value=c.sta_ssid;ap_ssid.value=c.ap_ssid;}catch(e){}
}
loadCfg();refresh();setInterval(refresh,500);
</script></body></html>)HTML";

static String jsonEscape(const char *s) {
  String out;
  while (*s) {
    if (*s == '"' || *s == '\\') out += '\\';
    out += *s++;
  }
  return out;
}

static void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }

static void handleStatus() {
  char buf[512];
  snprintf(buf, sizeof(buf),
    "{\"rpm_in\":[%.0f,%.0f],\"rpm_out\":[%.0f,%.0f],\"ratio\":[%.2f,%.2f],"
    "\"active\":[%s,%s],\"sta_connected\":%s,\"sta_ip\":\"%s\",\"ap_ip\":\"%s\","
    "\"pulses\":[%lu,%lu],\"uptime_s\":%lu}",
    inCh[0].rpm, inCh[1].rpm, simulatedRpm(0), simulatedRpm(1),
    cfg.ratio[0], cfg.ratio[1],
    outEnabled[0] ? "true" : "false", outEnabled[1] ? "true" : "false",
    (WiFi.status() == WL_CONNECTED) ? "true" : "false",
    WiFi.localIP().toString().c_str(), WiFi.softAPIP().toString().c_str(),
    (unsigned long)inCh[0].pulseCount, (unsigned long)inCh[1].pulseCount,
    (unsigned long)(millis() / 1000));
  server.send(200, "application/json", buf);
}

static void handleGetConfig() {
  String json = "{\"ratio\":[" + String(cfg.ratio[0], 2) + "," + String(cfg.ratio[1], 2) +
                "],\"sta_ssid\":\"" + jsonEscape(cfg.staSsid) +
                "\",\"ap_ssid\":\"" + jsonEscape(cfg.apSsid) + "\"}";
  server.send(200, "application/json", json);
}

static void handlePostConfig() {
  bool changed = false;
  for (uint8_t i = 0; i < NB_CH; i++) {
    String key = "ratio" + String(i);
    if (server.hasArg(key)) {
      float v = server.arg(key).toFloat();
      if (v >= RATIO_MIN && v <= RATIO_MAX) { cfg.ratio[i] = v; changed = true; }
    }
  }
  if (changed) configSave();
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

static void handlePostWifi() {
  if (server.hasArg("sta_ssid"))
    server.arg("sta_ssid").toCharArray(cfg.staSsid, sizeof(cfg.staSsid));
  if (server.hasArg("sta_pass") && server.arg("sta_pass").length() > 0)
    server.arg("sta_pass").toCharArray(cfg.staPass, sizeof(cfg.staPass));
  if (server.hasArg("ap_ssid") && server.arg("ap_ssid").length() > 0)
    server.arg("ap_ssid").toCharArray(cfg.apSsid, sizeof(cfg.apSsid));
  if (server.hasArg("ap_pass") && server.arg("ap_pass").length() >= 8)
    server.arg("ap_pass").toCharArray(cfg.apPass, sizeof(cfg.apPass));
  configSave();
  server.send(200, "text/html",
    "<meta charset=utf-8>Configuration enregistree. Redemarrage dans 1 s...");
  rebootAtMs = millis() + 1000;
}

static void handleReboot() {
  server.send(200, "text/plain", "Redemarrage...");
  rebootAtMs = millis() + 500;
}

// =============================================================================
// LED d'état (boucle principale uniquement — aucun rôle critique)
// =============================================================================

static void updateLed() {
  uint8_t nbActive = (outEnabled[0] ? 1 : 0) + (outEnabled[1] ? 1 : 0);
  bool ledOn;
  if (nbActive == 2) {
    ledOn = true;                                            // fixe
  } else if (nbActive == 0) {
    ledOn = (millis() % 1000) < 500;                         // clignotement lent
  } else {
    uint32_t ph = millis() % 1000;                           // double flash
    ledOn = (ph < 100) || (ph >= 250 && ph < 350);
  }
  digitalWrite(PIN_LED, ledOn ? LOW : HIGH);                 // LED active LOW
}

// =============================================================================
// Setup / Loop
// =============================================================================

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("[DeyeFan] Demarrage"));

  // --- Sorties au repos sûr avant tout : transistors bloqués, lignes relâchées.
  pinMode(PIN_TACH_OUT_9CM, OUTPUT); digitalWrite(PIN_TACH_OUT_9CM, LOW);
  pinMode(PIN_TACH_OUT_6CM, OUTPUT); digitalWrite(PIN_TACH_OUT_6CM, LOW);
  pinMode(PIN_LED, OUTPUT);          digitalWrite(PIN_LED, HIGH);  // LED éteinte

  // --- Entrées tach : pull-up interne en secours du pull-up externe de 10 kΩ
  //     (voir schéma). Le signal Noctua est un collecteur ouvert : c'est le
  //     pull-up côté Wemos qui définit le niveau haut à 3,3 V, jamais 12 V.
  pinMode(PIN_TACH_IN_9CM, INPUT_PULLUP);
  pinMode(PIN_TACH_IN_6CM, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_TACH_IN_9CM), tachIsr9cm, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_TACH_IN_6CM), tachIsr6cm, FALLING);

  // --- Configuration persistante.
  if (!configLoad()) {
    Serial.println(F("[DeyeFan] EEPROM vierge/invalide -> valeurs par defaut"));
    configSetDefaults();
    configSave();
  }
  Serial.printf("[DeyeFan] Ratios : 9cm=%.2f  6cm=%.2f\n", cfg.ratio[0], cfg.ratio[1]);

  // --- Démarrage du moteur de sortie NMI (Timer1, 5 MHz, mode single-shot).
  //     À partir d'ici, la génération tach est totalement autonome.
  //     Ne jamais utiliser analogWrite()/tone() ensuite (conflit Timer1).
  timer1_disable();
  ETS_FRC_TIMER1_INTR_ATTACH(NULL, NULL);
  ETS_FRC_TIMER1_NMI_INTR_ATTACH(timer1NmiIsr);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
  nmiProgrammed = NMI_POLL_TICKS;
  T1L = NMI_POLL_TICKS;

  // --- WiFi AP + STA simultanés. L'AP reste toujours disponible, même si le
  //     STA échoue : la configuration reste ainsi toujours accessible.
  WiFi.persistent(false);              // les paramètres sont gérés en EEPROM ici
  WiFi.mode(WIFI_AP_STA);
  WiFi.hostname("deye-fan");
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
                    IPAddress(255, 255, 255, 0));
  if (strlen(cfg.apPass) >= 8) WiFi.softAP(cfg.apSsid, cfg.apPass);
  else                         WiFi.softAP(cfg.apSsid);
  if (strlen(cfg.staSsid) > 0) {
    WiFi.begin(cfg.staSsid, cfg.staPass);
    Serial.printf("[DeyeFan] STA : connexion a '%s' (en arriere-plan)\n", cfg.staSsid);
  }
  Serial.printf("[DeyeFan] AP : '%s' -> http://192.168.4.1/\n", cfg.apSsid);

  if (MDNS.begin("deye-fan")) MDNS.addService("http", "tcp", 80);

  // --- Routes web.
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/wifi", HTTP_POST, handlePostWifi);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
  Serial.println(F("[DeyeFan] Serveur web actif"));
}

void loop() {
  // 1) Mesure + estimation (filtrée) + mise à jour des consignes NMI.
  processInputChannel(CH_9CM);
  processInputChannel(CH_6CM);
  updateOutputConsigne(CH_9CM);
  updateOutputConsigne(CH_6CM);

  // 2) Services non critiques : web, mDNS, LED, reconnexion STA.
  server.handleClient();
  MDNS.update();
  updateLed();

  static uint32_t lastStaRetry = 0;
  if (strlen(cfg.staSsid) > 0 && WiFi.status() != WL_CONNECTED &&
      (millis() - lastStaRetry) > 10000) {
    lastStaRetry = millis();
    WiFi.begin(cfg.staSsid, cfg.staPass);   // l'AP n'est pas interrompu (AP_STA)
  }

  // 3) Redémarrage différé demandé par l'interface web.
  if (rebootAtMs != 0 && (int32_t)(millis() - rebootAtMs) >= 0) {
    ESP.restart();
  }

  // Aucun delay() bloquant ici : la réactivité web reste bonne, et de toute
  // façon la précision du signal de sortie ne dépend pas de cette boucle.
  yield();
}
