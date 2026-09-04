/* =====================================================================================
 *  deye_fan.ino  --  Simulateur / multiplicateur de signal tachymetrique
 *  ------------------------------------------------------------------------------------
 *  Cible          : LOLIN(WEMOS) D1 mini  (ESP8266 / ESP-12S)  --  Arduino IDE
 *  Application    : Onduleur Deye SUN-8K-SG05LP1-EU-AM2-P dont les ventilateurs
 *                   d'origine NMB (06025VE-12N-CL 60mm et 09225VE-12N-CU 92mm) sont
 *                   remplaces par des Noctua NF-A6x25-FLX et NF-A9-FLX, plus lents.
 *
 *  Principe       : on lit le tachymetre d'un Noctua par canal, on multiplie la
 *                   frequence par un ratio configurable, et on regenere vers
 *                   l'onduleur un signal tachymetrique synthetique en collecteur
 *                   ouvert. L'onduleur croit donc voir un ventilateur rapide.
 *
 *  ------------------------------------------------------------------------------------
 *  ARCHITECTURE LOGICIELLE  --  isolation du traitement du signal vs WiFi
 *  ------------------------------------------------------------------------------------
 *  L'ESP8266 est mono-coeur et la pile WiFi du SDK Espressif s'execute dans des
 *  handlers d'interruption de niveau 1. Un timer Arduino classique
 *  (timer1_attachInterrupt), les interruptions GPIO (attachInterrupt) et le
 *  generateur de formes d'onde du core (tone/analogWrite/Servo) sont TOUS de
 *  niveau 1 : ils ne peuvent pas preempter le WiFi et se retrouvent retardes de
 *  plusieurs dizaines de microsecondes des que le trafic reseau est soutenu.
 *
 *  Pour obtenir une isolation reelle, ce firmware s'approprie le Timer1 et le
 *  raccorde au vecteur NMI (Non Maskable Interrupt, niveau 3) via
 *  ETS_FRC_TIMER1_NMI_INTR_ATTACH() -> NmiTimSetFunc(). Consequences :
 *
 *    - la NMI preempte le WiFi, le serveur web, le LwIP et la boucle principale ;
 *    - la NMI n'est pas masquee par noInterrupts()/xt_rsil(), donc meme les
 *      sections critiques du SDK ne la retardent pas ;
 *    - tout le code du moteur est en IRAM (IRAM_ATTR) et n'accede a aucune
 *      constante en flash : il continue donc de tourner pendant les acces flash
 *      (ecriture EEPROM, lecture SPIFFS, OTA) qui invalident le cache.
 *
 *  Contraintes que cela impose et qui sont respectees ici :
 *    - aucun appel a une fonction non-IRAM depuis la NMI ;
 *    - aucun flottant dans la NMI (pas de sauvegarde du contexte FP) ;
 *    - aucune allocation, aucun Serial, aucun appel SDK ;
 *    - acces GPIO par ecriture registre directe (GPI / GPOS / GPOC) ;
 *    - horodatage par le compteur de cycles CPU (rsr ccount), monotone a la
 *      frequence du CPU et totalement independant de tout ordonnanceur.
 *
 *  Le moteur est un ordonnanceur "earliest deadline" a un coup (Timer1 en mode
 *  TIM_SINGLE reprogramme a chaque passage) :
 *
 *    - echantillonnage des entrees tach a periode fixe (SAMPLE_INTERVAL_US),
 *      avec anti-rebond par comptage d'echantillons consecutifs ;
 *    - generation des fronts de sortie a des dates calculees en cycles CPU et
 *      accumulees (nextEdge += halfPeriod), donc sans derive cumulative ;
 *    - a chaque reveil on reprogramme le timer sur la plus proche echeance.
 *
 *  Debit d'interruptions : ~25 kHz (echantillonnage) + ~1 kHz (fronts de sortie),
 *  pour une ISR de l'ordre de 1,5 us => environ 4 % du CPU a 80 MHz.
 *
 *  La boucle principale (loop) ne fait que de la "paperasse" : elle lit des
 *  instantanes coherents produits par la NMI (protocole seqlock, car la NMI ne
 *  peut pas etre masquee), calcule les RPM en flottant, et publie vers la NMI de
 *  nouvelles demi-periodes par ecritures de mots de 32 bits alignes (atomiques
 *  sur Xtensa). Aucun verrou, aucune desactivation d'interruption.
 *
 *  ------------------------------------------------------------------------------------
 *  CABLAGE  --  voir SCHEMA.md pour le schema complet et la nomenclature
 *  ------------------------------------------------------------------------------------
 *    Canal 1 (92 mm) : entree D5 / GPIO14      sortie D1 / GPIO5
 *    Canal 2 (60 mm) : entree D6 / GPIO12      sortie D2 / GPIO4
 *    LED d'etat      : D4 / GPIO2 (LED integree, active a l'etat bas)
 *
 *    Entree  : tach Noctua (collecteur ouvert) -> 1 kOhm serie -> broche ESP,
 *              tiree au 3V3 par 10 kOhm, 47 nF vers GND (filtre ~3,4 kHz).
 *    Sortie  : broche ESP -> 2,2 kOhm -> base NPN (2N2222 / BC337 / S8050),
 *              emetteur a la masse commune, collecteur sur la broche tach du
 *              Deye. Montage collecteur ouvert : il fonctionne quelle que soit
 *              la tension de tirage interne du Deye (3,3 V, 5 V ou 12 V), ce qui
 *              repond a la contrainte "tension inconnue" sans adaptation.
 *              Resistance 10 kOhm base-emetteur pour garantir le transistor
 *              bloque pendant le reset et le boot de l'ESP.
 *
 *    Alimentation : les +12 V des deux connecteurs ventilateur sont combines par
 *              deux diodes Schottky (OU diode) puis abaisses a 5,0 V par un
 *              module buck MP1584EN / LM2596 vers la broche 5V du D1 mini. Le
 *              montage tolere que les deux connecteurs ne soient pas alimentes
 *              en meme temps ni a la meme tension : la source la plus haute
 *              alimente seule, sans courant de retour vers l'autre.
 *
 *  ------------------------------------------------------------------------------------
 *  Licence : MIT.  Fourni sans garantie -- vous intervenez sur un onduleur relie
 *  au reseau et a une batterie haute tension. Coupez et consignez tout avant
 *  d'ouvrir l'appareil.
 * ===================================================================================== */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>

extern "C" {
#include <ets_sys.h>
}

/* -------------------------------------------------------------------------------------
 *  Choix du vecteur d'interruption du moteur de signal
 *
 *  1 = Timer1 sur NMI (niveau 3). Preempte le WiFi : c'est le mode nominal et le
 *      seul qui garantisse l'isolation demandee.
 *  0 = Timer1 sur interruption de niveau 1 (repli de diagnostic). Fonctionne mais
 *      le signal subit alors la gigue du WiFi. Ne pas utiliser en exploitation.
 *
 *  Dans les deux cas le Timer1 est monopolise : il est donc interdit d'utiliser
 *  analogWrite(), tone() ou la bibliotheque Servo dans ce sketch.
 * ------------------------------------------------------------------------------------- */
#define ENGINE_USE_NMI 1

/* =====================================================================================
 *  1. AFFECTATION DES BROCHES
 * ===================================================================================== */

#define PIN_TACH_IN_A    14   /* D5  -- tach du Noctua NF-A9-FLX    (canal 1, 92 mm) */
#define PIN_TACH_IN_B    12   /* D6  -- tach du Noctua NF-A6x25-FLX (canal 2, 60 mm) */
#define PIN_TACH_OUT_A    5   /* D1  -- base du NPN vers le tach Deye 92 mm           */
#define PIN_TACH_OUT_B    4   /* D2  -- base du NPN vers le tach Deye 60 mm           */
#define PIN_LED           2   /* D4  -- LED integree, active a l'etat bas             */

/* GPIO16 est volontairement exclu : registres separes, pas de pull-up interne.
 * GPIO0 / GPIO15 sont exclus : broches de strap de demarrage.                        */

#define NUM_CHANNELS      2

/* =====================================================================================
 *  2. PARAMETRES DU MOTEUR DE SIGNAL
 * ===================================================================================== */

/* Periode d'echantillonnage des entrees tach. 40 us donne 25 kHz : tres au-dessus
 * des ~100 Hz max d'un tach (3000 tr/min x 2 impulsions / 60), et l'erreur residuelle
 * est encore divisee par le filtrage exponentiel en aval.                            */
#define SAMPLE_INTERVAL_US        40u

/* Nombre d'echantillons consecutifs identiques requis pour valider un changement
 * d'etat. 3 x 40 us = 120 us de rejet de glitch, tres loin des ~5 ms de largeur
 * d'impulsion utile. Le retard introduit est constant, il s'annule dans la mesure
 * de periode (difference de deux horodatages retardes de la meme quantite).          */
#define DEBOUNCE_SAMPLES          3u

/* Bornes de plausibilite d'une periode d'entree, en microsecondes.
 * 2 impulsions/tour => 200 tr/min  ->  150 ms ;  12000 tr/min  ->  2,5 ms.           */
#define IN_PERIOD_MIN_US          2000u
#define IN_PERIOD_MAX_US          200000u

/* Delai sans front au-dela duquel l'entree est declaree a l'arret.                   */
#define IN_TIMEOUT_MS             1200u

/* Filtre exponentiel sur la periode mesuree : alpha = 1 / 2^EMA_SHIFT.
 * 3 => alpha = 1/8, constante de temps ~8 periodes, soit ~100 ms a 1600 tr/min.      */
#define EMA_SHIFT                 3u

/* Bornes de la frequence de sortie, en centiemes de hertz (calcul entier).
 * 600 Hz = 18000 tr/min a 2 impulsions/tour : large marge au-dessus des
 * 9200 tr/min du NMB 60 mm d'origine.                                                */
#define OUT_FREQ_MIN_CHZ          50u      /* 0,50 Hz */
#define OUT_FREQ_MAX_CHZ          60000u   /* 600 Hz  */

/* Plancher de reprogrammation du Timer1, en ticks de 0,2 us. Evite toute rafale
 * d'interruptions si deux echeances tombent au meme instant.                         */
#define TIMER_MIN_TICKS           10u
#define TIMER_MAX_TICKS           0x7FFFFFu

/* Le Timer1 est cadence par l'APB (80 MHz fixe) divise par 16 => 5 MHz, soit
 * 0,2 us par tick, quelle que soit la frequence du CPU.                              */
#define TIMER_TICK_HZ             5000000u

/* =====================================================================================
 *  3. ETAT PARTAGE NMI <-> BOUCLE PRINCIPALE
 *
 *  Regles de coherence, sachant que la NMI ne peut PAS etre masquee :
 *
 *   - NMI -> loop  : structures multi-mots protegees par un seqlock (compteur
 *                    incremente avant et apres l'ecriture ; le lecteur relit
 *                    jusqu'a obtenir deux fois la meme valeur paire).
 *   - loop -> NMI  : uniquement des mots de 32 bits alignes, dont l'ecriture est
 *                    atomique sur Xtensa. La NMI les consomme aux frontieres de
 *                    front, ce qui rend les transitions continues.
 *
 *  Tous ces objets sont en DRAM (aucun const en flash) : la NMI reste utilisable
 *  meme quand le cache flash est desactive.
 * ===================================================================================== */

struct InputChannel {
  /* --- prive NMI --- */
  uint32_t pinMask;          /* 1 << gpio, pour lecture directe de GPI            */
  uint32_t lastEdgeCcy;      /* date du dernier front valide, en cycles CPU       */
  uint32_t minPeriodCcy;     /* bornes de plausibilite converties en cycles       */
  uint32_t maxPeriodCcy;
  uint8_t  debLevel;         /* niveau logique anti-rebonde courant               */
  uint8_t  debCount;         /* echantillons consecutifs contredisant debLevel    */
  uint8_t  primed;           /* un premier front a deja ete horodate              */

  /* --- publie vers la boucle principale, sous seqlock --- */
  volatile uint32_t seq;         /* impair = ecriture en cours                    */
  volatile uint32_t periodCcy;   /* periode filtree (EMA), en cycles CPU          */
  volatile uint32_t edgeCount;   /* fronts valides cumules, monotone              */
  volatile uint32_t rejectCount; /* fronts rejetes (hors bornes) -- diagnostic    */
};

struct OutputChannel {
  /* --- prive NMI --- */
  uint32_t pinMask;
  uint32_t nextEdgeCcy;      /* date du prochain basculement                      */
  uint32_t halfCcy;          /* demi-periode active, en cycles CPU                */
  uint8_t  running;          /* generation en cours                               */
  uint8_t  level;            /* niveau courant de la broche                       */

  /* --- commandes ecrites par la boucle principale (mots 32 bits atomiques) --- */
  volatile uint32_t cmdHalfCcy;  /* demi-periode demandee                         */
  volatile uint32_t cmdEnable;   /* 0 = arret (broche relachee), 1 = generation    */

  /* --- publie vers la boucle principale --- */
  volatile uint32_t pulseCount;  /* fronts montants emis, monotone -- diagnostic  */
};

static InputChannel  g_in[NUM_CHANNELS];
static OutputChannel g_out[NUM_CHANNELS];

static uint32_t g_cpuHz          = 80000000u;  /* frequence CPU reelle au boot     */
static uint32_t g_ccyPerTick     = 16u;        /* cycles CPU par tick Timer1       */
static uint32_t g_sampleIntervalCcy = 3200u;   /* SAMPLE_INTERVAL_US en cycles     */
static uint32_t g_nextSampleCcy  = 0;          /* prochaine echeance d'echantillon */
static volatile uint32_t g_nmiCount = 0;       /* compteur de reveils -- diagnostic*/

/* =====================================================================================
 *  4. PRIMITIVES BAS NIVEAU  (toutes utilisables depuis la NMI)
 * ===================================================================================== */

/* Compteur de cycles CPU : une seule instruction, aucune dependance. */
static inline uint32_t IRAM_ATTR ccyNow() {
  uint32_t c;
  __asm__ __volatile__("rsr %0,ccount" : "=a"(c));
  return c;
}

/* Comparaison de dates insensible au debordement du compteur 32 bits
 * (53,7 s a 80 MHz) : on teste le signe de la difference. */
static inline bool IRAM_ATTR ccyReached(uint32_t now, uint32_t deadline) {
  return (int32_t)(now - deadline) >= 0;
}

static inline void IRAM_ATTR gpioSet(uint32_t mask)   { GPOS = mask; }
static inline void IRAM_ATTR gpioClear(uint32_t mask) { GPOC = mask; }
static inline uint32_t IRAM_ATTR gpioRead(uint32_t mask) { return GPI & mask; }

/* =====================================================================================
 *  5. MOTEUR DE SIGNAL  (contexte NMI)
 * ===================================================================================== */

/* --- 5.1 Echantillonnage et mesure d'une entree tach ------------------------------- */
static inline void IRAM_ATTR sampleInput(InputChannel *ic, uint32_t now) {
  const uint8_t raw = gpioRead(ic->pinMask) ? 1 : 0;

  if (raw == ic->debLevel) {
    ic->debCount = 0;
    return;
  }
  if (++ic->debCount < DEBOUNCE_SAMPLES) {
    return;                                  /* changement pas encore confirme */
  }

  /* Transition confirmee. */
  ic->debCount = 0;
  ic->debLevel = raw;

  /* Le tach Noctua est un collecteur ouvert tire au 3V3 : l'impulsion est un
   * etat bas. On horodate le front descendant, un seul par demi-tour, ce qui
   * donne 2 fronts par tour. */
  if (raw != 0) {
    return;
  }

  if (ic->primed) {
    const uint32_t period = now - ic->lastEdgeCcy;
    if (period >= ic->minPeriodCcy && period <= ic->maxPeriodCcy) {
      /* Publication sous seqlock : la boucle principale lit periodCcy et
       * edgeCount ensemble et doit les voir coherents. */
      ic->seq++;
      __asm__ __volatile__("" ::: "memory");

      if (ic->periodCcy == 0) {
        ic->periodCcy = period;              /* amorcage du filtre */
      } else {
        /* EMA entiere : p += (x - p) / 2^EMA_SHIFT, sans flottant. */
        const uint32_t p = ic->periodCcy;
        ic->periodCcy = p - (p >> EMA_SHIFT) + (period >> EMA_SHIFT);
      }
      ic->edgeCount++;

      __asm__ __volatile__("" ::: "memory");
      ic->seq++;
    } else {
      /* Meme protocole seqlock : la boucle principale doit voir periodCcy = 0
       * et le compteur de rejets dans le meme instantane. */
      ic->seq++;
      __asm__ __volatile__("" ::: "memory");
      ic->rejectCount++;
      ic->periodCcy = 0;                     /* rupture de cadence : on reamorce */
      __asm__ __volatile__("" ::: "memory");
      ic->seq++;
    }
  }

  ic->lastEdgeCcy = now;
  ic->primed = 1;
}

/* --- 5.2 Generation d'une sortie tach ---------------------------------------------- */
static inline void IRAM_ATTR serviceOutput(OutputChannel *oc, uint32_t now) {
  /* Prise en compte des commandes de la boucle principale. */
  if (!oc->cmdEnable) {
    if (oc->running) {
      oc->running = 0;
      oc->level   = 0;
      gpioClear(oc->pinMask);                /* NPN bloque = ligne tach relachee */
    }
    return;
  }

  if (!oc->running) {
    const uint32_t h = oc->cmdHalfCcy;
    if (h == 0) {
      return;
    }
    oc->halfCcy     = h;
    oc->running     = 1;
    oc->level       = 1;
    oc->nextEdgeCcy = now + h;
    gpioSet(oc->pinMask);
    return;
  }

  /* Fronts dus. La boucle "while" absorbe un eventuel retard (par exemple apres
   * un changement brutal de demi-periode) sans jamais decaler la phase. */
  uint32_t iter = 0;
  while (ccyReached(now, oc->nextEdgeCcy)) {
    if (oc->level) {
      oc->level = 0;
      gpioClear(oc->pinMask);
    } else {
      oc->level = 1;
      gpioSet(oc->pinMask);
      oc->pulseCount++;
    }

    /* Une nouvelle demi-periode n'est adoptee qu'a une frontiere de front :
     * la transition de frequence est donc continue, sans impulsion tronquee. */
    const uint32_t h = oc->cmdHalfCcy;
    if (h != 0 && h != oc->halfCcy) {
      oc->halfCcy = h;
    }

    /* Accumulation, et non "now + halfCcy" : aucune derive cumulative meme si
     * la NMI est servie quelques cycles apres la date theorique. */
    oc->nextEdgeCcy += oc->halfCcy;

    /* Garde-fou : on ne rattrape jamais plus de 16 fronts en une passe. Au-dela
     * (demi-periode raccourcie d'un facteur enorme, ou date de reference
     * devenue absurde) on resynchronise sur l'instant courant plutot que de
     * prolonger l'ISR. */
    if (++iter >= 16u) {
      oc->nextEdgeCcy = now + oc->halfCcy;
      break;
    }
  }
}

/* --- 5.3 Handler NMI : ordonnanceur "plus proche echeance" ------------------------- */
static void IRAM_ATTR fanEngineIsr() {
#if ENGINE_USE_NMI
  /* En mode NMI nous sommes le handler brut : il faut acquitter le Timer1
   * nous-memes, exactement comme le fait le handler du core.
   * TIM_SINGLE + TIM_EDGE => TCAR = 0 et TCIT = 0. */
  if ((T1C & ((1 << TCAR) | (1 << TCIT))) == 0) {
    TEIE &= ~TEIE1;
  }
  T1I = 0;
#endif

  g_nmiCount++;

  uint32_t now = ccyNow();

  /* (a) Echantillonnage des entrees a cadence fixe. */
  if (ccyReached(now, g_nextSampleCcy)) {
    for (uint32_t i = 0; i < NUM_CHANNELS; i++) {
      sampleInput(&g_in[i], now);
    }
    g_nextSampleCcy += g_sampleIntervalCcy;
    /* Si nous avons pris trop de retard (cas pathologique), on se recale plutot
     * que d'accumuler des echantillons en rafale. */
    if (ccyReached(now, g_nextSampleCcy)) {
      g_nextSampleCcy = now + g_sampleIntervalCcy;
    }
  }

  /* (b) Fronts de sortie. */
  for (uint32_t i = 0; i < NUM_CHANNELS; i++) {
    serviceOutput(&g_out[i], now);
  }

  /* (c) Plus proche echeance parmi l'echantillonnage et les fronts en cours. */
  uint32_t next = g_nextSampleCcy;
  for (uint32_t i = 0; i < NUM_CHANNELS; i++) {
    if (g_out[i].running && (int32_t)(g_out[i].nextEdgeCcy - next) < 0) {
      next = g_out[i].nextEdgeCcy;
    }
  }

  now = ccyNow();
  int32_t deltaCcy = (int32_t)(next - now);
  if (deltaCcy < (int32_t)(TIMER_MIN_TICKS * g_ccyPerTick)) {
    deltaCcy = (int32_t)(TIMER_MIN_TICKS * g_ccyPerTick);
  }

  uint32_t ticks = (uint32_t)deltaCcy / g_ccyPerTick;
  if (ticks < TIMER_MIN_TICKS) ticks = TIMER_MIN_TICKS;
  if (ticks > TIMER_MAX_TICKS) ticks = TIMER_MAX_TICKS;

  timer1_write(ticks);   /* IRAM_ATTR dans le core : appel legal depuis la NMI */
}

/* --- 5.4 Demarrage du moteur ------------------------------------------------------- */
static void engineBegin() {
  g_cpuHz      = (uint32_t)ESP.getCpuFreqMHz() * 1000000u;
  g_ccyPerTick = g_cpuHz / TIMER_TICK_HZ;              /* 16 a 80 MHz, 32 a 160 MHz */
  if (g_ccyPerTick == 0) g_ccyPerTick = 1;
  g_sampleIntervalCcy = (g_cpuHz / 1000000u) * SAMPLE_INTERVAL_US;

  const uint8_t  inPins[NUM_CHANNELS]  = { PIN_TACH_IN_A,  PIN_TACH_IN_B  };
  const uint8_t  outPins[NUM_CHANNELS] = { PIN_TACH_OUT_A, PIN_TACH_OUT_B };
  const uint32_t cyclesPerUs = g_cpuHz / 1000000u;

  for (uint32_t i = 0; i < NUM_CHANNELS; i++) {
    /* Entrees : pull-up interne EN PLUS du pull-up externe de 10 kOhm. Le
     * montage fonctionne meme si la resistance externe n'est pas posee, mais
     * l'immunite au bruit est bien meilleure avec le reseau RC decrit dans
     * SCHEMA.md. */
    pinMode(inPins[i], INPUT_PULLUP);

    g_in[i].pinMask      = 1u << inPins[i];
    g_in[i].minPeriodCcy = IN_PERIOD_MIN_US * cyclesPerUs;
    g_in[i].maxPeriodCcy = IN_PERIOD_MAX_US * cyclesPerUs;
    g_in[i].debLevel     = gpioRead(g_in[i].pinMask) ? 1 : 0;
    g_in[i].debCount     = 0;
    g_in[i].primed       = 0;
    g_in[i].seq          = 0;
    g_in[i].periodCcy    = 0;
    g_in[i].edgeCount    = 0;
    g_in[i].rejectCount  = 0;

    /* Sorties : etat bas avant activation de la broche, pour que le NPN reste
     * bloque et que la ligne tach du Deye ne soit jamais tiree pendant le boot. */
    digitalWrite(outPins[i], LOW);
    pinMode(outPins[i], OUTPUT);
    digitalWrite(outPins[i], LOW);

    g_out[i].pinMask     = 1u << outPins[i];
    g_out[i].nextEdgeCcy = 0;
    g_out[i].halfCcy     = 0;
    g_out[i].running     = 0;
    g_out[i].level       = 0;
    g_out[i].cmdHalfCcy  = 0;
    g_out[i].cmdEnable   = 0;
    g_out[i].pulseCount  = 0;
  }

  g_nextSampleCcy = ccyNow() + g_sampleIntervalCcy;

#if ENGINE_USE_NMI
  /* Raccordement du Timer1 au vecteur NMI. C'est le point cle de l'isolation :
   * NmiTimSetFunc() deroute l'interruption FRC1 vers la NMI de niveau 3, qui
   * preempte la pile WiFi (niveau 1) et ignore xt_rsil(). */
  ETS_FRC_TIMER1_NMI_INTR_ATTACH(fanEngineIsr);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
  ETS_FRC1_INTR_ENABLE();
#else
  timer1_isr_init();
  timer1_attachInterrupt(fanEngineIsr);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
#endif

  timer1_write(g_sampleIntervalCcy / g_ccyPerTick);
}

/* --- 5.5 Lecture d'un instantane coherent depuis la boucle principale -------------- */
struct InputSnapshot {
  uint32_t periodCcy;
  uint32_t edgeCount;
  uint32_t rejectCount;
};

static InputSnapshot engineReadInput(uint32_t ch) {
  InputChannel *ic = &g_in[ch];
  InputSnapshot s;
  uint32_t s1, s2;
  uint32_t guard = 0;

  /* Seqlock : la NMI etant non masquable, on relit jusqu'a obtenir un compteur
   * pair et inchange, preuve qu'aucune ecriture n'a chevauche la lecture. */
  do {
    s1 = ic->seq;
    __asm__ __volatile__("" ::: "memory");
    s.periodCcy   = ic->periodCcy;
    s.edgeCount   = ic->edgeCount;
    s.rejectCount = ic->rejectCount;
    __asm__ __volatile__("" ::: "memory");
    s2 = ic->seq;
  } while (((s1 & 1u) || s1 != s2) && ++guard < 64u);

  return s;
}

/* --- 5.6 Commande d'une sortie depuis la boucle principale ------------------------- */
static void engineSetOutput(uint32_t ch, bool enable, uint32_t halfCcy) {
  /* Ecritures de mots 32 bits alignes : atomiques sur Xtensa, donc aucun besoin
   * de masquer les interruptions (ce qui serait de toute facon sans effet sur
   * une NMI). L'ordre importe : la demi-periode doit etre valide avant que la
   * NMI ne soit autorisee a demarrer la generation. */
  if (enable) {
    g_out[ch].cmdHalfCcy = halfCcy;
    __asm__ __volatile__("" ::: "memory");
    g_out[ch].cmdEnable  = 1;
  } else {
    g_out[ch].cmdEnable  = 0;
  }
}

/* =====================================================================================
 *  6. CONFIGURATION PERSISTANTE  (EEPROM emulee en flash)
 * ===================================================================================== */

#define CFG_MAGIC     0x44594E31u    /* "DYN1" */
#define CFG_VERSION   2
#define EEPROM_SIZE   512

/* Ratios par defaut.
 *
 *   Canal 1 -- 92 mm : NMB 09225VE-12N-CU d'origine, la variante "Q" du meme
 *      corps est donnee a 5600 tr/min ; la variante "N" tourne moins vite,
 *      typiquement 4000 a 4500 tr/min. Le Noctua NF-A9-FLX plafonne a
 *      1600 tr/min => ratio 2,80 pour afficher ~4480 tr/min.
 *
 *   Canal 2 -- 60 mm : NMB 06025VE-12N-CL d'origine, la variante "Q" est donnee
 *      a 9200 tr/min ; la variante "N" tourne typiquement 7000 a 7500 tr/min.
 *      Le Noctua NF-A6x25-FLX plafonne a 3000 tr/min => ratio 2,50 pour
 *      afficher ~7500 tr/min.
 *
 * Ces valeurs sont des points de depart. La bonne methode est de relever les
 * RPM reels des ventilateurs d'origine (l'interface web affiche les RPM lus,
 * il suffit de brancher un NMB sur une entree avant de le remplacer), puis
 * d'ajuster le ratio dans l'interface web -- sans recompiler ni recabler.       */
#define DEFAULT_RATIO_A_MILLI  2800u
#define DEFAULT_RATIO_B_MILLI  2500u

#define RATIO_MIN_MILLI         100u    /* 0,10 */
#define RATIO_MAX_MILLI       10000u    /* 10,00 */

struct Config {
  uint32_t magic;
  uint16_t version;
  uint16_t ratioMilli[NUM_CHANNELS];   /* ratio x 1000                          */
  uint8_t  pulsesInPerRev[NUM_CHANNELS];   /* impulsions/tour cote Noctua (2)   */
  uint8_t  pulsesOutPerRev[NUM_CHANNELS];  /* impulsions/tour cote Deye (2)     */
  uint16_t maxSimRpm[NUM_CHANNELS];    /* ecretage de securite                  */
  char     apSsid[33];
  char     apPass[65];
  char     staSsid[33];
  char     staPass[65];
  uint8_t  staEnabled;
  uint8_t  reserved[3];
  uint32_t crc;
};

static Config g_cfg;

static uint32_t crc32(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  while (len--) {
    crc ^= *data++;
    for (uint8_t k = 0; k < 8; k++) {
      crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1u)));
    }
  }
  return ~crc;
}

static void configDefaults() {
  memset(&g_cfg, 0, sizeof(g_cfg));
  g_cfg.magic   = CFG_MAGIC;
  g_cfg.version = CFG_VERSION;

  g_cfg.ratioMilli[0] = DEFAULT_RATIO_A_MILLI;
  g_cfg.ratioMilli[1] = DEFAULT_RATIO_B_MILLI;

  for (uint32_t i = 0; i < NUM_CHANNELS; i++) {
    g_cfg.pulsesInPerRev[i]  = 2;   /* standard pour tous les Noctua 3 et 4 fils */
    g_cfg.pulsesOutPerRev[i] = 2;   /* standard attendu par l'onduleur           */
    g_cfg.maxSimRpm[i]       = 12000;
  }

  /* Le SSID de l'AP integre un suffixe unique tire de l'ID de la puce, pour
   * eviter toute collision si plusieurs modules coexistent. */
  snprintf(g_cfg.apSsid, sizeof(g_cfg.apSsid), "DeyeFan-%06X", ESP.getChipId());
  strlcpy(g_cfg.apPass, "deyefan123", sizeof(g_cfg.apPass));  /* >= 8 caracteres */
  g_cfg.staSsid[0] = '\0';
  g_cfg.staPass[0] = '\0';
  g_cfg.staEnabled = 0;
}

static void configSave() {
  g_cfg.magic   = CFG_MAGIC;
  g_cfg.version = CFG_VERSION;
  g_cfg.crc     = crc32((const uint8_t *)&g_cfg, sizeof(Config) - sizeof(uint32_t));

  /* EEPROM.commit() efface puis reecrit un secteur de flash : le cache
   * d'instructions est desactive pendant plusieurs dizaines de millisecondes.
   * Le moteur de signal continue de tourner car tout son code est en IRAM et
   * ses donnees en DRAM -- c'est precisement l'une des raisons pour lesquelles
   * l'ISR n'accede a aucune constante en flash. */
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(0, g_cfg);
  EEPROM.commit();
  EEPROM.end();
}

static void configLoad() {
  EEPROM.begin(EEPROM_SIZE);
  Config tmp;
  EEPROM.get(0, tmp);
  EEPROM.end();

  const uint32_t crc = crc32((const uint8_t *)&tmp, sizeof(Config) - sizeof(uint32_t));
  if (tmp.magic == CFG_MAGIC && tmp.version == CFG_VERSION && tmp.crc == crc) {
    g_cfg = tmp;
    Serial.println(F("[cfg] configuration chargee depuis l'EEPROM"));
  } else {
    configDefaults();
    configSave();
    Serial.println(F("[cfg] EEPROM vide ou invalide -> valeurs par defaut"));
  }

  /* Bornage defensif : une EEPROM corrompue ne doit pas produire un signal absurde. */
  for (uint32_t i = 0; i < NUM_CHANNELS; i++) {
    if (g_cfg.ratioMilli[i] < RATIO_MIN_MILLI) g_cfg.ratioMilli[i] = RATIO_MIN_MILLI;
    if (g_cfg.ratioMilli[i] > RATIO_MAX_MILLI) g_cfg.ratioMilli[i] = RATIO_MAX_MILLI;
    if (g_cfg.pulsesInPerRev[i]  < 1 || g_cfg.pulsesInPerRev[i]  > 8) g_cfg.pulsesInPerRev[i]  = 2;
    if (g_cfg.pulsesOutPerRev[i] < 1 || g_cfg.pulsesOutPerRev[i] > 8) g_cfg.pulsesOutPerRev[i] = 2;
    if (g_cfg.maxSimRpm[i] < 500 || g_cfg.maxSimRpm[i] > 30000)       g_cfg.maxSimRpm[i]       = 12000;
  }
  if (g_cfg.apSsid[0] == '\0') {
    snprintf(g_cfg.apSsid, sizeof(g_cfg.apSsid), "DeyeFan-%06X", ESP.getChipId());
  }
  if (strlen(g_cfg.apPass) < 8) {
    strlcpy(g_cfg.apPass, "deyefan123", sizeof(g_cfg.apPass));
  }
}

/* =====================================================================================
 *  7. COUCHE APPLICATIVE  (boucle principale)
 * ===================================================================================== */

struct ChannelState {
  float    inRpm;          /* RPM mesures sur le Noctua                         */
  float    simRpm;         /* RPM presentes a l'onduleur                        */
  float    outFreqHz;      /* frequence du signal genere                        */
  bool     active;         /* signal en cours de generation                     */
  bool     clamped;        /* ecretage par maxSimRpm actif                      */
  uint32_t lastEdgeCount;  /* pour la detection d'activite                      */
  uint32_t lastEdgeMs;
  uint32_t rejectCount;
};

static ChannelState g_st[NUM_CHANNELS];

static const char *CH_NAME[NUM_CHANNELS] = { "92mm (NF-A9-FLX)", "60mm (NF-A6x25-FLX)" };

/* Recalcule l'etat d'un canal et reprogramme la sortie si necessaire. */
static void updateChannel(uint32_t ch) {
  ChannelState *st = &g_st[ch];
  const InputSnapshot snap = engineReadInput(ch);
  const uint32_t nowMs = millis();

  st->rejectCount = snap.rejectCount;

  if (snap.edgeCount != st->lastEdgeCount) {
    st->lastEdgeCount = snap.edgeCount;
    st->lastEdgeMs    = nowMs;
  }

  const bool signalPresent = (snap.periodCcy != 0) &&
                             ((uint32_t)(nowMs - st->lastEdgeMs) < IN_TIMEOUT_MS);

  if (!signalPresent) {
    st->inRpm     = 0.0f;
    st->simRpm    = 0.0f;
    st->outFreqHz = 0.0f;
    st->active    = false;
    st->clamped   = false;
    engineSetOutput(ch, false, 0);
    return;
  }

  /* Frequence d'entree, puis RPM. Le flottant est ici sans danger : nous sommes
   * dans la boucle principale, pas dans la NMI. */
  const float inFreqHz = (float)g_cpuHz / (float)snap.periodCcy;
  st->inRpm = inFreqHz * 60.0f / (float)g_cfg.pulsesInPerRev[ch];

  float sim = st->inRpm * ((float)g_cfg.ratioMilli[ch] / 1000.0f);
  st->clamped = false;
  if (sim > (float)g_cfg.maxSimRpm[ch]) {
    sim = (float)g_cfg.maxSimRpm[ch];
    st->clamped = true;
  }
  st->simRpm = sim;

  float outFreq = sim * (float)g_cfg.pulsesOutPerRev[ch] / 60.0f;
  const float fMin = (float)OUT_FREQ_MIN_CHZ / 100.0f;
  const float fMax = (float)OUT_FREQ_MAX_CHZ / 100.0f;
  if (outFreq < fMin) outFreq = fMin;
  if (outFreq > fMax) { outFreq = fMax; st->clamped = true; }
  st->outFreqHz = outFreq;

  const uint32_t halfCcy = (uint32_t)((float)g_cpuHz / (2.0f * outFreq));

  /* Bande morte de 0,2 % : inutile de republier une demi-periode quasi
   * identique a chaque tour de boucle. */
  const uint32_t cur = g_out[ch].cmdHalfCcy;
  const uint32_t diff = (halfCcy > cur) ? (halfCcy - cur) : (cur - halfCcy);
  if (!g_out[ch].cmdEnable || cur == 0 || diff > (cur / 500u)) {
    engineSetOutput(ch, true, halfCcy);
  }

  st->active = true;
}

/* LED d'etat, sur la LED integree (active a l'etat bas) :
 *    fixe allumee        -> les deux canaux simulent activement
 *    clignotement rapide -> un seul canal simule (l'autre attend un signal)
 *    clignotement lent   -> aucun signal d'entree, le systeme attend           */
static void updateLed() {
  uint32_t activeCount = 0;
  for (uint32_t i = 0; i < NUM_CHANNELS; i++) {
    if (g_st[i].active) activeCount++;
  }

  bool on;
  if (activeCount == NUM_CHANNELS) {
    on = true;
  } else if (activeCount > 0) {
    on = (millis() % 200u) < 100u;          /* 5 Hz */
  } else {
    on = (millis() % 1000u) < 100u;         /* 1 Hz, breve impulsion */
  }
  digitalWrite(PIN_LED, on ? LOW : HIGH);
}

/* =====================================================================================
 *  8. INTERFACE WEB
 * ===================================================================================== */

static ESP8266WebServer g_server(80);
static uint32_t g_rebootAtMs = 0;

static const char PAGE_INDEX[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Deye Fan Tach Simulator</title>
<style>
:root{--bg:#12151a;--card:#1c2028;--line:#2c313b;--fg:#e6e9ef;--mut:#8b93a3;--ok:#3ddc84;--warn:#ffb020;--bad:#ff5f56;--acc:#4da3ff}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:14px/1.5 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
.wrap{max-width:860px;margin:0 auto;padding:18px}
h1{font-size:19px;margin:0 0 4px}
.sub{color:var(--mut);font-size:12px;margin-bottom:18px}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:16px;margin-bottom:14px}
.card h2{font-size:14px;margin:0 0 12px;text-transform:uppercase;letter-spacing:.06em;color:var(--mut)}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(230px,1fr));gap:14px}
.metric{display:flex;justify-content:space-between;align-items:baseline;padding:6px 0;border-bottom:1px dashed var(--line)}
.metric:last-child{border-bottom:0}
.metric span{color:var(--mut);font-size:12px}
.metric b{font-size:19px;font-variant-numeric:tabular-nums}
.pill{display:inline-block;padding:2px 9px;border-radius:99px;font-size:11px;font-weight:600}
.pill.on{background:rgba(61,220,132,.15);color:var(--ok)}
.pill.off{background:rgba(255,176,32,.15);color:var(--warn)}
label{display:block;font-size:12px;color:var(--mut);margin:10px 0 4px}
input,select{width:100%;padding:8px 10px;background:#0e1116;border:1px solid var(--line);border-radius:6px;color:var(--fg);font:inherit}
button{margin-top:14px;padding:9px 16px;border:0;border-radius:6px;background:var(--acc);color:#06101c;font:inherit;font-weight:600;cursor:pointer}
button.sec{background:#333a46;color:var(--fg)}
.row{display:flex;gap:10px;flex-wrap:wrap}
.msg{margin-top:10px;font-size:12px;min-height:16px}
.msg.ok{color:var(--ok)}.msg.err{color:var(--bad)}
table{width:100%;border-collapse:collapse;font-size:12px}
td{padding:4px 0;color:var(--mut)}td+td{text-align:right;color:var(--fg);font-variant-numeric:tabular-nums}
</style></head><body><div class="wrap">
<h1>Deye Fan Tach Simulator</h1>
<div class="sub">Multiplicateur de signal tachymetrique &mdash; Wemos D1 mini &mdash; moteur de signal sur Timer1/NMI</div>

<div class="grid" id="chans"></div>

<div class="card"><h2>Ratios et conversion</h2>
<form id="fcfg"><div class="grid" id="cfgch"></div>
<button type="submit">Enregistrer</button>
<div class="msg" id="mcfg"></div></form></div>

<div class="card"><h2>Reseau WiFi</h2>
<form id="fwifi"><div class="grid">
<div>
<label>SSID du point d'acces (AP)</label><input name="apSsid" maxlength="32" required>
<label>Mot de passe AP (8 caracteres minimum)</label><input name="apPass" maxlength="64" minlength="8" required>
</div><div>
<label>SSID du reseau a rejoindre (STA)</label><input name="staSsid" maxlength="32">
<label>Mot de passe STA (laisser vide pour ne pas changer)</label><input name="staPass" type="password" maxlength="64">
<label>Mode station</label><select name="staEnabled"><option value="1">Active</option><option value="0">Desactive</option></select>
</div></div>
<div class="row"><button type="submit">Enregistrer</button>
<button type="button" class="sec" id="brb">Redemarrer</button></div>
<div class="msg" id="mwifi"></div></form></div>

<div class="card"><h2>Systeme</h2><table id="sys"></table></div>
</div><script>
const $=s=>document.querySelector(s);
let cfgBuilt=false,wifiBuilt=false;
const f1=(v,d=0)=>Number(v).toFixed(d);

function buildCfg(d){
  $('#cfgch').innerHTML=d.channels.map((c,i)=>`<div>
    <div style="font-weight:600;margin-bottom:6px">Canal ${i+1} &mdash; ${c.name}</div>
    <label>Ratio multiplicateur (0,10 &ndash; 10,00)</label>
    <input name="ratio${i}" type="number" step="0.01" min="0.1" max="10" value="${(c.ratio).toFixed(2)}" required>
    <label>Impulsions par tour &mdash; entree Noctua</label>
    <input name="ppin${i}" type="number" step="1" min="1" max="8" value="${c.ppIn}" required>
    <label>Impulsions par tour &mdash; sortie Deye</label>
    <input name="ppout${i}" type="number" step="1" min="1" max="8" value="${c.ppOut}" required>
    <label>Ecretage de securite (RPM simules max)</label>
    <input name="maxrpm${i}" type="number" step="100" min="500" max="30000" value="${c.maxSimRpm}" required>
  </div>`).join('');
  cfgBuilt=true;
}

function render(d){
  if(!cfgBuilt)buildCfg(d);
  if(!wifiBuilt){const f=$('#fwifi');f.apSsid.value=d.wifi.apSsid;f.apPass.value=d.wifi.apPass;
    f.staSsid.value=d.wifi.staSsid;f.staEnabled.value=d.wifi.staEnabled?'1':'0';wifiBuilt=true;}
  $('#chans').innerHTML=d.channels.map((c,i)=>`<div class="card">
    <h2>Canal ${i+1} &mdash; ${c.name}</h2>
    <div style="margin-bottom:8px"><span class="pill ${c.active?'on':'off'}">${c.active?'SIMULATION ACTIVE':'ATTENTE DE SIGNAL'}</span>
    ${c.clamped?' <span class="pill off">ECRETE</span>':''}</div>
    <div class="metric"><span>RPM lus (Noctua)</span><b>${f1(c.inRpm)}</b></div>
    <div class="metric"><span>RPM simules (vus par le Deye)</span><b>${f1(c.simRpm)}</b></div>
    <div class="metric"><span>Ratio applique</span><b>${c.ratio.toFixed(2)}&times;</b></div>
    <div class="metric"><span>Frequence de sortie</span><b>${f1(c.outHz,2)} Hz</b></div>
    <div class="metric"><span>Fronts lus / impulsions emises</span><b>${c.edges} / ${c.pulses}</b></div>
    <div class="metric"><span>Fronts rejetes (bruit)</span><b>${c.rejects}</b></div>
  </div>`).join('');
  $('#sys').innerHTML=`
    <tr><td>Moteur de signal</td><td>${d.sys.engine}</td></tr>
    <tr><td>Frequence CPU</td><td>${d.sys.cpuMHz} MHz</td></tr>
    <tr><td>Reveils NMI / s</td><td>${d.sys.nmiRate}</td></tr>
    <tr><td>Point d'acces</td><td>${d.wifi.apSsid} &mdash; ${d.wifi.apIp} (${d.wifi.apClients} client(s))</td></tr>
    <tr><td>Station</td><td>${d.wifi.staEnabled?(d.wifi.staConnected?d.wifi.staSsid+' &mdash; '+d.wifi.staIp:'connexion...'):'desactivee'}</td></tr>
    <tr><td>Nom mDNS</td><td>${d.sys.host}.local</td></tr>
    <tr><td>Duree de fonctionnement</td><td>${d.sys.uptime}</td></tr>
    <tr><td>Memoire libre</td><td>${d.sys.heap} octets</td></tr>`;
}

async function poll(){
  try{render(await(await fetch('/api/status')).json());}catch(e){}
  setTimeout(poll,600);
}

async function post(url,form,box){
  const m=$(box);m.className='msg';m.textContent='Envoi...';
  try{
    const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:new URLSearchParams(new FormData(form))});
    const t=await r.text();
    m.className='msg '+(r.ok?'ok':'err');m.textContent=t;
  }catch(e){m.className='msg err';m.textContent='Erreur reseau';}
}

$('#fcfg').addEventListener('submit',e=>{e.preventDefault();post('/api/config',e.target,'#mcfg');});
$('#fwifi').addEventListener('submit',e=>{e.preventDefault();post('/api/wifi',e.target,'#mwifi');});
$('#brb').addEventListener('click',async()=>{await fetch('/api/reboot',{method:'POST'});
  $('#mwifi').className='msg ok';$('#mwifi').textContent='Redemarrage en cours...';});
poll();
</script></body></html>)HTML";

static String uptimeText() {
  uint32_t s = millis() / 1000u;
  char buf[32];
  snprintf(buf, sizeof(buf), "%lud %02lu:%02lu:%02lu",
           (unsigned long)(s / 86400u), (unsigned long)((s / 3600u) % 24u),
           (unsigned long)((s / 60u) % 60u), (unsigned long)(s % 60u));
  return String(buf);
}

/* Cadence de reveil de la NMI, mesuree sur la derniere seconde. Utile pour
 * verifier d'un coup d'oeil que le moteur tourne bien. */
static uint32_t g_nmiRate = 0;

static void handleStatus() {
  String j;
  j.reserve(1400);
  j = F("{\"channels\":[");
  for (uint32_t i = 0; i < NUM_CHANNELS; i++) {
    if (i) j += ',';
    char buf[420];
    snprintf(buf, sizeof(buf),
      "{\"name\":\"%s\",\"inRpm\":%.1f,\"simRpm\":%.1f,\"ratio\":%.3f,"
      "\"outHz\":%.2f,\"active\":%s,\"clamped\":%s,\"ppIn\":%u,\"ppOut\":%u,"
      "\"maxSimRpm\":%u,\"edges\":%lu,\"pulses\":%lu,\"rejects\":%lu}",
      CH_NAME[i], g_st[i].inRpm, g_st[i].simRpm,
      (double)g_cfg.ratioMilli[i] / 1000.0, g_st[i].outFreqHz,
      g_st[i].active ? "true" : "false", g_st[i].clamped ? "true" : "false",
      (unsigned)g_cfg.pulsesInPerRev[i], (unsigned)g_cfg.pulsesOutPerRev[i],
      (unsigned)g_cfg.maxSimRpm[i], (unsigned long)g_st[i].lastEdgeCount,
      (unsigned long)g_out[i].pulseCount, (unsigned long)g_st[i].rejectCount);
    j += buf;
  }
  j += F("],\"wifi\":{");
  j += F("\"apSsid\":\""); j += g_cfg.apSsid;
  j += F("\",\"apPass\":\""); j += g_cfg.apPass;
  j += F("\",\"apIp\":\""); j += WiFi.softAPIP().toString();
  j += F("\",\"apClients\":"); j += WiFi.softAPgetStationNum();
  j += F(",\"staSsid\":\""); j += g_cfg.staSsid;
  j += F("\",\"staEnabled\":"); j += g_cfg.staEnabled ? F("true") : F("false");
  j += F(",\"staConnected\":"); j += (WiFi.status() == WL_CONNECTED) ? F("true") : F("false");
  j += F(",\"staIp\":\""); j += WiFi.localIP().toString();
  j += F("\"},\"sys\":{\"engine\":\"");
  j += ENGINE_USE_NMI ? F("Timer1 / NMI (niveau 3)") : F("Timer1 / niveau 1");
  j += F("\",\"cpuMHz\":"); j += ESP.getCpuFreqMHz();
  j += F(",\"nmiRate\":"); j += g_nmiRate;
  j += F(",\"host\":\"deye-fan\",\"uptime\":\""); j += uptimeText();
  j += F("\",\"heap\":"); j += ESP.getFreeHeap();
  j += F("}}");

  g_server.send(200, F("application/json"), j);
}

static void handleConfig() {
  Config next = g_cfg;

  for (uint32_t i = 0; i < NUM_CHANNELS; i++) {
    char key[16];

    snprintf(key, sizeof(key), "ratio%lu", (unsigned long)i);
    if (!g_server.hasArg(key)) { g_server.send(400, F("text/plain"), F("Parametre ratio manquant")); return; }
    const long milli = lroundf(g_server.arg(key).toFloat() * 1000.0f);
    if (milli < (long)RATIO_MIN_MILLI || milli > (long)RATIO_MAX_MILLI) {
      g_server.send(400, F("text/plain"), F("Ratio hors bornes (0,10 a 10,00)")); return;
    }
    next.ratioMilli[i] = (uint16_t)milli;

    snprintf(key, sizeof(key), "ppin%lu", (unsigned long)i);
    if (g_server.hasArg(key)) {
      const long v = g_server.arg(key).toInt();
      if (v < 1 || v > 8) { g_server.send(400, F("text/plain"), F("Impulsions/tour entree hors bornes")); return; }
      next.pulsesInPerRev[i] = (uint8_t)v;
    }

    snprintf(key, sizeof(key), "ppout%lu", (unsigned long)i);
    if (g_server.hasArg(key)) {
      const long v = g_server.arg(key).toInt();
      if (v < 1 || v > 8) { g_server.send(400, F("text/plain"), F("Impulsions/tour sortie hors bornes")); return; }
      next.pulsesOutPerRev[i] = (uint8_t)v;
    }

    snprintf(key, sizeof(key), "maxrpm%lu", (unsigned long)i);
    if (g_server.hasArg(key)) {
      const long v = g_server.arg(key).toInt();
      if (v < 500 || v > 30000) { g_server.send(400, F("text/plain"), F("Ecretage hors bornes")); return; }
      next.maxSimRpm[i] = (uint16_t)v;
    }
  }

  g_cfg = next;
  configSave();

  /* Prise d'effet immediate : updateChannel() recalculera la demi-periode au
   * prochain tour de boucle, et la NMI l'adoptera a la frontiere de front
   * suivante. Aucune interruption du signal, aucun redemarrage. */
  g_server.send(200, F("text/plain"), F("Enregistre, applique immediatement"));
}

static void applyWifi();

static void handleWifi() {
  if (!g_server.hasArg("apSsid") || !g_server.hasArg("apPass")) {
    g_server.send(400, F("text/plain"), F("Parametres AP manquants")); return;
  }
  const String apSsid = g_server.arg("apSsid");
  const String apPass = g_server.arg("apPass");
  if (apSsid.length() < 1 || apSsid.length() > 32) {
    g_server.send(400, F("text/plain"), F("SSID AP invalide")); return;
  }
  if (apPass.length() < 8 || apPass.length() > 63) {
    g_server.send(400, F("text/plain"), F("Mot de passe AP : 8 a 63 caracteres")); return;
  }

  strlcpy(g_cfg.apSsid, apSsid.c_str(), sizeof(g_cfg.apSsid));
  strlcpy(g_cfg.apPass, apPass.c_str(), sizeof(g_cfg.apPass));

  if (g_server.hasArg("staSsid")) {
    strlcpy(g_cfg.staSsid, g_server.arg("staSsid").c_str(), sizeof(g_cfg.staSsid));
  }
  /* Un champ mot de passe vide signifie "ne pas modifier", pour que l'on puisse
   * changer un autre reglage sans avoir a resaisir la cle du reseau. */
  if (g_server.hasArg("staPass") && g_server.arg("staPass").length() > 0) {
    strlcpy(g_cfg.staPass, g_server.arg("staPass").c_str(), sizeof(g_cfg.staPass));
  }
  if (g_server.hasArg("staEnabled")) {
    g_cfg.staEnabled = (g_server.arg("staEnabled").toInt() != 0) ? 1 : 0;
  }

  configSave();
  g_server.send(200, F("text/plain"), F("Enregistre, reconfiguration du WiFi..."));
  applyWifi();
}

static void handleReboot() {
  g_server.send(200, F("text/plain"), F("Redemarrage"));
  g_rebootAtMs = millis() + 400u;
}

static void serverBegin() {
  g_server.on("/", HTTP_GET, []() {
    g_server.sendHeader(F("Cache-Control"), F("no-store"));
    g_server.send_P(200, PSTR("text/html"), PAGE_INDEX);
  });
  g_server.on("/api/status", HTTP_GET,  handleStatus);
  g_server.on("/api/config", HTTP_POST, handleConfig);
  g_server.on("/api/wifi",   HTTP_POST, handleWifi);
  g_server.on("/api/reboot", HTTP_POST, handleReboot);
  g_server.onNotFound([]() { g_server.send(404, F("text/plain"), F("404")); });
  g_server.begin();
}

/* =====================================================================================
 *  9. WIFI  --  AP et STA simultanes
 * ===================================================================================== */

static uint32_t g_staRetryMs = 0;

static void applyWifi() {
  WiFi.persistent(false);          /* la config est deja dans notre EEPROM */
  WiFi.mode(WIFI_AP_STA);          /* AP et STA simultanes, comme demande  */

  /* Modem-sleep desactive : on ne veut ni latence variable ni reveils
   * intempestifs, et le module est alimente en permanence. */
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoReconnect(true);

  WiFi.softAP(g_cfg.apSsid, g_cfg.apPass);

  if (g_cfg.staEnabled && g_cfg.staSsid[0] != '\0') {
    WiFi.begin(g_cfg.staSsid, g_cfg.staPass);
    Serial.printf("[wifi] STA -> %s\n", g_cfg.staSsid);
  } else {
    WiFi.disconnect(false);
  }
  Serial.printf("[wifi] AP  -> %s  (%s)\n", g_cfg.apSsid, WiFi.softAPIP().toString().c_str());

  g_staRetryMs = millis();
}

static void wifiTick() {
  /* Nouvelle tentative de connexion STA toutes les 30 s si besoin. Non bloquant :
   * de toute facon le moteur de signal ne depend pas de la boucle principale. */
  if (!g_cfg.staEnabled || g_cfg.staSsid[0] == '\0') return;
  if (WiFi.status() == WL_CONNECTED) { g_staRetryMs = millis(); return; }
  if ((uint32_t)(millis() - g_staRetryMs) < 30000u) return;

  Serial.println(F("[wifi] STA non connectee, nouvelle tentative"));
  WiFi.begin(g_cfg.staSsid, g_cfg.staPass);
  g_staRetryMs = millis();
}

/* =====================================================================================
 *  10. SETUP / LOOP
 * ===================================================================================== */

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("=== Deye Fan Tach Simulator ==="));

  /* LED eteinte d'emblee (active a l'etat bas). */
  digitalWrite(PIN_LED, HIGH);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  configLoad();

  /* Le moteur demarre AVANT le WiFi : ainsi la generation de signal est
   * operationnelle des les premieres centaines de millisecondes, et l'onduleur
   * ne voit pas de trou de tachymetre pendant l'initialisation de la radio. */
  engineBegin();
  Serial.printf("[engine] CPU %u MHz, %u cycles/tick, echantillonnage %u us, mode %s\n",
                ESP.getCpuFreqMHz(), g_ccyPerTick, SAMPLE_INTERVAL_US,
                ENGINE_USE_NMI ? "NMI" : "niveau 1");

  applyWifi();
  serverBegin();

  if (MDNS.begin("deye-fan")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println(F("[mdns] http://deye-fan.local"));
  }

  for (uint32_t i = 0; i < NUM_CHANNELS; i++) {
    g_st[i].lastEdgeMs    = millis();
    g_st[i].lastEdgeCount = 0;
  }
}

void loop() {
  g_server.handleClient();
  MDNS.update();
  wifiTick();

  /* Rafraichissement du calcul a 50 Hz : bien plus rapide que la dynamique
   * thermique d'un ventilateur, et sans effet sur la stabilite du signal
   * puisque la NMI n'adopte une nouvelle demi-periode qu'aux frontieres de
   * front. */
  static uint32_t lastCalcMs = 0;
  if ((uint32_t)(millis() - lastCalcMs) >= 20u) {
    lastCalcMs = millis();
    for (uint32_t i = 0; i < NUM_CHANNELS; i++) {
      updateChannel(i);
    }
    updateLed();
  }

  /* Mesure de la cadence de la NMI et trace serie de courtoisie. */
  static uint32_t lastSecMs = 0, lastNmi = 0;
  if ((uint32_t)(millis() - lastSecMs) >= 1000u) {
    lastSecMs = millis();
    const uint32_t n = g_nmiCount;
    g_nmiRate = n - lastNmi;
    lastNmi   = n;

    static uint8_t tick = 0;
    if (++tick >= 5) {                    /* une ligne toutes les 5 s */
      tick = 0;
      Serial.printf("[etat] CH1 %.0f -> %.0f rpm (%.1f Hz, %s) | CH2 %.0f -> %.0f rpm (%.1f Hz, %s) | nmi/s %u\n",
                    g_st[0].inRpm, g_st[0].simRpm, g_st[0].outFreqHz, g_st[0].active ? "actif" : "attente",
                    g_st[1].inRpm, g_st[1].simRpm, g_st[1].outFreqHz, g_st[1].active ? "actif" : "attente",
                    g_nmiRate);
    }
  }

  if (g_rebootAtMs && (int32_t)(millis() - g_rebootAtMs) >= 0) {
    ESP.restart();
  }

  /* Rend la main a la pile reseau. delay(0) suffit : aucune temporisation de la
   * boucle principale ne peut affecter le signal, qui vit entierement en NMI. */
  delay(0);
}
