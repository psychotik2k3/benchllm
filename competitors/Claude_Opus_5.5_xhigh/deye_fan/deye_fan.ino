/*
 * ============================================================================
 *  deye_fan.ino — Simulateur de signal tachymétrique pour onduleur Deye
 *                 SUN-8K-SG05LP1-EU-AM2-P équipé de ventilateurs Noctua
 * ============================================================================
 *
 *  Carte cible : LOLIN(WEMOS) D1 mini V2.3.0 (ESP-12S / ESP8266)
 *  Core        : ESP8266 Arduino core 3.1.x (gestionnaire de cartes :
 *                https://arduino.esp8266.com/stable/package_esp8266com_index.json)
 *  Réglages IDE recommandés (menu Outils) :
 *    - Type de carte  : "LOLIN(WEMOS) D1 R2 & mini"
 *    - CPU Frequency  : 160 MHz   (80 MHz fonctionne aussi, le code s'adapte)
 *    - Flash Size     : 4MB (FS:2MB OTA:~1019KB)   (mise à jour web possible)
 *  Aucune bibliothèque externe : uniquement les bibliothèques livrées avec le core.
 *
 * ----------------------------------------------------------------------------
 *  RÔLE
 * ----------------------------------------------------------------------------
 *  Les Noctua NF-A9 FLX (1600 tr/min) et NF-A6x25 FLX (3000 tr/min) tournent
 *  bien moins vite que les NMB d'origine 09225VE-12N-CU (4500 tr/min) et
 *  06025VE-12N-CL (7000 tr/min). Pour chaque canal, le Wemos :
 *    1. lit le signal tach d'un Noctua (collecteur ouvert, 2 impulsions/tour),
 *    2. mesure sa période,
 *    3. la divise par un ratio configurable (RPM simulés = RPM lus × ratio),
 *    4. réémet vers l'onduleur un signal tach carré (50 %) via un NPN en
 *       collecteur ouvert : la tension de pull-up de l'onduleur (3,3 / 5 / 12 V)
 *       n'a donc aucune importance.
 *  Si le Noctua s'arrête (panne, rotor bloqué, ventilateur non alimenté), la
 *  sortie s'arrête aussi : l'onduleur voit 0 tr/min et peut lever son alarme.
 *  Le système ne fabrique JAMAIS de RPM à partir de rien (comportement sûr).
 *
 * ----------------------------------------------------------------------------
 *  ARCHITECTURE LOGICIELLE — ISOLATION TEMPS RÉEL
 * ----------------------------------------------------------------------------
 *  L'ESP8266 n'a qu'un cœur. La pile WiFi (SDK) masque régulièrement les
 *  interruptions « normales » (niveau 1) pendant des dizaines, voire des
 *  centaines de µs ; loop(), le serveur web et les interruptions GPIO
 *  (attachInterrupt) subissent donc une gigue imprévisible. Aucun de ces
 *  mécanismes n'est utilisé pour le traitement du signal.
 *
 *  Toute la chaîne tach (lecture des entrées + conversion + génération des
 *  sorties) s'exécute dans UNE SEULE routine : la NMI (interruption non
 *  masquable) du timer matériel FRC1 (Timer1). Une NMI ne peut être masquée
 *  par aucun code logiciel (SDK WiFi compris) ; c'est le mécanisme qu'utilise
 *  Espressif pour son PWM matériel « insensible au WiFi ».
 *
 *    - Code de la NMI en IRAM (IRAM_ATTR), toutes les fonctions appelées sont
 *      forcées « inline » : aucun accès à la flash, donc aucun défaut de cache,
 *      et la NMI peut s'exécuter même quand le cache flash est désactivé
 *      (sauvegarde de la configuration, mise à jour du firmware).
 *    - Données de la NMI en DRAM (variables globales normales, pas PROGMEM).
 *    - Aucune division, aucun flottant, aucun appel de bibliothèque dans la
 *      NMI : uniquement additions, décalages et multiplications 32 bits
 *      (instruction matérielle MULL du LX106).
 *    - Base de temps : le compteur de cycles CPU CCOUNT (12,5 ns à 80 MHz,
 *      6,25 ns à 160 MHz), lu directement par instruction.
 *
 *  Entrées : la NMI échantillonne les deux GPIO d'entrée toutes les 50 µs
 *  (20 kHz) avec un anti-rebond numérique (3 échantillons identiques). Chaque
 *  début d'impulsion est horodaté avec l'instant PLANIFIÉ de l'échantillon
 *  (grille fixe en cycles CCOUNT), pas avec l'instant réel d'entrée en NMI :
 *  la gigue de latence ne pollue donc pas la mesure. La période est la moyenne
 *  glissante sur les 8 dernières périodes (4 tours : l'asymétrie entre les deux
 *  pôles du moteur s'annule ; erreur de quantification <= 50 µs / 8 sur une
 *  fenêtre d'environ 80 ms), puis lissée par un filtre IIR du 1er ordre
 *  (alpha = 1/4).
 *
 *  Sorties : chaque front de sortie est planifié à un instant absolu en cycles
 *  CPU. Le timer est armé quelques µs AVANT l'échéance (NMI_LEAD_US), puis la
 *  NMI attend activement (quelques µs) l'instant exact avant d'écrire le
 *  registre GPIO. Résultat : la gigue de latence d'entrée en NMI est absorbée
 *  et chaque front tombe au cycle près. La nouvelle période (entrée × 1/ratio)
 *  n'est appliquée qu'en début de cycle : jamais d'impulsion tronquée.
 *
 *  Échanges avec le reste du programme : uniquement des mots 32 bits alignés
 *  (lecture/écriture atomiques sur LX106), sans verrou ni section critique :
 *    loop()  -> NMI : facteur de conversion par canal (ratio en virgule fixe)
 *    NMI -> loop()  : périodes mesurées/générées et compteurs de diagnostic
 *  Le serveur web peut être saturé ou bloqué : la NMI ne l'attend jamais.
 *  L'interface affiche le « retard max d'un front » mesuré dans la NMI elle-même,
 *  ce qui permet de vérifier l'isolation en conditions réelles.
 *
 *  ATTENTION : la NMI du Timer1 est exclusive. Ne pas utiliser analogWrite(),
 *  tone(), Servo ou startWaveform() dans ce programme : ils utilisent le même
 *  timer et remplaceraient le moteur tach.
 *
 * ----------------------------------------------------------------------------
 *  BROCHAGE (voir SCHEMA.md pour le schéma complet)
 * ----------------------------------------------------------------------------
 *    D5 / GPIO14 : entrée tach Noctua 9 cm   (via étage NPN inverseur)
 *    D6 / GPIO12 : entrée tach Noctua 6 cm   (via étage NPN inverseur)
 *    D1 / GPIO5  : sortie tach simulée 9 cm  (vers base NPN collecteur ouvert)
 *    D2 / GPIO4  : sortie tach simulée 6 cm  (vers base NPN collecteur ouvert)
 *    D4 / GPIO2  : LED d'état (LED bleue intégrée, active à l'état bas)
 *    5V          : alimentation 5 V issue du convertisseur DC-DC
 *
 *  LED d'état :
 *    allumée fixe         : simulation active sur les deux canaux
 *    clignotement 1 Hz    : simulation active sur un seul canal (l'autre attend)
 *    flash bref / 2 s     : aucun signal, attente des ventilateurs
 *
 * ----------------------------------------------------------------------------
 *  RÉSEAU
 * ----------------------------------------------------------------------------
 *  WiFi AP + STA simultanés, paramètres persistants (EEPROM émulée en flash).
 *    - Point d'accès par défaut : SSID "DeyeFan-XXXXXX", mot de passe
 *      "deyefan-setup", interface sur http://192.168.4.1/ (portail captif).
 *    - Réseau domestique (STA) configurable depuis l'interface ; accessible
 *      ensuite via son IP ou http://deye-fan.local/ (mDNS).
 *    - Mise à jour du firmware par le navigateur : http://<ip>/update
 *      (utilisateur "admin", mot de passe = mot de passe admin, ou à défaut
 *      celui du point d'accès).
 *  Console série 115200 bauds : taper "help".
 * ============================================================================
 */

#if !defined(ESP8266)
#error "Ce programme cible exclusivement l'ESP8266 (LOLIN(WEMOS) D1 mini)."
#endif

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <ets_sys.h>

#define FW_VERSION "1.0.0"

// ============================================================================
//  PARAMÈTRES MATÉRIELS
// ============================================================================

static constexpr uint8_t NUM_CH = 2;
static constexpr uint8_t CH_9CM = 0;
static constexpr uint8_t CH_6CM = 1;
static const char *const CH_NAME[NUM_CH] = { "9 cm", "6 cm" };

static constexpr uint8_t PIN_IN[NUM_CH]  = { 14 /* D5 */, 12 /* D6 */ };
static constexpr uint8_t PIN_OUT[NUM_CH] = {  5 /* D1 */,  4 /* D2 */ };
static constexpr uint8_t PIN_LED = 2;  // D4, LED intégrée, active à l'état bas

// Niveau lu sur le GPIO d'entrée pendant que le Noctua tire sa ligne tach à la
// masse. L'étage d'entrée NPN du schéma inverse : tach basse -> NPN bloqué ->
// GPIO haut. Mettre 0 si le tach est câblé directement (sans étage NPN).
static constexpr uint8_t IN_PULSE_LEVEL = 1;

// Niveau GPIO qui sature le NPN de sortie, donc tire la ligne tach de
// l'onduleur à la masse.
static constexpr bool OUT_ACTIVE_HIGH = true;

// Impulsions par tour : Noctua = 2 (documentation Noctua), NMB = 2.
static constexpr uint32_t PPR_IN  = 2;
static constexpr uint32_t PPR_OUT = 2;

// Ratios par défaut = vitesse nominale NMB d'origine / vitesse max Noctua.
// 7000 tr/min : fiche NMB 06025VE-12N. 4500 tr/min : estimation (classe de
// vitesse « N » de NMB, cf. 09225VA-12N) car NMB ne publie pas la valeur du
// 09225VE-12N ; à ajuster depuis l'interface web si besoin.
static constexpr float DEFAULT_RATIO[NUM_CH] = {
  4500.0f / 1600.0f,  // 9 cm : NMB 09225VE-12N-CU ~4500 tr/min / NF-A9 FLX 1600 tr/min
  7000.0f / 3000.0f   // 6 cm : NMB 06025VE-12N-CL 7000 tr/min / NF-A6x25 FLX 3000 tr/min
};
static constexpr float RATIO_MIN = 0.25f;
static constexpr float RATIO_MAX = 8.0f;

// ============================================================================
//  PARAMÈTRES DU MOTEUR TEMPS RÉEL (NMI)
// ============================================================================

static constexpr uint32_t POLL_US           = 50;    // échantillonnage des entrées (20 kHz)
static constexpr uint32_t NMI_LEAD_US       = 8;     // réveil anticipé avant un front de sortie
static constexpr uint32_t MIN_ARM_US        = 2;     // délai minimal programmé dans le timer
static constexpr uint8_t  DEBOUNCE_SAMPLES  = 3;     // 3 x 50 µs identiques pour valider un niveau
static constexpr uint32_t IN_MIN_EDGE_US    = 2000;  // fronts plus rapprochés = parasite (> 15000 tr/min)
static constexpr uint32_t IN_TIMEOUT_MS     = 500;   // plus d'impulsion depuis 500 ms -> signal perdu
static constexpr uint8_t  FILTER_SHIFT      = 2;     // filtre IIR : alpha = 1/2^FILTER_SHIFT
static constexpr uint32_t OUT_MIN_PERIOD_US = 500;   // période de sortie minimale (2 kHz)
static constexpr uint32_t LATE_THRESHOLD_US = 1;     // un front est compté « en retard » au-delà

static constexpr uint8_t RING_SIZE = 16;             // horodatages mémorisés (puissance de 2)
static constexpr uint8_t RING_MASK = RING_SIZE - 1;
static constexpr uint8_t AVG_MAX_SHIFT = 3;          // moyenne sur 2^3 = 8 périodes

// ============================================================================
//  PARAMÈTRES RÉSEAU
// ============================================================================

static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_MASK(255, 255, 255, 0);
static constexpr uint8_t  AP_CHANNEL = 6;
static constexpr uint32_t STA_CONNECT_TIMEOUT_MS = 20000;  // abandon d'une tentative STA
static constexpr uint32_t STA_RETRY_MS = 60000;            // pause entre deux tentatives
static const char DEFAULT_AP_PASS[] = "deyefan-setup";
static const char DEFAULT_HOSTNAME[] = "deye-fan";
static const char ADMIN_USER[] = "admin";

// ============================================================================
//  MOTEUR TEMPS RÉEL — DONNÉES (DRAM)
// ============================================================================

struct TachInput {
  uint32_t ring[RING_SIZE];  // horodatages (CCOUNT) des derniers débuts d'impulsion
  uint32_t lastEdge;         // dernier horodatage accepté
  uint32_t filtQ4;           // période filtrée, en cycles CPU x 16
  uint32_t mask;             // masque du GPIO
  uint8_t  wr;               // index d'écriture dans ring
  uint8_t  count;            // nombre d'horodatages valides (max 2^AVG_MAX_SHIFT + 1)
  uint8_t  level;            // niveau après anti-rebond
  uint8_t  deb;              // compteur d'anti-rebond
  bool     valid;            // période disponible
};

struct TachOutput {
  uint32_t nextEdge;  // instant absolu (CCOUNT) du prochain front
  uint32_t period;    // période du cycle en cours (cycles CPU)
  uint32_t lowTime;   // durée de la phase « ligne tach à la masse »
  uint32_t mask;      // masque du GPIO
  bool     running;
  bool     low;       // true : ligne de l'onduleur actuellement tirée à la masse
};

struct Engine {
  TachInput  in[NUM_CH];
  TachOutput out[NUM_CH];
  uint32_t nextPoll;
  uint32_t pollCcy, leadCcy, minArmCcy, minEdgeCcy, timeoutCcy, outMinCcy, lateCcy;
  uint32_t cpuMHz;
  uint8_t  timerShift;  // FRC1 compte à 80 MHz : décalage CCOUNT -> ticks timer
};

// Mots partagés avec loop(). Tous sur 32 bits, alignés : accès atomiques.
struct Shared {
  volatile uint32_t factorQ16[NUM_CH];  // écrit par loop() : période_sortie = période_entrée x f / 65536
  volatile uint32_t inPeriod[NUM_CH];   // période d'entrée filtrée (cycles), 0 = pas de signal
  volatile uint32_t outPeriod[NUM_CH];  // période de sortie générée (cycles), 0 = sortie arrêtée
  volatile uint32_t edges[NUM_CH];      // impulsions d'entrée acceptées
  volatile uint32_t pulses[NUM_CH];     // impulsions de sortie émises
  volatile uint32_t glitches[NUM_CH];   // impulsions d'entrée rejetées (parasites)
  volatile uint32_t nmiCount;           // nombre d'exécutions de la NMI
  volatile uint32_t nmiMaxCcy;          // durée max d'une NMI (cycles)
  volatile uint32_t lateMaxCcy;         // retard max d'un front de sortie (cycles)
  volatile uint32_t lateCount;          // fronts de sortie en retard de plus de LATE_THRESHOLD_US
  volatile uint32_t diagReset;          // écrit à 1 par loop(), remis à 0 par la NMI
};

static Engine E;
static Shared S;

// ============================================================================
//  CONFIGURATION PERSISTANTE ET ÉTAT WIFI — TYPES
//  (tous les types sont déclarés avant la première fonction : le préprocesseur
//  Arduino insère ses prototypes automatiques juste avant celle-ci)
// ============================================================================

static constexpr uint32_t CFG_MAGIC   = 0x4E414644;  // "DFAN"
static constexpr uint16_t CFG_VERSION = 1;
static constexpr size_t   EEPROM_SIZE = 512;

struct Config {
  uint32_t magic;
  uint16_t version;
  uint16_t length;
  float    ratio[NUM_CH];
  char     staSsid[33];
  char     staPass[65];
  char     apSsid[33];
  char     apPass[65];
  char     adminPass[33];
  char     hostname[33];
  uint32_t crc;
};
static_assert(sizeof(Config) <= EEPROM_SIZE, "Config trop grande pour l'EEPROM");

static Config cfg;

enum class StaState : uint8_t { Disabled, Connecting, Connected, Waiting };

// ============================================================================
//  MOTEUR TEMPS RÉEL — CODE NMI (IRAM, tout est inline)
// ============================================================================

#define NMI_INLINE static inline __attribute__((always_inline))

NMI_INLINE uint32_t cycles() {
  uint32_t c;
  __asm__ __volatile__("esync; rsr %0, ccount" : "=a"(c));
  return c;
}

NMI_INLINE void outAssert(uint32_t mask) {   // NPN saturé : ligne tach onduleur à la masse
  if (OUT_ACTIVE_HIGH) GPOS = mask; else GPOC = mask;
}

NMI_INLINE void outRelease(uint32_t mask) {  // NPN bloqué : ligne libérée (pull-up onduleur)
  if (OUT_ACTIVE_HIGH) GPOC = mask; else GPOS = mask;
}

// (x * f) >> 16 sans débordement et sans arithmétique 64 bits (qui appellerait
// une routine de la libgcc potentiellement en flash). Exact tant que le
// résultat tient sur 32 bits : x <= 2^27 et f <= 2^18 ici.
NMI_INLINE uint32_t mulQ16(uint32_t x, uint32_t f) {
  const uint32_t xh = x >> 16, xl = x & 0xFFFFu;
  const uint32_t fh = f >> 16, fl = f & 0xFFFFu;
  return ((xh * fh) << 16) + xh * fl + xl * fh + ((xl * fl) >> 16);
}

// Période de sortie à appliquer au prochain cycle du canal c (0 = arrêt).
NMI_INLINE uint32_t outPeriodFor(uint8_t c) {
  const TachInput &i = E.in[c];
  const uint32_t f = S.factorQ16[c];
  if (!i.valid || f == 0) return 0;
  uint32_t p = mulQ16(i.filtQ4 >> 4, f);
  if (p < E.outMinCcy) p = E.outMinCcy;
  return p;
}

// Début d'une impulsion tach Noctua détecté à l'instant t (CCOUNT).
NMI_INLINE void onTachPulse(uint8_t c, uint32_t t) {
  TachInput &i = E.in[c];
  if (i.count && (uint32_t)(t - i.lastEdge) < E.minEdgeCcy) {
    S.glitches[c]++;
    return;
  }
  i.ring[i.wr & RING_MASK] = t;
  i.wr++;
  i.lastEdge = t;
  if (i.count < (1u << AVG_MAX_SHIFT) + 1) i.count++;
  S.edges[c]++;
  if (i.count < 2) return;

  // Moyenne sur k = 1, 2, 4 ou 8 périodes (puissance de 2 => simple décalage).
  const uint8_t n = i.count - 1;
  const uint8_t sh = (n >= 8) ? 3 : (n >= 4) ? 2 : (n >= 2) ? 1 : 0;
  const uint32_t older = i.ring[(uint8_t)(i.wr - 1 - (1u << sh)) & RING_MASK];
  const uint32_t raw = (t - older) >> sh;

  if (!i.valid) {
    i.filtQ4 = raw << 4;
    i.valid = true;
  } else {
    i.filtQ4 += (uint32_t)((int32_t)((raw << 4) - i.filtQ4) >> FILTER_SHIFT);
  }
  S.inPeriod[c] = i.filtQ4 >> 4;
}

// Échantillonnage des entrées, détection de perte de signal, démarrage des sorties.
NMI_INLINE void servicePoll(uint32_t now) {
  const uint32_t gpi = GPI;
  // Horodatage = instant d'échantillonnage planifié (grille fixe de 50 µs) et
  // non l'instant réel d'entrée en NMI : la gigue de latence n'entre pas dans
  // la mesure de période.
  const uint32_t ts = E.nextPoll;
  for (uint8_t c = 0; c < NUM_CH; c++) {
    TachInput &i = E.in[c];
    const uint8_t lvl = (gpi & i.mask) ? 1 : 0;
    if (lvl != i.level) {
      if (++i.deb >= DEBOUNCE_SAMPLES) {
        i.level = lvl;
        i.deb = 0;
        if (lvl == IN_PULSE_LEVEL) onTachPulse(c, ts);
      }
    } else {
      i.deb = 0;
    }

    if (i.count && (uint32_t)(ts - i.lastEdge) > E.timeoutCcy) {
      i.count = 0;
      i.valid = false;
      i.filtQ4 = 0;
      S.inPeriod[c] = 0;
    }

    TachOutput &o = E.out[c];
    if (!o.running) {
      const uint32_t p = outPeriodFor(c);
      if (p) {
        o.period = p;
        o.lowTime = p >> 1;
        o.low = false;
        o.nextEdge = now + E.leadCcy + E.minArmCcy;
        o.running = true;
        S.outPeriod[c] = p;
      }
    }
  }
  E.nextPoll += E.pollCcy;
  if ((int32_t)(E.nextPoll - now) <= 0) E.nextPoll = now + E.pollCcy;  // NMI en retard : recalage
}

// Front de sortie du canal c : attente active jusqu'au cycle exact, puis bascule.
NMI_INLINE void serviceEdge(uint8_t c) {
  TachOutput &o = E.out[c];
  const uint32_t target = o.nextEdge;
  uint32_t t;
  do {
    t = cycles();
  } while ((int32_t)(target - t) > 0);

  if (o.low) outRelease(o.mask); else outAssert(o.mask);

  const uint32_t late = t - target;
  if (late > S.lateMaxCcy) S.lateMaxCcy = late;
  if (late > E.lateCcy) S.lateCount++;

  if (o.low) {
    // Fin de la phase basse : la phase haute termine le cycle en cours, puis le
    // cycle suivant utilisera la période recalculée (changement sans à-coup).
    o.low = false;
    o.nextEdge = target + (o.period - o.lowTime);
    const uint32_t p = outPeriodFor(c);
    if (p) {
      o.period = p;
      o.lowTime = p >> 1;
      S.outPeriod[c] = p;
    } else {
      o.running = false;  // plus de signal d'entrée : ligne laissée libérée
      S.outPeriod[c] = 0;
    }
  } else {
    o.low = true;
    o.nextEdge = target + o.lowTime;
    S.pulses[c]++;
  }
  // Si la NMI a été bloquée très longtemps, on repart de maintenant plutôt que
  // d'émettre une rafale de fronts pour « rattraper » le retard.
  if ((int32_t)(o.nextEdge - t) < (int32_t)E.minArmCcy) o.nextEdge = t + E.minArmCcy;
}

// Routine NMI du Timer1 : traite tous les évènements imminents puis réarme le
// timer pour le prochain (front de sortie - avance, ou échantillonnage).
static void IRAM_ATTR tachNmi() {
  const uint32_t t0 = cycles();
  if (S.diagReset) {
    S.nmiMaxCcy = 0;
    S.lateMaxCcy = 0;
    S.lateCount = 0;
    S.diagReset = 0;
  }

  for (uint8_t guard = 0; guard < 8; guard++) {
    const uint32_t now = cycles();
    int8_t ch = -1;
    int32_t dEdge = INT32_MAX;
    for (uint8_t c = 0; c < NUM_CH; c++) {
      if (E.out[c].running) {
        const int32_t d = (int32_t)(E.out[c].nextEdge - now);
        if (d < dEdge) { dEdge = d; ch = (int8_t)c; }
      }
    }
    if (ch >= 0 && dEdge <= (int32_t)E.leadCcy) { serviceEdge((uint8_t)ch); continue; }
    if ((int32_t)(E.nextPoll - now) <= (int32_t)E.minArmCcy) { servicePoll(now); continue; }
    break;
  }

  const uint32_t now = cycles();
  int32_t dt = (int32_t)(E.nextPoll - now);
  for (uint8_t c = 0; c < NUM_CH; c++) {
    if (E.out[c].running) {
      const int32_t d = (int32_t)(E.out[c].nextEdge - now) - (int32_t)E.leadCcy;
      if (d < dt) dt = d;
    }
  }
  if (dt < (int32_t)E.minArmCcy) dt = (int32_t)E.minArmCcy;
  T1L = ((uint32_t)dt >> E.timerShift) & 0x7FFFFFu;  // l'IT front du timer reste activée

  S.nmiCount++;
  const uint32_t dur = cycles() - t0;
  if (dur > S.nmiMaxCcy) S.nmiMaxCcy = dur;
}

// ============================================================================
//  MOTEUR TEMPS RÉEL — DÉMARRAGE / ARRÊT (contexte normal)
// ============================================================================

static void engineStart() {
  const uint32_t mhz = ESP.getCpuFreqMHz();  // 80 ou 160
  E.cpuMHz      = mhz;
  E.timerShift  = (mhz >= 160) ? 1 : 0;
  E.pollCcy     = POLL_US * mhz;
  E.leadCcy     = NMI_LEAD_US * mhz;
  E.minArmCcy   = MIN_ARM_US * mhz;
  E.minEdgeCcy  = IN_MIN_EDGE_US * mhz;
  E.timeoutCcy  = IN_TIMEOUT_MS * 1000u * mhz;
  E.outMinCcy   = OUT_MIN_PERIOD_US * mhz;
  E.lateCcy     = LATE_THRESHOLD_US * mhz;

  const uint32_t gpi = GPI;
  for (uint8_t c = 0; c < NUM_CH; c++) {
    E.in[c] = TachInput{};
    E.in[c].mask  = 1u << PIN_IN[c];
    E.in[c].level = (gpi & E.in[c].mask) ? 1 : 0;  // pas de faux front au démarrage
    E.out[c] = TachOutput{};
    E.out[c].mask = 1u << PIN_OUT[c];
    outRelease(E.out[c].mask);
  }
  E.nextPoll = cycles() + E.pollCcy;

  // Même séquence que le générateur de formes d'onde du core ESP8266 :
  // Timer1 en mode « un coup », IT sur front, routée vers la NMI.
  timer1_disable();
  ETS_FRC_TIMER1_INTR_ATTACH(NULL, NULL);
  ETS_FRC_TIMER1_NMI_INTR_ATTACH(tachNmi);
  timer1_enable(TIM_DIV1, TIM_EDGE, TIM_SINGLE);
  timer1_write(E.pollCcy >> E.timerShift);  // active aussi l'IT front (TEIE1)
}

static void engineStop() {
  timer1_disable();
  ETS_FRC_TIMER1_NMI_INTR_ATTACH(NULL);
  for (uint8_t c = 0; c < NUM_CH; c++) outRelease(E.out[c].mask);
}

static uint32_t factorFromRatio(float r) {
  if (!(r >= RATIO_MIN && r <= RATIO_MAX)) return 0;
  return (uint32_t)lroundf(65536.0f * (float)PPR_IN / (r * (float)PPR_OUT));
}

static float rpmFromPeriod(uint32_t periodCcy, uint32_t ppr) {
  if (!periodCcy) return 0.0f;
  return 60.0f * (float)E.cpuMHz * 1e6f / ((float)periodCcy * (float)ppr);
}

static float hzFromPeriod(uint32_t periodCcy) {
  if (!periodCcy) return 0.0f;
  return (float)E.cpuMHz * 1e6f / (float)periodCcy;
}

static float usFromCcy(uint32_t ccy) {
  return (float)ccy / (float)E.cpuMHz;
}

// ============================================================================
//  CONFIGURATION PERSISTANTE (EEPROM émulée en flash, avec CRC)
// ============================================================================

static uint32_t crc32(const uint8_t *p, size_t n) {
  uint32_t c = 0xFFFFFFFFu;
  while (n--) {
    c ^= *p++;
    for (uint8_t k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
  }
  return ~c;
}

static uint32_t configCrc(const Config &c) {
  return crc32(reinterpret_cast<const uint8_t *>(&c), offsetof(Config, crc));
}

static void copyStr(char *dst, size_t cap, const char *src) {
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = '\0';
}

static void configDefaults(Config &c) {
  memset(&c, 0, sizeof(c));
  c.magic = CFG_MAGIC;
  c.version = CFG_VERSION;
  c.length = sizeof(Config);
  for (uint8_t i = 0; i < NUM_CH; i++) c.ratio[i] = DEFAULT_RATIO[i];
  snprintf(c.apSsid, sizeof(c.apSsid), "DeyeFan-%06X", (unsigned)ESP.getChipId());
  copyStr(c.apPass, sizeof(c.apPass), DEFAULT_AP_PASS);
  copyStr(c.hostname, sizeof(c.hostname), DEFAULT_HOSTNAME);
}

static void configSanitize(Config &c) {
  c.staSsid[sizeof(c.staSsid) - 1] = '\0';
  c.staPass[sizeof(c.staPass) - 1] = '\0';
  c.apSsid[sizeof(c.apSsid) - 1] = '\0';
  c.apPass[sizeof(c.apPass) - 1] = '\0';
  c.adminPass[sizeof(c.adminPass) - 1] = '\0';
  c.hostname[sizeof(c.hostname) - 1] = '\0';
  for (uint8_t i = 0; i < NUM_CH; i++) {
    if (!(c.ratio[i] >= RATIO_MIN && c.ratio[i] <= RATIO_MAX)) c.ratio[i] = DEFAULT_RATIO[i];
  }
  if (!c.apSsid[0]) snprintf(c.apSsid, sizeof(c.apSsid), "DeyeFan-%06X", (unsigned)ESP.getChipId());
  if (strlen(c.apPass) < 8) copyStr(c.apPass, sizeof(c.apPass), DEFAULT_AP_PASS);
  if (!c.hostname[0]) copyStr(c.hostname, sizeof(c.hostname), DEFAULT_HOSTNAME);
}

static bool configLoad() {
  EEPROM.get(0, cfg);
  const bool ok = cfg.magic == CFG_MAGIC && cfg.version == CFG_VERSION &&
                  cfg.length == sizeof(Config) && cfg.crc == configCrc(cfg);
  if (!ok) configDefaults(cfg);
  configSanitize(cfg);
  return ok;
}

static void configSave() {
  cfg.magic = CFG_MAGIC;
  cfg.version = CFG_VERSION;
  cfg.length = sizeof(Config);
  cfg.crc = configCrc(cfg);
  EEPROM.put(0, cfg);
  EEPROM.commit();  // la NMI (IRAM/DRAM) continue pendant l'écriture flash
}

static void applyRatios() {
  for (uint8_t c = 0; c < NUM_CH; c++) S.factorQ16[c] = factorFromRatio(cfg.ratio[c]);
}

// ============================================================================
//  WIFI (AP + STA) — machine d'états STA non bloquante
// ============================================================================

static ESP8266WebServer server(80);
static ESP8266HTTPUpdateServer httpUpdater;
static DNSServer dns;
static StaState staState = StaState::Disabled;
static uint32_t staT0 = 0;
static uint32_t rebootAt = 0;     // redémarrage différé (0 = aucun)
static uint32_t wifiApplyAt = 0;  // reconfiguration WiFi différée (0 = aucune)

static const char *staStateName() {
  switch (staState) {
    case StaState::Disabled:   return "désactivé";
    case StaState::Connecting: return "connexion…";
    case StaState::Connected:  return "connecté";
    case StaState::Waiting:    return "échec, nouvel essai plus tard";
  }
  return "?";
}

static const char *updatePassword() {
  return cfg.adminPass[0] ? cfg.adminPass : cfg.apPass;
}

static void staBegin() {
  if (!cfg.staSsid[0]) {
    WiFi.disconnect(false);
    staState = StaState::Disabled;
    return;
  }
  WiFi.begin(cfg.staSsid, cfg.staPass[0] ? cfg.staPass : nullptr);
  staState = StaState::Connecting;
  staT0 = millis();
  Serial.printf("[WiFi] Connexion STA a \"%s\"...\n", cfg.staSsid);
}

// Tant que la STA cherche un réseau, l'ESP8266 change de canal et perturbe le
// point d'accès. On limite donc les tentatives, et on n'en relance pas tant
// qu'un client est connecté à l'AP (typiquement pendant la configuration).
static void staService() {
  const uint32_t now = millis();
  const bool up = WiFi.status() == WL_CONNECTED;
  switch (staState) {
    case StaState::Disabled:
      break;
    case StaState::Connecting:
      if (up) {
        staState = StaState::Connected;
        Serial.printf("[WiFi] STA connectee, IP %s, canal %d\n",
                      WiFi.localIP().toString().c_str(), WiFi.channel());
      } else if (now - staT0 > STA_CONNECT_TIMEOUT_MS) {
        WiFi.disconnect(false);
        staState = StaState::Waiting;
        staT0 = now;
        Serial.println(F("[WiFi] Echec STA, nouvel essai dans 60 s"));
      }
      break;
    case StaState::Connected:
      if (!up) {
        Serial.println(F("[WiFi] STA perdue"));
        staBegin();
      }
      break;
    case StaState::Waiting:
      if (now - staT0 > STA_RETRY_MS && WiFi.softAPgetStationNum() == 0) staBegin();
      break;
  }
}

static void wifiStart() {
  WiFi.persistent(false);         // le SDK n'écrit pas sa propre config en flash
  WiFi.setAutoReconnect(false);   // reconnexions gérées par staService()
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.hostname(cfg.hostname);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);
  WiFi.softAP(cfg.apSsid, cfg.apPass, AP_CHANNEL, false, 4);
  Serial.printf("[WiFi] AP \"%s\" sur %s\n", cfg.apSsid, WiFi.softAPIP().toString().c_str());
  staBegin();
}

static void netServicesStart() {
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", AP_IP);  // portail captif sur l'AP
  if (MDNS.begin(cfg.hostname)) MDNS.addService("http", "tcp", 80);
}

static void wifiApply() {
  MDNS.end();
  dns.stop();
  wifiStart();
  netServicesStart();
  httpUpdater.updateCredentials(ADMIN_USER, updatePassword());
}

static void safeRestart() {
  engineStop();
  delay(50);
  ESP.restart();
}

// ============================================================================
//  INTERFACE WEB
// ============================================================================

static const char INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Deye Fan Simulator</title>
<style>
:root{--bg:#0e151d;--card:#17212c;--fg:#e6edf3;--mut:#8d9cab;--ok:#2ecc71;--wait:#f0a020;--acc:#3fa7ff;--ln:#2a3846}
*{box-sizing:border-box}body{margin:0;font-family:system-ui,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--fg)}
header{padding:12px 18px;background:#111a23;display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid var(--ln)}
h1{font-size:18px;margin:0}h2{font-size:16px;margin:0 0 8px}
main{max-width:1000px;margin:auto;padding:14px;display:grid;gap:14px;grid-template-columns:repeat(auto-fit,minmax(310px,1fr))}
.card{background:var(--card);border-radius:10px;padding:14px;border:1px solid var(--ln)}
.big{font-size:32px;font-weight:700;font-variant-numeric:tabular-nums}.mut{color:var(--mut);font-size:13px}
.badge{padding:3px 9px;border-radius:10px;font-size:12px;font-weight:700}.on{background:var(--ok);color:#052512}.off{background:var(--wait);color:#2d1b00}
.row{display:flex;justify-content:space-between;align-items:center;gap:10px;margin:6px 0}
label{display:block;font-size:13px;color:var(--mut);margin-top:9px}
input{width:100%;padding:8px;border-radius:6px;border:1px solid var(--ln);background:#0e151d;color:var(--fg)}
input[type=checkbox]{width:auto;margin-right:6px}
button{margin-top:10px;padding:9px 14px;border:0;border-radius:6px;background:var(--acc);color:#04121f;font-weight:700;cursor:pointer}
button.sec{background:#2a3846;color:var(--fg)}
table{width:100%;border-collapse:collapse;font-size:13px}td{padding:3px 0;border-bottom:1px solid #1f2b37}td:last-child{text-align:right;font-variant-numeric:tabular-nums}
#msg{position:fixed;bottom:16px;left:50%;transform:translateX(-50%);background:#23364a;padding:9px 16px;border-radius:6px;display:none;box-shadow:0 4px 16px #0008}
</style></head><body>
<header><h1>Deye Fan Simulator</h1><span id="conn" class="mut">…</span></header>
<main>
<section class="card" id="c0">…</section>
<section class="card" id="c1">…</section>
<section class="card"><h2>Ratios</h2>
<p class="mut">RPM simulés = RPM lus × ratio. Défaut : 4500/1600 = 2,8125 (9 cm) et 7000/3000 = 2,3333 (6 cm). Appliqué immédiatement, sans coupure du signal.</p>
<label>Ratio 9 cm</label><input id="r0" type="number" step="0.001" min="0.25" max="8"><div class="mut" id="h0"></div>
<label>Ratio 6 cm</label><input id="r1" type="number" step="0.001" min="0.25" max="8"><div class="mut" id="h1"></div>
<button onclick="saveRatios()">Appliquer et enregistrer</button>
<button class="sec" onclick="defRatios()">Valeurs par défaut</button>
</section>
<section class="card"><h2>WiFi</h2>
<label>Réseau domestique (STA) — SSID (vide = AP seul)</label>
<div style="display:flex;gap:6px"><input id="ss" list="nets" maxlength="32"><button class="sec" style="margin:0" onclick="scan()">Scanner</button></div>
<datalist id="nets"></datalist>
<label>Mot de passe STA</label><input id="sp" type="password" maxlength="64" placeholder="(inchangé)">
<label><input type="checkbox" id="so">Réseau ouvert (sans mot de passe)</label>
<label>Point d'accès (AP) — SSID</label><input id="as" maxlength="32">
<label>Mot de passe AP (8 à 63 caractères)</label><input id="ap" type="password" maxlength="63" placeholder="(inchangé)">
<label>Nom d'hôte (http://nom.local)</label><input id="hn" maxlength="32">
<label>Mot de passe admin de l'interface (utilisateur « admin »)</label><input id="ad" type="password" maxlength="32" placeholder="(inchangé)">
<label><input type="checkbox" id="adc">Supprimer le mot de passe admin</label>
<button onclick="saveWifi()">Enregistrer et appliquer</button>
</section>
<section class="card"><h2>Système et isolation temps réel</h2>
<table id="sys"></table>
<button class="sec" onclick="act('/api/diag/reset')">RAZ diagnostics</button>
<button class="sec" onclick="location.href='/update'">Mise à jour firmware</button>
<button class="sec" onclick="act('/api/reboot','Redémarrer le module ?')">Redémarrer</button>
<button class="sec" onclick="act('/api/factory','Revenir à la configuration usine ?')">Config. usine</button>
</section>
</main>
<div id="msg"></div>
<script>
const $=i=>document.getElementById(i);
const f0=v=>Math.round(v).toLocaleString('fr-FR');
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
let busy=false;
const msg=t=>{const m=$('msg');m.textContent=t;m.style.display='block';clearTimeout(m._t);m._t=setTimeout(()=>m.style.display='none',4000);};
const post=async(u,d)=>{const r=await fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(d)});
 const j=await r.json().catch(()=>({ok:false,msg:'réponse invalide'}));if(!r.ok||!j.ok)throw new Error(j.msg||('HTTP '+r.status));return j;};
const card=c=>`<div class="row"><h2>Ventilateur ${c.name}</h2><span class="badge ${c.active?'on':'off'}">${c.active?'SIMULATION':'ATTENTE SIGNAL'}</span></div>
<div class="row"><div><div class="mut">RPM lus (Noctua)</div><div class="big">${f0(c.rpm_in)}</div><div class="mut">${c.f_in.toFixed(2)} Hz</div></div>
<div style="text-align:right"><div class="mut">RPM simulés (vers Deye)</div><div class="big">${f0(c.rpm_out)}</div><div class="mut">${c.f_out.toFixed(2)} Hz</div></div></div>
<div class="mut">ratio ×${c.ratio.toFixed(4)} · impulsions lues ${c.edges} · émises ${c.pulses} · parasites ${c.glitches}</div>`;
const tr=(k,v)=>`<tr><td>${k}</td><td>${v}</td></tr>`;
const render=s=>{
 s.ch.forEach((c,i)=>{$('c'+i).innerHTML=card(c);const r=parseFloat($('r'+i).value);
  $('h'+i).textContent=(c.rpm_in>0&&r>0)?`→ ${f0(c.rpm_in*r)} tr/min simulés avec ce ratio`:'';});
 const w=s.wifi,t=s.timing;
 $('sys').innerHTML=tr('Firmware',s.fw)+tr('Uptime',Math.floor(s.uptime/3600)+' h '+Math.floor(s.uptime%3600/60)+' min')
  +tr('Mémoire libre',s.heap+' o')+tr('CPU',s.cpu+' MHz')+tr('LED',esc(s.led))
  +tr('STA',esc(w.sta_ssid||'—')+' · '+esc(w.sta_state)+(w.sta_ip?' · '+w.sta_ip+' · '+w.rssi+' dBm':''))
  +tr('AP',esc(w.ap_ssid)+' · '+w.ap_ip+' · '+w.ap_clients+' client(s) · canal '+w.channel)
  +tr('Nom d\'hôte',esc(w.hostname)+'.local')
  +tr('Échantillonnage entrées',t.poll_us+' µs')+tr('NMI / s',t.nmi_rate)
  +tr('Durée max d\'une NMI',t.nmi_max_us.toFixed(2)+' µs')
  +tr('Retard max d\'un front de sortie',t.late_max_us.toFixed(3)+' µs')
  +tr('Fronts en retard (> 1 µs)',t.late_count);
};
const poll=async()=>{if(busy)return;busy=true;
 try{const r=await fetch('/api/status',{cache:'no-store'});render(await r.json());$('conn').textContent='en ligne';}
 catch(e){$('conn').textContent='hors ligne';}busy=false;};
const loadCfg=async()=>{try{const j=await (await fetch('/api/config',{cache:'no-store'})).json();
 $('r0').value=j.ratio[0].toFixed(4);$('r1').value=j.ratio[1].toFixed(4);$('ss').value=j.sta_ssid;$('as').value=j.ap_ssid;$('hn').value=j.hostname;
 $('sp').placeholder=j.sta_pass_set?'(inchangé)':'(aucun)';$('ad').placeholder=j.admin_pass_set?'(inchangé)':'(aucun)';}catch(e){}};
const saveRatios=async()=>{try{await post('/api/ratios',{r0:$('r0').value,r1:$('r1').value});msg('Ratios appliqués et enregistrés');}catch(e){msg('Erreur : '+e.message);}};
const defRatios=()=>{$('r0').value=(4500/1600).toFixed(4);$('r1').value=(7000/3000).toFixed(4);};
const scan=async()=>{msg('Scan WiFi en cours…');for(let k=0;k<20;k++){try{const j=await (await fetch('/api/scan',{cache:'no-store'})).json();
 if(j.status==='done'){$('nets').innerHTML=j.networks.map(n=>`<option value="${esc(n.ssid)}">${n.rssi} dBm${n.open?' (ouvert)':''}</option>`).join('');msg(j.networks.length+' réseau(x) trouvé(s)');return;}}catch(e){}
 await new Promise(r=>setTimeout(r,1000));}msg('Scan : délai dépassé');};
const saveWifi=async()=>{const d={sta_ssid:$('ss').value,ap_ssid:$('as').value,hostname:$('hn').value};
 if($('so').checked)d.sta_open='1';else if($('sp').value)d.sta_pass=$('sp').value;
 if($('ap').value)d.ap_pass=$('ap').value;if($('adc').checked)d.admin_clear='1';else if($('ad').value)d.admin_pass=$('ad').value;
 try{const j=await post('/api/wifi',d);msg(j.msg||'Enregistré');}catch(e){msg('Erreur : '+e.message);}};
const act=async(u,c)=>{if(c&&!confirm(c))return;try{const j=await post(u,{});msg(j.msg||'OK');}catch(e){msg('Erreur : '+e.message);}};
loadCfg();poll();setInterval(poll,500);
</script></body></html>
)HTML";

static void jsonStr(String &o, const char *s) {
  o += '"';
  for (; *s; ++s) {
    const char c = *s;
    switch (c) {
      case '"':  o += F("\\\""); break;
      case '\\': o += F("\\\\"); break;
      case '\n': o += F("\\n"); break;
      case '\r': o += F("\\r"); break;
      case '\t': o += F("\\t"); break;
      default:
        if ((uint8_t)c < 0x20) {
          char b[8];
          snprintf(b, sizeof(b), "\\u%04x", (unsigned)(uint8_t)c);
          o += b;
        } else {
          o += c;
        }
    }
  }
  o += '"';
}

static void sendJson(int code, const String &body) {
  server.sendHeader(F("Cache-Control"), F("no-store"));
  server.send(code, "application/json", body);
}

static void sendResult(bool ok, const char *message, int code = 0) {
  String j(F("{\"ok\":"));
  j += ok ? F("true") : F("false");
  j += F(",\"msg\":");
  jsonStr(j, message);
  j += '}';
  sendJson(code ? code : (ok ? 200 : 400), j);
}

static bool authOK() {
  if (!cfg.adminPass[0]) return true;
  if (server.authenticate(ADMIN_USER, cfg.adminPass)) return true;
  server.requestAuthentication(BASIC_AUTH, "deye-fan");
  return false;
}

static uint32_t nmiRate = 0;  // NMI par seconde, calculé dans loop()

static const char *ledDescription() {
  const bool a0 = S.outPeriod[0] != 0, a1 = S.outPeriod[1] != 0;
  if (a0 && a1) return "fixe : simulation active sur les 2 canaux";
  if (a0 || a1) return "clignote 1 Hz : simulation sur 1 canal";
  return "flash / 2 s : attente de signal";
}

static void handleRoot() {
  if (!authOK()) return;
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

static void handleStatus() {
  if (!authOK()) return;
  String j;
  j.reserve(1400);
  j += F("{\"fw\":\"" FW_VERSION "\",\"uptime\":");
  j += millis() / 1000;
  j += F(",\"heap\":");
  j += ESP.getFreeHeap();
  j += F(",\"cpu\":");
  j += E.cpuMHz;
  j += F(",\"led\":");
  jsonStr(j, ledDescription());
  j += F(",\"ch\":[");
  for (uint8_t c = 0; c < NUM_CH; c++) {
    const uint32_t pin = S.inPeriod[c];
    const uint32_t pout = S.outPeriod[c];
    if (c) j += ',';
    j += F("{\"name\":\"");
    j += CH_NAME[c];
    j += F("\",\"ratio\":");
    j += String(cfg.ratio[c], 4);
    j += F(",\"rpm_in\":");
    j += String(rpmFromPeriod(pin, PPR_IN), 1);
    j += F(",\"rpm_out\":");
    j += String(rpmFromPeriod(pout, PPR_OUT), 1);
    j += F(",\"f_in\":");
    j += String(hzFromPeriod(pin), 3);
    j += F(",\"f_out\":");
    j += String(hzFromPeriod(pout), 3);
    j += F(",\"active\":");
    j += pout ? F("true") : F("false");
    j += F(",\"edges\":");
    j += S.edges[c];
    j += F(",\"pulses\":");
    j += S.pulses[c];
    j += F(",\"glitches\":");
    j += S.glitches[c];
    j += '}';
  }
  j += F("],\"wifi\":{\"sta_ssid\":");
  jsonStr(j, cfg.staSsid);
  j += F(",\"sta_state\":");
  jsonStr(j, staStateName());
  j += F(",\"sta_ip\":\"");
  if (staState == StaState::Connected) j += WiFi.localIP().toString();
  j += F("\",\"rssi\":");
  j += (staState == StaState::Connected) ? WiFi.RSSI() : 0;
  j += F(",\"ap_ssid\":");
  jsonStr(j, cfg.apSsid);
  j += F(",\"ap_ip\":\"");
  j += WiFi.softAPIP().toString();
  j += F("\",\"ap_clients\":");
  j += WiFi.softAPgetStationNum();
  j += F(",\"channel\":");
  j += WiFi.channel();
  j += F(",\"hostname\":");
  jsonStr(j, cfg.hostname);
  j += F("},\"timing\":{\"poll_us\":");
  j += POLL_US;
  j += F(",\"nmi_rate\":");
  j += nmiRate;
  j += F(",\"nmi_max_us\":");
  j += String(usFromCcy(S.nmiMaxCcy), 2);
  j += F(",\"late_max_us\":");
  j += String(usFromCcy(S.lateMaxCcy), 3);
  j += F(",\"late_count\":");
  j += S.lateCount;
  j += F("}}");
  sendJson(200, j);
}

static void handleConfigGet() {
  if (!authOK()) return;
  String j(F("{\"ratio\":["));
  for (uint8_t c = 0; c < NUM_CH; c++) {
    if (c) j += ',';
    j += String(cfg.ratio[c], 4);
  }
  j += F("],\"sta_ssid\":");
  jsonStr(j, cfg.staSsid);
  j += F(",\"sta_pass_set\":");
  j += cfg.staPass[0] ? F("true") : F("false");
  j += F(",\"ap_ssid\":");
  jsonStr(j, cfg.apSsid);
  j += F(",\"hostname\":");
  jsonStr(j, cfg.hostname);
  j += F(",\"admin_pass_set\":");
  j += cfg.adminPass[0] ? F("true") : F("false");
  j += '}';
  sendJson(200, j);
}

static bool parseRatio(const String &s, float &out) {
  String t = s;
  t.trim();
  t.replace(',', '.');
  if (!t.length()) return false;
  const float v = t.toFloat();
  if (!(v >= RATIO_MIN && v <= RATIO_MAX)) return false;
  out = v;
  return true;
}

static void handleRatios() {
  if (!authOK()) return;
  float r[NUM_CH];
  static const char *const keys[NUM_CH] = { "r0", "r1" };
  for (uint8_t c = 0; c < NUM_CH; c++) {
    if (!parseRatio(server.arg(keys[c]), r[c])) {
      sendResult(false, "Ratio invalide (plage 0,25 à 8)");
      return;
    }
  }
  for (uint8_t c = 0; c < NUM_CH; c++) cfg.ratio[c] = r[c];
  applyRatios();  // effectif dès le prochain cycle de sortie
  configSave();
  Serial.printf("[CFG] Ratios : 9 cm x%.4f, 6 cm x%.4f\n", cfg.ratio[0], cfg.ratio[1]);
  sendResult(true, "Ratios appliqués et enregistrés");
}

static bool validHostname(const String &h) {
  if (h.length() < 1 || h.length() > 32 || h[0] == '-') return false;
  for (size_t k = 0; k < h.length(); k++) {
    const char c = h[k];
    if (!(isalnum((unsigned char)c) || c == '-')) return false;
  }
  return true;
}

static void handleWifi() {
  if (!authOK()) return;
  Config n = cfg;

  const String staSsid = server.arg("sta_ssid");
  if (staSsid.length() > 32) { sendResult(false, "SSID STA trop long (32 max)"); return; }
  copyStr(n.staSsid, sizeof(n.staSsid), staSsid.c_str());
  if (server.arg("sta_open") == "1") {
    n.staPass[0] = '\0';
  } else if (server.hasArg("sta_pass")) {
    const String p = server.arg("sta_pass");
    if (p.length() > 64) { sendResult(false, "Mot de passe STA trop long"); return; }
    copyStr(n.staPass, sizeof(n.staPass), p.c_str());
  }

  const String apSsid = server.arg("ap_ssid");
  if (apSsid.length() < 1 || apSsid.length() > 32) { sendResult(false, "SSID AP : 1 à 32 caractères"); return; }
  copyStr(n.apSsid, sizeof(n.apSsid), apSsid.c_str());
  if (server.hasArg("ap_pass")) {
    const String p = server.arg("ap_pass");
    if (p.length() < 8 || p.length() > 63) { sendResult(false, "Mot de passe AP : 8 à 63 caractères"); return; }
    copyStr(n.apPass, sizeof(n.apPass), p.c_str());
  }

  String host = server.arg("hostname");
  host.toLowerCase();
  if (!validHostname(host)) { sendResult(false, "Nom d'hôte : lettres, chiffres et '-' (32 max)"); return; }
  copyStr(n.hostname, sizeof(n.hostname), host.c_str());

  if (server.arg("admin_clear") == "1") {
    n.adminPass[0] = '\0';
  } else if (server.hasArg("admin_pass")) {
    const String p = server.arg("admin_pass");
    if (p.length() < 4 || p.length() > 32) { sendResult(false, "Mot de passe admin : 4 à 32 caractères"); return; }
    copyStr(n.adminPass, sizeof(n.adminPass), p.c_str());
  }

  cfg = n;
  configSave();
  wifiApplyAt = millis() + 1500;  // laisse partir la réponse HTTP avant de reconfigurer
  sendResult(true, "Enregistré. Le WiFi se reconfigure (le signal tach n'est pas interrompu) : reconnectez-vous si besoin.");
}

static void handleScan() {
  if (!authOK()) return;
  const int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true, false);
    sendJson(202, F("{\"status\":\"running\"}"));
    return;
  }
  if (n == WIFI_SCAN_RUNNING) {
    sendJson(202, F("{\"status\":\"running\"}"));
    return;
  }
  String j(F("{\"status\":\"done\",\"networks\":["));
  for (int i = 0; i < n; i++) {
    if (i) j += ',';
    j += F("{\"ssid\":");
    jsonStr(j, WiFi.SSID(i).c_str());
    j += F(",\"rssi\":");
    j += WiFi.RSSI(i);
    j += F(",\"open\":");
    j += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? F("true") : F("false");
    j += '}';
  }
  j += F("]}");
  WiFi.scanDelete();
  sendJson(200, j);
}

static void handleReboot() {
  if (!authOK()) return;
  rebootAt = millis() + 1000;
  sendResult(true, "Redémarrage…");
}

static void handleFactory() {
  if (!authOK()) return;
  configDefaults(cfg);
  configSave();
  rebootAt = millis() + 1000;
  sendResult(true, "Configuration usine restaurée, redémarrage…");
}

static void handleDiagReset() {
  if (!authOK()) return;
  S.diagReset = 1;
  sendResult(true, "Diagnostics remis à zéro");
}

// Portail captif : toute URL inconnue venant d'un client de l'AP est redirigée
// vers l'interface (déclenche l'ouverture automatique sur smartphone).
static void handleNotFound() {
  const String host = server.hostHeader();
  const String apIp = WiFi.softAPIP().toString();
  const bool ours = host.length() == 0 || host == apIp || host == WiFi.localIP().toString() ||
                    host.startsWith(cfg.hostname);
  if (!ours) {
    server.sendHeader(F("Location"), String(F("http://")) + apIp + '/', true);
    server.send(302, "text/plain", "");
    return;
  }
  server.send(404, "text/plain", "Not found");
}

static void webStart() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/config", HTTP_GET, handleConfigGet);
  server.on("/api/ratios", HTTP_POST, handleRatios);
  server.on("/api/wifi", HTTP_POST, handleWifi);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/factory", HTTP_POST, handleFactory);
  server.on("/api/diag/reset", HTTP_POST, handleDiagReset);
  server.onNotFound(handleNotFound);
  httpUpdater.setup(&server, "/update", ADMIN_USER, updatePassword());
  server.begin();
}

// ============================================================================
//  LED D'ÉTAT (non critique, gérée dans loop())
// ============================================================================

static inline void ledWrite(bool on) {
  if (on) GPOC = 1u << PIN_LED; else GPOS = 1u << PIN_LED;  // active à l'état bas
}

static void ledService() {
  const bool a0 = S.outPeriod[0] != 0, a1 = S.outPeriod[1] != 0;
  const uint32_t ms = millis();
  bool on;
  if (a0 && a1)      on = true;
  else if (a0 || a1) on = (ms % 1000) < 500;
  else               on = (ms % 2000) < 80;
  ledWrite(on);
}

// ============================================================================
//  CONSOLE SÉRIE (secours / mise au point sur établi)
// ============================================================================

static void printStatus() {
  for (uint8_t c = 0; c < NUM_CH; c++) {
    Serial.printf("[%s] lus %7.1f tr/min | simules %7.1f tr/min | ratio x%.4f | %s\n",
                  CH_NAME[c], rpmFromPeriod(S.inPeriod[c], PPR_IN),
                  rpmFromPeriod(S.outPeriod[c], PPR_OUT), cfg.ratio[c],
                  S.outPeriod[c] ? "SIMULATION" : "attente signal");
  }
  Serial.printf("[NMI] %u/s, duree max %.2f us, retard max front %.3f us, fronts en retard %u\n",
                (unsigned)nmiRate, usFromCcy(S.nmiMaxCcy), usFromCcy(S.lateMaxCcy),
                (unsigned)S.lateCount);
  Serial.printf("[WiFi] STA \"%s\" %s %s | AP \"%s\" %s\n", cfg.staSsid, staStateName(),
                staState == StaState::Connected ? WiFi.localIP().toString().c_str() : "",
                cfg.apSsid, WiFi.softAPIP().toString().c_str());
}

static void handleCommand(char *line) {
  char *cmd = strtok(line, " ");
  if (!cmd) return;
  if (!strcmp(cmd, "help")) {
    Serial.println(F("Commandes : status | ratio <9|6> <valeur> | sta <ssid> [motdepasse] |"
                     " ap <ssid> <motdepasse> | factory | reboot"));
  } else if (!strcmp(cmd, "status")) {
    printStatus();
  } else if (!strcmp(cmd, "ratio")) {
    const char *which = strtok(nullptr, " ");
    const char *val = strtok(nullptr, " ");
    float r;
    if (!which || !val || !parseRatio(String(val), r)) {
      Serial.println(F("Usage : ratio <9|6> <0.25..8>"));
      return;
    }
    const uint8_t c = (!strcmp(which, "6")) ? CH_6CM : CH_9CM;
    cfg.ratio[c] = r;
    applyRatios();
    configSave();
    Serial.printf("Ratio %s = %.4f enregistre\n", CH_NAME[c], r);
  } else if (!strcmp(cmd, "sta")) {
    const char *ssid = strtok(nullptr, " ");
    const char *pass = strtok(nullptr, " ");
    copyStr(cfg.staSsid, sizeof(cfg.staSsid), ssid ? ssid : "");
    copyStr(cfg.staPass, sizeof(cfg.staPass), pass ? pass : "");
    configSave();
    wifiApplyAt = millis() + 200;
    Serial.println(F("STA enregistree"));
  } else if (!strcmp(cmd, "ap")) {
    const char *ssid = strtok(nullptr, " ");
    const char *pass = strtok(nullptr, " ");
    if (!ssid || !pass || strlen(pass) < 8) {
      Serial.println(F("Usage : ap <ssid> <motdepasse 8+ caracteres>"));
      return;
    }
    copyStr(cfg.apSsid, sizeof(cfg.apSsid), ssid);
    copyStr(cfg.apPass, sizeof(cfg.apPass), pass);
    configSave();
    wifiApplyAt = millis() + 200;
    Serial.println(F("AP enregistre"));
  } else if (!strcmp(cmd, "factory")) {
    configDefaults(cfg);
    configSave();
    Serial.println(F("Configuration usine, redemarrage"));
    rebootAt = millis() + 200;
  } else if (!strcmp(cmd, "reboot")) {
    rebootAt = millis() + 200;
  } else {
    Serial.println(F("Commande inconnue, taper help"));
  }
}

static void serialService() {
  static char buf[128];
  static uint8_t len = 0;
  while (Serial.available()) {
    const char ch = (char)Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n') {
      buf[len] = '\0';
      handleCommand(buf);
      len = 0;
    } else if (len < sizeof(buf) - 1) {
      buf[len++] = ch;
    }
  }
}

// ============================================================================
//  SETUP / LOOP
// ============================================================================

void setup() {
  // 1) Sorties d'abord : NPN bloqués (ligne tach onduleur libérée), sans glitch.
  for (uint8_t c = 0; c < NUM_CH; c++) {
    outRelease(1u << PIN_OUT[c]);
    pinMode(PIN_OUT[c], OUTPUT);
    outRelease(1u << PIN_OUT[c]);
    pinMode(PIN_IN[c], INPUT_PULLUP);  // pull-up externe 10 k déjà présente, l'interne ne gêne pas
  }
  ledWrite(false);
  pinMode(PIN_LED, OUTPUT);
  ledWrite(false);

  Serial.begin(115200);
  Serial.println();
  Serial.println(F("=== Deye Fan Simulator v" FW_VERSION " ==="));

  EEPROM.begin(EEPROM_SIZE);
  const bool loaded = configLoad();
  Serial.println(loaded ? F("[CFG] Configuration chargee") : F("[CFG] Configuration par defaut"));
  applyRatios();

  // 2) Moteur tach démarré AVANT le WiFi : le signal ne dépend pas du réseau.
  engineStart();
  Serial.printf("[NMI] Moteur tach actif, CPU %u MHz, echantillonnage %u us\n",
                (unsigned)E.cpuMHz, (unsigned)POLL_US);

  // 3) Réseau.
  wifiStart();
  netServicesStart();
  webStart();
  Serial.println(F("[WEB] Interface sur http://192.168.4.1/ (AP) et via l'IP STA"));
}

void loop() {
  server.handleClient();
  dns.processNextRequest();
  MDNS.update();
  staService();
  ledService();
  serialService();

  static uint32_t lastSec = 0, lastNmi = 0, lastPrint = 0;
  const uint32_t ms = millis();
  if (ms - lastSec >= 1000) {
    const uint32_t n = S.nmiCount;
    nmiRate = (uint32_t)((uint64_t)(n - lastNmi) * 1000u / (ms - lastSec));
    lastNmi = n;
    lastSec = ms;
  }
  if (ms - lastPrint >= 10000) {
    lastPrint = ms;
    printStatus();
  }

  if (wifiApplyAt && (int32_t)(ms - wifiApplyAt) >= 0) {
    wifiApplyAt = 0;
    wifiApply();
  }
  if (rebootAt && (int32_t)(ms - rebootAt) >= 0) {
    safeRestart();
  }
  delay(1);
}
