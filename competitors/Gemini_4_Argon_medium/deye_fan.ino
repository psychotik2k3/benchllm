/*
 * ======================================================================================
 *  SIMULATEUR / MULTIPLICATEUR DE SIGNAL TACHYMÈTRE (RPM) POUR ONDULEUR SOLAIRE DEYE
 *  Modèle cible : Deye SUN-8K-SG05LP1-EU-AM2-P
 *  Carte cible  : LOLIN(WEMOS) D1 mini V2.3.0 (ESP-12S / ESP8266)
 *  Fichier      : deye_fan.ino
 * ======================================================================================
 *
 *  CONTEXTE MATÉRIEL :
 *  - Canal 1 (60 mm) : 2x Noctua NF-A6x25-FLX (~3000 RPM max) en remplacement du
 *                      NMB 06025VE-12N-CL d'origine (~7000 RPM max).
 *                      Ratio nominal par défaut : 7000 / 3000 = 2.33
 *  - Canal 2 (92 mm) : 2x Noctua NF-A9-FLX (~1600 RPM max) en remplacement du
 *                      NMB 09225VE-12N-CU d'origine (~5600 RPM max).
 *                      Ratio nominal par défaut : 5600 / 1600 = 3.50
 *
 *  ARCHITECTURE LOGICIELLE TEMPS-RÉEL (ISOLATION TOTALE DU WIFI / SERVEUR WEB) :
 *  Sur ESP8266 (mono-cœur Xtensa LX106), la pile WiFi et le serveur HTTP s'exécutent
 *  au niveau thread (niveau 0) et via des interruptions masquables de niveau 1.
 *  Pour garantir une isolation totale de la chaîne de conversion tachymétrique :
 *
 *  1. AUCUN traitement du signal dans loop() :
 *     La boucle principale loop() ne participe jamais à la lecture des entrées, ni au
 *     calcul de la période de sortie, ni au basculement des broches de sortie.
 *     Même un blocage de 500 ms dans loop() lors d'une requête HTTP lourde n'a aucun
 *     effet sur le signal généré vers l'onduleur.
 *
 *  2. Acquisition et conversion en IRAM sur compteur matériel de cycles CPU (ccount) :
 *     Les interruptions GPIO sur front descendant (IRAM_ATTR) lisent directement le
 *     registre matériel 32 bits `ccount` du processeur (80 ou 160 MHz, résolution de
 *     12,5 ns à 80 MHz) via l'instruction assembleur `rsr ccount`.
 *     Un filtre anti-rebond/anti-parasites (rejet des impulsions < 2,5 ms issues des
 *     commutations IGBT de l'onduleur) et une moyenne glissante sur 4 périodes (soit
 *     exactement 2 tours mécaniques complets, éliminant l'asymétrie bipolaire de la
 *     sonde à effet Hall du ventilateur) sont calculés directement dans l'ISR.
 *
 *  3. Conversion arithmétique en virgule fixe Q16.16 sans flottant ni division en ISR :
 *     Le processeur Xtensa LX106 ne possède pas d'unité de division matérielle.
 *     Pour éviter tout appel aux routines logicielles et garantir une exécution en
 *     moins de 15 cycles d'horloge (< 200 ns) entièrement en IRAM :
 *     - Le thread principal précalcule l'inverse du ratio en virgule fixe :
 *       invRatioQ16 = round(65536.0 / ratio)
 *     - L'ISR calcule directement la demi-période de sortie en cycles CPU par une
 *       simple multiplication entière 64 bits suivie d'un décalage binaire :
 *       halfPeriodCycles = ((uint64_t)avgInputPeriodCycles * invRatioQ16) >> 17
 *
 *  4. Génération des sorties par Timer1 matériel (FRC1) cablé en NMI (Niveau 3) :
 *     Le Timer matériel FRC1 est attaché à l'interruption non-masquable (NMI) via
 *     `ETS_FRC_TIMER1_NMI_INTR_ATTACH`. L'interruption NMI (priorité maximale, niveau 3)
 *     préempte même les interruptions de la pile WiFi MAC de niveau 1.
 *     L'ordonnanceur NMI programme le registre matériel `T1L` (mode one-shot, diviseur 1)
 *     pour se réveiller à l'échéance exacte du prochain front de sortie, ajuste la phase
 *     au cycle d'horloge près sur `ccount`, et commute les broches via les registres
 *     matériels `GPOS` (Set) et `GPOC` (Clear) en un seul cycle CPU.
 *     La communication entre les 3 niveaux (Thread 0, ISR GPIO niveau 1, NMI niveau 3)
 *     repose sur le principe "Single-Writer" avec des mots 32 bits alignés atomiques.
 *
 *  5. Sécurité intrinsèque (Fail-Safe Watchdog matériel en NMI) :
 *     Si un ventilateur Noctua s'arrête ou se bloque (aucune impulsion reçue pendant
 *     plus de 500 ms), la NMI coupe immédiatement le signal simulé correspondant
 *     (sortie collecteur ouvert au repos) afin que l'onduleur Deye détecte la panne
 *     réelle du ventilateur et déclenche ses sécurités thermiques.
 *
 *  BROCHAGE WEMOS D1 MINI V2.3.0 :
 *  - D5 (GPIO14) : Entrée Tach Canal 1 (Noctua 60 mm NF-A6x25-FLX)
 *  - D6 (GPIO12) : Entrée Tach Canal 2 (Noctua 92 mm NF-A9-FLX)
 *  - D1 (GPIO5)  : Sortie Tach Simulée Canal 1 -> Base NPN Q_OUT1 -> Deye (60 mm)
 *  - D2 (GPIO4)  : Sortie Tach Simulée Canal 2 -> Base NPN Q_OUT2 -> Deye (92 mm)
 *  - D4 (GPIO2)  : LED d'état intégrée (LED_BUILTIN, active à l'état BAS)
 *  - D7 (GPIO13) : Sortie LED d'état externe optionnelle (active à l'état HAUT)
 *  Note : D1, D2, D5, D6, D7 (GPIO 5, 4, 14, 12, 13) n'ont aucune contrainte de
 *         boot sur l'ESP8266. Même si un ventilateur s'arrête avec sa sortie Hall à
 *         l'état bas, le Wemos redémarre toujours sans blocage.
 * ======================================================================================
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

extern "C" {
  #include "ets_sys.h"
  #include "user_interface.h"
}

// ======================================================================================
//  DÉFINITION DES BROCHES MATÉRIELLES (WEMOS D1 MINI V2.3.0)
// ======================================================================================
static constexpr uint8_t PIN_TACH_IN_CH1  = 14; // D5 (GPIO14) - Entrée Noctua 60 mm
static constexpr uint8_t PIN_TACH_IN_CH2  = 12; // D6 (GPIO12) - Entrée Noctua 92 mm
static constexpr uint8_t PIN_TACH_OUT_CH1 = 5;  // D1 (GPIO5)  - Sortie vers Deye 60 mm
static constexpr uint8_t PIN_TACH_OUT_CH2 = 4;  // D2 (GPIO4)  - Sortie vers Deye 92 mm
static constexpr uint8_t PIN_STATUS_LED   = 2;  // D4 (GPIO2)  - LED_BUILTIN (Active LOW)
static constexpr uint8_t PIN_EXT_LED      = 13; // D7 (GPIO13) - LED externe (Active HIGH)

static constexpr uint32_t MASK_IN_CH1     = (1UL << PIN_TACH_IN_CH1);
static constexpr uint32_t MASK_IN_CH2     = (1UL << PIN_TACH_IN_CH2);
static constexpr uint32_t MASK_OUT_CH1    = (1UL << PIN_TACH_OUT_CH1);
static constexpr uint32_t MASK_OUT_CH2    = (1UL << PIN_TACH_OUT_CH2);

// ======================================================================================
//  CONSTANTES DU MOTEUR TEMPS-RÉEL (CYCLES CPU & TIMINGS)
// ======================================================================================
static constexpr uint8_t  NUM_CHANNELS           = 2;
static constexpr uint8_t  PULSES_PER_REV         = 2;      // Standard ventilateurs PC 3 fils

// Paramètres temporels exprimés en microsecondes (convertis en cycles CPU à l'init)
static constexpr uint32_t DEBOUNCE_MIN_PERIOD_US = 2500UL;   // Max 12 000 RPM en entrée (anti-bruit IGBT)
static constexpr uint32_t FAN_TIMEOUT_US         = 500000UL; // 500 ms sans impulsion => ventilateur arrêté
static constexpr uint32_t MIN_OUT_HALF_PERIOD_US = 500UL;    // Limite haute sécurité sortie : 1000 Hz (30 000 RPM)
static constexpr uint32_t MAX_NMI_SLEEP_US       = 5000UL;   // Réveil NMI max toutes les 5 ms (watchdog interne)
static constexpr uint32_t NMI_SPIN_WINDOW_US     = 2UL;      // Fenêtre d'alignement fin de phase (2 µs)
static constexpr uint32_t NMI_PRESCHEDULE_US     = 2UL;      // Anticipation latence d'entrée NMI (2 µs)

// Variables de seuils en cycles d'horloge CPU (calculées selon F_CPU 80/160 MHz)
static volatile uint32_t g_cyclesPerUs           = 80;
static volatile uint32_t g_debounceMinCycles     = 2500UL * 80UL;
static volatile uint32_t g_fanTimeoutCycles      = 500000UL * 80UL;
static volatile uint32_t g_minOutHalfCycles      = 500UL * 80UL;
static volatile uint32_t g_maxNmiSleepCycles     = 5000UL * 80UL;
static volatile uint32_t g_nmiSpinWindowCycles   = 2UL * 80UL;
static volatile uint32_t g_nmiPrescheduleCycles  = 2UL * 80UL;

// ======================================================================================
//  STRUCTURE D'ÉTAT TEMPS-RÉEL SANS VERROU (SINGLE-WRITER ATOMIC 32-BIT)
// ======================================================================================
struct TachChannelRT {
  // Écrit par le thread principal (Niveau 0), lu par l'ISR GPIO (Niveau 1)
  volatile uint32_t invRatioQ16;          // round(65536.0 / ratio)

  // Écrit exclusivement par l'ISR GPIO (Niveau 1), lu par NMI (Niveau 3) et Thread 0
  volatile uint32_t lastEdgeCycle;        // Timestamp ccount du dernier front descendant valide
  volatile uint32_t measuredPeriodCycles; // Période d'entrée moyennée sur 4 impulsions (cycles CPU)
  volatile uint32_t targetHalfCycles;     // Demi-période de sortie calculée (cycles CPU, 0 = arrêt)
  volatile uint32_t totalPulses;          // Compteur total d'impulsions lues
  volatile uint8_t  hasSignal;            // 1 après amorçage d'au moins 2 fronts, 0 sinon

  // Historique interne privé de l'ISR GPIO (2 tours complets = 4 périodes)
  uint32_t          periodRing[4];
  uint8_t           ringIndex;
  uint8_t           ringCount;
};

// État privé exclusif de l'ordonnanceur NMI (Niveau 3)
struct NmiOutputState {
  uint32_t nextToggleCycle;
  uint8_t  pinHigh;
  uint8_t  active;
};

static TachChannelRT  g_ch[NUM_CHANNELS];
static NmiOutputState g_nmi[NUM_CHANNELS];
static const uint32_t g_outMasks[NUM_CHANNELS] = { MASK_OUT_CH1, MASK_OUT_CH2 };

// ======================================================================================
//  CONFIGURATION PERSISTANTE (EEPROM)
// ======================================================================================
static constexpr uint32_t CONFIG_MAGIC   = 0xDE7EFA26UL;
static constexpr uint16_t CONFIG_VERSION = 1;
static constexpr size_t   EEPROM_SIZE    = 512;

struct PersistentConfig {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  float    ratio[NUM_CHANNELS]; // Ratio Ch1 (60mm) et Ch2 (92mm)
  char     staSsid[33];
  char     staPass[65];
  char     apSsid[33];
  char     apPass[65];
  uint8_t  apChannel;
  uint8_t  staEnabled;
  uint8_t  pad[2];
  uint32_t crc32;
};

static PersistentConfig g_cfg;
static ESP8266WebServer g_server(80);

// Gestion non-bloquante de la reconnexion WiFi STA et de la LED
static uint32_t g_lastWifiCheckMs      = 0;
static bool     g_wifiReconnectPending = false;
static uint32_t g_wifiReconnectAtMs    = 0;
static uint32_t g_lastLedUpdateMs      = 0;

// ======================================================================================
//  FONCTIONS BAS NIVEAU EN IRAM (LECTURE COMPTEUR DE CYCLES CPU)
// ======================================================================================
#pragma GCC optimize ("O2")

static inline IRAM_ATTR uint32_t getCycleCountIRQ() {
  uint32_t ccount;
  __asm__ __volatile__("rsr %0,ccount" : "=a"(ccount));
  return ccount;
}

/*
 * Recalcule la demi-période de sortie en cycles CPU à partir de la période d'entrée
 * et du multiplicateur inverse en virgule fixe Q16.16.
 * Aucune division ni opération flottante : 100 % sûr en contexte ISR / NMI.
 */
static inline IRAM_ATTR uint32_t computeHalfPeriodCyclesIRAM(uint32_t inPeriodCycles, uint32_t invRatioQ16) {
  if (inPeriodCycles == 0 || invRatioQ16 == 0) {
    return 0;
  }
  // T_out_full = (inPeriodCycles * invRatioQ16) >> 16
  // T_out_half = T_out_full / 2 = (inPeriodCycles * invRatioQ16) >> 17
  uint32_t halfCycles = (uint32_t)(((uint64_t)inPeriodCycles * (uint64_t)invRatioQ16) >> 17);
  uint32_t minHalf = g_minOutHalfCycles;
  if (halfCycles < minHalf) {
    halfCycles = minHalf;
  }
  return halfCycles;
}

// ======================================================================================
//  ROUTINE D'INTERRUPTION NMI SUR TIMER1 (FRC1) - NIVEAU 3 (PRIORITÉ ABSOLUE)
//  Génère les signaux tachymétriques simulés avec une précision inférieure à la µs,
//  totalement insensible au trafic WiFi et aux requêtes du serveur Web.
// ======================================================================================
static IRAM_ATTR void onTimer1NMI() {
  // Vérifie si le CPU tourne à 160 MHz (bit 0 du registre système 0x3FF00014)
  // Le Timer1 (FRC1) tourne toujours à 80 MHz (horloge APB), tandis que `ccount`
  // tourne à F_CPU (80 ou 160 MHz).
  const uint32_t turboShift = ((*(volatile uint32_t*)0x3FF00014) & 1U) ? 1U : 0U;

  uint32_t now = getCycleCountIRQ();
  const uint32_t timeoutCycles = g_fanTimeoutCycles;
  const uint32_t spinWindow    = g_nmiSpinWindowCycles;

  uint32_t nextWakeCycle = now + g_maxNmiSleepCycles;

  for (uint8_t ch = 0; ch < NUM_CHANNELS; ++ch) {
    const uint32_t outMask   = g_outMasks[ch];
    const uint32_t halfCycle = g_ch[ch].targetHalfCycles;
    const uint32_t lastEdge  = g_ch[ch].lastEdgeCycle;
    const uint8_t  hasSig    = g_ch[ch].hasSignal;

    // Vérification du chien de garde (Watchdog) d'arrêt ventilateur :
    // Si aucune impulsion n'est arrivée depuis plus de 500 ms, on coupe immédiatement
    // la sortie pour que l'onduleur Deye détecte un éventuel arrêt réel du ventilateur.
    if (!hasSig || halfCycle == 0 || ((uint32_t)(now - lastEdge) > timeoutCycles)) {
      if (g_nmi[ch].active || g_nmi[ch].pinHigh) {
        GPOC = outMask; // Transistor NPN bloqué -> collecteur ouvert au repos
        g_nmi[ch].pinHigh = 0;
        g_nmi[ch].active  = 0;
      }
      continue;
    }

    // Démarrage initial du canal (amorçage sans glitch)
    if (!g_nmi[ch].active) {
      g_nmi[ch].active          = 1;
      g_nmi[ch].pinHigh         = 1;
      GPOS                      = outMask;
      g_nmi[ch].nextToggleCycle = now + halfCycle;
    } else {
      int32_t remaining = (int32_t)(g_nmi[ch].nextToggleCycle - now);

      // Si l'échéance du front est dans la fenêtre fine (<= 2 µs), on attend le
      // cycle d'horloge exact pour éliminer toute gigue (jitter) résiduelle.
      if (remaining <= (int32_t)spinWindow) {
        while ((int32_t)(g_nmi[ch].nextToggleCycle - getCycleCountIRQ()) > 0) {
          // Attente active sub-microseconde sur `ccount`
        }
        now = getCycleCountIRQ();

        // Basculement matériel atomique en 1 cycle CPU via registres GPOS / GPOC
        if (g_nmi[ch].pinHigh) {
          GPOC = outMask;
          g_nmi[ch].pinHigh = 0;
        } else {
          GPOS = outMask;
          g_nmi[ch].pinHigh = 1;
        }

        // Avancement de phase sans dérive cumulative
        g_nmi[ch].nextToggleCycle += halfCycle;

        // Sécurité de resynchronisation en cas de changement brusque de période
        if ((int32_t)(g_nmi[ch].nextToggleCycle - now) <= (int32_t)spinWindow) {
          g_nmi[ch].nextToggleCycle = now + halfCycle;
        }
      }
    }

    // Recherche de la prochaine échéance la plus proche entre les deux canaux
    if ((int32_t)(g_nmi[ch].nextToggleCycle - nextWakeCycle) < 0) {
      nextWakeCycle = g_nmi[ch].nextToggleCycle;
    }
  }

  // Programmation du Timer1 (FRC1) pour le prochain front
  now = getCycleCountIRQ();
  int32_t deltaCycles = (int32_t)(nextWakeCycle - now) - (int32_t)g_nmiPrescheduleCycles;
  int32_t minCycles   = (int32_t)(g_cyclesPerUs << 2); // Minimum 4 µs
  if (deltaCycles < minCycles) {
    deltaCycles = minCycles;
  }

  // Conversion cycles CPU -> ticks Timer1 (80 MHz, registre 23 bits max 8 388 607)
  uint32_t timerTicks = ((uint32_t)deltaCycles) >> turboShift;
  if (timerTicks > 0x7FFFF0UL) {
    timerTicks = 0x7FFFF0UL;
  }
  T1L = timerTicks;
}

// ======================================================================================
//  ROUTINES D'INTERRUPTION GPIO D'ACQUISITION TACHYMÈTRE (IRAM_ATTR)
//  Mesure la période exacte en cycles CPU, filtre sur 2 révolutions (4 impulsions)
//  et met à jour instantanément la consigne du synthétiseur NMI.
// ======================================================================================
static inline IRAM_ATTR void processTachEdgeIRAM(uint8_t ch, uint32_t pinMask) {
  const uint32_t now = getCycleCountIRQ();

  // Vérification matérielle que la ligne est bien à l'état BAS (rejette les parasites)
  if (GPI & pinMask) {
    return;
  }

  TachChannelRT &c = g_ch[ch];
  const uint32_t prevEdge = c.lastEdgeCycle;
  const uint32_t dt       = now - prevEdge;

  // Si le canal était arrêté (timeout dépassé), ce front sert de référence t0
  if (!c.hasSignal || (dt > g_fanTimeoutCycles)) {
    c.lastEdgeCycle = now;
    c.ringIndex     = 0;
    c.ringCount     = 0;
    c.hasSignal     = 1;
    c.totalPulses   = c.totalPulses + 1;
    return;
  }

  // Filtre anti-rebond / réjection du bruit haute fréquence (ex: hacheur onduleur)
  if (dt < g_debounceMinCycles) {
    return;
  }

  c.lastEdgeCycle = now;
  c.totalPulses   = c.totalPulses + 1;

  // Insertion dans le buffer circulaire de 4 périodes (2 tours complets du ventilateur)
  c.periodRing[c.ringIndex] = dt;
  c.ringIndex = (c.ringIndex + 1) & 0x03U;
  if (c.ringCount < 4) {
    c.ringCount++;
  }

  // Calcul de la période moyenne en cycles CPU sans division (décalages binaires uniquement)
  uint32_t avgPeriodCycles;
  if (c.ringCount == 4) {
    uint32_t sum4 = c.periodRing[0] + c.periodRing[1] + c.periodRing[2] + c.periodRing[3];
    avgPeriodCycles = sum4 >> 2; // Moyenne exacte sur 4 périodes (2 tours complets)
  } else if (c.ringCount >= 2) {
    avgPeriodCycles = (c.periodRing[0] + c.periodRing[1]) >> 1;
  } else {
    avgPeriodCycles = dt;
  }

  c.measuredPeriodCycles = avgPeriodCycles;

  // Mise à jour immédiate de la demi-période de sortie pour le Timer1 NMI
  uint32_t newHalfCycles = computeHalfPeriodCyclesIRAM(avgPeriodCycles, c.invRatioQ16);
  c.targetHalfCycles     = newHalfCycles;

  // Si le Timer1 dormait sur une longue période d'inactivité, on rapproche son réveil
  if (T1L > 800UL) {
    T1L = 400UL; // Réveil NMI dans 5 µs pour prise en compte immédiate
  }
}

static IRAM_ATTR void isrTachInCh1() {
  processTachEdgeIRAM(0, MASK_IN_CH1);
}

static IRAM_ATTR void isrTachInCh2() {
  processTachEdgeIRAM(1, MASK_IN_CH2);
}

#pragma GCC optimize ("Os")

// ======================================================================================
//  GESTION DES RATIOS ET CALCULS DE CONVERSION POUR L'INTERFACE UTILISATEUR
// ======================================================================================
static void applyChannelRatio(uint8_t ch, float ratio) {
  if (ch >= NUM_CHANNELS) return;
  if (ratio < 0.10f) ratio = 0.10f;
  if (ratio > 10.00f) ratio = 10.00f;

  g_cfg.ratio[ch] = ratio;
  uint32_t invQ16 = (uint32_t)((65536.0f / ratio) + 0.5f);
  if (invQ16 == 0) invQ16 = 1;

  // Écriture 32 bits atomique pour l'ISR
  g_ch[ch].invRatioQ16 = invQ16;

  // Mise à jour immédiate si le ventilateur tourne déjà
  uint32_t curPeriod = g_ch[ch].measuredPeriodCycles;
  if (g_ch[ch].hasSignal && curPeriod > 0) {
    g_ch[ch].targetHalfCycles = computeHalfPeriodCyclesIRAM(curPeriod, invQ16);
  }
}

static bool isChannelActive(uint8_t ch) {
  if (ch >= NUM_CHANNELS) return false;
  if (!g_ch[ch].hasSignal || g_ch[ch].measuredPeriodCycles == 0) return false;
  uint32_t now      = getCycleCountIRQ();
  uint32_t lastEdge = g_ch[ch].lastEdgeCycle;
  return ((uint32_t)(now - lastEdge) <= g_fanTimeoutCycles);
}

static float getChannelInputHz(uint8_t ch) {
  if (!isChannelActive(ch)) return 0.0f;
  uint32_t periodCycles = g_ch[ch].measuredPeriodCycles;
  if (periodCycles == 0) return 0.0f;
  float cpuHz = (float)(g_cyclesPerUs * 1000000UL);
  return cpuHz / (float)periodCycles;
}

static float getChannelInputRPM(uint8_t ch) {
  // 2 impulsions par tour => RPM = (Hz * 60) / 2 = Hz * 30
  return getChannelInputHz(ch) * (60.0f / (float)PULSES_PER_REV);
}

static float getChannelOutputHz(uint8_t ch) {
  if (!isChannelActive(ch)) return 0.0f;
  uint32_t halfCycles = g_ch[ch].targetHalfCycles;
  if (halfCycles == 0) return 0.0f;
  float cpuHz = (float)(g_cyclesPerUs * 1000000UL);
  return cpuHz / (float)(halfCycles * 2UL);
}

static float getChannelOutputRPM(uint8_t ch) {
  return getChannelOutputHz(ch) * (60.0f / (float)PULSES_PER_REV);
}

// ======================================================================================
//  PERSISTANCE EEPROM AVEC INTÉGRITÉ CRC32
// ======================================================================================
static uint32_t calculateCRC32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; ++b) {
      if (crc & 1U) {
        crc = (crc >> 1) ^ 0xEDB88320UL;
      } else {
        crc >>= 1;
      }
    }
  }
  return ~crc;
}

static void loadDefaultConfig() {
  memset(&g_cfg, 0, sizeof(g_cfg));
  g_cfg.magic      = CONFIG_MAGIC;
  g_cfg.version    = CONFIG_VERSION;
  // Canal 1 : Noctua NF-A6x25-FLX (3000 RPM) -> NMB 06025VE-12N-CL (7000 RPM) => 2.33
  g_cfg.ratio[0]   = 2.33f;
  // Canal 2 : Noctua NF-A9-FLX (1600 RPM)    -> NMB 09225VE-12N-CU (5600 RPM) => 3.50
  g_cfg.ratio[1]   = 3.50f;
  strncpy(g_cfg.apSsid, "Deye-Fan-Simulator", sizeof(g_cfg.apSsid) - 1);
  strncpy(g_cfg.apPass, "deye12345", sizeof(g_cfg.apPass) - 1);
  g_cfg.apChannel  = 6;
  g_cfg.staEnabled = 0;
  g_cfg.staSsid[0] = '\0';
  g_cfg.staPass[0] = '\0';
}

static bool loadConfigFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  PersistentConfig tmp;
  EEPROM.get(0, tmp);

  if (tmp.magic != CONFIG_MAGIC || tmp.version != CONFIG_VERSION) {
    loadDefaultConfig();
    return false;
  }

  uint32_t expectedCrc = calculateCRC32((const uint8_t*)&tmp, offsetof(PersistentConfig, crc32));
  if (tmp.crc32 != expectedCrc) {
    loadDefaultConfig();
    return false;
  }

  g_cfg = tmp;
  for (uint8_t ch = 0; ch < NUM_CHANNELS; ++ch) {
    if (isnan(g_cfg.ratio[ch]) || g_cfg.ratio[ch] < 0.10f || g_cfg.ratio[ch] > 10.00f) {
      g_cfg.ratio[ch] = (ch == 0) ? 2.33f : 3.50f;
    }
  }
  g_cfg.staSsid[sizeof(g_cfg.staSsid) - 1] = '\0';
  g_cfg.staPass[sizeof(g_cfg.staPass) - 1] = '\0';
  g_cfg.apSsid[sizeof(g_cfg.apSsid) - 1]   = '\0';
  g_cfg.apPass[sizeof(g_cfg.apPass) - 1]   = '\0';
  if (g_cfg.apChannel < 1 || g_cfg.apChannel > 13) {
    g_cfg.apChannel = 6;
  }
  return true;
}

static bool saveConfigToEEPROM() {
  g_cfg.magic   = CONFIG_MAGIC;
  g_cfg.version = CONFIG_VERSION;
  g_cfg.crc32   = calculateCRC32((const uint8_t*)&g_cfg, offsetof(PersistentConfig, crc32));
  EEPROM.put(0, g_cfg);
  return EEPROM.commit();
}

// ======================================================================================
//  CONFIGURATION DU WIFI (MODE SIMULTANÉ AP + STA)
// ======================================================================================
static void setupWiFi() {
  // Désactive la sauvegarde automatique SDK en flash à chaque boot (évite l'usure flash)
  WiFi.persistent(false);

  // Mode AP + STA simultané obligatoire
  WiFi.mode(WIFI_AP_STA);

  // Désactive le Modem-Sleep : courant d'alimentation constant et latence minimale
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.hostname("deye-fan-sim");

  // Démarrage du Point d'Accès (AP)
  const char *apSsid = (strlen(g_cfg.apSsid) > 0) ? g_cfg.apSsid : "Deye-Fan-Simulator";
  bool apOk = false;
  if (strlen(g_cfg.apPass) >= 8) {
    apOk = WiFi.softAP(apSsid, g_cfg.apPass, g_cfg.apChannel, false, 4);
  } else {
    apOk = WiFi.softAP(apSsid, nullptr, g_cfg.apChannel, false, 4);
  }
  (void)apOk;

  // Démarrage de la connexion Station (STA) si configurée
  if (g_cfg.staEnabled && strlen(g_cfg.staSsid) > 0) {
    WiFi.begin(g_cfg.staSsid, g_cfg.staPass);
  } else {
    WiFi.disconnect();
  }
}

// ======================================================================================
//  INTERFACE WEB EMBARQUÉE (SPA HTML5 / CSS3 / JS EN PROGMEM)
// ======================================================================================
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Deye SUN-8K — Simulateur Tachymètre Noctua</title>
  <style>
    :root {
      --bg: #0f172a;
      --card: #1e293b;
      --border: #334155;
      --text: #f8fafc;
      --muted: #94a3b8;
      --accent: #38bdf8;
      --ok: #22c55e;
      --warn: #f59e0b;
      --danger: #ef4444;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      background: var(--bg);
      color: var(--text);
      line-height: 1.5;
      padding: 1.25rem;
      max-width: 980px;
      margin: 0 auto;
    }
    header {
      display: flex;
      flex-wrap: wrap;
      justify-content: space-between;
      align-items: center;
      gap: 1rem;
      padding-bottom: 1rem;
      margin-bottom: 1.5rem;
      border-bottom: 1px solid var(--border);
    }
    h1 { font-size: 1.35rem; font-weight: 700; color: var(--text); }
    .subtitle { font-size: 0.85rem; color: var(--muted); }
    .badge {
      display: inline-flex;
      align-items: center;
      gap: 0.5rem;
      padding: 0.4rem 0.85rem;
      border-radius: 999px;
      font-size: 0.82rem;
      font-weight: 600;
      background: #1e293b;
      border: 1px solid var(--border);
    }
    .dot {
      width: 10px;
      height: 10px;
      border-radius: 50%;
      background: var(--warn);
    }
    .dot.active {
      background: var(--ok);
      box-shadow: 0 0 10px var(--ok);
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(310px, 1fr));
      gap: 1.25rem;
      margin-bottom: 1.25rem;
    }
    .card {
      background: var(--card);
      border: 1px solid var(--border);
      border-radius: 0.75rem;
      padding: 1.25rem;
    }
    .card-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 1rem;
    }
    .card-title { font-size: 1.05rem; font-weight: 600; color: var(--accent); }
    .fan-meta { font-size: 0.78rem; color: var(--muted); margin-bottom: 1rem; }
    .metrics {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 0.75rem;
      margin-bottom: 1rem;
    }
    .metric-box {
      background: rgba(15, 23, 42, 0.65);
      border: 1px solid var(--border);
      border-radius: 0.5rem;
      padding: 0.75rem;
      text-align: center;
    }
    .metric-label { font-size: 0.75rem; color: var(--muted); text-transform: uppercase; }
    .metric-val { font-size: 1.65rem; font-weight: 700; margin: 0.15rem 0; font-variant-numeric: tabular-nums; }
    .metric-sub { font-size: 0.75rem; color: var(--muted); font-variant-numeric: tabular-nums; }
    .sim-val { color: var(--ok); }
    label { display: block; font-size: 0.85rem; color: var(--muted); margin-bottom: 0.35rem; }
    .input-row { display: flex; gap: 0.5rem; align-items: center; margin-bottom: 0.65rem; }
    input[type="number"], input[type="text"], input[type="password"] {
      width: 100%;
      padding: 0.55rem 0.75rem;
      background: #0f172a;
      border: 1px solid var(--border);
      border-radius: 0.4rem;
      color: var(--text);
      font-size: 0.95rem;
    }
    .presets { display: flex; flex-wrap: wrap; gap: 0.4rem; margin-top: 0.4rem; }
    button {
      cursor: pointer;
      padding: 0.55rem 1rem;
      border-radius: 0.4rem;
      border: none;
      font-weight: 600;
      font-size: 0.88rem;
      background: var(--accent);
      color: #0f172a;
      transition: opacity 0.15s;
    }
    button:hover { opacity: 0.9; }
    .btn-preset {
      background: #334155;
      color: var(--text);
      padding: 0.3rem 0.6rem;
      font-size: 0.75rem;
    }
    .btn-danger { background: var(--danger); color: #fff; }
    .status-bar {
      font-size: 0.82rem;
      color: var(--muted);
      display: flex;
      flex-wrap: wrap;
      gap: 1.25rem;
      justify-content: space-between;
    }
    .toast {
      position: fixed;
      bottom: 1.25rem;
      right: 1.25rem;
      padding: 0.75rem 1.2rem;
      border-radius: 0.5rem;
      background: #0284c7;
      color: #fff;
      font-weight: 600;
      display: none;
      z-index: 50;
    }
  </style>
</head>
<body>
  <header>
    <div>
      <h1>Deye SUN-8K-SG05LP1 — Simulateur Tachymètre Noctua</h1>
      <div class="subtitle">Isolation matérielle Timer1 NMI &bull; Wemos D1 Mini (ESP8266)</div>
    </div>
    <div class="badge">
      <span id="sysDot" class="dot"></span>
      <span id="sysStateText">Connexion...</span>
    </div>
  </header>

  <div class="grid">
    <!-- CANAL 1 : 60 mm -->
    <div class="card">
      <div class="card-header">
        <span class="card-title">Canal 1 — Ventilateur 60 mm (D5 &rarr; D1)</span>
        <span id="ch0Badge" class="badge">En attente</span>
      </div>
      <div class="fan-meta">
        Entrée : <strong>Noctua NF-A6x25-FLX</strong> (~3000 RPM) &bull;
        Cible Deye : <strong>NMB 06025VE-12N-CL</strong> (7000 RPM)
      </div>
      <div class="metrics">
        <div class="metric-box">
          <div class="metric-label">RPM Lus (Noctua)</div>
          <div id="ch0InRpm" class="metric-val">0</div>
          <div id="ch0InHz" class="metric-sub">0.0 Hz</div>
        </div>
        <div class="metric-box">
          <div class="metric-label">RPM Simulés (Deye)</div>
          <div id="ch0OutRpm" class="metric-val sim-val">0</div>
          <div id="ch0OutHz" class="metric-sub">0.0 Hz</div>
        </div>
      </div>
      <label for="ratio0">Ratio multiplicateur Canal 1 (0.10 à 10.00)</label>
      <div class="input-row">
        <input type="number" id="ratio0" step="0.01" min="0.10" max="10.00" value="2.33">
        <button onclick="saveRatios()">Appliquer</button>
      </div>
      <div class="presets">
        <button class="btn-preset" onclick="setPreset(0, 2.33)">Standard 12V (×2.33 &rarr; 7000)</button>
        <button class="btn-preset" onclick="setPreset(0, 2.92)">Avec L.N.A 2400 (×2.92)</button>
        <button class="btn-preset" onclick="setPreset(0, 1.00)">Direct (×1.00)</button>
      </div>
    </div>

    <!-- CANAL 2 : 92 mm -->
    <div class="card">
      <div class="card-header">
        <span class="card-title">Canal 2 — Ventilateur 92 mm (D6 &rarr; D2)</span>
        <span id="ch1Badge" class="badge">En attente</span>
      </div>
      <div class="fan-meta">
        Entrée : <strong>Noctua NF-A9-FLX</strong> (~1600 RPM) &bull;
        Cible Deye : <strong>NMB 09225VE-12N-CU</strong> (5600 RPM)
      </div>
      <div class="metrics">
        <div class="metric-box">
          <div class="metric-label">RPM Lus (Noctua)</div>
          <div id="ch1InRpm" class="metric-val">0</div>
          <div id="ch1InHz" class="metric-sub">0.0 Hz</div>
        </div>
        <div class="metric-box">
          <div class="metric-label">RPM Simulés (Deye)</div>
          <div id="ch1OutRpm" class="metric-val sim-val">0</div>
          <div id="ch1OutHz" class="metric-sub">0.0 Hz</div>
        </div>
      </div>
      <label for="ratio1">Ratio multiplicateur Canal 2 (0.10 à 10.00)</label>
      <div class="input-row">
        <input type="number" id="ratio1" step="0.01" min="0.10" max="10.00" value="3.50">
        <button onclick="saveRatios()">Appliquer</button>
      </div>
      <div class="presets">
        <button class="btn-preset" onclick="setPreset(1, 3.50)">Standard 12V (×3.50 &rarr; 5600)</button>
        <button class="btn-preset" onclick="setPreset(1, 4.48)">Avec L.N.A 1250 (×4.48)</button>
        <button class="btn-preset" onclick="setPreset(1, 1.00)">Direct (×1.00)</button>
      </div>
    </div>
  </div>

  <!-- CONFIGURATION WIFI AP + STA -->
  <div class="card" style="margin-bottom: 1.25rem;">
    <div class="card-header">
      <span class="card-title">Configuration WiFi (Mode simultané AP + STA persistant)</span>
      <span id="wifiInfoBadge" class="subtitle"></span>
    </div>
    <div class="grid" style="margin-bottom: 0.75rem;">
      <div>
        <label for="staSsid">Réseau WiFi Domestique (Client STA — SSID)</label>
        <div class="input-row">
          <input type="text" id="staSsid" maxlength="32" placeholder="Nom du réseau WiFi (SSID)">
        </div>
        <label for="staPass">Mot de passe WiFi STA (laisser vide pour conserver l'actuel)</label>
        <div class="input-row">
          <input type="password" id="staPass" maxlength="64" placeholder="••••••••">
        </div>
      </div>
      <div>
        <label for="apSsid">Point d'Accès Local (AP — SSID)</label>
        <div class="input-row">
          <input type="text" id="apSsid" maxlength="32" placeholder="Deye-Fan-Simulator">
        </div>
        <label for="apPass">Mot de passe AP (min. 8 caractères, vide = conserver)</label>
        <div class="input-row">
          <input type="password" id="apPass" maxlength="64" placeholder="••••••••">
        </div>
      </div>
    </div>
    <div style="display:flex; justify-content:space-between; flex-wrap:wrap; gap:0.75rem;">
      <button onclick="saveWifi()">Enregistrer &amp; Appliquer le WiFi</button>
      <button class="btn-danger" onclick="resetFactory()">Réinitialiser valeurs d'usine</button>
    </div>
  </div>

  <div class="card status-bar">
    <span>IP AP : <strong id="apIp">-</strong></span>
    <span>IP STA : <strong id="staIp">-</strong></span>
    <span>Signal STA : <strong id="staRssi">-</strong></span>
    <span>Uptime : <strong id="uptime">-</strong></span>
  </div>

  <div id="toast" class="toast"></div>

  <script>
    let initialLoaded = false;

    function showToast(msg, isErr) {
      const t = document.getElementById('toast');
      t.textContent = msg;
      t.style.background = isErr ? '#ef4444' : '#0284c7';
      t.style.display = 'block';
      setTimeout(() => { t.style.display = 'none'; }, 3000);
    }

    function setPreset(ch, val) {
      document.getElementById('ratio' + ch).value = val.toFixed(2);
      saveRatios();
    }

    async function fetchStatus() {
      try {
        const res = await fetch('/api/status');
        if (!res.ok) return;
        const d = await res.json();

        const dot = document.getElementById('sysDot');
        const st  = document.getElementById('sysStateText');
        if (d.simulating) {
          dot.className = 'dot active';
          st.textContent = 'Simulation RPM Active (LED Fixe)';
        } else {
          dot.className = 'dot';
          st.textContent = 'En attente de signal ventilateur';
        }

        for (let i = 0; i < 2; i++) {
          const ch = d.channels[i];
          document.getElementById('ch' + i + 'InRpm').textContent  = Math.round(ch.in_rpm);
          document.getElementById('ch' + i + 'InHz').textContent   = ch.in_hz.toFixed(1) + ' Hz';
          document.getElementById('ch' + i + 'OutRpm').textContent = Math.round(ch.out_rpm);
          document.getElementById('ch' + i + 'OutHz').textContent  = ch.out_hz.toFixed(1) + ' Hz';
          const b = document.getElementById('ch' + i + 'Badge');
          b.textContent = ch.active ? ('Actif (×' + ch.ratio.toFixed(2) + ')') : 'Arrêté / Attente';
          b.style.borderColor = ch.active ? '#22c55e' : '#334155';

          if (!initialLoaded) {
            document.getElementById('ratio' + i).value = ch.ratio.toFixed(2);
          }
        }

        if (!initialLoaded) {
          document.getElementById('staSsid').value = d.wifi.sta_ssid || '';
          document.getElementById('apSsid').value  = d.wifi.ap_ssid  || '';
          initialLoaded = true;
        }

        document.getElementById('apIp').textContent    = d.wifi.ap_ip;
        document.getElementById('staIp').textContent   = d.wifi.sta_connected ? d.wifi.sta_ip : 'Non connecté';
        document.getElementById('staRssi').textContent = d.wifi.sta_connected ? (d.wifi.rssi + ' dBm') : '-';
        document.getElementById('uptime').textContent  = d.uptime_s + ' s';
      } catch (e) {
        document.getElementById('sysStateText').textContent = 'Hors ligne...';
      }
    }

    async function saveRatios() {
      const r0 = parseFloat(document.getElementById('ratio0').value);
      const r1 = parseFloat(document.getElementById('ratio1').value);
      const body = new URLSearchParams({ ratio0: r0, ratio1: r1 });
      const res = await fetch('/api/ratios', { method: 'POST', body });
      if (res.ok) {
        showToast('Ratios enregistrés et appliqués en temps réel !', false);
        fetchStatus();
      } else {
        showToast('Erreur lors de la sauvegarde des ratios', true);
      }
    }

    async function saveWifi() {
      const body = new URLSearchParams({
        sta_ssid: document.getElementById('staSsid').value.trim(),
        sta_pass: document.getElementById('staPass').value,
        ap_ssid:  document.getElementById('apSsid').value.trim(),
        ap_pass:  document.getElementById('apPass').value
      });
      const res = await fetch('/api/wifi', { method: 'POST', body });
      if (res.ok) {
        document.getElementById('staPass').value = '';
        document.getElementById('apPass').value  = '';
        showToast('Paramètres WiFi sauvegardés ! Reconnexion en cours...', false);
      } else {
        showToast('Paramètres WiFi invalides (mot de passe AP >= 8 car.)', true);
      }
    }

    async function resetFactory() {
      if (!confirm('Réinitialiser les ratios (2.33 / 3.50) et les paramètres WiFi par défaut ?')) return;
      const res = await fetch('/api/reset', { method: 'POST' });
      if (res.ok) {
        initialLoaded = false;
        showToast('Configuration d\'usine restaurée !', false);
        fetchStatus();
      }
    }

    fetchStatus();
    setInterval(fetchStatus, 500);
  </script>
</body>
</html>
)rawliteral";

// ======================================================================================
//  HANDLERS DU SERVEUR HTTP (NIVEAU THREAD 0 UNIQUEMENT)
// ======================================================================================
static void escapeJsonString(char *dst, size_t dstSize, const char *src) {
  size_t j = 0;
  for (size_t i = 0; src[i] != '\0' && (j + 2) < dstSize; ++i) {
    char c = src[i];
    if (c == '\"' || c == '\\') {
      dst[j++] = '\\';
    }
    dst[j++] = c;
  }
  dst[j] = '\0';
}

static void handleRoot() {
  g_server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

static void handleApiStatus() {
  bool ch0Active = isChannelActive(0);
  bool ch1Active = isChannelActive(1);
  bool simulating = (ch0Active || ch1Active);
  bool staConn    = (WiFi.status() == WL_CONNECTED);

  char escStaSsid[70];
  char escApSsid[70];
  escapeJsonString(escStaSsid, sizeof(escStaSsid), g_cfg.staSsid);
  escapeJsonString(escApSsid,  sizeof(escApSsid),  g_cfg.apSsid);

  IPAddress apIp  = WiFi.softAPIP();
  IPAddress staIp = WiFi.localIP();

  char json[768];
  snprintf(
    json, sizeof(json),
    "{\"simulating\":%s,\"uptime_s\":%lu,"
    "\"channels\":["
      "{\"id\":1,\"active\":%s,\"ratio\":%.2f,\"in_rpm\":%.1f,\"in_hz\":%.2f,\"out_rpm\":%.1f,\"out_hz\":%.2f,\"pulses\":%lu},"
      "{\"id\":2,\"active\":%s,\"ratio\":%.2f,\"in_rpm\":%.1f,\"in_hz\":%.2f,\"out_rpm\":%.1f,\"out_hz\":%.2f,\"pulses\":%lu}"
    "],"
    "\"wifi\":{\"sta_ssid\":\"%s\",\"ap_ssid\":\"%s\",\"sta_connected\":%s,"
    "\"sta_ip\":\"%u.%u.%u.%u\",\"ap_ip\":\"%u.%u.%u.%u\",\"rssi\":%d}}",
    simulating ? "true" : "false",
    (unsigned long)(millis() / 1000UL),
    ch0Active ? "true" : "false", g_cfg.ratio[0], getChannelInputRPM(0), getChannelInputHz(0), getChannelOutputRPM(0), getChannelOutputHz(0), (unsigned long)g_ch[0].totalPulses,
    ch1Active ? "true" : "false", g_cfg.ratio[1], getChannelInputRPM(1), getChannelInputHz(1), getChannelOutputRPM(1), getChannelOutputHz(1), (unsigned long)g_ch[1].totalPulses,
    escStaSsid, escApSsid, staConn ? "true" : "false",
    staIp[0], staIp[1], staIp[2], staIp[3],
    apIp[0],  apIp[1],  apIp[2],  apIp[3],
    staConn ? (int)WiFi.RSSI() : -100
  );

  g_server.sendHeader("Cache-Control", "no-store");
  g_server.send(200, "application/json", json);
}

static void handleApiRatios() {
  bool changed = false;
  if (g_server.hasArg("ratio0")) {
    float r0 = g_server.arg("ratio0").toFloat();
    if (r0 >= 0.10f && r0 <= 10.00f) {
      applyChannelRatio(0, r0);
      changed = true;
    }
  }
  if (g_server.hasArg("ratio1")) {
    float r1 = g_server.arg("ratio1").toFloat();
    if (r1 >= 0.10f && r1 <= 10.00f) {
      applyChannelRatio(1, r1);
      changed = true;
    }
  }

  if (!changed) {
    g_server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_ratio\"}");
    return;
  }

  saveConfigToEEPROM();
  g_server.send(200, "application/json", "{\"ok\":true}");
}

static void handleApiWifi() {
  if (g_server.hasArg("ap_ssid")) {
    String apSsid = g_server.arg("ap_ssid");
    apSsid.trim();
    if (apSsid.length() > 0 && apSsid.length() <= 32) {
      strncpy(g_cfg.apSsid, apSsid.c_str(), sizeof(g_cfg.apSsid) - 1);
      g_cfg.apSsid[sizeof(g_cfg.apSsid) - 1] = '\0';
    }
  }

  if (g_server.hasArg("ap_pass")) {
    String apPass = g_server.arg("ap_pass");
    if (apPass.length() > 0) {
      if (apPass.length() < 8 || apPass.length() > 64) {
        g_server.send(400, "application/json", "{\"ok\":false,\"error\":\"ap_pass_length\"}");
        return;
      }
      strncpy(g_cfg.apPass, apPass.c_str(), sizeof(g_cfg.apPass) - 1);
      g_cfg.apPass[sizeof(g_cfg.apPass) - 1] = '\0';
    }
  }

  if (g_server.hasArg("sta_ssid")) {
    String staSsid = g_server.arg("sta_ssid");
    staSsid.trim();
    strncpy(g_cfg.staSsid, staSsid.c_str(), sizeof(g_cfg.staSsid) - 1);
    g_cfg.staSsid[sizeof(g_cfg.staSsid) - 1] = '\0';
    g_cfg.staEnabled = (staSsid.length() > 0) ? 1 : 0;
  }

  if (g_server.hasArg("sta_pass")) {
    String staPass = g_server.arg("sta_pass");
    if (staPass.length() > 0 || !g_cfg.staEnabled) {
      strncpy(g_cfg.staPass, staPass.c_str(), sizeof(g_cfg.staPass) - 1);
      g_cfg.staPass[sizeof(g_cfg.staPass) - 1] = '\0';
    }
  }

  saveConfigToEEPROM();
  g_server.send(200, "application/json", "{\"ok\":true}");

  // Planifie l'application de la nouvelle configuration WiFi après l'envoi de la réponse HTTP
  g_wifiReconnectPending = true;
  g_wifiReconnectAtMs    = millis() + 250UL;
}

static void handleApiReset() {
  loadDefaultConfig();
  applyChannelRatio(0, g_cfg.ratio[0]);
  applyChannelRatio(1, g_cfg.ratio[1]);
  saveConfigToEEPROM();
  g_server.send(200, "application/json", "{\"ok\":true}");

  g_wifiReconnectPending = true;
  g_wifiReconnectAtMs    = millis() + 250UL;
}

static void setupWebServer() {
  g_server.on("/",           HTTP_GET,  handleRoot);
  g_server.on("/api/status", HTTP_GET,  handleApiStatus);
  g_server.on("/api/ratios", HTTP_POST, handleApiRatios);
  g_server.on("/api/wifi",   HTTP_POST, handleApiWifi);
  g_server.on("/api/reset",  HTTP_POST, handleApiReset);
  g_server.begin();
}

// ======================================================================================
//  GESTION VISUELLE DE LA LED D'ÉTAT (NON-BLOQUANTE)
//  - Simulation active (au moins 1 canal simule des RPM) : LED ALLUMÉE FIXE
//  - Attente de signal (aucun ventilateur en rotation)   : 1 flash court (100 ms) / 2 s
// ======================================================================================
static void updateStatusLed() {
  uint32_t nowMs = millis();
  if ((uint32_t)(nowMs - g_lastLedUpdateMs) < 25UL) {
    return;
  }
  g_lastLedUpdateMs = nowMs;

  bool simulating = (isChannelActive(0) || isChannelActive(1));
  bool ledOn      = false;

  if (simulating) {
    ledOn = true; // Allumée fixe pendant la simulation active
  } else {
    // Flash court d'attente (100 ms toutes les 2000 ms)
    ledOn = ((nowMs % 2000UL) < 100UL);
  }

  // LED_BUILTIN sur Wemos D1 Mini (D4/GPIO2) est active à l'état BAS (LOW)
  digitalWrite(PIN_STATUS_LED, ledOn ? LOW : HIGH);
  // LED externe optionnelle sur D7 (GPIO13) est active à l'état HAUT (HIGH)
  digitalWrite(PIN_EXT_LED,    ledOn ? HIGH : LOW);
}

// ======================================================================================
//  INITIALISATION DU MOTEUR TEMPS-RÉEL (GPIO + INTERRUPTIONS + NMI TIMER1)
// ======================================================================================
static void initRealtimeTachEngine() {
  uint32_t cpuMhz        = ESP.getCpuFreqMHz();
  g_cyclesPerUs          = cpuMhz;
  g_debounceMinCycles    = DEBOUNCE_MIN_PERIOD_US * cpuMhz;
  g_fanTimeoutCycles     = FAN_TIMEOUT_US         * cpuMhz;
  g_minOutHalfCycles     = MIN_OUT_HALF_PERIOD_US * cpuMhz;
  g_maxNmiSleepCycles    = MAX_NMI_SLEEP_US       * cpuMhz;
  g_nmiSpinWindowCycles  = NMI_SPIN_WINDOW_US     * cpuMhz;
  g_nmiPrescheduleCycles = NMI_PRESCHEDULE_US    * cpuMhz;

  memset((void*)g_ch,  0, sizeof(g_ch));
  memset((void*)g_nmi, 0, sizeof(g_nmi));

  applyChannelRatio(0, g_cfg.ratio[0]);
  applyChannelRatio(1, g_cfg.ratio[1]);

  // Configuration des sorties vers les transistors NPN collecteur ouvert (repos = LOW)
  pinMode(PIN_TACH_OUT_CH1, OUTPUT);
  pinMode(PIN_TACH_OUT_CH2, OUTPUT);
  GPOC = MASK_OUT_CH1 | MASK_OUT_CH2;

  // Configuration des entrées tachymètres Noctua (pull-up externe 4,7 kΩ + pull-up interne)
  pinMode(PIN_TACH_IN_CH1, INPUT_PULLUP);
  pinMode(PIN_TACH_IN_CH2, INPUT_PULLUP);

  // Attachement des interruptions matérielles sur front descendant (IRAM)
  attachInterrupt(digitalPinToInterrupt(PIN_TACH_IN_CH1), isrTachInCh1, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_TACH_IN_CH2), isrTachInCh2, FALLING);

  // Configuration du Timer1 matériel (FRC1) en interruption non-masquable (NMI - Niveau 3)
  timer1_disable();
  ETS_FRC_TIMER1_INTR_ATTACH(NULL, NULL);
  ETS_FRC_TIMER1_NMI_INTR_ATTACH(onTimer1NMI);
  timer1_enable(TIM_DIV1, TIM_EDGE, TIM_SINGLE);
  timer1_write(800UL); // Premier déclenchement dans 10 µs (@ 80 MHz)
}

// ======================================================================================
//  SETUP & LOOP ARDUINO
// ======================================================================================
void setup() {
  // Configuration immédiate des broches de sortie au repos sûr (transistors NPN bloqués)
  pinMode(PIN_TACH_OUT_CH1, OUTPUT);
  pinMode(PIN_TACH_OUT_CH2, OUTPUT);
  digitalWrite(PIN_TACH_OUT_CH1, LOW);
  digitalWrite(PIN_TACH_OUT_CH2, LOW);

  pinMode(PIN_STATUS_LED, OUTPUT);
  pinMode(PIN_EXT_LED,    OUTPUT);
  digitalWrite(PIN_STATUS_LED, HIGH); // LED_BUILTIN éteinte initialement
  digitalWrite(PIN_EXT_LED,    LOW);

  Serial.begin(115200);

  // 1. Chargement de la configuration persistante (EEPROM + CRC32)
  loadConfigFromEEPROM();

  // 2. Démarrage immédiat du moteur temps-réel NMI (opérationnel avant même le WiFi)
  initRealtimeTachEngine();

  // 3. Démarrage du WiFi simultané AP + STA et du serveur Web
  setupWiFi();
  setupWebServer();
}

void loop() {
  // Le traitement tachymétrique est 100 % isolé en interruption IRAM + NMI Timer1.
  // La boucle loop() ne gère que le serveur HTTP, la supervision WiFi et la LED d'état.
  g_server.handleClient();
  updateStatusLed();

  uint32_t nowMs = millis();

  // Reconfiguration WiFi différée après modification dans l'interface Web
  if (g_wifiReconnectPending && (int32_t)(nowMs - g_wifiReconnectAtMs) >= 0) {
    g_wifiReconnectPending = false;
    setupWiFi();
  }

  // Surveillance périodique non-bloquante de la connexion STA (toutes les 15 s)
  if (g_cfg.staEnabled && strlen(g_cfg.staSsid) > 0) {
    if ((uint32_t)(nowMs - g_lastWifiCheckMs) >= 15000UL) {
      g_lastWifiCheckMs = nowMs;
      if (WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
      }
    }
  }
}
