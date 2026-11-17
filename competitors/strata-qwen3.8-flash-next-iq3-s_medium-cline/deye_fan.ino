/*****************************************************************************************
 * deye_fan.ino ------------------------------------------------------------------------
 *  Emulateur tachometrique pour onduleur solaire DEYE SUN-8K-SG05LP1-EU-AM2-P
 *  Carte : LOLIN(WEMOS) D1 mini V2.3.0 (module ESP-12S), core "esp8266" d'Arduino IDE
 *
 *  BUT --------------------------------------------------------------------------------
 *   Les ventilateurs d'origine (NMB 06025VE-12N-CL 60 mm / NMB 09225VE-12N-CU 90 mm)
 *   sont remplaces par des Noctua NF-A6x25-FLX et NF-A9-FLX qui tournent nettement moins
 *   vite. L'onduleur surveille le signal tachymetrique (tach) de chaque ventilateur et
 *   peut declencher une alarme "ventilateur" si les RPM lui paraissent trop basses.
 *   Ce module :
 *     - lit le tach reel de chaque Noctua sur son propre canal ;
 *     - reemet vers l'onduleur un tach synthetise a la frequence (RPM x ratio) ;
 *     - expose une interface web (ratios reglables a chaud, WiFi AP+STA, RPM en direct) ;
 *     - allume une LED quand la simulation est active.
 *
 *  ARCHITECTURE LOGICIELLE (isolation temps reel / WiFi) ------------------------------
 *   Deux mondes hermetiques :
 *     [A] TEMPS REEL : ISR du HARDWARE TIMER 1 (FRC1), code et donnees en IRAM. Cette
 *         ISR est cadencee par le peripherique materiel (40 kHz fixes), compte les fronts
 *         d'entree, mesure les periodes, calcule les RPM et fabrique les fronts de sortie
 *         par accumulateur de phase (Bresenham). Elle n'appelle AUCUNE fonction des
 *         bibliotheques Arduino, n'accede jamais a la flash (ni PROGMEM, ni LittleFS),
 *         n'utilise ni String, ni float, ni Serial, ni malloc ; elle ne peut donc ni etre
 *         bloquee ni etre preemptee (priorite materielle). Le signal envoye a l'onduleur
 *         est materialise par des ecritures directes dans les registres GPIO.
 *     [B] NON TEMPS REEL : setup()/loop() (WiFi AP+STA, serveur HTTP, LittleFS, mDNS).
 *         Il ne fait que LIRE des mots 32 bits atomiques publies par l'ISR et ECRIRE des
 *         consignes (ratio, offset, ...) dans d'autres mots 32 bits atomiques. Aucune
 *         requete HTTP, aucune ecriture flash, aucune temporisation de [B] n'a d'effet
 *         mesurable sur [A].
 *   Les deux sens du lien sont atomiques (un mot de 32 bits par variable) : pas de mutex,
 *   pas de zone critique, pas d'effet de bord reentrable.
 *
 *   Pourquoi NE PAS utiliser attachInterrupt() sur les tach : l'ESP8266 n'a qu'un seul
 *   bloc d'interruption GPIO partage avec la pile WiFi et son gestionnaire ROM est long.
 *   La scrutation synchronisee par le timer1 (une seule lecture 32 bits du registre
 *   GPIO_IN pour les 2 canaux, ~10 cycles) est plus rapide, a un jitter borne a 1 tick,
 *   et surtout rigoureusement identique d'un cycle a l'autre : l'erreur de mesure est
 *   systematique, donc annulee dans le calcul front-montant -> front-montant.
 *
 *  COMPILATION ------------------------------------------------------------------------
 *   Arduino IDE -> carte "LOLIN(WEMOS) D1 R2 & mini" (package esp8266 by ESP8266 Community)
 *   Options : CPU frequency = 80 MHz (NE PAS mettre 160 MHz : le timer1 FRC1 derive de
 *             l'horloge CPU, tout serait divise par 2 ; pour 160 MHz passer TICKS_PER_US
 *             a 10), Flash size = 4M (1M FS), lwIP = "Normal (legacy)", C++ exceptions off.
 *   Dependances : uniquement des bibliotheques du core (LittleFS, ESP8266WiFi,
 *                 ESP8266WebServer, ESP8266mDNS). Aucune librairie externe.
 *   Ne PAS utiliser Servo / IRremoteESP8266 / NeoPixel "bit-bang timer" : ils sont eux
 *                 aussi proprietaires du timer1 et entreraient en conflit.
 *
 *  VOIR AUSSI : SCHEMA.md (meme dossier) alimentation, entrees, sorties, nomenclature.
 ***************************************************************************************/

#include <Arduino.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

/*------------------------------------------------------------------------------------
 * 1. BROCHAGE   (GPIO / nom D1 mini / broche physique module ESP-12S)
 *
 *   GPIO4  (D2,  pin 8)  -> ENTREE tach canal 9 cm (tach Noctua NF-A9-FLX)
 *   GPIO5  (D3,  pin 9)  -> ENTREE tach canal 6 cm (tach Noctua NF-A6x25-FLX)
 *   GPIO13 (D7,  pin 15) -> SORTIE tach vers l'onduleur, connecteur ventilateur 9 cm
 *   GPIO14 (D5,  pin 16) -> SORTIE tach vers l'onduleur, connecteur ventilateur 6 cm
 *   GPIO16 (D0,  pin 20) -> LED de statut (anode via 330 ohms vers GND)
 *
 *   Broches volontairement EVITEES :
 *     GPIO0 / GPIO2 / GPIO15 : strapage du mode de boot (non chargeables),
 *     GPIO1 / GPIO3          : UART0 (programmation + logs serie),
 *     GPIO6..GPIO11          : flash interne du module.
 *   GPIO16 n'est pas une entree d'interruption materielle : sans importance ici, la
 *   lecture des tach se fait par scrutation cadencee par le timer1 materiel.
 *----------------------------------------------------------------------------------*/
#define PIN_TACH_IN_9CM   4     // D2
#define PIN_TACH_IN_6CM   5     // D3
#define PIN_TACH_OUT_9CM  13    // D7
#define PIN_TACH_OUT_6CM  14    // D5
#define PIN_STATUS_LED    16    // D0

#define NCHANNEL 2              // canal 0 = 9 cm, canal 1 = 6 cm

/* Masques de bits ESP8266 : un bit par GPIO */
#define BIT_IN(ch)   (1UL << ((ch) == 0 ? PIN_TACH_IN_9CM  : PIN_TACH_IN_6CM))
#define BIT_OUT(ch)  (1UL << ((ch) == 0 ? PIN_TACH_OUT_9CM : PIN_TACH_OUT_6CM))

/* Les tach des Noctua comme ceux du Deye sont en collecteur ouvert "actif a l'etat bas" :
   la sortie du module est aussi en collecteur ouvert et l'impulsion est materialisee en
   TIRANT A LA MASSE (le repos est laisse a la pull-up interne du Deye, de tension
   inconnue : 3,3 V, 5 V ou 12 V).  Mettre 0 pour une impulsion a l'etat haut.         */
#define OUT_PULSE_LEVEL_LOW  1

/*------------------------------------------------------------------------------------
 * 2. PARAMETRES TEMPS REEL (fixes a la compilation ; le reste est reglable en ligne)
 *
 *   Cadence de l'ISR : FRC1 = horloge CPU (80 MHz) / 16 => 5 ticks par microseconde.
 *   TICK_TICKS = 125 ticks => 25 us => 40 000 interruptions/s.
 *   Pourquoi 40 kHz : le jitter d'un front de sortie est borne a 25 us (un seul tick),
 *   soit 0,5 % a 500 Hz de tach ; et comme l'accumulateur de phase est exact, l'erreur
 *   ne s'accumule pas : la frequence MOYENNEE vue par l'onduleur est juste a 1e-5 pres.
 *----------------------------------------------------------------------------------*/
#define T1_DIVIDER       TIM_DIV16   // TIM_DIV1 = 80 MHz (80 ticks/us), TIM_DIV16 = 5 MHz
#define TICKS_PER_US     5           // coherent avec TIM_DIV16 a 80 MHz de CPU
#define TICK_TICKS       125         // rechargement => periode d'ISR de 25 us
#define TICK_HZ          40000       // frequence de l'ISR = 5 000 000 / TICK_TICKS

/* Deglitchit des entrees : un front n'est retenu que si le nouveau niveau reste stable
   DEBOUNCE_TICKS (100 us). Rejette les pointes HF tout en introduisant un retard
   CONSTANT, donc annule dans la mesure periode front-montant -> front-montant.        */
#define DEBOUNCE_TICKS   4
/* Periode minimale acceptee (0,5 ms => 2 kHz de tach => 60 000 RPM a 2 imp/tour).     */
#define MIN_PERIOD_TICKS 20
/* Periode maximale : au-dela le ventilateur est arrete (1,5 s).                       */
#define MAX_PERIOD_TICKS 60000
/* Fenetre de moyennage des RPM : la consigne de sortie n'est recalculee que toutes les
   WINDOW_MS ; le signal de sortie, lui, reste continu et regulier entre les mises a
   jour (l'averageur evite simplement le tremblement de la frequence synthetisee).     */
#define WINDOW_MS        400
/* Largeur d'impulsion de sortie : 5 ms (comme les tach NMB d'origine, impulsion
   longue), automatiquement ramenee a period/3 si la periode est trop courte.         */
#define PULSE_HIGH_TICKS 200

/* Garde-fous */
#define RATIO_MIN_X100   10          // x0,10
#define RATIO_MAX_X100   1000        // x10,00
#define OFFSET_MAX_RPM   3000        // offset maxi ajoute apres multiplication
#define RPM_MAX          20000       // RPM plafonnes (mesures et simulation)
#define PPR_MIN          1
#define PPR_MAX          4
#define TIMEOUT_MIN_MS   100
#define TIMEOUT_MAX_MS   20000
/* Plancher de stabilite : le Deye declenche une alarme en dessous d'environ
   400..600 RPM sur les ventilateurs d'origine. Si (RPM lus x ratio) descend sous ce
   seuil alors que le ventilateur tourne, on force la simulation a ce plancher.       */
#define RPM_SIM_FLOOR    600

/*------------------------------------------------------------------------------------
 * 3. CONFIGURATION PERSISTANTE  (LittleFS, fichier texte cles=valeurs, voir 6.)
 *   Tout est reglable SANS recompilation ni ouverture du boitier.
 *----------------------------------------------------------------------------------*/
#define CONFIG_FILE "/config.txt"

typedef struct {
  uint16_t ratioX100[NCHANNEL];  // multiplicateur x100 (190 = x1,90)
  uint8_t  pprIn[NCHANNEL];      // impulsions par tour du capteur du Noctua (1..4)
  uint8_t  pprOut[NCHANNEL];     // impulsions par tour a reemettre vers le Deye (1..4)
  int16_t  offsetRpm[NCHANNEL];  // offset ajoute apres multiplication (RPM)
  uint16_t timeoutMs;            // inactivite entree -> etat "attente de signal"
  uint8_t  holdOnLoss[NCHANNEL]; // 1 = fige la derniere valeur simulee si l'entree meurt
  char     apSsid[33];
  char     apPass[65];           // 8..63 caracteres ; "0" = reseau ouvert
  char     staSsid[33];          // vide = pas de connexion station
  char     staPass[65];
  uint16_t testRpm;              // RPM simules en mode test
  uint8_t  testMode;             // 1 = mode test (verification des seuils d'alarme Deye)
} AppConfig;

/* Ratios par defaut = rapport des vitesses nominales NMB d'origine / Noctua :
     canal 9 cm : NMB 09225VE ~2850 tr/min  / NF-A9-FLX 1500 tr/min  => x1,90
     canal 6 cm : NMB 06025VE ~5600 tr/min  / NF-A6x25  ~2900 tr/min => x1,93
   A ajuster finement avec la page web (voir SCHEMA.md, paragraphe "REGLAGE").       */
static const AppConfig CFG_DEFAULTS = {
  { 190, 193 },        // ratioX100
  { 2, 2 },            // pprIn   (Noctua : 2 impulsions par tour)
  { 2, 2 },            // pprOut  (NMB d'origine : 2 impulsions par tour)
  { 0, 0 },            // offsetRpm
  1200,                // timeoutMs
  { 0, 0 },            // holdOnLoss
  "DeyeFan-Config",    // apSsid
  "deyefan123",        // apPass
  "",                  // staSsid
  "",                  // staPass
  1200,                // testRpm
  0                    // testMode
};

/* Copie de travail, modifiable a chaud depuis la page web et persistee dans LittleFS */
static AppConfig cfg = CFG_DEFAULTS;

/*------------------------------------------------------------------------------------
 * 4. MONDE TEMPS REEL [A] : etat et code de l'ISR timer1 (tout en IRAM)
 *
 *   Seules des variables de type uint32_t (un mot) transitent entre [A] et [B] : les
 *   instructions d'acces a un mot de 32 bits en RAM interne sont atomiques sur un
 *   Xtensa, il n'y a donc aucune zone critique et aucun verrou a poser.
 *----------------------------------------------------------------------------------*/

/* ---- [A] -> [B] : resultats publies (lus par loop(), jamais ecrits par loop()) ---- */
static volatile uint32_t pubInRpm[NCHANNEL];      // RPM reels lus sur le Noctua
static volatile uint32_t pubSimRpm[NCHANNEL];     // RPM simules et reemis vers le Deye
static volatile uint32_t pubInPulses[NCHANNEL];   // compteur d'impulsions tach lues
static volatile uint32_t pubSimPulses[NCHANNEL];  // compteur d'impulsions tach reemises
static volatile uint32_t pubAlive;                // bit0 = canal 0 actif, bit1 = canal 1
static volatile uint32_t pubIsrPeriodTicks[NCHANNEL];  // periode du train de sortie (ticks)
static volatile uint32_t pubIsrPulseTicks[NCHANNEL];   // largeur d'impulsion de sortie (ticks)

/* ---- [B] -> [A] : consignes (ecrites par loop(), lues par l'ISR) ------------------ */
static volatile uint32_t cmdRatioX100[NCHANNEL];
static volatile uint32_t cmdOffsetRpm[NCHANNEL];
static volatile uint32_t cmdPprIn[NCHANNEL];
static volatile uint32_t cmdPprOut[NCHANNEL];
static volatile uint32_t cmdTimeoutTicks;         // delai "attente de signal" en ticks
static volatile uint32_t cmdHoldOnLoss;           // bit0/bit1 par canal
static volatile uint32_t cmdTest;                 // (testMode << 16) | testRpm
static volatile uint32_t cmdWindowTicks;          // fenetre de moyennage en ticks

/* ---- Etat interne de l'ISR -------------------------------------------------------
   Donnees NON const : elles sont placees par le compilateur dans la RAM interne
   (.dram0.data), toujours accessible y compris pendant les operations flash, ce qui
   est exactement ce qu'il faut pour un etat manipule sous interruption. Le code de
   l'ISR, lui, est force en IRAM (IRAM_ATTR) pour ne jamais lire la flash.            */
static struct {
  uint8_t  stable;        // niveau stable de l'entree (0/1)
  uint8_t  run;           // ticks passes sur le niveau candidat (deglitchit)
  uint8_t  pulseLeft;     // ticks restants de l'impulsion de sortie en cours
  uint8_t  pprIn, pprOut; // miroirs rapides de la configuration
  uint32_t age;           // ticks depuis le dernier front accepte (watchdog, 32 bits)
  uint16_t ageRise;       // ticks depuis le dernier front montant (mesure de periode)
  uint16_t pmax;          // periode maximale observee dans la fenetre (gros plan)
  uint16_t pWin;          // nombre de periodes retenues dans la fenetre
  uint32_t psum;          // somme des periodes retenues (ticks)
  uint32_t phase;         // accumulateur de phase du train de sortie (16.16)
  uint32_t inc;           // increment de phase par tick (0 si sortie arretée)
  uint16_t pulseTicks;    // largeur d'impulsion de sortie (ticks)
  uint16_t periodTicks;   // periode du train de sortie (ticks)
} st[NCHANNEL];

/* Etat de la LED de statut (rafraichie lentement, sans contrainte de temps) */
static uint32_t ledCounter = 0;
static uint8_t  ledState = 0;

/* Ecrit la LED de statut. Registre dedie pour GPIO16, ecriture bit-band sinon :
   dans les deux cas il s'agit d'un simple acces materiel, aucun appel de fonction.  */
static void IRAM_ATTR ledWrite(uint8_t onOff)
{
  if (PIN_STATUS_LED == 16) {
    GP16O = onOff ? 1u : 0u;
  } else {
    if (onOff) GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, (1UL << PIN_STATUS_LED));
    else       GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, (1UL << PIN_STATUS_LED));
  }
}

/* Sortie d'un canal : met la ligne au repos et coupe l'oscillateur numerique. */
static void IRAM_ATTR outKill(uint8_t ch)
{
  st[ch].inc = 0;
  st[ch].phase = 0;
  st[ch].pulseLeft = 0;
  st[ch].pulseTicks = 0;
  st[ch].periodTicks = 0;
#if OUT_PULSE_LEVEL_LOW
  GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, BIT_OUT(ch));   // repos = non conduit (libre)
#else
  GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, BIT_OUT(ch));
#endif
}

/* Reconfigure l'oscillateur numerique de sortie pour `rpm` tours/simules.
   inc = f * 2^16 / TICK_HZ  avec  f = rpm * pprOut / 60  (frequence de tach en Hz).
   L'increment est un entier : le train de sortie est parfaitement regulier et sa
   frequence moyenne est exactement rpm*pprOut/60, sans le moindre derive.           */
static void IRAM_ATTR outConfigure(uint8_t ch, uint32_t rpm)
{
  if (rpm > RPM_MAX) rpm = RPM_MAX;
  if (rpm < 1) rpm = 1;
  uint64_t num = ((uint64_t)rpm * (uint64_t)st[ch].pprOut) << 16;
  uint32_t inc = (uint32_t)(num / (60ULL * (uint64_t)TICK_HZ));
  if (inc < 1) inc = 1;
  st[ch].inc = inc;
  uint32_t per = 65536UL / inc;              // periode du train en ticks
  if (per > MAX_PERIOD_TICKS) per = MAX_PERIOD_TICKS;
  if (per < 6) per = 6;
  uint32_t hi = per / 3;                     // rapport cyclique <= 1/3
  if (hi > PULSE_HIGH_TICKS) hi = PULSE_HIGH_TICKS;
  if (hi < 1) hi = 1;
  st[ch].periodTicks = (uint16_t)per;
  st[ch].pulseTicks  = (uint16_t)hi;
}

/* Declarations complementaires : indicateurs de qualite du signal mesure */
static volatile uint32_t pubMaxPeriod[NCHANNEL];  // periode maxi (gros plan) de la fenetre
static volatile uint32_t pubFloorRpm[NCHANNEL];   // plancher applique a la simulation

/* Compteurs de fenetre (decales pour etaler la charge) et derniere valeur simulee
   connue (utilisee par l'option "figer en cas de perte de signal").                  */
static uint16_t IRAM_ATTR winCnt[NCHANNEL]     = { 0, TICK_HZ / 8 };
static uint32_t IRAM_ATTR lastSimRpm[NCHANNEL] = { 0, 0 };

/*------------------------------------------------------------------------------------
 * ISR du timer1 materiel (FRC1) : 40 000 executions/s, ~120 cycles, ~0,4 % du CPU.
 * Regles de securite respectees ici :
 *   - code et donnees en IRAM (IRAM_ATTR) : aucun acces flash, pas meme a PROGMEM ;
 *   - aucun appel de fonction de bibliothique (String, Serial, math, malloc...) ;
 *   - aucune operation flottante (le core devrait sauver/restaurer le FPU dans l'ISR,
 *     ce qui allongerait le temps passe sous interruption) ;
 *   - acces materiels directs aux registres GPIO (lecture/ecriture en 1 cycle) ;
 *   - duree totale < 3 us : l'ISR ne peut donc jamais empieter sur le service WiFi.
 * Le timer etant en auto-recharge (TIM_LOOP), la cadence est materielle : si une
 * interruption ROM du WiFi retarde l'entree dans cette ISR, la cadence repart du
 * compteur materiel et la frequence moyennee vue par l'onduleur reste exacte.
 *----------------------------------------------------------------------------------*/
static void IRAM_ATTR isrTimer1()
{
  /* Une seule lecture 32 bits couvre les deux tach d'entree */
  const uint32_t gpIn = GPIO_REG_READ(GPIO_IN_ADDRESS);
  const uint32_t timeout  = cmdTimeoutTicks;
  const uint32_t holdMask = cmdHoldOnLoss;
  uint8_t alive = 0;

  for (uint8_t ch = 0; ch < NCHANNEL; ch++) {
    const uint8_t lvl = (gpIn & BIT_IN(ch)) ? 1 : 0;

    /* ---------------- 1) entree : deglitchit + datation des fronts -------------- */
    if (st[ch].age     < 0xFFFFFFFFu) st[ch].age++;
    if (st[ch].ageRise < 0xFFFFu)     st[ch].ageRise++;

    if (lvl == st[ch].stable) {
      st[ch].run = 0;
    } else {
      st[ch].run++;
      if (st[ch].run >= DEBOUNCE_TICKS) {          // nouveau niveau confirme = front
        st[ch].stable = lvl;
        st[ch].run = 0;
        st[ch].age = 0;
        if (lvl) {                                 // front montant -> periode d'un tour
          uint16_t per = st[ch].ageRise;
          st[ch].ageRise = 0;
          pubInPulses[ch]++;
          if (per >= MIN_PERIOD_TICKS && per <= MAX_PERIOD_TICKS) {
            st[ch].psum += per;
            if (st[ch].pWin < 0xFFFEu) st[ch].pWin++;
            if (per > st[ch].pmax) st[ch].pmax = per;
          }
        }
      }
    }
    if (st[ch].age <= timeout) alive |= (uint8_t)(1u << ch);

    /* ---------------- 2) sortie : oscillateur numerique (Bresenham) ------------- */
    if (st[ch].inc) {
      if (!alive && !(holdMask & (1u << ch))) {
        outKill(ch);                               // entree muette -> on cesse d'emettre
      } else if (st[ch].pulseLeft) {
        st[ch].pulseLeft--;
        if (!st[ch].pulseLeft) {                   // fin d'impulsion : relachement
#if OUT_PULSE_LEVEL_LOW
          GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, BIT_OUT(ch));
#else
          GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, BIT_OUT(ch));
#endif
          pubSimPulses[ch]++;
        }
      } else {
        st[ch].phase += st[ch].inc;
        if (st[ch].phase >= 0x10000UL) {           // debordement -> nouvelle impulsion
          st[ch].phase -= 0x10000UL;
#if OUT_PULSE_LEVEL_LOW
          GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, BIT_OUT(ch)); // conduit -> etat bas
#else
          GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, BIT_OUT(ch));
#endif
          st[ch].pulseLeft = st[ch].pulseTicks;
        }
      }
    }
    /* ---------------- 3) fin de fenetre : RPM mesures + consignes de sortie ----- */
    if (winCnt[ch]) {
      winCnt[ch]--;
      if (winCnt[ch]) continue;                    // fenetre en cours
    }
    winCnt[ch] = (uint16_t)cmdWindowTicks;         // recharge (400 ticks = 400 ms)

    /* miroirs IRAM des consignes : chaque consigne tient dans un mot, lecture atomique */
    st[ch].pprIn  = (uint8_t)cmdPprIn[ch];
    st[ch].pprOut = (uint8_t)cmdPprOut[ch];
    if (st[ch].pprIn  < PPR_MIN) st[ch].pprIn  = PPR_MIN;
    if (st[ch].pprIn  > PPR_MAX) st[ch].pprIn  = PPR_MAX;
    if (st[ch].pprOut < PPR_MIN) st[ch].pprOut = PPR_MIN;
    if (st[ch].pprOut > PPR_MAX) st[ch].pprOut = PPR_MAX;

    uint32_t rpmIn = 0;
    if (st[ch].pWin && st[ch].psum) {
      /* rpm = (TICK_HZ*60/pprIn) * N_periodes / somme_des_periodes */
      const uint32_t k = ((uint32_t)TICK_HZ * 60UL) / (uint32_t)st[ch].pprIn;
      rpmIn = (uint32_t)(((uint64_t)k * (uint64_t)st[ch].pWin) / (uint64_t)st[ch].psum);
      if (rpmIn > RPM_MAX) rpmIn = RPM_MAX;
    }
    pubInRpm[ch] = rpmIn;
    pubMaxPeriod[ch] = st[ch].pmax;

    uint32_t target = rpmIn;
    if (alive & (1u << ch)) {
      /* frequence simulee = RPM x ratio + offset, exprimee en tours/min simules */
      target = (uint32_t)(((uint64_t)rpmIn * cmdRatioX100[ch]) / 100ULL);
      if ((int32_t)cmdOffsetRpm[ch] < 0) target -= (uint32_t)(-(int32_t)cmdOffsetRpm[ch]);
      else                               target += (uint32_t)(int32_t)cmdOffsetRpm[ch];

      /* Plancher : on ne propose jamais au Deye une frequence inferieure au gros plan
         reellement mesure (x1,2 de marge) ni inferieure a RPM_SIM_FLOOR. Cela rend la
         simulation volontairement toujours un peu plus rapide que le ventilateur :
         l'onduleur voit un ventilateur en pleine sante, jamais en train de caler.   */
      uint32_t floorRpm = RPM_SIM_FLOOR;
      if (st[ch].pmax) {
        uint32_t minInst = (uint32_t)((12ULL * (uint64_t)TICK_HZ * 60ULL) /
                                      (10ULL * (uint64_t)st[ch].pmax));
        if (minInst > floorRpm) floorRpm = minInst;
      }
      pubFloorRpm[ch] = floorRpm;
      if (target < floorRpm) target = floorRpm;

      const uint32_t tst = cmdTest;                // mode test force (verif des alarmes)
      if (tst & 0x10000UL) target = (tst & 0xFFFFUL);
    } else {
      pubFloorRpm[ch] = 0;
      if (!(holdMask & (1u << ch))) {              // entree muette : plus rien n'emet
        pubSimRpm[ch] = 0;
        st[ch].pWin = 0; st[ch].psum = 0; st[ch].pmax = 0;
        continue;
      }
      target = lastSimRpm[ch];                     // option "figer la derniere valeur"
    }
    if (target > RPM_MAX) target = RPM_MAX;

    lastSimRpm[ch] = target;
    pubSimRpm[ch] = target;
    outConfigure(ch, target);
    pubIsrPeriodTicks[ch] = st[ch].periodTicks;
    pubIsrPulseTicks[ch]  = st[ch].pulseTicks;

    st[ch].pWin = 0; st[ch].psum = 0; st[ch].pmax = 0;
  }

  pubAlive = alive;

  /* ---------------- LED de statut, rafraichie 2 fois par seconde -----------------
     allumee  : le module emet au moins un tach synthetise  -> "je simule"
     eteinte   : aucun tach synthetise (attente de signal)   -> "j'attends"
     1 Hz      : mode test actif                                                         */
  if (++ledCounter >= (TICK_HZ / 2)) {
    ledCounter = 0;
    if (cmdTest & 0x10000UL)      ledState ^= 1;
    else ledState = (st[0].inc || st[1].inc) ? 1 : 0;
    ledWrite(ledState);
  }
}
/*------------------------------------------------------------------------------------
 * 5. DEMARRAGE DU MONDE TEMPS REEL + APPLICATION DES CONSIGNES
 *----------------------------------------------------------------------------------*/
static void startRealtimeTimer()
{
  /* timer1 (FRC1) : peripherique materiel pur, independant de la pile logicielle.
     TIM_DIV16 => 5 ticks/us ; TIM_EDGE + TIM_LOOP => auto-recharge matrielle, donc
     AUCUN rearmement logiciel dans l'ISR : la periode de 25 us est tenue par le
     compteur materiel et ne peut pas deriver, quelle que soit la charge logicielle. */
  timer1_isr_init();
  timer1_attachInterrupt(isrTimer1);
  timer1_enable(T1_DIVIDER, TIM_EDGE, TIM_LOOP);
  timer1_write(TICK_TICKS);
}

/* Diffuse la configuration vers le monde temps reel. Une seule ecriture de 32 bits
   par consigne : l'ISR ne voit jamais de valeur a moitie ecrite, et le serveur HTTP
   peut appeler cette fonction a tout moment sans risque ni verrou.                 */
static void applyConfig()
{
  uint32_t hold = 0;
  for (uint8_t ch = 0; ch < NCHANNEL; ch++) {
    cmdRatioX100[ch] = cfg.ratioX100[ch];
    cmdOffsetRpm[ch] = (uint32_t)(int32_t)cfg.offsetRpm[ch];
    cmdPprIn[ch]     = cfg.pprIn[ch];
    cmdPprOut[ch]    = cfg.pprOut[ch];
    if (cfg.holdOnLoss[ch]) hold |= (1u << ch);
  }
  cmdHoldOnLoss = hold;
  cmdTimeoutTicks = (uint32_t)(((uint32_t)cfg.timeoutMs * (uint32_t)TICK_HZ) / 1000UL);
  if (cmdTimeoutTicks < 4) cmdTimeoutTicks = 4;
  cmdWindowTicks = (uint32_t)(((uint32_t)WINDOW_MS * (uint32_t)TICK_HZ) / 1000UL);
  if (cmdWindowTicks < 40) cmdWindowTicks = 40;
  cmdTest = ((uint32_t)cfg.testMode << 16) | (uint32_t)cfg.testRpm;
}

static inline uint16_t clampU16(long v, long lo, long hi)
{
  if (v < lo) v = lo;
  if (v > hi) v = hi;
  return (uint16_t)v;
}

/*------------------------------------------------------------------------------------
 * 6. PERSISTANCE (LittleFS). Format : une cle par ligne, "cle=valeur".
 *   Simple, lisible, edevenable a la main, et surtout : aucune ecriture flash n'est
 *   faite tant que le module ne recoit pas une demande explicite de sauvegarde.
 *----------------------------------------------------------------------------------*/
static void cfgSet(const char* key, const char* val)
{
  if      (!strcmp(key, "ratio1"))   cfg.ratioX100[0]  = clampU16(atol(val), RATIO_MIN_X100, RATIO_MAX_X100);
  else if (!strcmp(key, "ratio2"))   cfg.ratioX100[1]  = clampU16(atol(val), RATIO_MIN_X100, RATIO_MAX_X100);
  else if (!strcmp(key, "pprin1"))   cfg.pprIn[0]      = (uint8_t)clampU16(atol(val), PPR_MIN, PPR_MAX);
  else if (!strcmp(key, "pprin2"))   cfg.pprIn[1]      = (uint8_t)clampU16(atol(val), PPR_MIN, PPR_MAX);
  else if (!strcmp(key, "pprout1"))  cfg.pprOut[0]     = (uint8_t)clampU16(atol(val), PPR_MIN, PPR_MAX);
  else if (!strcmp(key, "pprout2"))  cfg.pprOut[1]     = (uint8_t)clampU16(atol(val), PPR_MIN, PPR_MAX);
  else if (!strcmp(key, "off1"))     cfg.offsetRpm[0]  = (int16_t)atol(val);
  else if (!strcmp(key, "off2"))     cfg.offsetRpm[1]  = (int16_t)atol(val);
  else if (!strcmp(key, "toff"))     cfg.timeoutMs     = clampU16(atol(val), TIMEOUT_MIN_MS, TIMEOUT_MAX_MS);
  else if (!strcmp(key, "hold1"))    cfg.holdOnLoss[0] = (uint8_t)(atol(val) ? 1 : 0);
  else if (!strcmp(key, "hold2"))    cfg.holdOnLoss[1] = (uint8_t)(atol(val) ? 1 : 0);
  else if (!strcmp(key, "testrpm"))  cfg.testRpm       = clampU16(atol(val), 0, RPM_MAX);
  else if (!strcmp(key, "apssid"))   strncpy(cfg.apSsid, val, sizeof(cfg.apSsid) - 1),   cfg.apSsid[sizeof(cfg.apSsid) - 1] = 0;
  else if (!strcmp(key, "appass"))   strncpy(cfg.apPass, val, sizeof(cfg.apPass) - 1),   cfg.apPass[sizeof(cfg.apPass) - 1] = 0;
  else if (!strcmp(key, "stassid"))  strncpy(cfg.staSsid, val, sizeof(cfg.staSsid) - 1), cfg.staSsid[sizeof(cfg.staSsid) - 1] = 0;
  else if (!strcmp(key, "stapass"))  strncpy(cfg.staPass, val, sizeof(cfg.staPass) - 1), cfg.staPass[sizeof(cfg.staPass) - 1] = 0;
}

static bool loadConfig()
{
  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f) return false;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (!line.length() || line.charAt(0) == '#') continue;
    int eq = line.indexOf('=');
    if (eq <= 0) continue;
    String k = line.substring(0, eq);
    String v = line.substring(eq + 1);
    k.trim(); v.trim();
    cfgSet(k.c_str(), v.c_str());
  }
  f.close();
  return true;
}

static bool saveConfig()
{
  File f = LittleFS.open(CONFIG_FILE, "w");
  if (!f) return false;
  f.println("# configuration de l'emulateur tach Deye (edenable a la main)");
  f.printf("ratio1=%u\r\n",   cfg.ratioX100[0]);
  f.printf("ratio2=%u\r\n",   cfg.ratioX100[1]);
  f.printf("pprin1=%u\r\n",   cfg.pprIn[0]);
  f.printf("pprin2=%u\r\n",   cfg.pprIn[1]);
  f.printf("pprout1=%u\r\n",  cfg.pprOut[0]);
  f.printf("pprout2=%u\r\n",  cfg.pprOut[1]);
  f.printf("off1=%d\r\n",     cfg.offsetRpm[0]);
  f.printf("off2=%d\r\n",     cfg.offsetRpm[1]);
  f.printf("toff=%u\r\n",     cfg.timeoutMs);
  f.printf("hold1=%u\r\n",    cfg.holdOnLoss[0]);
  f.printf("hold2=%u\r\n",    cfg.holdOnLoss[1]);
  f.printf("testrpm=%u\r\n",  cfg.testRpm);
  f.printf("apssid=%s\r\n",   cfg.apSsid);
  f.printf("appass=%s\r\n",   cfg.apPass);
  f.printf("stassid=%s\r\n",  cfg.staSsid);
  f.printf("stapass=%s\r\n",  cfg.staPass);
  f.close();
  return true;
}

static uint32_t rebootAt = 0;      // 0 = pas de redemarrage demande
static void requestReboot(uint32_t delayMs) { rebootAt = millis() + delayMs; }

/*------------------------------------------------------------------------------------
 * 7. INTERFACE WEB (monde [B] uniquement) : page de configuration + mesures en direct
 *    (la page est stockee en flash/PROGMEM et sert par morceaux : elle n'est jamais
 *     construite a la volee, ce qui evite toute fragmentation de tas)
 *----------------------------------------------------------------------------------*/
static ESP8266WebServer server(80);

static const char PAGE_HTML[] PROGMEM =
R"rawlpage(
<!DOCTYPE html><html lang=fr><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Emulateur tach Deye</title>
<style>
body{font-family:system-ui,Arial,sans-serif;background:#10151c;color:#dfe6ee;margin:0;padding:14px}
h1{font-size:1.1rem;margin:0 0 12px}
.g{display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(310px,1fr))}
.c{background:#18202a;border:1px solid #2a3644;border-radius:10px;padding:12px}
.c h2{font-size:.95rem;margin:0 0 8px;color:#79c0ff}
table{width:100%;border-collapse:collapse;font-size:.88rem}
td{padding:3px 0;border-bottom:1px solid #253040}
td:last-child{text-align:right;font-variant-numeric:tabular-nums}
.b{font-size:1.45rem;font-weight:700}
.badge{display:inline-block;padding:2px 8px;border-radius:99px;font-size:.72rem}
.on{background:#14431f;color:#7ee28f}.off{background:#3c2020;color:#f2a0a0}
label{display:block;font-size:.74rem;color:#94a3b3;margin-top:7px}
input,select{width:100%;box-sizing:border-box;padding:6px;background:#0e141b;color:#e8eef5;border:1px solid #33414f;border-radius:6px}
.row{display:flex;gap:6px}.row>*{flex:1}
button{margin-top:9px;padding:8px 10px;background:#2b6cb0;color:#fff;border:0;border-radius:6px;cursor:pointer;font-size:.85rem}
button.w{background:#8a5a1a}button.r{background:#7a2626}
.h{font-size:.72rem;color:#8b98a7;margin-top:7px}
</style></head><body>
<h1>Emulateur tachymetrique - Deye SUN-8K-SG05LP1</h1>
<div class=g>
<div class=c><h2>Canal 1 - ventilateur 90 mm</h2>
<table>
<tr><td>RPM lus (Noctua NF-A9)</td><td id=i0 class=b>-</td></tr>
<tr><td>RPM simules vers le Deye</td><td id=s0 class=b>-</td></tr>
<tr><td>Etat</td><td><span id=a0 class="badge off">attente</span></td></tr>
<tr><td>Ratio applique</td><td id=r0>-</td></tr>
<tr><td>Plancher applique</td><td id=f0>-</td></tr>
<tr><td>Impulsions lues / emises</td><td id=p0>-</td></tr>
<tr><td>Trame de sortie</td><td id=q0>-</td></tr>
</table>
<div class=row><div><label>Ratio (&times;)</label><input id=ratio0 type=text></div>
<div><label>Offset (RPM)</label><input id=off0 type=number step=10></div></div>
<div class=row><div><label>PPR entree</label><input id=pprin0 type=number min=1 max=4></div>
<div><label>PPR sortie</label><input id=pprout0 type=number min=1 max=4></div>
<div><label>Perte de signal</label><select id=hold0><option value=0>cesse d'emettre</option><option value=1>fige la valeur</option></select></div></div>
</div>
<div class=c><h2>Canal 2 - ventilateur 60 mm</h2>
<table>
<tr><td>RPM lus (Noctua NF-A6x25)</td><td id=i1 class=b>-</td></tr>
<tr><td>RPM simules vers le Deye</td><td id=s1 class=b>-</td></tr>
<tr><td>Etat</td><td><span id=a1 class="badge off">attente</span></td></tr>
<tr><td>Ratio applique</td><td id=r1>-</td></tr>
<tr><td>Plancher applique</td><td id=f1>-</td></tr>
<tr><td>Impulsions lues / emises</td><td id=p1>-</td></tr>
<tr><td>Trame de sortie</td><td id=q1>-</td></tr>
</table>
<div class=row><div><label>Ratio (&times;)</label><input id=ratio1 type=text></div>
<div><label>Offset (RPM)</label><input id=off1 type=number step=10></div></div>
<div class=row><div><label>PPR entree</label><input id=pprin1 type=number min=1 max=4></div>
<div><label>PPR sortie</label><input id=pprout1 type=number min=1 max=4></div>
<div><label>Perte de signal</label><select id=hold1><option value=0>cesse d'emettre</option><option value=1>fige la valeur</option></select></div></div>
</div>
)rawlpage"
R"rawlpage(
<div class=c><h2>Reglages globaux</h2>
<label>Delai d'attente de signal (s)</label><input id=toff type=text>
<label>RPM simules en mode test</label><input id=testrpm type=number min=0 max=4000 step=50>
<div class=row><button id=btnsave>Enregistrer les reglages</button>
<button id=btest class=w>Demarrer le test</button></div>
<div class=h>Le mode test emet une frequence forcee (LED clignotante) afin de determiner a
quel seuil le Deye declenche son alarme ventilateur. A laisser eteint en service normal.</div>
</div>
<div class=c><h2>WiFi (persistant)</h2>
<label>Reseau d'accueil cree par le module (AP)</label><input id=apssid type=text>
<label>Cle AP (WPA2, 8 car. minimum ; 0 = reseau ouvert)</label><input id=appass type=password placeholder="inchangee si laisse vide">
<label>Reseau a rejoindre (STA, vide = aucun)</label><input id=stassid type=text>
<label>Cle STA</label><input id=stapass type=password placeholder="inchangee si laisse vide">
<button id=btnwifi>Enregistrer WiFi et redemarrer</button>
<div class=h>Mode AP+STA simultane : le module cree en permanence son reseau d'urgence
(192.168.4.1) et se connecte en parallele a votre reseau. Les parametres sont conserves
entre les redemarrages.</div>
</div>
<div class=c><h2>Systeme</h2>
<table>
<tr><td>Firmware</td><td id=ver>-</td></tr>
<tr><td>Duree de marche</td><td id=up>-</td></tr>
<tr><td>Mont libre</td><td id=heap>-</td></tr>
<tr><td>Cadence ISR</td><td id=isr>-</td></tr>
<tr><td>IP du point d'acces</td><td id=ipap>-</td></tr>
<tr><td>Etat station / RSSI</td><td id=sta>-</td></tr>
<tr><td>IP station</td><td id=ipsta>-</td></tr>
</table>
<div class=row><button class=r id=btnfac>Retour aux reglages d'usine</button></div>
<div class=h>Acces local : http://deye-fan.local ou http://192.168.4.1</div>
</div>
</div>
<script>
var filled=false,tst=0;
function T(i,v){var e=document.getElementById(i);if(e)e.textContent=v}
function V(i){return document.getElementById(i).value}
function stat(d){
 for(var k=0;k<2;k++){var c=d.ch[k];
  T('i'+k,c.inRpm);T('s'+k,c.simRpm);T('r'+k,'x'+(c.ratio/100).toFixed(2));
  T('f'+k,c.floor?c.floor:'-');T('p'+k,c.inPul+' / '+c.simPul);
  T('q'+k,c.outHz+' Hz / '+c.pulseUs+' us');
  var a=document.getElementById('a'+k);
  a.textContent=c.alive?'simulation active':'attente de signal';
  a.className='badge '+(c.alive?'on':'off');}
 T('ver',d.ver);T('up',d.up);T('heap',d.heap);T('isr',d.isrHz+' Hz / '+d.isrUs+' us');
 T('ipap',d.apIp);T('sta',d.staOk?'connecte ('+d.rssi+' dBm)':'deconnecte');T('ipsta',d.staIp);
 if(!filled){filled=true;
  for(var j=0;j<2;j++){var e=d.ch[j];
   document.getElementById('ratio'+j).value=(e.cfgRatio/100).toFixed(2);
   document.getElementById('off'+j).value=e.cfgOff;
   document.getElementById('pprin'+j).value=e.pprIn;
   document.getElementById('pprout'+j).value=e.pprOut;
   document.getElementById('hold'+j).value=e.cfgHold;}
  document.getElementById('toff').value=d.toff;
  document.getElementById('testrpm').value=d.testRpm;
  document.getElementById('apssid').value=d.apSsid;
  document.getElementById('stassid').value=d.staSsid;
  tst=d.testMode;
  document.getElementById('btest').textContent=tst?'Arreter le test':'Demarrer le test';}}
function poll(){fetch('/api/status',{cache:'no-store'}).then(function(r){return r.json()}).then(stat)
 .catch(function(){T('sta','erreur reseau')})}
function get(u){return fetch(u,{cache:'no-store'}).then(function(r){return r.json()})
 .then(function(j){return j.ok}).catch(function(){return false})}
function Q(o){return Object.keys(o).map(function(k){return k+'='+encodeURIComponent(o[k])}).join('&')}
setInterval(poll,1000);poll();
document.getElementById('btnsave').onclick=function(){
 get('/cfg?'+Q({ratio0:V('ratio0'),off0:V('off0'),pprin0:V('pprin0'),pprout0:V('pprout0'),hold0:V('hold0'),
 ratio1:V('ratio1'),off1:V('off1'),pprin1:V('pprin1'),pprout1:V('pprout1'),hold1:V('hold1'),
 toff:V('toff'),testrpm:V('testrpm')})).then(function(o){alert(o?'Reglages enregistres et appliques':'Echec de lenregistrement')})};
document.getElementById('btest').onclick=function(){tst=tst?0:1;
 get('/test?on='+tst+'&rpm='+V('testrpm'));
 document.getElementById('btest').textContent=tst?'Arreter le test':'Demarrer le test'};
document.getElementById('btnwifi').onclick=function(){
 var o={apssid:V('apssid'),stassid:V('stassid')};
 if(V('appass'))o.appass=V('appass');
 if(V('stapass'))o.stapass=V('stapass');
 get('/wifi?'+Q(o)).then(function(o2){alert(o2?'WiFi enregistre, redemarrage en cours':'Echec')})};
document.getElementById('btnfac').onclick=function(){
 if(confirm('Effacer la configuration enregistree ?'))
  get('/factory').then(function(){alert('Redemarrage sur les reglages d usine')})};
</script></body></html>
)rawlpage";
/*------------------------------------------------------------------------------------
 * 8. SERVEUR HTTP (monde [B]). Il ne lit que des mots atomiques publies par l'ISR et
 *    n'ecrit que des consignes : aucune temporisation, aucune attente, donc aucune
 *    possibility de generer le moindre retard dans le monde temps reel.
 *----------------------------------------------------------------------------------*/
static const char FW_VERSION[] = "deye_fan 1.0";

static String jEsc(const char* s)
{
  String o;
  for (const char* p = s; *p; p++) {
    char c = *p;
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((unsigned char)c < 32) o += ' ';
    else o += c;
  }
  return o;
}

static String argStr(const char* key) { return server.hasArg(key) ? server.arg(key) : String(); }

static long argLong(const char* key, long cur, long lo, long hi)
{
  if (!server.hasArg(key)) return cur;
  String s = server.arg(key); s.trim();
  if (!s.length()) return cur;
  return clampU16(atol(s.c_str()), lo, hi);
}

/* Le champ ratio accepte l'ecriture fractionnaire francaise ou americaine ("1,9" / "1.9") */
static uint16_t argRatioX100(const char* key, uint16_t cur)
{
  if (!server.hasArg(key)) return cur;
  String s = server.arg(key); s.trim(); s.replace(',', '.');
  if (!s.length()) return cur;
  float f = s.toFloat();
  long v = (long)(f * 100.0f + (f < 0.0f ? -0.5f : 0.5f));
  return clampU16(v, RATIO_MIN_X100, RATIO_MAX_X100);
}

/* Champ en secondes ("1.2") converti en millisecondes */
static uint16_t argMillis(const char* key, uint16_t cur)
{
  if (!server.hasArg(key)) return cur;
  String s = server.arg(key); s.trim(); s.replace(',', '.');
  if (!s.length()) return cur;
  long ms = (long)(s.toFloat() * 1000.0f);
  return clampU16(ms, TIMEOUT_MIN_MS, TIMEOUT_MAX_MS);
}

static void setStr(char* dst, size_t n, const String& src)
{
  strncpy(dst, src.c_str(), n - 1);
  dst[n - 1] = 0;
}

static void handlePage()
{
  server.send_P(200, "text/html", PAGE_HTML);
}

static void handleStatus()
{
  String j;
  j.reserve(1100);
  j  = "{\"ver\":\"" + String(FW_VERSION) + "\"";
  j += ",\"up\":" + String(millis() / 1000UL);
  j += ",\"heap\":" + String(ESP.getFreeHeap());
  j += ",\"isrHz\":" + String((uint32_t)TICK_HZ) + ",\"isrUs\":" + String((uint32_t)TICK_US);
  j += ",\"toff\":" + String((uint32_t)(cfg.timeoutMs / 1000)) + "." +
       String((uint32_t)((cfg.timeoutMs % 1000) / 100));
  j += ",\"testMode\":" + String(cfg.testMode ? 1 : 0) + ",\"testRpm\":" + String(cfg.testRpm);
  j += ",\"apSsid\":\"" + jEsc(cfg.apSsid) + "\",\"staSsid\":\"" + jEsc(cfg.staSsid) + "\"";
  j += ",\"apIp\":\"" + WiFi.softAPIP().toString() + "\"";
  j += ",\"staOk\":" + String(WiFi.status() == WL_CONNECTED ? 1 : 0);
  j += ",\"rssi\":" + String(WiFi.RSSI());
  j += ",\"staIp\":\"" + WiFi.localIP().toString() + "\"";
  j += ",\"ch\":[";
  for (uint8_t ch = 0; ch < NCHANNEL; ch++) {
    /* copie locale des mots atomiques publies par l'ISR : lecture sans verrou */
    const uint32_t inRpm  = pubInRpm[ch];
    const uint32_t simRpm = pubSimRpm[ch];
    const uint32_t inPul  = pubInPulses[ch];
    const uint32_t simPul = pubSimPulses[ch];
    const uint32_t alive  = (pubAlive >> ch) & 1UL;
    const uint32_t flr    = pubFloorRpm[ch];
    const uint32_t maxUs  = (uint32_t)pubMaxPeriod[ch] * (uint32_t)TICK_US;
    const uint32_t perUs  = (uint32_t)pubIsrPeriodTicks[ch] * (uint32_t)TICK_US;
    const uint32_t pulUs  = (uint32_t)pubIsrPulseTicks[ch] * (uint32_t)TICK_US;
    const uint32_t outHz  = perUs ? (1000000UL / perUs) : 0UL;
    const uint32_t applied= (inRpm && simRpm) ? (simRpm * 100UL / inRpm) : 0UL;
    if (ch) j += ",";
    j += "{\"inRpm\":"    + String(inRpm);
    j += ",\"simRpm\":"   + String(simRpm);
    j += ",\"alive\":"    + String(alive);
    j += ",\"floor\":"    + String(flr);
    j += ",\"inPul\":"    + String(inPul);
    j += ",\"simPul\":"   + String(simPul);
    j += ",\"maxUs\":"    + String(maxUs);
    j += ",\"outHz\":"    + String(outHz);
    j += ",\"pulseUs\":"  + String(pulUs);
    j += ",\"ratio\":"    + String(applied);
    j += ",\"cfgRatio\":" + String(cfg.ratioX100[ch]);
    j += ",\"cfgOff\":"   + String(cfg.offsetRpm[ch]);
    j += ",\"pprIn\":"    + String(cfg.pprIn[ch]);
    j += ",\"pprOut\":"   + String(cfg.pprOut[ch]);
    j += ",\"cfgHold\":"  + String(cfg.holdOnLoss[ch] ? 1 : 0);
    j += "}";
  }
  j += "]}";
  server.send(200, "application/json", j);
}
/* Reglages de simulation : appliques immediatement (aucun redemarrage necessaire)     */
static void handleCfg()
{
  cfg.ratioX100[0]   = argRatioX100("ratio0", cfg.ratioX100[0]);
  cfg.ratioX100[1]   = argRatioX100("ratio1", cfg.ratioX100[1]);
  cfg.offsetRpm[0]   = (int16_t)argLong("off0",   cfg.offsetRpm[0], -OFFSET_MAX_RPM, OFFSET_MAX_RPM);
  cfg.offsetRpm[1]   = (int16_t)argLong("off1",   cfg.offsetRpm[1], -OFFSET_MAX_RPM, OFFSET_MAX_RPM);
  cfg.pprIn[0]       = (uint8_t)argLong("pprin0",  cfg.pprIn[0],  PPR_MIN, PPR_MAX);
  cfg.pprIn[1]       = (uint8_t)argLong("pprin1",  cfg.pprIn[1],  PPR_MIN, PPR_MAX);
  cfg.pprOut[0]      = (uint8_t)argLong("pprout0", cfg.pprOut[0], PPR_MIN, PPR_MAX);
  cfg.pprOut[1]      = (uint8_t)argLong("pprout1", cfg.pprOut[1], PPR_MIN, PPR_MAX);
  cfg.holdOnLoss[0]  = (uint8_t)argLong("hold0",   cfg.holdOnLoss[0], 0, 1);
  cfg.holdOnLoss[1]  = (uint8_t)argLong("hold1",   cfg.holdOnLoss[1], 0, 1);
  cfg.timeoutMs      = argMillis("toff", cfg.timeoutMs);
  cfg.testRpm        = (uint16_t)argLong("testrpm", cfg.testRpm, 0, RPM_MAX);
  applyConfig();                       /* propagation immediate vers le monde [A] */
  const bool ok = saveConfig();
  server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"ok\":0}");
  Serial.printf("[web] reglages appliques : r1=x%u.%02u r2=x%u.%02u toff=%u ms ok=%d\n",
                cfg.ratioX100[0] / 100, cfg.ratioX100[0] % 100,
                cfg.ratioX100[1] / 100, cfg.ratioX100[1] % 100, cfg.timeoutMs, ok);
}

/* Mode test : emission forcee a une frequence imposee (recherche du seuil d'alarme)    */
static void handleTest()
{
  const long on = argLong("on", cfg.testMode, 0, 1);
  cfg.testMode = (uint8_t)on;
  if (server.hasArg("rpm")) cfg.testRpm = (uint16_t)argLong("rpm", cfg.testRpm, 0, RPM_MAX);
  applyConfig();
  const bool ok = saveConfig();
  server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"ok\":0}");
  Serial.printf("[web] mode test = %u (RPM forcees = %u)\n", cfg.testMode, cfg.testRpm);
}

/* WiFi : AP et STA simultanees ; un redemarrage propre est demande apres la reponse   */
static void handleWifi()
{
  const String ap  = argStr("apssid");
  const String app = argStr("appass");
  const String st  = argStr("stassid");
  const String stp = argStr("stapass");

  if (ap.length())  setStr(cfg.apSsid,  sizeof(cfg.apSsid),  ap);
  if (st.length())  setStr(cfg.staSsid, sizeof(cfg.staSsid), st);
  if (app.length()) {
    if (app == "0") setStr(cfg.apPass, sizeof(cfg.apPass), String(""));
    else if (app.length() < 8) {
      server.send(200, "application/json", "{\"ok\":0,\"err\":\"cle AP trop courte\"}");
      return;
    } else setStr(cfg.apPass, sizeof(cfg.apPass), app);
  }
  if (stp.length()) setStr(cfg.staPass, sizeof(cfg.staPass), stp);

  const bool ok = saveConfig();
  server.send(200, "application/json", ok ? "{\"ok\":1}" : "{\"ok\":0}");
  Serial.println("[web] WiFi enregistre, redemarrage dans 2 s");
  if (ok) requestReboot(2000);
}

/* Usine : supprime le fichier de configuration et redemarre sur les valeurs par defaut */
static void handleFactory()
{
  LittleFS.remove(CONFIG_FILE);
  cfg = CFG_DEFAULTS;
  applyConfig();
  server.send(200, "application/json", "{\"ok\":1}");
  Serial.println("[web] retour aux reglages d'usine, redemarrage dans 1,5 s");
  requestReboot(1500);
}

static void handleNotFound()
{
  server.send(404, "application/json", "{\"ok\":0,\"err\":\"introuvable\"}");
}

/*------------------------------------------------------------------------------------
 * 9. OUVERTURE DU WiFi EN MODE AP + STA SIMULTANES
 *----------------------------------------------------------------------------------*/
static void wifiSetup()
{
  WiFi.persistent(false);          // la config WiFi est dans /config.txt, pas dans la flash core
  WiFi.mode(WIFI_OFF);
  WiFi.hostname("deye-fan");
  WiFi.mode(WIFI_AP_STA);          // AP et STA en meme temps, des la premiere seconde

  /* Point d'acces d'urgence : toujours actif, on peut toujours retrouver le module */
  if (strlen(cfg.apPass) >= 8) WiFi.softAP(cfg.apSsid, cfg.apPass);
  else                         WiFi.softAP(cfg.apSsid);   // AP ouverte (cle "0")
  WiFi.setSleep(false);            // pas d'economie d'energie radio : horloge stable,
                                   // donc pas de gigue additionnelle sur les interruptions

  if (strlen(cfg.staSsid) > 0) {
    WiFi.setAutoReconnect(true);
    WiFi.begin(cfg.staSsid, strlen(cfg.staPass) ? cfg.staPass : nullptr);
  }
}

/*------------------------------------------------------------------------------------
 * 10. DEMARRAGE
 *----------------------------------------------------------------------------------*/
void setup()
{
  /* (1) Broches. Les sorties tach sont placees au repos (transistors bloques) AVANT
         leur activation en sortie : ainsi aucune etat parasite n'apparait sur la ligne
         tach de l'onduleur pendant le demarrage du module.                          */
  GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, BIT_OUT(0) | BIT_OUT(1));
  pinMode(PIN_TACH_OUT_9CM, OUTPUT);
  pinMode(PIN_TACH_OUT_6CM, OUTPUT);
  pinMode(PIN_TACH_IN_9CM,  INPUT_PULLUP);   // pull-up interne en secours de la R externe
  pinMode(PIN_TACH_IN_6CM,  INPUT_PULLUP);
  pinMode(PIN_STATUS_LED,   OUTPUT);
  ledWrite(0);

  Serial.begin(115200);
  delay(60);
  Serial.println();
  Serial.printf("=== %s | ESP8266 %u bits | flash %u o | heap %u o ===\n",
                FW_VERSION, (unsigned)ESP.getChipId(), (unsigned)ESP.getFlashChipSize(),
                (unsigned)ESP.getFreeHeap());
  Serial.printf("    ISR timer1 : %u Hz (%u us) | fenetre de mesure %u ms | entrees "
                "GPIO%u/GPIO%u | sorties GPIO%u/GPIO%u | LED GPIO%u\n",
                (unsigned)TICK_HZ, (unsigned)TICK_US, (unsigned)WINDOW_MS,
                (unsigned)PIN_TACH_IN_9CM, (unsigned)PIN_TACH_IN_6CM,
                (unsigned)PIN_TACH_OUT_9CM, (unsigned)PIN_TACH_OUT_6CM,
                (unsigned)PIN_STATUS_LED);

  /* (2) Configuration persistante */
  if (!LittleFS.begin(true)) {
    Serial.println("[ERR] LittleFS indisponible : reglages non persistants");
  } else if (loadConfig()) {
    Serial.printf("[CFG] configuration lue dans %s\n", CONFIG_FILE);
  } else {
    Serial.println("[CFG] premiere utilisation : ecriture des reglages d'usine");
    saveConfig();
  }

  /* (3) Alignement de l'etat temps reel sur le niveau reel des entrees : aucun front
         fantome n'est compte a la mise sous tension. Cette initialisation a lieu avant
         le demarrage du timer, donc sans concurrence avec l'ISR.                    */
  {
    const uint32_t inReg = GPIO_REG_READ(GPIO_IN_ADDRESS);
    for (uint8_t ch = 0; ch < NCHANNEL; ch++) {
      st[ch].stable     = (inReg & BIT_IN(ch)) ? 1 : 0;
      st[ch].run        = 0;
      st[ch].age        = 0;
      st[ch].ageRise    = 0;
      st[ch].pmax       = 0;
      st[ch].pWin       = 0;
      st[ch].psum       = 0;
      st[ch].phase      = 0;
      st[ch].inc        = 0;
      st[ch].pulseLeft  = 0;
      st[ch].pulseTicks = 0;
      st[ch].periodTicks= 0;
    }
  }
  applyConfig();
  startRealtimeTimer();      /* des ici, le tach reemis vers l'onduleur est vivant */

  /* (4) WiFi AP+STA puis interface web */
  wifiSetup();
  server.on("/",         HTTP_GET, handlePage);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/cfg",      HTTP_GET, handleCfg);
  server.on("/test",     HTTP_GET, handleTest);
  server.on("/wifi",     HTTP_GET, handleWifi);
  server.on("/factory",  HTTP_GET, handleFactory);
  server.onNotFound(handleNotFound);
  server.begin();
  if (MDNS.begin("deye-fan")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[NET] mDNS actif : http://deye-fan.local");
  }
  Serial.printf("[NET] AP \"%s\" -> http://%s\n", cfg.apSsid,
                WiFi.softAPIP().toString().c_str());
  if (strlen(cfg.staSsid)) Serial.printf("[NET] connexion STA vers \"%s\" en cours...\n", cfg.staSsid);
  Serial.println("[OK] emulation tach demarree");
}

/*------------------------------------------------------------------------------------
 * 11. BOUCLE PRINCIPALE : ne contient que du service reseau. Elle ne peut pas
 *     influencer la regularite du tach reemis (tout est dans l'ISR du timer1).
 *----------------------------------------------------------------------------------*/
void loop()
{
  /* redemarrage eventuel demande par une page de configuration */
  if (rebootAt && (int32_t)(millis() - rebootAt) >= 0) {
    rebootAt = 0;
    Serial.println("[SYS] redemarrage demande");
    delay(50);
    ESP.restart();
  }

  server.handleClient();
  MDNS.update();

  /* filet de securite : reconnexion station periodique */
  static uint32_t lastTry = 0;
  if (strlen(cfg.staSsid) && WiFi.status() != WL_CONNECTED &&
      (millis() - lastTry) > 20000UL) {
    lastTry = millis();
    Serial.println("[NET] deconnexion STA, nouvelle tentative");
    WiFi.begin(cfg.staSsid, strlen(cfg.staPass) ? cfg.staPass : nullptr);
  }

  /* trace serie periodique : informative, sans aucun effet possible sur le signal */
  static uint32_t lastLog = 0;
  if ((millis() - lastLog) >= 2000UL) {
    lastLog = millis();
    Serial.printf("[TACH] 90mm : %5u -> %5u tr/min | 60mm : %5u -> %5u tr/min | actif=%02u | heap=%u\n",
                  (unsigned)pubInRpm[0], (unsigned)pubSimRpm[0],
                  (unsigned)pubInRpm[1], (unsigned)pubSimRpm[1],
                  (unsigned)pubAlive, (unsigned)ESP.getFreeHeap());
  }
}
