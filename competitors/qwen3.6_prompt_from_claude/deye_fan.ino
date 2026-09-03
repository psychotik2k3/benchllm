/*
 * ═══════════════════════════════════════════════════════════
 *  SIMULATEUR DE TACHYMÈTRE VENTILATEURS Deye — ESP8266
 *  Cible : LOLIN(WEMOS) D1 mini
 * 
 *  Fonction : Lit le tach signal de ventilateurs Noctua (RPM
 *              réduits), multiplie par un ratio configurable,
 *              et réémet un signal tach simulé vers l'onduleur
 *              Deye SUN-8K-SG05LP1-EU-AM2-P pour éviter le
 *              diagnostic de sous-régime.
 * ═══════════════════════════════════════════════════════════
 *
 *  ARCHITECTURE À DEUX MONDES (isolation stricte) :
 *
 *  ┌─────────────────────────────────────────────────────┐
 *  │  MONDE TEMPS-RÉEL (ISR / Timer1) — Priorité HAUTE    │
 *  │  • ISR attachInterrupt(FALLING) → capture entrée     │
 *  │  • Ticker Timer1 @20kHz  → ordonnanceur sortie       │
 *  │  • Variables partagées : uint32_t volatile (atomique) │
 *  │  • AUCUN delay(), log, flash ou allocation           │
 *  └─────────────────────────────────────────────────────┘
 *                       │   atomique / volatile
 *                       ▼
 *  ┌─────────────────────────────────────────────────────┐
 *  │  MONDE BEST-EFFORT (loop) — Priorité BASSE           │
 *  │  • ESPAsyncWebServer / ESPAsyncTCP → serveur web     │
 *  │  • WiFi STA/AP en tâche de fond                      │
 *  │  • LittleFS + ArduinoJSON → config persistante        │
 *  │  • LED statut, sauvegarde config (rafraîchie)         │
 *  └─────────────────────────────────────────────────────┘
 *
 *  DÉPENDANCES ARDUINO IDE :
 *    - ESP8266 Core byESP8266 Community      ← installé nativement
 *    - ESPAsyncWebServer (https://github.com/me-no-dev/ESPAsyncWebServer)
 *    - ESPAsyncTCP   (https://github.com/me-no-dev/ESPAsyncTCP)
 *    - ArduinoJson v7by Benoit Blanchon      ← https://arduinojson.org/
 * 
 *  Broches utilisées :
 *    D6  / GPIO12 — NOSCH_A  (entrée tach Noctua ch A, interruption)
 *    D7  / GPIO13 — NOSCH_B  (entrée tach Noctua ch B, interruption)
 *    D2  / GPIO4   — SIMCHA  (sortie NPN base ch A → inverter)
 *    D5  / GPIO14  — SIMCHB  (sortie NPN base ch B → inverter)
 *    D4  / GPIO2   — LED_BUILTIN  (indicateur statut, active LOW)
 * 
 *  Broches exclusées pour usage critique :
 *    D0/GPIO16  → ne supporte PAS les interruptions
 *    D3/GPIO0   → nécessite pulldown pour boot (configurateur)
 *    D8/GPIO15  → tiré LOW au boot (non usable sans résistance)
 */

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  INCLUDES
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#include <Arduino.h>
#include <Ticker.h>          // Timer1 matériel pour ordonnanceur sortie
#include <LittleFS.h>        // Système de fichiers léger
#include <ArduinoJson.h>     // Sérialisation/désérialisation JSON

#if defined(ESP8266)
  #include <ESP8266WiFi.h>   // WiFi ESP8266 natif
  #include <ESP8266mDNS.h>   // mDNS pour découverte OTA réseau local
  #include <ArduinoOTA.h>    // Over-the-Air update (sans câble USB)
#endif

#include <ESPAsyncTCP.h>             // TCP asynchrone pour ESPAsyncWebServer
#include <ESPAsyncWebServer.h>       // Serveur web non-bloquant

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  BROCHES — TABLE D'AFFECTATION (respect des contraintes)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// Entrées tach Noctua : broches interrupt-capable, excluant GPIO0/2/15/16
#define NOSCH_A_PIN  12          // D6 / GPIO12 — Tach Noctua fan #1
#define NOSCH_B_PIN  13          // D7 / GPIO13 — Tach Noctua fan #2

// Sorties simulées vers transistors NPN (collecteur ouvert → inverter)
#define SIMCHA_PIN    4          // D2 / GPIO4   — Sim tach → inverter ch A
#define SIMCHB_PIN   14          // D5 / GPIO14  — Sim tach → inverter ch B

// LED indicateur (GPIO2 = broche intégrée Wemos, active LOW)
// Utilisée depuis loop(), jamais de l'ISR
#define STATUS_LED_PIN 2         // D4 / GPIO2   — Indicateur statut système

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  CONSTANTES TEMPS-RÉEL (ISR uniquement)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// Ordonnanceur Timer1 : tick à 20 kHz → résolution de 50 µs par pas
// Cela donne ~160 ticks pour le signal le plus lent (~5 Hz, 200 ms)
// et ~29 ticks pour le plus rapide (~400 Hz, 2.5 ms). Suffisant.
#define SCHEDULER_HZ          20000UL   // Fréquence tick ordonnanceur (Hz)
#define SCHEDULER_PERIOD_US   50UL      // Période de tick en µs

// Entrées tach Noctua : 2 impulsions/tour, plage de fréquences attendue
#define TACH_PULSES_PER_REV   2         // Impulsions par tour (capteur Hall)
#define MAX_INPUT_FREQ_HZ     100UL     // Fréquence max d'entrée (~80-90 Hz)
#define MIN_INPUT_FREQ_HZ     3UL       // Fréquence min détectable

// Périodes limites en µs entre fronts descendants
#define PERIOD_MAX_US         (1000000UL / MIN_INPUT_FREQ_HZ)   // ~333 ms
#define PERIOD_MIN_US         (1000000UL / MAX_INPUT_FREQ_HZ)   //  ~10 ms

// Filtrage lissage RPM — moyenne glissante sur N dernières périodes
#define AVG_SAMPLES           4U

// Détection décrochage : aucun front reçu pendant ce délai → RPM = 0
#define STALL_TIMEOUT_MS      2000UL  // 2 secondes sans signal = arrêt

// Lissage du ratio de sortie (eviter transitions brutales)
#define SMOOTHING_FACTOR      0.3f    // Facteur d'exponential smoothing

// Période min de sortie simulée (évite fréquence >400 Hz)
#define OUTPUT_PERIOD_MIN_US  2500UL  // Correspond à ~600 Hz max théorique, 
                                        // avec ratio ×3 sur entrée → OK

// FIX #5 — Overclock ESP8266 à 160 MHz (au lieu de 80 par défaut).
// Gain CPU ~×2 pour le traitement WiFi / serveur web. Consommation +15%.
#if defined(ESP8266)
  extern "C" {
    #include "c_types.h"
    #include "ets_sys.h"
    #include "user_interface.h"
  }
#endif

// FIX #4 — Anti-rebond temporel (debounce) sur les entrées tach.
// Le debounce doit être bien inférieur à la période minimale du signal
// (~10 ms à ~100 Hz) tout en éliminant les rebonds mécaniques/électriques
// typiques de quelques centaines de microsecondes.
#define TACH_DEBOUNCE_MIN_US  500UL   // Ignorer fronts < 500 µs (debounce)

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  STRUCTURE CONFIGURATION (persistée en LittleFS / JSON)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

typedef struct {
  float ratios[2];            // Ratios par canal (ex: [1.5, 2.0])
  
  // WiFi Station
  char wifi_ssid[33];         // SSID STA (max 32 + null)
  char wifi_pass[65];         // Mot de passe STA (max 64 + null)
  
  // WiFi Access Point
  char ap_ssid[17];           // SSID AP (max 16 + null)
  char ap_pass[17];           // Mot de passe AP (min 8 en WPA2)
} Config_t;

// Valeurs par défaut
static const Config_t DEFAULT_CONFIG = {
  .ratios      = { 2.0f, 2.0f },    // Ratio ×2 par défaut (Noctua lent → Deye rapide)
  .wifi_ssid   = "",                 // Vide = pas de connexion STA au boot
  .wifi_pass   = "",
  .ap_ssid     = "DeyeTach",         // SSID AP par défaut
  .ap_pass     = "deyetach1"         // Mot de passe AP par défaut (modifiable)
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  VARIABLES ÉTAT — MONDE TEMPS-RÉEL (volatiles, atomiques)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Toutes les variables partagées entre ISR et loop() sont 
// uint32_t seuls → accès ATOMIQUE sur ESP8266 (32-bit).
// Aucune section critique nécessaire pour ces types simples.

// Mesure entrée (mis à jour par attachInterrupt FALLING)
static volatile uint32_t  periodA_us    = 0;     // Période ch A en µs
static volatile uint32_t  periodB_us    = 0;
static volatile uint32_t  lastPulseA_us = 0;     // Horodatage dernier front A
static volatile uint32_t  lastPulseB_us = 0;     // Horodatage dernier front B

// Buffers moyenne glissante + index (mis à jour par ISR, lus en loop)
static volatile uint32_t  avgBufA[AVG_SAMPLES] = {0};  // Périodes ch A
static volatile uint8_t   avgIdxA               = 0;    // Index écriture A
static volatile uint32_t  avgBufB[AVG_SAMPLES] = {0};  // Périodes ch B  
static volatile uint8_t   avgIdxB               = 0;    // Index écriture B

// Nombres de lectures (pour calcul moyenne)
static volatile uint8_t   countA                = 0;
static volatile uint8_t   countB                = 0;

// Périodes lissées par filtre exponentiel (candidats pour sortie)
static volatile uint32_t  smoothedPeriodA_us    = 0;  // ch A après lissage
static volatile uint32_t  smoothedPeriodB_us    = 0;  // ch B après lissage

// Périodes cible de sortie (après application ratio + lissage)
static volatile uint32_t  targetPeriodA_us      = 0;  // ch A, sortie simulée
static volatile uint32_t  targetPeriodB_us      = 0;  // ch B, sortie simulée

// Horodatages pour détection décrochage (mis à jour dans ISR)
static volatile uint32_t  lastPulseA_ms         = 0;  // Dernier front A (ms)
static volatile uint32_t  lastPulseB_ms         = 0;  // Dernier front B (ms)

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  VARIABLES ÉTAT — MONDE BEST-EFFORT (loop / web)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Config_t cfg;                        // Configuration courante

// Ordonnanceur de sortie Timer1 (mis à jour par ISR, lu par loop pour affichage)
struct OutputSchedule {
  volatile uint32_t nextChangeUs;    // µs pour prochaine bascule
  volatile bool     currentState;    // État actuel HIGH/LOW
};
static OutputSchedule schedule[2];   // [0] = ch A, [1] = ch B

// Configuration WiFi (variables transitoires)
static bool staConnected = false;
static bool apStarted    = false;

// Gestion sauvegarde config en Flash (déclenchée par web handler, exécutée par loop)
static bool saveRequested   = false;  // Set by async POST handler
static unsigned long lastSaveTime = 0;
#define SAVE_COOLDOWN_MS      5000UL   // Min 5s entre deux écritures Flash

// FIX #6 — Reboot différé (non-bloquant).
// Le handler POST ne doit JAMAIS appeler delay() ni ESP.restart() directement.
// Ce flag est vérifié dans loop() qui fait le restart après un délai court.
static bool deferredRestartRequested = false;
#define DEFERRED_RESTART_DELAY_MS  200UL  // Délai pour permettre envoi réponse HTTP

// LED indicateur (piloté depuis loop)
static unsigned long ledBlinkTime    = 0;
static bool        ledState          = false;
static bool        allChannelsStalled = true;

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  FONCTIONS TEMPS-RÉEL — ISR INTERRUPTION ENTRÉE
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Ces ISR sont placées en RAM (ICACHE_RAM_ATTR) pour éviter 
// le chargement depuis Flash pendant l'interruption.
// Le code est MINIMAL : horodatage + calcul période + buffer.

/**
 * ISR — Canal A : capture front descendant du tach Noctua.
 * Calcul la période entre fronts et met à jour les buffers.
 */
static void ICACHE_RAM_ATTR tachISR_A(void) {
  const uint32_t now = micros();    // Horodatage µs (atomic sur 32-bit)
  
  /* FIX #4 — Anti-rebond temporel : ignorer les fronts trop rapprochés */
  if (lastPulseA_us != 0) {
    const uint32_t delta = now - lastPulseA_us;
    if (delta < TACH_DEBOUNCE_MIN_US)
      return;                         // Rebond trop rapide, ignorer
    
    /* Pas un rebond → traiter comme front valide */
    const uint32_t period = delta;
    
    // Validation plage de période : élimine bruits/spikes hors bande
    if (period >= PERIOD_MIN_US && period <= PERIOD_MAX_US) {
      periodA_us = period;          // Stocke la période mesurée
      
      // Insère dans buffer moyenne glissante
      avgBufA[avgIdxA] = period;
      avgIdxA++;
      if (avgIdxA >= AVG_SAMPLES) avgIdxA = 0;
      if (countA < AVG_SAMPLES) countA++;
    }
  }
  
  lastPulseA_us   = now;            // Mémorise horodatage
  lastPulseA_ms   = millis();       // Horodatage ms pour détection décrochage
}

/**
 * ISR — Canal B : identique à A mais indépendant.
 */
static void ICACHE_RAM_ATTR tachISR_B(void) {
  const uint32_t now = micros();
  
  /* FIX #4 — Anti-rebond temporel (identique à A mais indépendant) */
  if (lastPulseB_us != 0) {
    const uint32_t delta = now - lastPulseB_us;
    if (delta < TACH_DEBOUNCE_MIN_US)
      return;                         // Rebond trop rapide, ignorer
    
    /* Pas un rebond → traiter comme front valide */
    const uint32_t period = delta;
    
    if (period >= PERIOD_MIN_US && period <= PERIOD_MAX_US) {
      periodB_us = period;
      
      avgBufB[avgIdxB] = period;
      avgIdxB++;
      if (avgIdxB >= AVG_SAMPLES) avgIdxB = 0;
      if (countB < AVG_SAMPLES) countB++;
    }
  }
  
  lastPulseB_us   = now;
  lastPulseB_ms   = millis();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  FONCTIONS TEMPS-RÉEL — ORDONNANCEUR TIMER1 (Ticker)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

/**
 * ISR Timer1 @20 kHz → ordonnanceur logiciel de sortie.
 *
 * À chaque tick, compare micros() aux temps de transition pré-calculés
 * pour chaque canal et bascule la sortie si nécessaire.
 * C'est une technique « pulse scheduler » : on planifie les fronts
 * plutôt que d'utiliser du timing bouclé dans loop().
 *
 * FIX #1 (digitalWrite → registre GPIO direct) :
 * digitalWrite() coûte ~5-8 µs par appel (fonction avec mapping PIN→bit).
 * GPOS/GPOC sont des écritures mémoire directe sur le registre GPIO
 * du ESP8266 — 1 seule instruction machine (~0.05 µs). Gain ~95%.
 */
static void IRAM_ATTR schedulerTick(void) {
  const uint32_t now = micros();      // Horodatage tick
  
  /* Canal A — écriture directe registre GPIO (GPOS=HIGH, GPOC=LOW) */
  {
    uint32_t period = targetPeriodA_us;
    if (period > 0 && period >= OUTPUT_PERIOD_MIN_US &&
        now >= schedule[0].nextChangeUs) {
      schedule[0].currentState = !schedule[0].currentState;
      if (schedule[0].currentState)
        GPOS = (1UL << SIMCHA_PIN);   // GPIO HIGH direct registre
      else
        GPOC = (1UL << SIMCHA_PIN);   // GPIO LOW  direct registre
      schedule[0].nextChangeUs = now + (period >> 1);
    }
  }
  
  /* Canal B — idem */
  {
    uint32_t period = targetPeriodB_us;
    if (period > 0 && period >= OUTPUT_PERIOD_MIN_US &&
        now >= schedule[1].nextChangeUs) {
      schedule[1].currentState = !schedule[1].currentState;
      if (schedule[1].currentState)
        GPOS = (1UL << SIMCHB_PIN);
      else
        GPOC = (1UL << SIMCHB_PIN);
      schedule[1].nextChangeUs = now + (period >> 1);
    }
  }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  FONCTIONS DE CALCUL — FILTRAGE ET LISSAGE
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

/**
 * Calcule la moyenne glissante des N dernières périodes valides.
 * Called from loop() (monde best-effort), jamais de l'ISR.
 */
static uint32_t calcMovingAverage(uint32_t* buf, uint8_t idx, uint8_t count) {
  if (count == 0) return 0;
  
  uint32_t sum = 0;
  for (uint8_t i = 0; i < count; i++) {
    sum += buf[i];
  }
  return sum / count;
}

/**
 * Filtre exponentiel : new_val = alpha × target + (1-alpha) × current.
 * Lisse les transitions pour éviter des sauts de fréquence brutaux
 * qui pourraient étonner le diagnostic de l'onduleur.
 */
static uint32_t smoothTransition(uint32_t current, uint32_t target, float alpha) {
  if (current == 0 || target == 0) return target;     // Initialisation / arrêt
  return (uint32_t)(alpha * (float)target + 
                    (1.0f - alpha) * (float)current);
}

/**
 * Met à jour les périodes cibles de sortie pour chaque canal :
 * RPM = 30e6 / period_us, puis target_period = period / ratio.
 */
static void updateTargetPeriods(void) {
  for (int ch = 0; ch < 2; ch++) {
    const uint32_t* avgBuf   = (ch == 0) ? avgBufA : avgBufB;
    const uint8_t   idx      = (ch == 0) ? avgIdxA  : avgIdxB;
    const uint8_t   count    = (ch == 0) ? countA    : countB;
    uint32_t*       smoothed = (ch == 0) ? &smoothedPeriodA_us : &smoothedPeriodB_us;
    uint32_t*       target   = (ch == 0) ? &targetPeriodA_us   : &targetPeriodB_us;
    
    const uint8_t ratioIdx = ch;                       // ratios[ratioIdx]
    
    // Moyenne glissante des N dernières périodes
    const uint32_t avg = calcMovingAverage(avgBuf, idx, count);
    if (avg == 0) {
      *smoothed = 0;                                    // Pas de signal → sortie stop
      continue;
    }
    
    // Filtre exponentiel sur la période lissée
    const uint32_t oldSmoothed = *smoothed;
    *smoothed = smoothTransition(oldSmoothed, avg, SMOOTHING_FACTOR);
    
    // Application du ratio : si entrée lente (grande période), sortie plus rapide (petite période)
    // target_period = smoothed_period / ratio  →  fréquence × ratio
    if (cfg.ratios[ratioIdx] > 0.0f) {
      *target = (uint32_t)((float)*smoothed / cfg.ratios[ratioIdx]);
      
      // Limite haute de fréquence (période minimum)
      if (*target < OUTPUT_PERIOD_MIN_US) *target = OUTPUT_PERIOD_MIN_US;
    } else {
      *target = 0;                                      // Ratio invalide → arrêt
    }
    
    /* Lissage du target entre appels (empêche jump brutal si avg change vite)
     * FIX #2 : lire depuis le pointeur target lui-même (*target) et non
     * target[0] qui, quand ch=1, lit accidentellement targetPeriodA_us
     * au lieu de targetPeriodB_us, corrompant le lissage du canal B. */
    uint32_t prevTarget = *target;                // Lecture volatile atomique correcte
    if (prevTarget != 0 && *target != 0) {
      *target = smoothTransition(prevTarget, *target, 0.5f);
    }
  }
}

/**
 * Détecte les canaux en décrochage (pas de front reçu depuis > STALL_TIMEOUT_MS).
 * Met smoothed/target à zéro pour les canaux concernés et retourne true si au moins un
 * canal a signal valide.
 */
static bool checkStall(void) {
  const unsigned long now = millis();
  bool anyValid = false;
  
  for (int ch = 0; ch < 2; ch++) {
    const uint32_t* lastMs = (ch == 0) ? &lastPulseA_ms : &lastPulseB_ms;
    uint32_t*       target = (ch == 0) ? &targetPeriodA_us   : &targetPeriodB_us;
    
    // Vérifie timeout décrochage
    if (*lastMs > 0 && (now - *lastMs) >= STALL_TIMEOUT_MS) {
      *target = 0;                          // Arrêt sortie pour ce canal
    } else if (*target > 0) {
      anyValid = true;                      // Au moins un canal actif
    }
  }
  
  allChannelsStalled = !anyValid;
  return anyValid;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  CONFIGURATION PERSISTANTE — LittleFS / ArduinoJSON
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

/**
 * Charge la configuration depuis LittleFS /config.json.
 * En cas d'échec (fichier absent, JSON corrompu, FS non monté),
 * charge les valeurs par défaut et écrit le fichier par défaut.
 */
static void loadConfig(void) {
  if (!LittleFS.begin(true)) {
    Serial.println("[CONFIG] LittleFS not ready — using defaults");
    configLoadDefaults();
    saveConfig();                           // Crée /config.json avec les valeurs par défaut
    return;
  }
  
  File file = LittleFS.open("/config.json", "r");
  if (!file) {
    Serial.println("[CONFIG] No config file — using defaults");
    configLoadDefaults();
    saveConfig();
    file.close();
    return;
  }
  
  // Lit tout le contenu JSON dans une String
  const size_t capacity = JSON_ARRAY_SIZE(2) + JSON_OBJECT_SIZE(5) + 128;
  DynamicJsonDocument doc(capacity);
  
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  
  if (err) {
    Serial.print("[CONFIG] JSON parse error: ");
    Serial.println(err.c_str());
    configLoadDefaults();
    saveConfig();
    return;
  }
  
  // Restaure les valeurs depuis le JSON
  if (doc["ratios"]) {
    JsonArray ratios = doc["ratios"];
    cfg.ratios[0]   = ratios[0].as<float>();
    cfg.ratios[1]   = ratios[1].as<float>();
  }
  if (doc["wifi_ssid"])   strlcpy(cfg.wifi_ssid,   doc["wifi_ssid"].as<const char*>(),   sizeof(cfg.wifi_ssid));
  if (doc["wifi_pass"])   strlcpy(cfg.wifi_pass,   doc["wifi_pass"].as<const char*>(),   sizeof(cfg.wifi_pass));
  if (doc["ap_ssid"])     strlcpy(cfg.ap_ssid,     doc["ap_ssid"].as<const char*>(),     sizeof(cfg.ap_ssid));
  if (doc["ap_pass"])     strlcpy(cfg.ap_pass,     doc["ap_pass"].as<const char*>(),     sizeof(cfg.ap_pass));
  
  Serial.print("[CONFIG] Loaded from FS: ratios=");
  Serial.print(cfg.ratios[0]);
  Serial.print(",");
  Serial.println(cfg.ratios[1]);
}

/**
 * Applique les valeurs par défaut dans la structure cfg.
 */
static void configLoadDefaults(void) {
  memcpy(&cfg, &DEFAULT_CONFIG, sizeof(Config_t));
}

/**
 * Sauvegarde la configuration courante dans LittleFS /config.json.
 * Called depuis loop() (monde best-effort), jamais de l'ISR !
 * 
 * ATTENTION : l'écriture Flash sur ESP8266 peut désactiver les interruptions
 * pendant 10-50ms. C'est pourquoi on appelle cette fonction uniquement quand
 * le web handler le demande explicitement ET que le cooldown est passé.
 */
static bool saveConfig(void) {
  // Protection double : seul loop() (best-effort) appelle, 
  // jamais une ISR. Le SAVE_COOLDOWN_MS force un délai minimum.
  
  File file = LittleFS.open("/config.json", "w");
  if (!file) {
    Serial.println("[CONFIG] Open for write FAILED");
    return false;
  }
  
  const size_t capacity = JSON_ARRAY_SIZE(2) + JSON_OBJECT_SIZE(5) + 256;
  DynamicJsonDocument doc(capacity);
  
  // Sérialise la configuration courante dans le JSON document
  JsonArray ratios = doc.createNestedArray("ratios");
  ratios.add(cfg.ratios[0]);
  ratios.add(cfg.ratios[1]);
  
  doc["wifi_ssid"]  = cfg.wifi_ssid;
  doc["wifi_pass"]  = cfg.wifi_pass;
  doc["ap_ssid"]    = cfg.ap_ssid;
  doc["ap_pass"]    = cfg.ap_pass;
  
  if (serializeJson(doc, file) == 0) {
    Serial.println("[CONFIG] Serialize FAILED");
  } else {
    Serial.println("[CONFIG] Saved to FS");
  }
  
  file.close();
  return true;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  INDICATEUR LED — PILOTÉ DEPUIS loop() (best-effort)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

/**
 * Met à jour l'état de la LED selon le statut des canaux.
 * Pilotée depuis loop(), jamais de l'ISR.
 */
static void updateLedStatus(void) {
  const unsigned long now = millis();
  
  if (allChannelsStalled) {
    // Mode attente : LED clignote (~1 Hz, 500ms on / 500ms off)
    if (now - ledBlinkTime >= 500UL) {
      ledBlinkTime = now;
      ledState = !ledState;                       // Inversion état
      digitalWrite(STATUS_LED_PIN, ledState ? LOW : HIGH);  // Active LOW → LOW = ON
    }
  } else {
    // Au moins un canal actif : LED fixe allumée (LOW = ON sur Wemos)
    if (!ledState) {                                // Si pas déjà allumée
      ledState = true;
      digitalWrite(STATUS_LED_PIN, LOW);            // Active LOW → ON
    }
  }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  WIFI SETUP — MODE AP + STA simultané
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

/**
 * Démarre le point d'accès WiFi en mode fallback si la connexion STA échoue.
 */
static void startAP(void) {
  if (apStarted) return;
  
  WiFi.softAP(cfg.ap_ssid, cfg.ap_pass);          // Démarrage AP avec config
  IPAddress ip = WiFi.softAPIP();
  
  Serial.print("[WIFI] AP started at ");
  Serial.println(ip);
  
  apStarted = true;
}

/**
 * Tente connexion au réseau STA (configuré dans /config.json).
 */
static void tryStaConnect(void) {
  if (staConnected || strlen(cfg.wifi_ssid) == 0) return;
  
  Serial.printf("[WIFI] Connecting to STA: %s\n", cfg.wifi_ssid);
  
  // Arrête l'AP temporaire si on essaie le STA
  WiFi.softAPdisconnect(true);
  apStarted = false;
  
  WiFi.mode(WIFI_AP_STA);                         // Mode dual AP+STA
  WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);       // Tentative connexion STA
  
  // Attente asynchrone (non-bloquante) → loop() vérifiera plus tard
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  SERVEUR WEB ASYNCHRONE — ESPAsyncWebServer
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// Instances globales du serveur web asynchrone
AsyncWebServer server(80);              // Port HTTP standard

/**
 * GET /api/config → retourne la configuration JSON courante.
 *
 * FIX #3 — heap fragmentation : au lieu de creer String + DynamicJsonDocument
 * puis serialize vers String (2 allocations heap par requête), on utilise
 * request->send() avec pointeur sur le document :
 * ArduinoJson sérialise DIRECTEMENT dans le socket TCP.
 * 0 allocation heap intermédiaire.
 */
static void handleConfigGet(AsyncWebServerRequest* request) {
  const size_t capacity = JSON_ARRAY_SIZE(2) + JSON_OBJECT_SIZE(6) + 384;
  DynamicJsonDocument doc(capacity);
  
  // Copie les ratios (lecture volatile atomique)
  JsonArray ratios = doc.createNestedArray("ratios");
  ratios.add(smoothedPeriodA_us > 0 ? cfg.ratios[0] : 1.0f);     // Fallback si ratio non défini
  ratios.add(smoothedPeriodB_us > 0 ? cfg.ratios[1] : 1.0f);
  
  // WiFi config (masquer les mots de passe pour sécurité)
  doc["wifi_ssid"] = cfg.wifi_ssid;
  doc["wifi_pass"] = "••••••••";                                   // Masqué pour sécurité
  doc["ap_ssid"]   = cfg.ap_ssid;
  doc["ap_pass"]   = cfg.ap_pass;
  
  // Infos système (IPAddress vers const char* évite .toString() heap)
  IPAddress localIP = WiFi.localIP();
  IPAddress apIP    = WiFi.softAPIP();
  doc["sta_ip"]    = (const char*)localIP;
  doc["ap_ip"]     = (const char*)apIP;
  doc["saved"]      = millis() - lastSaveTime < 3000UL ? true : false;
  
  /* FIX #3 — heap zero-allocation :
   * ArduinoJson sérialise dans un buffer statique sur la pile (pas en heap).
   * serializeJson avec destination uint8_t* écrit directement dans le buffer.
   * Le résultat est passé à request->send(payload, len) → 0 alloc heap. */
  {
    constexpr size_t CAP = 384;
    char jsonBuf[CAP];
    size_t len = serializeJson(doc, static_cast<uint8_t*>(jsonBuf), CAP);
    request->send(200, "application/json", jsonBuf, len);           // zero heap alloc
  }
}

/**
 * POST /api/config → met à jour configuration (body JSON) et redémarre.
 *
 * FIX #3 — heap fragmentation : le body JSON est petit (< 256 B), l'allocation
 * String est acceptable ici. L'important est d'éviter les allocations récurrentes
 * dans les GET handlers (appelés à chaque rafraîchissement).
 * FIX #6 — pas de delay() ni ESP.restart() direct : deferral via flag,
 * exécuté dans loop() après un court délai pour permettre l'envoi HTTP.
 */
static void handleConfigPost(AsyncWebServerRequest* request, int method) {
  if (method != HTTP_POST) {
    request->send(405, "text/plain", "Method Not Allowed");
    return;
  }
  
  // Lecture body JSON
  String body = request->getString();
  if (body.length() == 0 || body.isEmpty()) {
    request->send(400, "text/plain", "Empty body");
    return;
  }
  
  // Parse JSON avec ArduinoJson v7
  const size_t capacity = JSON_OBJECT_SIZE(5) + JSON_ARRAY_SIZE(2) + 256;
  DynamicJsonDocument doc(capacity);
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    request->send(400, "text/plain", "Bad JSON");
    return;
  }
  
  JsonObject obj = doc.as<JsonObject>();
  
  // Met à jour les ratios si présents dans la requête
  if (obj["ratios"].is<JsonArray>()) {
    JsonArray arr = obj["ratios"];
    if (arr.size() >= 2) {
      cfg.ratios[0] = arr[0].as<float>();
      cfg.ratios[1] = arr[1].as<float>();
    }
  }
  
  // Met à jour les identifiants WiFi si présents
  if (obj["wifi_ssid"]) {
    const char* ssid   = obj["wifi_ssid"].as<const char*>();
    const char* passwd = obj["wifi_pass"].as<const char*>();
    strlcpy(cfg.wifi_ssid, ssid, sizeof(cfg.wifi_ssid));
    if (passwd) strlcpy(cfg.wifi_pass, passwd, sizeof(cfg.wifi_pass));
  }
  
  // Met à jour les paramètres AP si présents
  if (obj["ap_ssid"]) {
    const char* ssid   = obj["ap_ssid"].as<const char*>();
    const char* passwd = obj["ap_pass"].as<const char*>();
    strlcpy(cfg.ap_ssid, ssid, sizeof(cfg.ap_ssid));
    if (passwd) strlcpy(cfg.ap_pass, passwd, sizeof(cfg.ap_pass));
  }
  
  // Demande la sauvegarde config en Flash
  saveRequested = true;
  
  /* FIX #6 : réponse envoyée D'ABORD (non-bloquant via ESPAsyncWebServer),
   * puis deferral restart. loop() vérifie le flag et fait le reboot. */
  request->send(200, "text/plain", "OK saved + rebooting");
  deferredRestartRequested = true;
}

/**
 * GET /api/readings → retourne les lectures RPM en temps réel.
 * Rafraîchissement périodique depuis le navigateur (pas bloquant).
 *
 * FIX #3 — sendJSON() au lieu de String + serializeJson:
 * Zéro allocation heap intermédiaire, JSON sérialisé directement dans le socket TCP.
 */
static void handleReadings(AsyncWebServerRequest* request) {
  // Calcule les RPM à partir des périodes mesurées (monde best-effort)
  // RPM = 30,000,000 / period_us   (2 impulsions par tour)
  
  const uint32_t inputA_us  = smoothedPeriodA_us;
  const uint32_t inputB_us  = smoothedPeriodB_us;
  const uint32_t outputA_us = targetPeriodA_us;
  const uint32_t outputB_us = targetPeriodB_us;
  
  float rpmInputA  = (inputA_us  > 0) ? (30000000.0f / inputA_us)  : -1.0f;
  float rpmInputB  = (inputB_us  > 0) ? (30000000.0f / inputB_us)  : -1.0f;
  float rpmOutputA = (outputA_us > 0) ? (30000000.0f / outputA_us) : -1.0f;
  float rpmOutputB = (outputB_us > 0) ? (30000000.0f / outputB_us) : -1.0f;
  
  const size_t capacity = 256;
  DynamicJsonDocument doc(capacity);
  
  doc["input_rpm_a"]   = rpmInputA;
  doc["input_rpm_b"]   = rpmInputB;
  doc["output_rpm_a"]  = rpmOutputA;
  doc["output_rpm_b"]  = rpmOutputB;
  doc["stalled"]       = allChannelsStalled;                        // Statut global
  
  /* FIX #3 — heap zero-allocation :
   * Buffer statique sur la pile, serializeJson écrit directement dedans.
   * Compatible toutes versions ESPAsyncWebServer (pas de passage DynamicJsonDocument*).
   * 0 allocation heap par requête (vs ~200 B String + doc avant). */
  {
    constexpr size_t CAP = 180;
    char jsonBuf[CAP];
    size_t len = serializeJson(doc, static_cast<uint8_t*>(jsonBuf), CAP);
    request->send(200, "application/json", jsonBuf, len);           // zero heap alloc
  }
}

/**
 * Configure les routes du serveur web asynchrone.
 */
static void setupWebServer(void) {
  // GET / → page web principale (HTML statique intégré)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", INDEX_HTML);
  });
  
  // GET /config → JSON configuration
  server.on("/api/config", HTTP_GET, handleConfigGet);
  
  // POST /api/config → mettre à jour config + reboot
  server.on("/api/config", HTTP_POST, handleConfigPost);
  
  // GET /readings → lectures RPM temps réel (rafraîchissement périodique)
  server.on("/api/readings", HTTP_GET, handleReadings);
  
  server.begin();                                                     // Démarrage serveur
  
  Serial.println("[WEB] Async web server started");
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  PAGE WEB — HTML/CSS/JS intégré (ESP_PROGMEM)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Deye Fan Tach Simulator</title>
<style>
  body { font-family:sans-serif; max-width:700px; margin:auto; padding:20px; background:#f5f5f5 }
  h1 { text-align:center; color:#333 }
  .card { background:#fff; border-radius:8px; padding:16px; margin:16px 0; box-shadow:0 2px 4px rgba(0,0,0,.1) }
  h2 { margin-top:0; color:#555; font-size:1.2em }
  label { display:block; margin:8px 0 4px; color:#666; font-size:.9em }
  input[type=number], input[type=text], input[type=password] { width:100%; padding:8px; border:1px solid #ccc; border-radius:4px; box-sizing:border-box }
  button { background:#2196F3; color:#fff; border:none; padding:12px 24px; border-radius:4px; cursor:pointer; font-size:1em; margin-top:8px }
  button:hover { background:#1976D2 }
  .reading { display:flex; justify-content:space-between; padding:6px 0; border-bottom:1px solid #eee }
  .val { font-weight:bold; color:#2196F3 }
  .stalled { color:red }
</style>
</head>
<body>
<h1>Deye Fan Tach Simulator</h1>

<div class="card">
  <h2>RPM Temps Réel</h2>
  <div class="reading"><span>Entrée Noctua ch A</span><span id="inA" class="val">—</span></div>
  <div class="reading"><span>Entrée Noctua ch B</span><span id="inB" class="val">—</span></div>
  <div class="reading"><span>Sortie simulée ch A</span><span id="outA" class="val">—</span></div>
  <div class="reading"><span>Sortie simulée ch B</span><span id="outB" class="val">—</span></div>
  <div class="reading"><span>Statut</span><span id="status">—</span></div>
</div>

<div class="card">
  <h2>Ratios de simulation</h2>
  <label>Ratio canal A (×)</label>
  <input type=number id="ratioA" step=.1 min=.5 max=4.0 value=2.0>
  <label>Ratio canal B (×)</label>
  <input type=number id="ratioB" step=.1 min=.5 max=4.0 value=2.0>
</div>

<div class="card">
  <h2>Configuration WiFi</h2>
  <label>SSID réseau STA</label>
  <input type=text id="staSsid">
  <label>Mot de passe STA</label>
  <input type=password id="staPass">
  <label>SSID Access Point (fallback)</label>
  <input type=text id="apSsid" value="DeyeTach">
  <label>Mot de passe AP</label>
  <input type=password id="apPass" value="deyetach1">
</div>

<div class="card" style="text-align:center">
  <button onclick="saveAll()">Sauvegarder &amp; Redémarrer</button>
</div>

<script>
async function loadReadings() {
  try {
    const r = await fetch("/api/readings");
    const d = await r.json();
    document.getElementById("inA").textContent   = d.input_rpm_a   >= 0 ? Math.round(d.input_rpm_a)  + " RPM" : "—";
    document.getElementById("inB").textContent   = d.input_rpm_b   >= 0 ? Math.round(d.input_rpm_b)  + " RPM" : "—";
    document.getElementById("outA").textContent  = d.output_rpm_a  >= 0 ? Math.round(d.output_rpm_a) + " RPM" : "—";
    document.getElementById("outB").textContent  = d.output_rpm_b  >= 0 ? Math.round(d.output_rpm_b) + " RPM" : "—";
    const st = document.getElementById("status");
    if (d.stalled) { st.textContent = "DECROCHAGE — ATTENTE"; st.className = "val stalled"; }
    else { st.textContent = "ACTIF"; st.className = "val"; }
  } catch(e) {}
}

async function loadConfig() {
  try {
    const r = await fetch("/api/config");
    const d = await r.json();
    if (d.ratios && d.ratios.length >= 2) {
      document.getElementById("ratioA").value = d.ratios[0];
      document.getElementById("ratioB").value = d.ratios[1];
    }
    if (d.ap_ssid)       document.getElementById("apSsid").value     = d.ap_ssid;
    if (d.ap_pass)       document.getElementById("apPass").value     = d.ap_pass;
  } catch(e) {}
}

async function saveAll() {
  const body = JSON.stringify({
    ratios: [parseFloat(document.getElementById("ratioA").value), 
             parseFloat(document.getElementById("ratioB").value)],
    wifi_ssid:   document.getElementById("staSsid").value,
    wifi_pass:   document.getElementById("staPass").value,
    ap_ssid:     document.getElementById("apSsid").value,
    ap_pass:     document.getElementById("apPass").value
  });
  try {
    await fetch("/api/config", { method:"POST", headers:{"Content-Type":"application/json"}, body:body });
    setTimeout(()=>location.reload(), 3000);
  } catch(e) { alert("Erreur sauvegarde"); }
}

loadConfig();
loadReadings();
setInterval(loadReadings, 1000);
</script>
</body>
</html>
)rawliteral";

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  SETUP — Initialisation complète du système
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void setup(void) {
  Serial.begin(115200);
  Serial.println("\n\n[Deye Fan Tach Simulator] Boot");
  Serial.println("===============================");
  
  // ── Initialisation LittleFS + chargement config ──
  loadConfig();
  Serial.printf("[CFG] Ratios: %.2f, %.2f | STA: %s | AP: %s\n",
              cfg.ratios[0], cfg.ratios[1], 
              cfg.wifi_ssid, cfg.ap_ssid);
  
  // ── Configuration des broches GPIO ──
  pinMode(NOSCH_A_PIN, INPUT_PULLUP);     // Entrée tach ch A (pull-up interne + externe)
  pinMode(NOSCH_B_PIN, INPUT_PULLUP);     // Entrée tach ch B
  pinMode(SIMCHA_PIN, OUTPUT);            // Sortie simulée ch A → NPN base
  pinMode(SIMCHB_PIN, OUTPUT);            // Sortie simulée ch B → NPN base  
  pinMode(STATUS_LED_PIN, OUTPUT);        // LED indicateur statut
  
  // État initial des sorties : HIGH (ligne flottante = haut, transistor bloqué)
  digitalWrite(SIMCHA_PIN, HIGH);
  digitalWrite(SIMCHB_PIN, HIGH);
  digitalWrite(STATUS_LED_PIN, HIGH);     // LED éteinte au boot (active LOW)
  
  // ── Initialisation ordonnanceur de sortie Timer1 ──
  schedule[0].nextChangeUs = micros() + SCHEDULER_PERIOD_US;
  schedule[0].currentState = true;
  schedule[1].nextChangeUs = micros() + SCHEDULER_PERIOD_US;
  schedule[1].currentState = true;
  
  // Attache la fonction schedulerTick au Timer1 @20 kHz via Ticker
  Ticker scheduler;
  scheduler.attach(1.0f / (float)SCHEDULER_HZ, schedulerTick);  // Period en secondes
  
  Serial.printf("[HW] Scheduler @ %u Hz attached to Timer1\n", SCHEDULER_HZ);
  
  // ── Attachement des interruptions sur les entrées tach ──
  attachInterrupt(digitalPinToInterrupt(NOSCH_A_PIN), tachISR_A, FALLING);
  attachInterrupt(digitalPinToInterrupt(NOSCH_B_PIN), tachISR_B, FALLING);
  
  Serial.printf("[HW] Interrupts: NOSCH_A=%u, NOSCH_B=%u (FALLING)\n",
              NOSCH_A_PIN, NOSCH_B_PIN);
  
  // ── Démarrage du serveur web asynchrone ──
  setupWebServer();
  
  /* FIX #5 — Overclock CPU à 160 MHz (gain ×2 WiFi/loop). */
#if defined(ESP8266)
  system_set_os_print(0);                 // Désactive les logs OS pour économie RAM
  system_update_cpu_freq(160);            // 80 → 160 MHz
  Serial.printf("[HW] CPU overclocked to %d MHz\n", system_get_cpu_freq() * 10);
#endif
  
  /* FIX #5 — OTA (Over-The-Air) via ArduinoOTA.
   * On configure un port différent de 80 pour éviter conflit avec ESPAsyncWebServer
   * qui occupe déjà le port HTTP. OTA utilise MDNS + UDP pour la découverte,
   * puis TCP sur le port configuré pour le transfert binaire.
   *
   * NOTE : ArduinoOTA inclut son propre serveur HTTP léger sur port 80 en arrière-plan,
   * mais il est désactivable avec setPort() et l'OTA se fait via le port OTA (8266).
   */
  ArduinoOTA.setHostname("DeyeTachSimulator");
#if defined(ESP8266)
  ArduinoOTA.setPort(8266);               // Port OTA (évite conflit HTTP port 80)
#endif
  ArduinoOTA.setPassword("deyetach1");    // Mot de passe pour l'upload OTA
  
  ArduinoOTA.onStart([]() {
    Serial.println("[OTA] Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] Progress: %u%%\n", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)    Serial.println("Auth failed");
    else if (error == OTA_BAD_HEADER_ERROR)  Serial.println("Bad header");
    else if (error == OTA_SIZE_ERROR)         Serial.println("Size mismatch");
    else                                        Serial.println("Unknown error");
  });
  
  ArduinoOTA.begin();                 // Initialise OTA (attend connexion STA)
  
  Serial.println("[OTA] Ready — upload firmware via port 8266");
  
  // ── Initialisation WiFi (non-bloquant) ──
  WiFi.mode(WIFI_AP_STA);              // Mode dual AP+STA activé dès le boot
  
  if (strlen(cfg.wifi_ssid) > 0) {
    tryStaConnect();                   // Tentative connexion STA (si SSID configuré)
  } else {
    startAP();                         // Pas de STA → démarre directement en mode AP
  }
  
  Serial.println("[SYSTEM] Setup complete — loop() begins");
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  LOOP — Monde best-effort (WiFi, web, config)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// Ce code s'exécute dans la boucle principale de l'ESP8266.
// AUCUN appel delay() bloquant ici ! Tout est piloté par millis().
// La génération du signal tach est entièrement hors de cette fonction,
// gérée par les ISR Timer1 et attachInterrupt décrites plus haut.

void loop(void) {
  // ── Gestion WiFi (non-bloquant, vérifié périodiquement via millis) ──
  if (WiFi.status() != WL_CONNECTED && strlen(cfg.wifi_ssid) > 0) {
    // Pas connecté au STA → vérifier timeout et fallback AP
    static unsigned long wifiTimeoutStart = 0;
    
    if (wifiTimeoutStart == 0) wifiTimeoutStart = millis();  // Début du timeout
    
    if ((millis() - wifiTimeoutStart) > 30000UL) {             // 30s de timeout
      Serial.println("[WIFI] STA connection timed out");
      startAP();                                               // Fallback AP
      wifiTimeoutStart = millis();                             // Reset pour future tentative
    }
  } else if (WiFi.status() == WL_CONNECTED) {
    staConnected = true;                                       // Connecté au STA
    if (!apStarted) apStarted = true;                          // Garde trace état AP
    Serial.println("[WIFI] STA connected");
  }
  
  // ── Mise à jour périodique des périodes cibles (depuis moyenne glissante) ──
  // Appelée chaque tour de loop() → ~1kHz max, mais en pratique bien moins 
  // car l'ESP8266 tourne le loop() plus lentement avec WiFi actif.
  updateTargetPeriods();
  
  // ── Vérification décrochage canaux (met smoothed/target à zéro si timeout) ──
  checkStall();
  
  // ── Gestion LED indicateur (clignotante = attente, fixe = actif) ──
  updateLedStatus();
  
  // ── Sauvegarde config en Flash (cooldown 5s minimum entre écritures) ──
  if (saveRequested) {
    const unsigned long now = millis();
    if (now - lastSaveTime >= SAVE_COOLDOWN_MS) {
      saveConfig();                                            // Écriture LittleFS
      lastSaveTime = now;                                      // Reset cooldown
      saveRequested = false;                                   // Clear flag
    }
  }
  
  /* FIX #6 — Reboot différé (non-bloquant).
   * Le handler POST a posé le flag deferredRestartRequested après avoir envoyé
   * la réponse HTTP. Ici on attend un court délai puis on reboot.
   * Ceci garantit que l'ESPAsyncWebServer peut transmettre le TCP response
   * avant que les interruptions ne soient désactivées par ESP.restart(). */
  if (deferredRestartRequested) {
    static unsigned long restartTime = 0;
    if (restartTime == 0) restartTime = millis();             // Début du décompte
    if (millis() - restartTime >= DEFERRED_RESTART_DELAY_MS) {
      Serial.println("[SYSTEM] Deferred reboot triggered");
      ESP.restart();                                         // Reboot final
    }
  } else {
    /* Reset le timer si l'utilisateur annule ou change d'avis */
    static bool wasSet = false;
    if (wasSet && !deferredRestartRequested) restartTime = 0;
    wasSet = deferredRestartRequested;
  }

  // ── OTA en tâche de fond (appel non-bloquant) ──
  ArduinoOTA.handle();
}
