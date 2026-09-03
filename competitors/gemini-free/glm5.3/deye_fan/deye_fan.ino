/************************************************************************************************************************
 *  deye_fan.ino — v1.0 — "Deye Fan Tach"
 *  Multiplicateur de signaux tach pour onduleur hybride Deye SUN-8K-SG05LP1-EU-AM2-P, lorsque les ventilateurs
 *  NMB d'origine (bruyants) sont remplacés par des Noctua bien plus lentes.
 *
 *  PRINCIPE
 *  --------
 *  Ventilateurs d'origine : NMB 09225VE-12N-CU (92 mm) et NMB 06025VE-12N-CL (60 mm), 12 V, 3 fils, signal tach
 *  de 2 impulsions par tour (Hz = tr/min / 30). Ils sont remplacés par 2 x Noctua NF-A9-FLX (92 mm) et
 *  2 x Noctua NF-A6x25-FLX (60 mm) : débit d'air total équivalent, mais tach 2 à 3 fois plus lents.
 *
 *  Ce firmware :
 *    - mesure la fréquence tach de chaque paire de Noctua (tach à collecteur ouvert câblés en parallèle) ;
 *    - applique par canal un ratio multiplicateur réglable via l'interface web, SANS recompilation ;
 *    - régénère vers l'onduleur un signal carré 50 % à la fréquence multipliée, en collecteur ouvert (NPN),
 *      compatible avec la tension de pull-up de l'onduleur quelle qu'elle soit (3,3 / 5 / 12 V) ;
 *    - affiche les RPM mesurés et simulés sur une interface web (AP + STA simultanés, réglages en EEPROM).
 *
 *  ISOLATION TEMPORELLE (point clé)
 *  --------------------------------
 *  La génération du signal de sortie est faite EXCLUSIVEMENT dans l'interruption matérielle du timer FRC1
 *  (timer1) de l'ESP8266, en mode EDGE, ré-armée à chaque front à partir du compteur de cycles CPU (CCOUNT),
 *  comme le générateur de formes d'onde du coeur Arduino ESP8266. Conséquences :
 *    - le WiFi, le serveur web, le DNS ou une requête HTTP ne peuvent ni désynchroniser le signal, ni modifier
 *      durablement sa fréquence ;
 *    - chaque front est recalculé en TEMPS ABSOLU : aucune dérive cumulée ; la gigue se limite à la latence de
 *      l'interruption (~1 µs) et un front retardé (ex : écriture flash) est immédiatement resynchronisé ;
 *    - la mesure d'entrée n'est qu'un horodatage d'interruption GPIO : le comptage d'impulsions sur une fenêtre
 *      de temps est insensible à la charge réseau.
 *
 *  BROCHAGE (Wemos D1 mini V2.3.0 / ESP8266)
 *  -----------------------------------------
 *    D5  (GPIO14)  entrée tach canal 1 : tach des 2 Noctua 92 mm en parallèle (pull-up externe 10 k vers 3V3)
 *    D6  (GPIO12)  sortie tach canal 1 -> base NPN -> broche tach du connecteur ventilateur 92 mm de l'onduleur
 *    D7  (GPIO13)  entrée tach canal 2 : tach des 2 Noctua 60 mm en parallèle (pull-up externe 10 k vers 3V3)
 *    D1  (GPIO5)   sortie tach canal 2 -> base NPN -> broche tach du connecteur ventilateur 60 mm de l'onduleur
 *    D4  (GPIO2)   LED embarquée : FIXE = simulation active sur tous les canaux / CLIGNOTE = attente de signal
 *    5V            alimentation : buck 5 V alimenté via diodes Schottky par les DEUX connecteurs ventilateurs
 *    GND           masse commune avec l'onduleur (via les broches GND des connecteurs ventilateurs)
 *
 *  INTERFACE WEB : AP permanent "DeyeFan-xxxxxx" / "noctua12v" @ 192.168.4.1 (portail captif), STA + AP
 *  simultanés persistants (EEPROM + CRC), mDNS deye-fan.local ; ratios par canal, nb de tach en parallèle,
 *  mode "cache-panne", configuration Wi-Fi, scan, redémarrage.
 *
 *  ÉLECTRONIQUE : voir schema_electronique.md (alimentation, circuits d'entrée/sortie, BOM, contrôles).
 *
 *  LICENCE : MIT — fourni "tel quel", sans garantie. À tester SUR BANCALE avant mise en service. Aucune
 *  modification de l'onduleur n'est requise.
 ***********************************************************************************************************************/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <EEPROM.h>

// =====================================================================================================================
// 1. BROCHAGE ET CONSTANTES
// =====================================================================================================================
#define COUNT_CH 2

static const uint8_t PIN_CH1_IN  = 14;  // D5 : entrée tach canal 1 (92 mm)
static const uint8_t PIN_CH1_OUT = 12;  // D6 : sortie tach canal 1 (92 mm)
static const uint8_t PIN_CH2_IN  = 13;  // D7 : entrée tach canal 2 (60 mm)
static const uint8_t PIN_CH2_OUT = 5;   // D1 : sortie tach canal 2 (60 mm)
static const uint8_t PIN_LED     = 2;   // D4 : LED embarquée (allumée à l'état BAS)

// --- Conversion tach <-> RPM : 2 impulsions par tour, donc f(Hz) = RPM / 30
static const float TACH_PPR = 2.0f;
#define RPM_TO_HZ(rpm) ((rpm) / (60.0f / TACH_PPR))
#define HZ_TO_RPM(hz)  ((hz) * (60.0f / TACH_PPR))
static const float CPU_HZ = 80000000.0f;   // timer FRC1 et compteur de cycles : 80 MHz

// --- Limites de génération : FRC1 sur 23 bits @ 80 MHz -> demi-période max 104,8 ms -> f mini 4,77 Hz
static const float OUT_MIN_HZ = 5.0f;
static const float OUT_MAX_HZ = 1500.0f;

// --- Mesure des entrées
#define IN_DEBOUNCE_US  400UL       // anti-rebond : une impulsion tach réelle dure plus d'1 ms
#define IN_LOST_MS      2500UL      // aucun front pendant ce délai -> signal considéré perdu
#define BLIND_DELAY_MS  5000UL      // délai supplémentaire avant la simulation "cache-panne"
#define MEAS_PERIOD_MS  100UL       // période de mesure / asservissement de la sortie
#define MEAS_WINDOW_US  2000000UL   // fenêtre de mesure max (2 s)
#define EMA_ALPHA       0.25f       // lissage exponentiel de la fréquence mesurée
#define EDGE_BUF_BITS   7
#define EDGE_BUF_N      (1 << EDGE_BUF_BITS)    // 128 horodatages par canal
#define EDGE_BUF_MASK   (EDGE_BUF_N - 1)

// --- Générateur de sortie
#define IDLE_TICKS    80000UL   // 1 ms : cadence de l'ISR quand rien ne tourne (applique les demandes)
#define MIN_TICKS     100UL     // ré-armement mini (1,25 µs) : sécurité
#define LED_BLINK_MS  350UL     // cadence de clignotement de la LED en attente

// =====================================================================================================================
// 2. PARAMÈTRES PERSISTANTS (EEPROM, avec CRC)
// =====================================================================================================================
struct Settings {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  char     staSsid[33];             // Wi-Fi "station" : box/routeur
  char     staPass[65];
  char     apSsid[33];              // point d'accès permanent
  char     apPass[65];
  char     hostname[33];            // nom mDNS (ex : deye-fan.local)
  float    ratio[COUNT_CH];         // multiplicateur appliqué à la fréquence tach PAR VENTILATEUR
  uint8_t  parallelFans[COUNT_CH];  // nb de tach câblés en parallèle sur l'entrée (1..4), pour l'affichage RPM
  uint8_t  enabled[COUNT_CH];       // canal actif
  uint8_t  blindRun[COUNT_CH];      // "cache-panne" : simuler une fréquence fixe si le signal entrant est perdu
  float    blindRpm[COUNT_CH];      // RPM simulés en mode cache-panne
  uint32_t crc;
};

static const uint32_t SETTINGS_MAGIC   = 0xDEYEF001UL;
static const uint16_t SETTINGS_VERSION = 1;
static Settings S;

static uint32_t crc32buf(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFFUL;
  while (len--) {
    crc ^= *data++;
    for (uint8_t b = 0; b < 8; b++) crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1)));
  }
  return crc ^ 0xFFFFFFFFUL;
}

static void settingsDefaults() {
  memset(&S, 0, sizeof(S));
  S.magic   = SETTINGS_MAGIC;
  S.version = SETTINGS_VERSION;
  S.size    = sizeof(Settings);
  uint32_t id = ESP.getChipId() & 0xFFFFFFUL;
  snprintf(S.apSsid, sizeof(S.apSsid), "DeyeFan-%06X", (unsigned)id);
  strlcpy(S.apPass, "noctua12v", sizeof(S.apPass));
  strlcpy(S.hostname, "deye-fan", sizeof(S.hostname));
  // Hypothèses par défaut (À CALIBRER via l'interface web, voir README) :
  //   canal 1 (92 mm) : NMB ~4000 tr/min vs NF-A9-FLX 1600 tr/min   -> ratio ~2.5
  //   canal 2 (60 mm) : NMB ~7000 tr/min vs NF-A6x25-FLX 3100 tr/min -> ratio ~2.3
  S.ratio[0]        = 2.5f;    S.ratio[1]        = 2.3f;
  S.parallelFans[0] = 2;       S.parallelFans[1] = 2;
  S.enabled[0]      = 1;       S.enabled[1]      = 1;
  S.blindRun[0]     = 0;       S.blindRun[1]     = 0;
  S.blindRpm[0]     = 4000.0f; S.blindRpm[1]     = 7000.0f;
}

static void settingsLoad() {
  EEPROM.begin(sizeof(Settings) + 64);
  Settings tmp;
  EEPROM.get(0, tmp);
  bool ok = (tmp.magic == SETTINGS_MAGIC && tmp.version == SETTINGS_VERSION && tmp.size == sizeof(Settings));
  if (ok) ok = (crc32buf((const uint8_t *)&tmp, offsetof(Settings, crc)) == tmp.crc);
  if (ok) { S = tmp; Serial.println(F("[CFG] paramètres chargés depuis l'EEPROM")); }
  else    { settingsDefaults(); Serial.println(F("[CFG] EEPROM vierge ou invalide -> paramètres par défaut")); }
}

static bool settingsSave() {
  S.magic   = SETTINGS_MAGIC;
  S.version = SETTINGS_VERSION;
  S.size    = sizeof(Settings);
  S.crc     = crc32buf((const uint8_t *)&S, offsetof(Settings, crc));
  EEPROM.put(0, S);
  return EEPROM.commit();
}

// =====================================================================================================================
// 3. CAPTURE DES ENTRÉES (interruption GPIO -> tampon circulaire d'horodatages)
// =====================================================================================================================
// Le tach d'un ventilateur est un collecteur ouvert : la ligne est tirée vers le haut (10 k vers 3V3 ici) et le
// ventilateur la met à la masse 2 fois par tour. On ne fait dans l'interruption qu'un horodatage (micros) du
// front DESCENDANT ; toute la mesure est faite dans loop(), hors interruption.

struct InChannel {
  volatile uint32_t edges[EDGE_BUF_N];
  volatile uint32_t head;
  volatile uint32_t count;
  volatile uint32_t lastEdgeUs;
};
static InChannel INP[COUNT_CH];

static void IRAM_ATTR inEdge(uint8_t idx) {
  uint32_t now = micros();
  InChannel &c = INP[idx];
  if ((uint32_t)(now - c.lastEdgeUs) < IN_DEBOUNCE_US) return;  // anti-rebond
  c.lastEdgeUs = now;
  c.edges[c.head] = now;
  c.head = (c.head + 1) & EDGE_BUF_MASK;
  if (c.count < EDGE_BUF_N) c.count++;
}

static void IRAM_ATTR isrInCh1() { inEdge(0); }
static void IRAM_ATTR isrInCh2() { inEdge(1); }

// =====================================================================================================================
// 4. MESURE DE FRÉQUENCE (dans loop(), HORS interruptions)
// =====================================================================================================================
// Méthode : comptage d'impulsions sur une fenêtre de temps -> f = (n-1) / (t_dernier - t_premier).
// Ce comptage est intrinsèquement insensible à la gigue des horodatages individuels (charge WiFi, etc.), et le
// lissage exponentiel rend la fréquence ré-émise stable. Avec 2 tach en parallèle, les impulsions des deux
// ventilateurs s'additionnent : on divise par parallelFans pour retrouver la vitesse D'UN ventilateur.

struct Meas {
  bool  valid;
  float combHz;   // fréquence combinée mesurée sur l'entrée (tous ventilateurs)
  float fanHz;    // fréquence PAR VENTILATEUR, lissée (EMA)
  float rpmIn;    // RPM mesurés d'un ventilateur
};
static Meas meas[COUNT_CH];

static void measureChannel(uint8_t ch) {
  InChannel &c = INP[ch];
  static uint32_t snap[EDGE_BUF_N];   // copie hors interruption (lecture à chaud, appelée depuis loop)

  noInterrupts();
  uint32_t head = c.head;
  uint32_t cnt  = c.count;
  for (uint32_t i = 0; i < cnt; i++) snap[i] = c.edges[(head - 1 - i) & EDGE_BUF_MASK]; // du plus récent au plus ancien
  interrupts();

  meas[ch].valid = false;
  if (cnt < 4) return;                                              // pas assez d'impulsions
  uint32_t newest = snap[0];
  if ((uint32_t)(micros() - newest) > (IN_LOST_MS * 1000UL)) return; // signal trop ancien

  // ne garder que les impulsions de la fenêtre de mesure
  uint32_t kept = 0, oldest = newest;
  for (uint32_t i = 0; i < cnt; i++) {
    if ((uint32_t)(newest - snap[i]) > MEAS_WINDOW_US) break;
    oldest = snap[i];
    kept++;
  }
  if (kept < 4 || newest == oldest) return;

  float combined = (kept - 1) * 1000000.0f / (float)(newest - oldest);

  // lissage exponentiel (stabilité de la fréquence ré-émise)
  static float emaHz[COUNT_CH]   = {0.0f, 0.0f};
  static bool  emaInit[COUNT_CH] = {false, false};
  if (!emaInit[ch]) { emaHz[ch] = combined; emaInit[ch] = true; }
  else emaHz[ch] += EMA_ALPHA * (combined - emaHz[ch]);

  uint8_t par = S.parallelFans[ch];
  if (par < 1) par = 1;
  if (par > 4) par = 4;

  meas[ch].valid  = true;
  meas[ch].combHz = combined;
  meas[ch].fanHz  = emaHz[ch] / par;
  meas[ch].rpmIn  = HZ_TO_RPM(meas[ch].fanHz);
}

// =====================================================================================================================
// 5. GÉNÉRATION DU SIGNAL DE SORTIE (100 % interruption matérielle — isolation totale du WiFi/web)
// =====================================================================================================================
// Principe : le timer matériel FRC1 (timer1) déclenche une interruption en mode EDGE. À chaque interruption on
// bascule les GPIO dont le front est dû, puis on ré-arme le timer sur l'instant ABSOLU (compteur de cycles CPU)
// du prochain front. Comme chaque front est ancré sur l'horloge réelle (et non sur le temps écoulé depuis le
// front précédent), il n'y a AUCUNE dérive cumulée : la gigue se limite à la latence d'exécution de
// l'interruption (~1 µs) et un retard exceptionnel (ex : écriture flash) est immédiatement resynchronisé.
//
// Logique de sortie : GPIO HAUT -> NPN passant -> ligne tach BASSE. Donc :
//    - canal arrêté : GPIO BAS  -> transistor bloqué -> ligne tach au repos HAUTE (= « ventilateur à l'arrêt ») ;
//    - canal actif  : carré 50 % (un front descendant de tach par période, comme un vrai tach).
//
// Les changements de paramètres (loop -> ISR) passent par une boîte aux lettres (pend*) validée par pendFlag :
// ils sont appliqués par l'ISR à la prochaine occasion, SANS rupture de phase (la nouvelle demi-période s'applique
// au front suivant, nextEdge n'est pas modifié).

struct OutChannel {
  volatile uint32_t active;     // génération en cours
  volatile uint32_t level;      // niveau GPIO courant (1 = ligne tach BASSE)
  volatile uint32_t half;       // demi-période en cycles CPU (80 MHz)
  volatile uint32_t nextEdge;   // instant absolu (CCOUNT) du prochain basculement
  volatile uint32_t pendFlag;   // demande en attente (écrite par loop, consommée par l'ISR)
  volatile uint32_t pendHalf;
  volatile uint32_t pendActive;
  uint32_t mask;                // bit du GPIO dans GPOS/GPOC
};
static OutChannel OUTP[COUNT_CH];

static void outRequest(uint8_t ch, uint32_t halfTicks, bool start) {
  OUTP[ch].pendHalf   = halfTicks;        // ordre important : champs d'abord, pendFlag en DERNIER
  OUTP[ch].pendActive = start ? 1u : 0u;
  OUTP[ch].pendFlag   = 1u;
}

void IRAM_ATTR outIsr() {
  uint32_t now = ESP.getCycleCount();
  uint32_t nextDelta = IDLE_TICKS;                        // si rien à faire : réveil toutes les 1 ms
  for (uint8_t i = 0; i < COUNT_CH; i++) {
    OutChannel &c = OUTP[i];
    if (c.pendFlag) {                                     // application des demandes de loop()
      c.pendFlag = 0u;
      c.half = c.pendHalf;
      if (c.pendActive) {
        if (!c.active) { c.active = 1u; c.level = 1u; GPOS = c.mask; c.nextEdge = now + c.half; }
      } else if (c.active) {
        c.active = 0u; c.level = 0u; GPOC = c.mask;       // repos : ligne tach HAUTE
      }
    }
    if (!c.active) continue;

    uint8_t late = 0;                                     // rattraper les fronts dus (normal : 0 ou 1 itération)
    while ((int32_t)(now - c.nextEdge) >= 0) {
      c.level ^= 1u;
      if (c.level) GPOS = c.mask; else GPOC = c.mask;
      c.nextEdge += c.half;
      if (++late >= 3) break;
    }
    if (late >= 3) {                                      // ISR exceptionnellement en retard : resynchronisation
      c.level = 1u; GPOS = c.mask;
      c.nextEdge = now + c.half;
    }
    uint32_t d = c.nextEdge - now;
    if (d < nextDelta) nextDelta = d;
  }
  if (nextDelta < MIN_TICKS) nextDelta = MIN_TICKS;
  if (nextDelta > 0x7FFFFFUL) nextDelta = 0x7FFFFFUL;     // limite du FRC1 (23 bits)
  timer1_write(nextDelta);                                // ré-armement (fonction IRAM du coeur)
}

// =====================================================================================================================
// 6. ASSERVISSEMENT PAR CANAL (dans loop(), toutes les MEAS_PERIOD_MS)
// =====================================================================================================================
// f_sortie = f_mesurée_par_ventilateur x ratio, bornée à [OUT_MIN_HZ ; OUT_MAX_HZ].
// Si le signal entrant disparaît plus de IN_LOST_MS (+ délai BLIND_DELAY_MS) et si le mode "cache-panne" est
// activé, on simule une vitesse fixe (blindRpm). Sinon on arrête la génération (ligne tach au repos HAUTE).

struct ChCtl {
  bool     running;      // le générateur émet actuellement
  bool     blind;        // en mode cache-panne
  uint32_t lastValidMs;  // dernier instant avec un signal entrant valide
};
static ChCtl ctl[COUNT_CH];

struct ChView {          // valeurs affichées via /api/status
  float outHz;
  float rpmOut;
};
static ChView stv[COUNT_CH];

static void controlTick(uint8_t ch) {
  if (!S.enabled[ch]) {
    if (ctl[ch].running) { outRequest(ch, 0, false); ctl[ch].running = false; ctl[ch].blind = false; }
    stv[ch].outHz = 0.0f; stv[ch].rpmOut = 0.0f;
    return;
  }
  if (meas[ch].valid) {
    ctl[ch].blind = false;
    ctl[ch].lastValidMs = millis();
    float f = meas[ch].fanHz * S.ratio[ch];
    f = constrain(f, OUT_MIN_HZ, OUT_MAX_HZ);
    outRequest(ch, (uint32_t)(0.5f * CPU_HZ / f), true);
    ctl[ch].running = true;
    stv[ch].outHz = f;
    stv[ch].rpmOut = HZ_TO_RPM(f);
    return;
  }
  uint32_t sinceValid = millis() - ctl[ch].lastValidMs;
  if (S.blindRun[ch] && sinceValid > (uint32_t)(IN_LOST_MS + BLIND_DELAY_MS)) {
    ctl[ch].blind = true;                                 // cache-panne : vitesse fixe
    float f = constrain(RPM_TO_HZ(S.blindRpm[ch]), OUT_MIN_HZ, OUT_MAX_HZ);
    outRequest(ch, (uint32_t)(0.5f * CPU_HZ / f), true);
    ctl[ch].running = true;
    stv[ch].outHz = f;
    stv[ch].rpmOut = HZ_TO_RPM(f);
  } else if (ctl[ch].running) {
    outRequest(ch, 0, false);                             // signal perdu : ligne tach au repos
    ctl[ch].running = false;
    stv[ch].outHz = 0.0f;
    stv[ch].rpmOut = 0.0f;
  }
}

// =====================================================================================================================
// 7. LED D'ÉTAT
// =====================================================================================================================
// LED embarquée (D4/GPIO2, allumée à l'état BAS) :
//   FIXE     = simulation active sur TOUS les canaux activés
//   CLIGNOTE = au moins un canal activé attend un signal tach
// (LED pilotée depuis loop() ; la génération du signal n'en dépend pas.)

static void ledTick() {
  bool anyEnabled = false, allRunning = true;
  for (uint8_t ch = 0; ch < COUNT_CH; ch++) {
    if (S.enabled[ch]) { anyEnabled = true; if (!ctl[ch].running) allRunning = false; }
  }
  if (anyEnabled && allRunning) digitalWrite(PIN_LED, LOW);                    // fixe : on simule
  else digitalWrite(PIN_LED, ((millis() / LED_BLINK_MS) & 1UL) ? HIGH : LOW);  // clignote : attente
}

// =====================================================================================================================
// 8. RÉSEAU : AP + STA simultanés, portail captif, mDNS
// =====================================================================================================================
static DNSServer dnsServer;
static uint32_t lastStaAttempt = 0;

static void wifiSetup() {
  WiFi.persistent(false);                       // éviter les écritures flash du SDK (l'EEPROM gère la persistance)
  WiFi.setSleepMode(WIFI_NONE_SLEEP);           // pas de modem-sleep : latence réseau constante
  WiFi.mode(WIFI_AP_STA);                       // AP + STA simultanés
  WiFi.hostname(S.hostname);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(S.apSsid, S.apPass);              // AP toujours actif (accès de secours garanti)
  if (S.staSsid[0]) WiFi.begin(S.staSsid, S.staPass);
  WiFi.setAutoReconnect(true);
  lastStaAttempt = millis();
}

static void wifiTick() {
  // filet de sécurité : si la station reste déconnectée, on retente toutes les 30 s
  if (S.staSsid[0] && WiFi.status() != WL_CONNECTED && (uint32_t)(millis() - lastStaAttempt) > 30000UL) {
    Serial.println(F("[WIFI] nouvelle tentative de connexion"));
    WiFi.disconnect();
    WiFi.begin(S.staSsid, S.staPass);
    lastStaAttempt = millis();
  }
}

// =====================================================================================================================
// 9. DIVERS (formatage, échappement JSON, redémarrage programmé)
// =====================================================================================================================
static void fToBuf(char *out, size_t n, float v, int dec) { dtostrf(v, 0, dec, out); }

static void jsonEsc(char *out, size_t n, const char *in) {
  size_t o = 0;
  for (; *in && o + 7 < n; in++) {
    char c = *in;
    if (c == '"' || c == '\\') { out[o++] = '\\'; out[o++] = c; }
    else if ((unsigned char)c < 0x20) o += snprintf(out + o, n - o, "\\u%04X", (unsigned)c);
    else out[o++] = c;
  }
  out[o] = 0;
}

static bool     rebootPending = false;
static uint32_t rebootAtMs    = 0;
static void scheduleReboot(uint32_t delayMs) { rebootPending = true; rebootAtMs = millis() + delayMs; }

// =====================================================================================================================
// 10. SERVEUR WEB
// =====================================================================================================================
// API :
//   GET  /                 page unique (HTML embarqué en PROGMEM)
//   GET  /api/status       état temps réel (JSON)
//   GET  /api/config       paramètres (JSON, sans mots de passe)
//   POST /api/config       applique les ratios/options (formulaire url-encodé)
//   POST /api/wifi         enregistre le Wi-Fi + redémarre
//   GET  /api/scan         lance un scan asynchrone des réseaux
//   GET  /api/scan.json    résultat du scan
//   POST /api/reboot       redémarre
//   POST /api/factoryreset | remise à zéro + redémarrage
// Les réponses sont construites dans des buffers statiques (pas de fragmentation mémoire).

static ESP8266WebServer server(80);
extern const char INDEX_HTML[] PROGMEM;   // défini plus bas (section 12)

// --- lecture/validation d'un argument de formulaire
static bool argF(const char *key, float lo, float hi, float *dst) {
  if (!server.hasArg(key)) return true;            // absent : valeur inchangée
  float v = server.arg(key).toFloat();
  if (isnan(v) || v < lo || v > hi) return false;  // invalide : refus global
  *dst = v;
  return true;
}

static bool argU8(const char *key, long lo, long hi, uint8_t *dst) {
  if (!server.hasArg(key)) return true;
  long v = server.arg(key).toInt();
  if (v < lo || v > hi) return false;
  *dst = (uint8_t)v;
  return true;
}

static bool argBool(const char *key, uint8_t *dst) {
  if (!server.hasArg(key)) return true;
  *dst = (server.arg(key) != "0" && server.arg(key) != "off" && server.arg(key) != "false") ? 1u : 0u;
  return true;
}

// --- GET /api/status : état temps réel
static void handleStatus() {
  char buf[1600];
  char eH[40], eS[70], eA[70], ipS[20], ipA[20];
  jsonEsc(eH, sizeof(eH), S.hostname);
  jsonEsc(eS, sizeof(eS), S.staSsid);
  jsonEsc(eA, sizeof(eA), S.apSsid);
  strlcpy(ipS, WiFi.localIP().toString().c_str(), sizeof(ipS));
  strlcpy(ipA, WiFi.softAPIP().toString().c_str(), sizeof(ipA));
  int n = snprintf(buf, sizeof(buf),
      "{\"ok\":1,\"host\":\"%s\",\"heap\":%u,\"up\":%lu,"
      "\"sta\":{\"ssid\":\"%s\",\"conn\":%d,\"ip\":\"%s\",\"rssi\":%d},"
      "\"ap\":{\"ssid\":\"%s\",\"ip\":\"%s\",\"clients\":%d},\"ch\":[",
      eH, (unsigned)ESP.getFreeHeap(), (unsigned long)(millis() / 1000UL),
      eS, (WiFi.status() == WL_CONNECTED) ? 1 : 0, ipS, (int)WiFi.RSSI(),
      eA, ipA, WiFi.softAPgetStationNum());
  for (uint8_t ch = 0; ch < COUNT_CH && n > 0 && n < (int)sizeof(buf) - 200; ch++) {
    char fi[12], ri[12], fo[12], ro[12];
    fToBuf(fi, sizeof(fi), meas[ch].combHz, 2);
    fToBuf(ri, sizeof(ri), meas[ch].rpmIn, 0);
    fToBuf(fo, sizeof(fo), stv[ch].outHz, 2);
    fToBuf(ro, sizeof(ro), stv[ch].rpmOut, 0);
    n += snprintf(buf + n, sizeof(buf) - n,
        "%s{\"en\":%d,\"run\":%d,\"blind\":%d,\"valid\":%d,"
        "\"rpmIn\":%s,\"combHz\":%s,\"outHz\":%s,\"rpmOut\":%s}",
        ch ? "," : "", (int)S.enabled[ch], ctl[ch].running ? 1 : 0, ctl[ch].blind ? 1 : 0,
        meas[ch].valid ? 1 : 0, ri, fi, fo, ro);
  }
  strlcpy(buf + n, "]}", sizeof(buf) - n);
  server.send(200, "application/json", buf);
}

// --- GET /api/config
static void handleConfigGet() {
  char buf[512], r0[12], r1[12], b0[12], b1[12], eS[70], eA[70], eH[40];
  fToBuf(r0, sizeof(r0), S.ratio[0], 2);
  fToBuf(r1, sizeof(r1), S.ratio[1], 2);
  fToBuf(b0, sizeof(b0), S.blindRpm[0], 0);
  fToBuf(b1, sizeof(b1), S.blindRpm[1], 0);
  jsonEsc(eS, sizeof(eS), S.staSsid);
  jsonEsc(eA, sizeof(eA), S.apSsid);
  jsonEsc(eH, sizeof(eH), S.hostname);
  snprintf(buf, sizeof(buf),
      "{\"ch\":[{\"ratio\":%s,\"par\":%u,\"en\":%u,\"blind\":%u,\"blindRpm\":%s},"
      "{\"ratio\":%s,\"par\":%u,\"en\":%u,\"blind\":%u,\"blindRpm\":%s}],"
      "\"staSsid\":\"%s\",\"apSsid\":\"%s\",\"hostname\":\"%s\"}",
      r0, S.parallelFans[0], S.enabled[0], S.blindRun[0], b0,
      r1, S.parallelFans[1], S.enabled[1], S.blindRun[1], b1, eS, eA, eH);
  server.send(200, "application/json", buf);
}

// --- POST /api/config : applique et mémorise les réglages des canaux
static void handleConfigPost() {
  bool ok = true;
  for (uint8_t ch = 0; ch < COUNT_CH; ch++) {
    char k[20];
    snprintf(k, sizeof(k), "ch%u_ratio", ch + 1);
    ok &= argF(k, 0.1f, 20.0f, &S.ratio[ch]);
    snprintf(k, sizeof(k), "ch%u_par", ch + 1);
    ok &= argU8(k, 1, 4, &S.parallelFans[ch]);
    snprintf(k, sizeof(k), "ch%u_blindrpm", ch + 1);
    ok &= argF(k, 300.0f, 12000.0f, &S.blindRpm[ch]);
    snprintf(k, sizeof(k), "ch%u_en", ch + 1);
    ok &= argBool(k, &S.enabled[ch]);
    snprintf(k, sizeof(k), "ch%u_blind", ch + 1);
    ok &= argBool(k, &S.blindRun[ch]);
  }
  if (!ok) { server.send(400, "application/json", "{\"ok\":0,\"err\":\"valeur hors plage\"}"); return; }
  if (!settingsSave()) { server.send(500, "application/json", "{\"ok\":0,\"err\":\"échec écriture EEPROM\"}"); return; }
  Serial.println(F("[CFG] réglages mis à jour via l'interface web"));
  server.send(200, "application/json", "{\"ok\":1}");
}

// --- POST /api/wifi : enregistre et redémarre
static void handleWifiPost() {
  bool bad = false;
  if (server.hasArg("sta_ssid")) {
    String v = server.arg("sta_ssid");
    if (v.length() > 32) bad = true; else strlcpy(S.staSsid, v.c_str(), sizeof(S.staSsid));
  }
  if (server.hasArg("sta_pass") && server.arg("sta_pass").length() > 0) {   // vide = inchangé
    String v = server.arg("sta_pass");
    if (v.length() > 64) bad = true; else strlcpy(S.staPass, v.c_str(), sizeof(S.staPass));
  }
  if (server.hasArg("ap_ssid")) {
    String v = server.arg("ap_ssid");
    if (v.length() < 1 || v.length() > 32) bad = true; else strlcpy(S.apSsid, v.c_str(), sizeof(S.apSsid));
  }
  if (server.hasArg("ap_pass") && server.arg("ap_pass").length() > 0) {     // vide = inchangé
    String v = server.arg("ap_pass");
    if (v.length() < 8 || v.length() > 64) bad = true; else strlcpy(S.apPass, v.c_str(), sizeof(S.apPass));
  }
  if (server.hasArg("hostname")) {
    String v = server.arg("hostname"); v.toLowerCase();
    String clean;
    for (size_t i = 0; i < v.length(); i++) {
      char c = v[i];
      if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') clean += c;
    }
    if (clean.length() >= 1) strlcpy(S.hostname, clean.c_str(), sizeof(S.hostname));
  }
  if (bad) {
    server.send(400, "application/json", "{\"ok\":0,\"err\":\"paramètres invalides (SSID/AP requis, mot de passe AP >= 8 caractères)\"}");
    return;
  }
  if (!settingsSave()) { server.send(500, "application/json", "{\"ok\":0,\"err\":\"échec écriture EEPROM\"}"); return; }
  Serial.println(F("[WIFI] configuration enregistrée, redémarrage"));
  scheduleReboot(1500);
  server.send(200, "application/json", "{\"ok\":1,\"msg\":\"enregistré, redémarrage en cours\"}");
}

// --- scan asynchrone des réseaux (n'interrompt pas la génération du signal)
static void handleScan() {
  int8_t r = WiFi.scanComplete();
  if (r == WIFI_SCAN_FAILED) WiFi.scanNetworks(true, true);
  server.send(200, "application/json", "{\"started\":1}");
}

static void handleScanJson() {
  int8_t r = WiFi.scanComplete();
  if (r == WIFI_SCAN_RUNNING || r == WIFI_SCAN_FAILED) {
    if (r == WIFI_SCAN_FAILED) WiFi.scanNetworks(true, true);
    server.send(200, "application/json", "{\"busy\":1}");
    return;
  }
  String out = "{\"busy\":0,\"nets\":[";
  int lim = (r > 20) ? 20 : r;
  for (int i = 0; i < lim; i++) {
    char esc[72], item[110];
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) ssid = "(masqué)";
    jsonEsc(esc, sizeof(esc), ssid.c_str());
    snprintf(item, sizeof(item), "%s{\"s\":\"%s\",\"r\":%d,\"sec\":%d}",
             i ? "," : "", esc, (int)WiFi.RSSI(i), (int)WiFi.encryptionType(i));
    out += item;
  }
  out += "]}";
  WiFi.scanDelete();
  server.send(200, "application/json", out);
}

// --- redémarrage / remise à zéro
static void handleReboot() {
  scheduleReboot(500);
  server.send(200, "application/json", "{\"ok\":1,\"msg\":\"redémarrage en cours\"}");
}

static void handleFactoryReset() {
  settingsDefaults();
  settingsSave();
  scheduleReboot(1200);
  server.send(200, "application/json", "{\"ok\":1,\"msg\":\"paramètres d'usine restaurés\"}");
}

// --- pages
static void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

static void handleNotFound() {
  if (server.uri().startsWith("/api/")) {
    server.send(404, "application/json", "{\"ok\":0,\"err\":\"endpoint inconnu\"}");
    return;
  }
  // comportement « portail captif » : toute URL inconnue renvoie la page principale
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

static void webSetup() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/index.html", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/config", HTTP_GET, handleConfigGet);
  server.on("/api/config", HTTP_POST, handleConfigPost);
  server.on("/api/wifi", HTTP_POST, handleWifiPost);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/scan.json", HTTP_GET, handleScanJson);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/factoryreset", HTTP_POST, handleFactoryReset);
  server.onNotFound(handleNotFound);
  server.begin();
}

// =====================================================================================================================
// 11. INTERFACE WEB — page unique embarquée en PROGMEM (aucune bibliothèque externe)
// =====================================================================================================================
// La page se rafraîchit via /api/status (1 s) et poste les formulaires en url-encodé vers /api/config et /api/wifi.

const char INDEX_HTML[] PROGMEM = R"HTMLLIT(<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Deye Fan — Multiplicateur tach</title>
<style>
:root{--bg:#0e1116;--card:#171c24;--bord:#232a35;--acc:#4fb286;--tx:#dfe6ee;--mut:#8a97a8}
*{box-sizing:border-box}
body{margin:0;font:15px/1.45 system-ui,'Segoe UI',Arial,sans-serif;background:var(--bg);color:var(--tx)}
header{padding:14px 18px;background:#12161d;border-bottom:1px solid var(--bord);display:flex;flex-wrap:wrap;gap:8px;align-items:baseline}
h1{font-size:19px;margin:0 auto 0 0}
#sysinfo{color:var(--mut);font-size:12.5px;text-align:right}
main{max-width:940px;margin:0 auto;padding:14px;display:grid;gap:14px}
.card{background:var(--card);border:1px solid var(--bord);border-radius:10px;padding:14px 16px}
.card h2{margin:0 0 10px;font-size:16px;display:flex;align-items:center;gap:8px;flex-wrap:wrap}
.badge{padding:2px 10px;border-radius:20px;font-size:12.5px;font-weight:600}
.b-run{background:#173327;color:#6fd3a2}
.b-blind{background:#3a2a1a;color:#e8a06a}
.b-wait{background:#3a321a;color:#e8c27a}
.b-off{background:#2a2f37;color:#9aa5b1}
.kv{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:8px 18px;margin:10px 0}
.kv div b{display:block;font-size:11.5px;color:var(--mut);font-weight:600;text-transform:uppercase;letter-spacing:.4px}
.kv div span{font-size:19px;font-variant-numeric:tabular-nums}
form{display:grid;gap:10px;margin-top:12px}
.row{display:flex;flex-wrap:wrap;gap:10px;align-items:center}
label{font-size:12.5px;color:var(--mut);display:flex;flex-direction:column;gap:3px}
label.cb{flex-direction:row;align-items:center;color:var(--tx);font-size:14px}
input[type=number],input[type=text],input[type=password],select{background:#0f1319;border:1px solid #2a3442;color:var(--tx);border-radius:6px;padding:7px 9px;font-size:14px;max-width:170px}
button{background:var(--acc);border:0;color:#08210f;font-weight:700;padding:9px 16px;border-radius:7px;font-size:14px;cursor:pointer}
button.sec{background:#2a3442;color:var(--tx)}
button.danger{background:#e0584f;color:#fff}
.msg{font-size:13px;min-height:16px}
.ok{color:#6fd3a2}.err{color:#e0584f}
#nets table{width:100%;border-collapse:collapse;font-size:13px;margin-top:8px}
#nets td,#nets th{padding:5px 6px;border-bottom:1px solid var(--bord);text-align:left}
#nets th{color:var(--mut);font-weight:600}
#nets tr:hover td{background:#1c232e}
.hint{color:var(--mut);font-size:12.5px}
footer{max-width:940px;margin:0 auto;padding:6px 16px 26px;color:var(--mut);font-size:12px}
</style>
</head>
<body>
<header>
 <h1>Deye Fan — Multiplicateur tach</h1>
 <div id="sysinfo">Chargement…</div>
</header>
<main>
 <section class="card">
  <h2>Canal 1 — 92 mm (2 × NF-A9-FLX) <span class="badge b-off" id="st0">…</span></h2>
  <div class="kv">
   <div><b>RPM ventilateur (mesuré)</b><span id="rpmin0">—</span></div>
   <div><b>Fréq. entrée (combinée)</b><span id="hzin0">—</span></div>
   <div><b>RPM simulé → Deye</b><span id="rpmout0">—</span></div>
   <div><b>Fréq. sortie</b><span id="hzout0">—</span></div>
  </div>
  <form onsubmit="saveCh(0);return false">
   <div class="row">
    <label>Ratio multiplicateur<input type="number" id="ratio0" min="0.1" max="20" step="0.05"></label>
    <label>Tach en parallèle<input type="number" id="par0" min="1" max="4" step="1"></label>
    <label>RPM fixes (cache-panne)<input type="number" id="blindrpm0" min="300" max="12000" step="50"></label>
   </div>
   <div class="row">
    <label class="cb"><input type="checkbox" id="en0">Canal activé</label>
    <label class="cb"><input type="checkbox" id="blind0">Simuler sans signal (cache-panne)</label>
    <button>Appliquer</button><span class="msg" id="m0"></span>
   </div>
  </form>
 </section>
 <section class="card">
  <h2>Canal 2 — 60 mm (2 × NF-A6x25-FLX) <span class="badge b-off" id="st1">…</span></h2>
  <div class="kv">
   <div><b>RPM ventilateur (mesuré)</b><span id="rpmin1">—</span></div>
   <div><b>Fréq. entrée (combinée)</b><span id="hzin1">—</span></div>
   <div><b>RPM simulé → Deye</b><span id="rpmout1">—</span></div>
   <div><b>Fréq. sortie</b><span id="hzout1">—</span></div>
  </div>
  <form onsubmit="saveCh(1);return false">
   <div class="row">
    <label>Ratio multiplicateur<input type="number" id="ratio1" min="0.1" max="20" step="0.05"></label>
    <label>Tach en parallèle<input type="number" id="par1" min="1" max="4" step="1"></label>
    <label>RPM fixes (cache-panne)<input type="number" id="blindrpm1" min="300" max="12000" step="50"></label>
   </div>
   <div class="row">
    <label class="cb"><input type="checkbox" id="en1">Canal activé</label>
    <label class="cb"><input type="checkbox" id="blind1">Simuler sans signal (cache-panne)</label>
    <button>Appliquer</button><span class="msg" id="m1"></span>
   </div>
  </form>
 </section>
 <section class="card">
  <h2>Réseau</h2>
  <form onsubmit="saveWifi();return false">
   <div class="row">
    <label>SSID station (box)<input type="text" id="sta_ssid" maxlength="32"></label>
    <label>Mot de passe station<input type="password" id="sta_pass" maxlength="64" placeholder="(vide = inchangé)"></label>
    <label>SSID du point d'accès<input type="text" id="ap_ssid" maxlength="32"></label>
   </div>
   <div class="row">
    <label>Mot de passe AP<input type="password" id="ap_pass" maxlength="64" placeholder="(vide = inchangé, min. 8 car.)"></label>
    <label>Nom mDNS<input type="text" id="hostname" maxlength="32"></label>
    <button>Enregistrer &amp; redémarrer</button>
    <button type="button" class="sec" onclick="doScan()">Scanner les réseaux</button>
    <span class="msg" id="mw"></span>
   </div>
   <div id="nets"></div>
  </form>
 </section>
 <section class="card">
  <h2>Système</h2>
  <div class="row">
   <button type="button" class="danger" onclick="doReboot()">Redémarrer</button>
   <button type="button" class="sec" onclick="doReset()">Réglages d'usine</button>
   <span class="msg" id="ms"></span>
  </div>
  <p class="hint">LED embarquée : <b>fixe</b> = simulation active sur tous les canaux activés · <b>clignote</b> = attente d'un signal tach.</p>
 </section>
</main>
<footer>Deye Fan Tach v1.0 — Wemos D1 mini — génération du signal isolée sur le timer matériel FRC1 (aucune influence du WiFi).</footer>
<script>
const $=id=>document.getElementById(id);
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const fmt=(v,d=0)=>v==null?'—':(+v).toFixed(d);
let nets=[];

async function jget(u){const r=await fetch(u);return r.json();}
async function post(u,body){const r=await fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(body).toString()});return r.json();}

async function loadStatus(){
 try{
  const s=await jget('/api/status');
  $('sysinfo').innerHTML='STA : '+(s.sta.conn?esc(s.sta.ip)+' ('+s.sta.rssi+' dBm)':'non connecté')+' · AP : '+esc(s.ap.ip)+'<br>heap : '+Math.round(s.heap/1024)+' ko · marche : '+fmt(s.up/3600,1)+' h';
  for(let i=0;i<2;i++){
   const c=s.ch[i];
   $('rpmin'+i).textContent=c.valid?fmt(c.rpmIn)+' tr/min':'—';
   $('hzin'+i).textContent=c.valid?fmt(c.combHz,1)+' Hz':'—';
   $('rpmout'+i).textContent=c.run?fmt(c.rpmOut)+' tr/min':'—';
   $('hzout'+i).textContent=c.run?fmt(c.outHz,1)+' Hz':'—';
   const b=$('st'+i);
   b.className='badge '+(c.run?(c.blind?'b-blind':'b-run'):(c.en?'b-wait':'b-off'));
   b.textContent=c.run?(c.blind?'Simulation (cache-panne)':'Simulation'):(c.en?'Attente signal':'Désactivé');
  }
 }catch(e){$('sysinfo').textContent='Erreur de communication';}
}

async function loadCfg(){
 try{
  const c=await jget('/api/config');
  for(let i=0;i<2;i++){
   $('ratio'+i).value=c.ch[i].ratio;
   $('par'+i).value=c.ch[i].par;
   $('blindrpm'+i).value=c.ch[i].blindRpm;
   $('en'+i).checked=!!c.ch[i].en;
   $('blind'+i).checked=!!c.ch[i].blind;
  }
  $('sta_ssid').value=c.staSsid;
  $('ap_ssid').value=c.apSsid;
  $('hostname').value=c.hostname;
 }catch(e){}
}

async function saveCh(i){
 const b={};b['ch'+(i+1)+'_ratio']=$('ratio'+i).value;b['ch'+(i+1)+'_par']=$('par'+i).value;
 b['ch'+(i+1)+'_blindrpm']=$('blindrpm'+i).value;
 b['ch'+(i+1)+'_en']=$('en'+i).checked?1:0;b['ch'+(i+1)+'_blind']=$('blind'+i).checked?1:0;
 try{
  const r=await post('/api/config',b);
  $('m'+i).textContent=r.ok?'Enregistré ✓':(r.err||'Erreur');
  $('m'+i).className='msg '+(r.ok?'ok':'err');
 }catch(e){$('m'+i).textContent='Erreur réseau';$('m'+i).className='msg err';}
 setTimeout(()=>{$('m'+i).textContent='';},4000);
}

async function saveWifi(){
 try{
  const r=await post('/api/wifi',{sta_ssid:$('sta_ssid').value,sta_pass:$('sta_pass').value,ap_ssid:$('ap_ssid').value,ap_pass:$('ap_pass').value,hostname:$('hostname').value});
  $('mw').textContent=r.ok?'Enregistré — redémarrage…':(r.err||'Erreur');
  $('mw').className='msg '+(r.ok?'ok':'err');
 }catch(e){$('mw').textContent='Erreur réseau';$('mw').className='msg err';}
}

async function doScan(){
 $('nets').innerHTML='<span class="hint">Scan en cours…</span>';
 try{await jget('/api/scan');}catch(e){}
 for(let t=0;t<12;t++){
  await new Promise(r=>setTimeout(r,1500));
  try{
   const s=await jget('/api/scan.json');
   if(!s.busy){
    nets=s.nets||[];
    $('nets').innerHTML='<table><tr><th>SSID</th><th>Signal</th><th>Sécurité</th></tr>'+
      nets.map((n,i)=>'<tr onclick="pick('+i+')" style="cursor:pointer"><td>'+esc(n.s)+'</td><td>'+n.r+' dBm</td><td>'+(n.sec?'verrouillé':'ouvert')+'</td></tr>').join('')+'</table>';
    return;
   }
  }catch(e){}
 }
 $('nets').innerHTML='<span class="hint">Scan indisponible</span>';
}
function pick(i){if(nets[i])$('sta_ssid').value=nets[i].s;}

async function doReboot(){
 if(!confirm('Redémarrer le module ?'))return;
 await post('/api/reboot',{});
 $('ms').textContent='Redémarrage en cours…';$('ms').className='msg ok';
}
async function doReset(){
 if(!confirm("Restaurer les réglages d'usine (ratios et Wi-Fi) et redémarrer ?"))return;
 await post('/api/factoryreset',{});
 $('ms').textContent='Réinitialisation en cours…';$('ms').className='msg ok';
}

loadStatus();setInterval(loadStatus,1000);loadCfg();
</script>
</body>
</html>)HTMLLIT";

// =====================================================================================================================
// 12. DÉMARRAGE ET BOUCLE PRINCIPALE
// =====================================================================================================================
// Ordre volontaire dans setup() : broches, capture et GÉNÉRATEUR sont démarrés AVANT le réseau, pour que la
// simulation puisse démarrer dès que les ventilateurs tournent, même si le WiFi met du temps à se connecter.

void setup() {
  Serial.begin(115200);
  delay(20);
  ESP.setCpuFreqMHz(80);                 // base de temps garantie : 80 MHz (timer FRC1 + compteur de cycles)
  Serial.println();
  Serial.println(F("=== Deye Fan Tach v1.0 ==="));

  settingsLoad();

  // --- broches
  pinMode(PIN_CH1_OUT, OUTPUT); digitalWrite(PIN_CH1_OUT, LOW);   // repos : ligne tach HAUTE
  pinMode(PIN_CH2_OUT, OUTPUT); digitalWrite(PIN_CH2_OUT, LOW);
  pinMode(PIN_CH1_IN, INPUT_PULLUP);                              // pull-up externe 10 k + interne en appoint
  pinMode(PIN_CH2_IN, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT); digitalWrite(PIN_LED, HIGH);          // LED éteinte

  OUTP[0].mask = 1UL << PIN_CH1_OUT;
  OUTP[1].mask = 1UL << PIN_CH2_OUT;

  // --- capture des entrées
  attachInterrupt(digitalPinToInterrupt(PIN_CH1_IN), isrInCh1, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_CH2_IN), isrInCh2, FALLING);

  // --- générateur de sortie sur le timer matériel FRC1 (EDGE, ré-armé à chaque front par l'ISR)
  timer1_isr_init();
  timer1_attachInterrupt(outIsr);
  timer1_enable(TIM_DIV1, TIM_EDGE, TIM_SINGLE);
  timer1_write(IDLE_TICKS);

  // --- réseau (AP + STA simultanés), portail captif, mDNS, serveur web
  wifiSetup();
  dnsServer.start(53, "*", WiFi.softAPIP());
  if (MDNS.begin(S.hostname)) MDNS.addService("http", "tcp", 80);
  webSetup();

  Serial.print(F("[NET] AP \""));
  Serial.print(S.apSsid);
  Serial.print(F("\" @ "));
  Serial.println(WiFi.softAPIP());
  if (S.staSsid[0]) {
    Serial.print(F("[NET] STA \""));
    Serial.print(S.staSsid);
    Serial.println(F("\" ..."));
  }
  Serial.println(F("[RUN] générateur tach actif."));
}

void loop() {
  uint32_t now = millis();
  static uint32_t lastMeas = 0;

  server.handleClient();           // requêtes web (aucun effet sur la génération : elle est en interruption)
  dnsServer.processNextRequest();  // portail captif (côté AP)
  MDNS.update();
  wifiTick();

  if (now - lastMeas >= MEAS_PERIOD_MS) {
    lastMeas = now;
    for (uint8_t ch = 0; ch < COUNT_CH; ch++) {
      measureChannel(ch);          // mesure (hors interruption)
      controlTick(ch);             // consigne de sortie (boîte aux lettres vers l'ISR)
    }
  }

  ledTick();                       // LED : fixe = simulation, clignote = attente

  if (rebootPending && (int32_t)(now - rebootAtMs) >= 0) {
    Serial.println(F("[SYS] redémarrage"));
    delay(100);
    ESP.restart();
  }

  delay(2);                        // cède la main à la pile WiFi (watchdog logiciel)
}










