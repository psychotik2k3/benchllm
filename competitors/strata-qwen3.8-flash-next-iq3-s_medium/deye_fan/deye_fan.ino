/**********************************************************************
 * deye_fan.ino -- Simulateur de tachymetre pour onduleur Deye
 *                 SUN-8K-SG05LP1-EU-AM2-P (ventilateurs Noctua)
 * Carte : LOLIN(WEMOS) D1 R2 & mini (ESP8266 / ESP-12S)
 **********************************************************************
 *
 * BUT
 * ---
 * Les ventilateurs d'origine (NMB 06025VE-12N-CL 6 cm et 09225VE-12N-CU
 * 9 cm, 12 V) tournent tres vite. Les Noctua de remplacement (NF-A6x25-FLX,
 * NF-A9-FLX) tournent beaucoup plus lentement : l'onduleur verrait des RPM
 * anormalement bas et pourrait lever une alarme ventilateur ou modifier sa
 * consigne. Ce montage :
 *    1. lit le signal tach d'un Noctua 9 cm (canal A) et d'un Noctua 6 cm
 *       (canal B), chacun sur son propre canal ;
 *    2. regene vers l'onduleur un signal tach "multiplie"
 *       (RPM simules = RPM lus x ratio du canal) ;
 *    3. regle les ratios depuis une interface web, sans recompiler et sans
 *       toucher au cablage, et affiche en temps reel RPM lus / RPM simules ;
 *    4. fonctionne en WiFi AP + STA simultanes, tout est persistant en flash ;
 *    5. allume/clignote la LED du Wemos : rapide quand la simulation est
 *       active, lent quand le systeme attend un signal ventilateur.
 *
 * ARCHITECTURE -- ISOLATION TEMPS REEL / RESEAU
 * ---------------------------------------------
 * Tout ce qui touche au signal tach vit UNIQUEMENT dans l'interruption du
 * TIMER1 materiel du ESP8266. C'est un compteur materiel libre, independant
 * du SDK, du noyau, du WiFi et de la boucle Arduino :
 *   - a chaque "tick" (50 us) l'ISR echantillonne les 2 entrees tach
 *     (anti-rebond), mesure les periodes (median de 8 + moyenne glissante)
 *     et fait avancer la machine d'etat qui emet les fronts de sortie ;
 *   - l'ISR se re-arme elle-meme des son entree : la periode suivante du
 *     tick vaut exactement TICK_CYCLES cycles CPU. Aucune derive, aucune
 *     accumulation, quoi que fasse le reste du logiciel.
 * La boucle Arduino (WiFi, serveur HTTP, JSON, page web, ecriture flash de
 * la config) ne fait que LIRE/ECRIRE des variables 32 bits et des broches
 * GPIO en configuration. Elle ne fabrique JAMAIS un front. Le pire jitter
 * possible sur un front de sortie est donc 1 tick = 50 us, soit 0,06 a
 * 1,7 % selon la vitesse du ventilateur -- tres en dessous de ce qu'un
 * controleur de ventilateur (qui moyenne le tach sur plusieurs tours) peut
 * detecter. Une requete HTTP, une association WiFi, une tache de calibration
 * RF du SDK : aucun effet mesurable sur le signal emis.
 *
 * NIVEAUX LOGIQUES -- TENSION D'ONDULEUR INCONNUE (3,3 / 5 / 12 V)
 * ----------------------------------------------------------------
 *   ENTREE : le tach d'un Noctua est un collecteur ouvert ; Noctua precise que
 *   ce signal "ne porte pas de tension" et doit etre rappele au niveau voulu
 *   (2 impulsions par tour). On rappele donc le noeud mesure (R0 10k vers le
 *   12 V ventilateurs, note README 2.2) et on l'envoie sur la base d'un NPN en
 *   commutateur (R1 10k, pull-up collecteur R3 10k vers 3V3) : niveau-shift
 *   propre quelle que soit la tension de rappel (3,3, 5 ou 12 V) -- la broche
 *   ESP ne voit jamais plus de 3,3 V. Un diviseur + diode de protection ferait
 *   dependre le courant de la tension de rappel et de sa valeur : le NPN, non.
 *   SORTIE : le GPIO commande la base d'un NPN dont le COLLECTEUR va sur la
 *   broche tach du Deye et l'EMETTEUR sur la masse du Deye : collecteur
 *   ouvert simple, "en parallele" sur le tach d'origine. Un collecteur
 *   ouvert fonctionne a 3,3 V comme a 12 V sans aucun changement materiel.
 *   Une diode Schottky (1N5819) entre la base et le 3V3 plafonne la broche
 *   ESP a ~3,6 V meme si le Deye envoie du 12 V dans le circuit.
 *   Cablage : par defaut le tach reel du Noctua va au seul circuit de mesure,
 *   le tach du Deye au seul collecteur du NPN de sortie (README 2.4). L'option
 *   "tach du ventilateur aussi relie au Deye" (jumper J2 / diode D5) est
 *   prevue dans le firmware (reglage directTach) : elle active la compensation
 *   de recouvrement des creneaux decrite dans le README.
 * Voir README.md : schema complet, nomenclature, regles de cablage (masse
 * commune obligatoire, 2 diodes OR sur l'alimentation pour tolerer les 2
 * connecteurs a des tensions differentes / non alimentes en meme temps).
 *
 * BROCHAGE ESP (Wemos D1 mini V2.3.0)
 *   canal A (9 cm) : entree GPIO4 (D2)   sortie GPIO13 (D7, broche SCK !)
 *   canal B (6 cm) : entree GPIO5 (D3)   sortie GPIO15 (D8, broche HS !)
 *   LED integrate  : GPIO2 (D4, active LOW)
 * GPIO13/GPIO15 sont des broches de flash SPI inutilisees en mode flash
 * normal : elles sont libres ici. Ne pas brancher de peripherique SPI en
 * meme temps. Contournement possible : OUT A -> GPIO1 (TX, couper le pont
 * USB) et OUT B -> GPIO16.
 * Contraintes de demarrage respectees par le schema : pas de pull-up externe
 * sur GPIO15 (le 10k base-emetteur du NPN fait pull-down), GPIO2 charge
 * seulement par 4,7k vers la base du NPN de sortie.
 *
 * COMPILATION
 *   Carte : "LOLIN(WEMOS) D1 R2 & mini" ; Flash size 4M (LittleFS 1M) ;
 *   CPU 80 MHz ; lwIP V2 (HH) ; upload 921600.
 *   Bibliotheques : uniquement celles du core ESP8266 (LittleFS,
 *   ESP8266WebServer, ESP8266WiFi, ESP8266mDNS, DNSServer, Ticker).
 *
 * REGLAGE DES RATIOS (interface web, sans recompiler) :
 *   Relever d'abord les RPM affichees avec le ventilateur D'ORIGINE en place
 *   (commutateur de bypass en position 1, canal en mode BYPASS : l'entree lit
 *   le tach reel du ventilateur en place et la sortie le re-emet a l'identique),
 *   puis regler le ratio pour que "RPM simulees" retrouve cette valeur a
 *   ~10 % pres. Valeurs indicatives (a verifier sur votre machine) :
 *     NF-A9 FLX ~1450 tr/min (le raccord "faible bruit" NAEN le bride ~1050 :
 *     le retirer), NF-A6x25 FLX ~1800 tr/min ;
 *     NMB 09225VE ~2600-3300 tr/min, NMB 06025VE ~4500-5300 tr/min.
 *   => ratios typiques ~x1,8-x2,3 (9 cm) et ~x2,5-x3,0 (6 cm).
 **********************************************************************/

#include <Arduino.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <Ticker.h>

/* USE_US_TIMER : option officielle du core ESP8266 qui corrige la resolution
 * reelle du timer1. Elle est deja active par defaut dans les cores recents
 * (drapeau de compilation USE_US_TIMER=1) ; la redefinition identique est
 * sans effet. TICK_CYCLES etant donne en cycles CPU (800 cycles = exactement
 * 50 us a 80 MHz = exactement 50 us au prescaler 16 MHz du timer1), la
 * valeur est la meme avec ou sans cette option. */
#define USE_US_TIMER 1

/* =====================================================================
 * 1. CONSTANTES MATERIEL ET TEMPS REEL
 * ===================================================================== */

#define TICK_US      50        /* periode du tick (us) = jitter max d'un front de sortie */
#define TICK_CYCLES  800       /* 50 us a 80 MHz ; 800 = multiple exact de 16 : aucune erreur d'arrondi */

#define LED_PIN      2         /* LED integrate du Wemos D1 mini, active LOW */

/* Chaque entree recoit le signal DU NPN de niveau, donc INVERSE du tach du
 * ventilateur ; chaque sortie commande la base du NPN de sortie, egalement
 * inverseur : la logique globale est directe (creneau bas Noctua -> creneau
 * bas vers le Deye). */
#define IN_PIN_A     4         /* D2 */
#define OUT_PIN_A    13        /* D7 */
#define IN_PIN_B     5         /* D3 */
#define OUT_PIN_B    15        /* D8 */

#define NB_CANAL 2

static const int IN_PINS[NB_CANAL]  = { IN_PIN_A, IN_PIN_B };
static const int OUT_PINS[NB_CANAL] = { OUT_PIN_A, OUT_PIN_B };

/* Regle du multiplicateur.
 * RATIO_SCALE = 100 : les ratios sont stockes en x100 (700 = x7,00). RATIO_MAX
 * est choisi pour que le calcul de la periode de sortie reste dans les bornes
 * prevues (le calcul est fait en 64 bits, un ratio plus grand ne deborderait
 * pas : seule l'interface devrait alors etre etendue). */
#define RATIO_SCALE   100
#define RATIO_MAX     2000   /* x20,00 */
#define RATIO_MIN     100    /* x1,00 */

/* Forme du signal emis */
#define CH_PULSE_TICKS   45    /* creneau bas emis : 45 x 50 us = 2,25 ms (spec tach courante) */
#define MIN_OUT_TICKS    40    /* periode de sortie minimale : 2 ms (garde-fou ratio trop eleve) */
#define DEBOUNCE_TICKS   1     /* niveau confirme sur 2 echantillons consecutifs (100 us) */
#define GLITCH_TICKS     4     /* periode < 4 ticks (200 us) ignoree : parasite */

/* Garde-fou de plausibilite : si 8 periodes mesurees consecutives descendent
 * sous ce seuil, le signal est estime "trappe" (parasite, entree mal cablee,
 * eventuelle commande de vitesse du Deye recopiee par erreur) et la sortie du
 * canal est coupee jusqu'a intervention (reglage + redemarrage).
 * 6000 ticks = 300 ms = 200 tr/min pour un tach a 2 impulsions par tour :
 * tres en dessous du ralenti reel des Noctua, donc aucune fausse alerte en
 * fonctionnement normal. A abaisser (jusqu'a ~1200 = 60 ms = 1000 tr/min) si
 * vous voulez une surveillance plus serree. */
#define TRAP_PERIOD_TICKS 6000
#define TRAP_EDGE_LIMIT   8    /* fronts "trop rapides" consecutifs -> defaut */

/* Perte de signal / LED / demarrage */
#define INPUT_LOSS_TICKS 20000 /* 1 s sans front d'entree -> entree consideree perdue */
#define BLINK_ACTIVE_MS  100   /* clignotement rapide : au moins un canal simule */
#define BLINK_IDLE_MS    500   /* clignotement lent : en attente d'un signal ventilateur */
#define BLINK_CUT_MS     1000  /* clignotement tres lent : simulation globalement coupee */

enum ChanMode : uint8_t { MODE_OFF = 0, MODE_SIM = 1, MODE_BYPASS = 2 };
/* MODE_OFF    : sortie flottante -> l'onduleur voit son propre niveau haut (0 RPM).
 * MODE_SIM    : periode de sortie = periode mesuree filtree / ratio.
 * MODE_BYPASS : recopie fidele du tach du ventilateur (ratio ignore) -- sert a
 *               relever les RPM reels / d'origine avant de regler les ratios. */

/* =====================================================================
 * 2. CONFIGURATION PERSISTANTE (LittleFS, 2 exemplaires + CRC)
 * ===================================================================== */

#define CFG_MAGIC   0x46445944u   /* "DYDF" */
#define CFG_VERSION 5
#define CFG_SLOTS   2             /* 2 exemplaires : si une ecriture est coupee, l'autre reste valide */

struct ChannelCfg {
  uint16_t ratio_x100;   /* multiplicateur x RATIO_SCALE : 100 = x1,00 ... 2000 = x20,00 */
  uint8_t  mode;         /* ChanMode */
  uint8_t  enabled;      /* 0 = sortie forcee inactive sur ce canal */
};

struct Config {
  uint32_t   magic, version, len;
  ChannelCfg ch[2];
  uint8_t    ppr;            /* impulsions par tour du tach (Noctua = 2) */
  uint8_t    master_en;      /* 0 = simulation coupee globalement (interrupteur web) */
  uint8_t    directTach;     /* 1 = tach reel du ventilateur aussi relie au Deye (jumper J2) :
                              *     compensation du recouvrement des creneaux activee */
  uint8_t    rfu1;
  char       ssid[33];       /* client WiFi (STA) ; vide = point d'acces seul */
  char       pass[65];
  char       ap_ssid[33];    /* point d'acces du simulateur (toujours actif) */
  char       ap_pass[65];    /* vide = AP ouvert (signale sur la page : a changer) */
  uint16_t   crc;
};

static Config cfg;
static bool     cfgDirty = false;
static uint32_t cfgDirtyMs = 0;

static uint16_t crc16(const uint8_t *p, uint16_t n) {
  uint16_t c = 0xFFFF;
  while (n--) { c ^= *p++; for (uint8_t b = 0; b < 8; b++) c = (c & 1) ? (c >> 1) ^ 0xA001 : (c >> 1); }
  return c;
}

static void cfgDefaults() {
  memset(&cfg, 0, sizeof(cfg));
  cfg.magic = CFG_MAGIC; cfg.version = CFG_VERSION; cfg.len = sizeof(Config);
  cfg.ch[0].ratio_x100 = 200;  cfg.ch[0].mode = MODE_SIM;  cfg.ch[0].enabled = 1;
  cfg.ch[1].ratio_x100 = 200;  cfg.ch[1].mode = MODE_SIM;  cfg.ch[1].enabled = 1;
  cfg.ppr = 2;                 /* Noctua : 2 impulsions par revolution */
  cfg.master_en = 1;
  cfg.directTach = 0;          /* cablage normal : tach du ventilateur mesure seulement */
  cfg.ssid[0] = 0; cfg.pass[0] = 0;
  strcpy(cfg.ap_ssid, "DeyeFan-Config");
  cfg.ap_pass[0] = 0;
  cfg.crc = crc16((const uint8_t *)&cfg, sizeof(Config) - 2);
}

/* Les ecritures flash ont LIEU UNIQUEMENT dans la boucle principale (jamais
 * dans l'ISR) : elles ne peuvent donc pas deplacer un front tach. Le core
 * ESP8266 execute d'ailleurs flash write/erase depuis l'IRAM sans couper les
 * interruptions : le timer1 materiel, lui, continue de tourner. */
static void cfgSave() {
  cfg.crc = crc16((const uint8_t *)&cfg, sizeof(Config) - 2);
  for (uint8_t i = 0; i < CFG_SLOTS; i++) {
    File f = LittleFS.open(String("/cfg") + i + ".bin", "w");
    if (f) { f.write((uint8_t *)&cfg, sizeof(Config)); f.close(); }
  }
  cfgDirty = false;
}

static bool cfgLoad() {
  for (uint8_t i = 0; i < CFG_SLOTS; i++) {
    File f = LittleFS.open(String("/cfg") + i + ".bin", "r");
    if (!f) continue;
    Config t;
    size_t n = f.read((uint8_t *)&t, sizeof(t));
    f.close();
    if (n == sizeof(t) && t.magic == CFG_MAGIC && t.version == CFG_VERSION &&
        t.ppr >= 1 && t.ppr <= 12 &&
        t.crc == crc16((const uint8_t *)&t, sizeof(Config) - 2)) {
      memcpy(&cfg, &t, sizeof(cfg));
      return true;
    }
  }
  return false;
}

static void cfgMarkDirty() { cfgDirty = true; cfgDirtyMs = millis(); }

/* =====================================================================
 * 3. ETAT TEMPS REEL DES CANAUX (RAM simple, acces 32 bits atomiques)
 * ===================================================================== */

#define MED_N 8

struct ChanState {
  /* --- entree : acquisition dans l'ISR --- */
  volatile uint8_t  inStable, inRun, inRunCnt, inPrev;
  volatile uint32_t cntIn;                 /* ticks depuis le dernier front montant */
  volatile uint32_t cntLow;                /* ticks depuis le dernier front descendant (duree du creneau du ventilateur) */
  volatile uint32_t lowTicks;              /* duree mesuree du creneau bas du tach ventilateur (ticks) */
  volatile uint32_t sinceEdge;             /* ticks depuis le dernier front valide */
  volatile uint32_t ring[MED_N];           /* periodes brutes (0 = vide) */
  volatile uint32_t ringLow[MED_N];        /* largeurs de creneau brutes (0 = vide) */
  volatile uint8_t  ringIdx, ringCount, ringLowIdx;
  volatile uint32_t lastInPeriod;          /* median des 8 dernieres periodes (ticks) */
  volatile uint32_t ema;                   /* periode filtree (moyenne glissante 1/32) */
  volatile uint16_t trapCnt;               /* fronts "trop rapides" consecutifs */
  /* --- sortie : machine d'etat dans l'ISR --- */
  volatile uint32_t outTarget;             /* periode de sortie visee (ticks) */
  volatile uint32_t outNext;               /* compteurs d'etat de sortie */
  volatile uint32_t outPulse;              /* largeur du creneau emis */
  volatile uint8_t  outState;              /* 0 = repos (GPIO haut), 1 = creneau (GPIO bas) */
  /* --- reglages copies pour l'ISR + etats composites --- */
  volatile uint8_t  mode;                  /* copie ISR du mode, ecrite par la boucle */
  volatile uint16_t ratioX100;             /* copie ISR du ratio */
  volatile uint8_t  inputLost, trapped, simActive;
  volatile uint32_t outPeriodUsed;         /* periode de sortie reellement appliquee */
};
static ChanState cs[NB_CANAL];

/* Cablage du tach cote onduleur : 1 = le tach reel du ventilateur est egalement relie au Deye
 * (jumper J2 / diode D5, README 2.4). Les creneaux du ventilateur et ceux du simulateur se
 * chevauchent alors sur le fil tach : la periode emise est allongee d'autant pour que le Deye
 * recoive bien le nombre d'impulsions demande. 0 = cablage normal, aucune compensation. */
static volatile uint8_t compRecouvrement = 0;

/* Horloge et LED : pas d'appel a micros()/millis() dans l'ISR. */
static volatile uint32_t isrUs = 0;        /* temps ecoule, tenu par l'ISR */
static volatile uint32_t ledHalfTicks = 5000; /* demi-periode de clignotement en ticks (calculee par la boucle : pas de division dans l'ISR) */
static uint32_t ledPhase = 0, ledState = 1;

static inline void gpioWrite(int pin, uint8_t v) {
  if (v) GPO_SET_SET = bit(pin); else GPO_SET_CLEAR = bit(pin);
}

/* Coupe la sortie d'un canal (NPN bloque). Appelee depuis la boucle ET l'ISR. */
IRAM_ATTR static void outStop(ChanState *c, int pin) {
  GPO_SET_SET = bit(pin);                  /* GPIO haut -> NPN bloque -> tach Deye flotte a son niveau haut */
  c->outState = 0;
  c->outNext = 0;
}

/* Median des valeurs non nulles d'un tampon circulaire. Appelee seulement a un
 * front d'entree (donc 15-100 Hz max), jamais a chaque tick : cout negligeable.
 * IRAM_ATTR obligatoire : appelee depuis l'ISR. */
IRAM_ATTR static uint32_t medianFilter(const volatile uint32_t *ring) {
  uint32_t v[MED_N];
  uint8_t n = 0;
  for (uint8_t i = 0; i < MED_N; i++) {
    uint32_t x = ring[i];
    if (x) {
      uint8_t j = n;
      while (j && v[j - 1] > x) { v[j] = v[j - 1]; j--; }
      v[j] = x; n++;
    }
  }
  if (!n) return 0;
  return v[n >> 1];
}

/* Machine d'etat de sortie -- executee a chaque tick (50 us).
 * MODE_SIM : la sortie tourne "libre" a la periode filtree / ratio ;
 * MODE_BYPASS : elle re-emet les impulsions de l'entree (le front d'entree
 * remet le compteur a zero : le signal emis suit le ventilateur).
 * Le compteur compte a rebours "1 -> 0 = front a emettre" : on teste donc
 * outNext == 1 (et non 0), ce qui garantit qu'une sortie arretee par
 * outStop() (compteur a zero) n'emet jamais de front parasite. */
IRAM_ATTR static void outService(ChanState *c, int pin) {
  if (c->outNext) c->outNext--;

  if (c->outState == 0) {
    if (c->outNext == 1) {                 /* front descendant : tach tire a la masse */
      GPO_SET_CLEAR = bit(pin);            /* GPIO bas -> NPN sature -> collecteur ~0,3 V */
      c->outState = 1;
      c->outNext = c->outPulse;
    }
  } else {
    if (c->outNext == 1) {                 /* front montant : fin du creneau */
      GPO_SET_SET = bit(pin);
      c->outState = 0;
      uint32_t t = c->outTarget;
      if (t < MIN_OUT_TICKS) t = MIN_OUT_TICKS;
      c->outNext = t;
      /* le creneau emis doit rester strictement plus court que la periode
       * (protection si MIN_OUT_TICKS etait abaisse en dessous de CH_PULSE_TICKS) */
      if (c->outPulse >= t) c->outPulse = (t > 1) ? (t - 1) : 1;
      c->simActive = 1;
    }
  }
}

/* ---------------------------------------------------------------------
 * ISR du TIMER1 : coeur du systeme. Duree typique < 3 us sur 50 us.
 * Aucun appel SDK, aucun flottant, aucun acces flash.
 * ------------------------------------------------------------------- */
IRAM_ATTR static void tickISR() {
  /* Re-arme IMMEDIATE : le timer1 du ESP8266 est un compteur "one-shot".
   * En le re-armant ici, la periode suivante vaut exactement TICK_CYCLES
   * cycles CPU, sans derive ni accumulation. timer1_disable() avant
   * timer1_write() : contournement officiel de l'errata de re-arme du
   * timer1 rappele dans les exemples du core. */
  timer1_disable();
  timer1_write(TICK_CYCLES);

  isrUs += TICK_US;

  /* ---- LED : emission du rythme dans l'ISR (le rythme est choisi par la
     boucle, la division n'y est jamais faite). LED active LOW. ---- */
  if (++ledPhase >= ledHalfTicks) {
    ledPhase = 0;
    ledState ^= 1;
    gpioWrite(LED_PIN, ledState);
  }

  for (int i = 0; i < NB_CANAL; i++) {
    ChanState *c = &cs[i];

    /* ---------- ENTREE ----------
       La broche ESP voit le signal DU NPN de niveau, INVERSE du tach du
       Noctua : un creneau bas du Noctua = niveau HAUT ici. La periode
       complete du tach = temps entre deux fronts montants de la broche. */
    uint32_t raw = GPIO_INPUT_REG(IN_PINS[i]) ? 1u : 0u;

    /* anti-rebond : le nouveau niveau doit etre lu de facon coherente pendant
     * DEBOUNCE_TICKS+1 echantillons (soit 2 ticks = 100 us) pour etre accepte.
     * Un pic d'une seule periode d'horloge (souffle de commutation) ne cree
     * donc aucun front parasite. */
    if (raw != c->inStable) {
      if (raw == c->inRun) { if (++c->inRunCnt > DEBOUNCE_TICKS) { c->inStable = raw; c->inRunCnt = 0; } }
      else                 { c->inRun = raw; c->inRunCnt = 1; }
    }

    c->cntIn++;
    c->cntLow++;
    c->sinceEdge++;

    if (c->inStable != c->inPrev) {        /* transition confirmee par l'anti-rebond */
      c->inPrev = c->inStable;
      if (c->inStable == 0) {              /* front DESCENDANT sur la broche = creneau du tach ventilateur */
        c->cntLow = 0;
      } else {                             /* front MONTANT = bord de mesure de la periode */
        uint32_t per = c->cntIn;
        uint32_t low = c->cntLow;          /* largeur du creneau qui vient de finir (ticks) */
        c->cntIn = 0;
        c->cntLow = 0;
        if (per >= GLITCH_TICKS) {
          c->sinceEdge = 0;
          /* mesure de la largeur de creneau du ventilateur (sert a la compensation
           * de recouvrement quand le tach reel est aussi relie au Deye) */
          if (low >= GLITCH_TICKS && low < per) {
            c->ringLow[c->ringLowIdx] = low;
            c->ringLowIdx = (c->ringLowIdx + 1) % MED_N;
            uint32_t lm = medianFilter(c->ringLow);
            if (lm) c->lowTicks = lm;
          }
          if (per < TRAP_PERIOD_TICKS) {   /* trop rapide : parasite ou regime hors plage */
            if (c->trapCnt < 0xFFFF) c->trapCnt++;
            if (c->trapCnt >= TRAP_EDGE_LIMIT) {
              c->trapped = 1;
              outStop(c, OUT_PINS[i]);
            }
          } else {
            c->trapCnt = 0;
            c->ring[c->ringIdx] = per;
            c->ringIdx = (c->ringIdx + 1) % MED_N;
            if (c->ringCount < MED_N) c->ringCount++;
            uint32_t med = medianFilter(c->ring);
            if (med) {
              c->lastInPeriod = med;
              /* moyenne glissante 1/32 : suivi doux des variations de consigne */
              c->ema = c->ema ? c->ema + ((int32_t)med - (int32_t)c->ema) / 32 : med;
            }
            c->inputLost = 0;

            /* BYPASS : re-emission a l'identique, alignee sur le ventilateur.
             * Si un creneau est en cours d'emission, on se contente d'aligner
             * la periode : pas de "double impulsion" parasite. */
            if (c->mode == MODE_BYPASS && !c->trapped && med) {
              uint32_t t = med;
              c->outPulse = med + 1;       /* on reproduit la largeur de creneau mesuree */
              if (compRecouvrement && c->lowTicks) {
                /* meme regle que pour SIM : la periode emise doit depasser notre creneau
                 * de la periode complete du ventilateur pour qu'aucun front ne soit perdu */
                uint32_t tmin = med + c->outPulse + 2;
                if (t < tmin) t = tmin;
              }
              if (t < MIN_OUT_TICKS) t = MIN_OUT_TICKS;
              c->outTarget = t;
              c->outPeriodUsed = t;
              if (c->outState == 0) c->outNext = 1;  /* front descendant des le tick suivant */
            }
          }
        }
      }
    }

    /* Perte du signal ventilateur (Noctua arrete / deconnecte) : la periode
     * filtree est gelee, la sortie continue d'emettre le dernier RPM connu
     * (comportement volontaire : un Noctua a l'arret = ventilateur HS,
     * l'onduleur doit voir des RPM figes et non un trou dans le signal). */
    if (!c->inputLost && c->sinceEdge > INPUT_LOSS_TICKS) c->inputLost = 1;

    /* ---------- SORTIE ---------- */
    if ((c->mode == MODE_SIM || c->mode == MODE_BYPASS) && !c->trapped) {
      if (c->mode == MODE_SIM) {
        /* periode de sortie = periode filtree / ratio. Calcul entier 32 bits,
         * sans debordement possible : ema < 2^28 (le compteur de periode ne peut
         * pas depasser 4 millions de ticks) et RATIO_SCALE*50 = 5000, done le
         * produit reste sous 2^31. Aucune division 64 bits n'est appelee ici :
         * les auxiliaires 64 bits du compilateur ne sont pas garantis en IRAM,
         * et une virgule flottante n'a rien a faire dans une ISR. */
        uint32_t e = c->ema;
        uint16_t r = c->ratioX100;         /* x RATIO_SCALE, borne basse garantie cote boucle */
        if (e && r >= RATIO_SCALE) {
          uint32_t t = (e * ((uint32_t)TICK_US * RATIO_SCALE) + r / 2) / r;  /* arrondi : biais moyen nul */
          /* Compensation de recouvrement (cablage J2 ferme : tach reel du ventilateur
           * egalement relie au Deye, README 2.4). Le Deye ne voit un front montant que si
           * le ventilateur ET le simulateur sont flottants a la fois. On impose donc
           *   periode_emise - creneau_emis >= periode_du_ventilateur
           * ce qui garantit qu'a la fin de chaque creneau eme, le ventilateur est au repos :
           * chaque creneau eme produit exactement un front vu par le Deye, et aucun front
           * parasite ne peut s'ajouter. Le gain maximal utile est alors
           *   ratio_max = periode_ventilateur / (largeur_creneau + largeur_creneau_ventilateur)
           * (~x4 a x5 pour des Noctua, tres au-dela du besoin). En cablage normal (J2
           * ouvert), cette compensation est desactivee : ratio jusqu'a x20. */
          if (compRecouvrement && c->lowTicks && e > c->lowTicks + 8) {
            uint32_t tmin = e + CH_PULSE_TICKS + 2;
            if (t < tmin) t = tmin;
          }
          c->outPulse = CH_PULSE_TICKS;
          if (t < MIN_OUT_TICKS) t = MIN_OUT_TICKS;
          c->outTarget = t;
          c->outPeriodUsed = t;
        }
      }
      outService(c, OUT_PINS[i]);
    }
    /* MODE_OFF ou defaut "trappe" : sortie deja forcee a l'etat repos */
  }
}

/* =====================================================================
 * 4. APPLICATION DE LA CONFIG DANS L'ETAT ISR
 * ===================================================================== */

static void clampRatio(uint16_t *r) { if (*r < RATIO_MIN) *r = RATIO_MIN; if (*r > RATIO_MAX) *r = RATIO_MAX; }

/* Rythme de clignotement de la LED, converti en demi-periodes de ticks :
 * la conversion est faite ici (dans la boucle) et jamais dans l'ISR. */
static void setLedBlinkMs(uint32_t ms) {
  uint32_t h = (ms * 1000UL) / (TICK_US * 2UL);
  ledHalfTicks = h ? h : 1;
}

static void applyChannelSettings() {
  compRecouvrement = cfg.directTach ? 1 : 0;
  for (int i = 0; i < NB_CANAL; i++) {
    clampRatio(&cfg.ch[i].ratio_x100);
    uint8_t mode = (cfg.master_en && cfg.ch[i].enabled) ? cfg.ch[i].mode : MODE_OFF;
    cs[i].ratioX100 = cfg.ch[i].ratio_x100;
    cs[i].mode      = mode;
    if (mode == MODE_OFF) outStop(&cs[i], OUT_PINS[i]);
  }
  setLedBlinkMs(BLINK_IDLE_MS);
}

/* =====================================================================
 * 5. WiFi (AP + STA simultanes) + SERVEUR WEB
 * ===================================================================== */

static ESP8266WebServer server(80);
static DNSServer   dns;
static Ticker      rebootTicker;
static uint32_t    lastStaAttempt = 0;
static uint32_t    lastClientMs = 0;   /* horodatage du dernier echange HTTP (evite d'ecrire la flash pendant une session web) */

static void wifiApStart() {
  /* Point d'acces du simulateur : toujours actif, canaux 1 par defaut.
   * (Si votre routeur est sur un autre canal, vous pouvez forcer
   * WiFi.channel() avant softAP pour un coexistence AP+STA plus propre.) */
  if (cfg.ap_pass[0]) WiFi.softAP(cfg.ap_ssid, cfg.ap_pass, 1);
  else                WiFi.softAP(cfg.ap_ssid, NULL, 1);
  /* DNS de secours type portail captif : tout nom repond sur l'AP. */
  dns.start(53, "*", WiFi.softAPIP());
}

static void wifiApply() {
  WiFi.persistent(false);            /* la config est geree ici (LittleFS), pas dans l'SDK */
  WiFi.mode(WIFI_OFF);               /* remise a plat propre avant AP+STA */
  delay(120);                        /* le core demande un leger delai apres WIFI_OFF */
  WiFi.mode(WIFI_AP_STA);            /* AP et STA en simultane */
  WiFi.setHostname("deye-fan");
  wifiApStart();
  if (cfg.ssid[0]) { lastStaAttempt = millis(); WiFi.begin(cfg.ssid, cfg.pass); }
  /* Aucune economie d'energie WiFi : sans cela le SDK declenche des phases
   * de calibration RF qui peuvent bloquer le noyau quelques millisecondes.
   * Le timer1 materiel tourne neanmoins, mais on supprime toute source de
   * perturbation logicielle du chemin critique. */
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
}

static void wifiService() {
  if (cfg.ssid[0] && WiFi.status() != WL_CONNECTED &&
      millis() - lastStaAttempt > 5000) {
    lastStaAttempt = millis();
    WiFi.disconnect(false);          /* false : ne pas eteindre le point d'acces */
    WiFi.begin(cfg.ssid, cfg.pass);
  }
}

/* ----------------------------- etat JSON ----------------------------- */
static void sendState() {
  char buf[640];
  int  o = 0;
  uint8_t ppr = cfg.ppr ? cfg.ppr : 2;
  o += snprintf(buf + o, sizeof(buf) - o,
                "{\"up\":%lu,\"ppr\":%u,\"mst\":%u,\"dir\":%u",
                (unsigned long)millis(), cfg.ppr, cfg.master_en, cfg.directTach);
  for (int i = 0; i < NB_CANAL; i++) {
    ChanState *c = &cs[i];
    uint32_t pIn  = c->lastInPeriod;                                   /* ticks */
    uint32_t pOut = c->outPeriodUsed;
    /* RPM = 60 s / (periode x nb d'impulsions par tour), en microsecondes */
    uint32_t rpmIn  = pIn  ? (60000000UL / ((uint32_t)TICK_US * ppr * pIn))  : 0;
    uint32_t rpmOut = pOut ? (60000000UL / ((uint32_t)TICK_US * ppr * pOut)) : 0;
    o += snprintf(buf + o, sizeof(buf) - o,
                  ",\"c%u\":{\"med\":%lu,\"out\":%lu,\"rin\":%lu,\"rout\":%lu,"
                  "\"ratio\":%u,\"mode\":%u,\"en\":%u,\"lost\":%u,\"trap\":%u,\"sim\":%u}",
                  i, (unsigned long)pIn, (unsigned long)pOut,
                  (unsigned long)rpmIn, (unsigned long)rpmOut,
                  cs[i].ratioX100, cs[i].mode, cfg.ch[i].enabled,
                  c->inputLost, c->trapped, c->simActive);
  }
  o += snprintf(buf + o, sizeof(buf) - o,
                ",\"wif\":%d,\"rssi\":%d,\"ip\":\"%s\",\"apip\":\"%s\",\"ap\":\"%s\"}",
                (int)WiFi.status(), WiFi.RSSI(),
                WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "-",
                WiFi.softAPIP().toString().c_str(), cfg.ap_ssid);
  server.send(200, "application/json", buf);
}

/* ------------------------ reglages ratios / modes -------------------- */
static void handleControl() {
  bool changed = false;

  if (server.hasArg("mst"))  { cfg.master_en    = server.arg("mst").toInt() ? 1 : 0; changed = true; }
  if (server.hasArg("dir"))  { cfg.directTach   = server.arg("dir").toInt() ? 1 : 0; changed = true; }
  if (server.hasArg("ppr"))  { long p = server.arg("ppr").toInt(); if (p >= 1 && p <= 12) cfg.ppr = (uint8_t)p; changed = true; }

  for (int i = 0; i < NB_CANAL; i++) {
    if (server.hasArg(String("r") + i)) { long rr = server.arg(String("r") + i).toInt();
                                          cfg.ch[i].ratio_x100 = (uint16_t)(rr < 0 ? 0 : (rr > 0xFFFF ? 0xFFFF : rr)); changed = true; }
    if (server.hasArg(String("m") + i)) { long m = server.arg(String("m") + i).toInt();
                                          if (m >= 0 && m <= 2) cfg.ch[i].mode = (uint8_t)m; changed = true; }
    if (server.hasArg(String("e") + i)) { cfg.ch[i].enabled = server.arg(String("e") + i).toInt() ? 1 : 0; changed = true; }
  }

  if (changed) { cfgMarkDirty(); applyChannelSettings(); }
  lastClientMs = millis();
  sendState();
}

/* ------------------------------ config WiFi -------------------------- */
static void copyField(char *dst, size_t n, const String &s) {
  strncpy(dst, s.c_str(), n - 1);
  dst[n - 1] = 0;
}

static void handleWifi() {
  if (server.hasArg("ssid"))   copyField(cfg.ssid,   sizeof(cfg.ssid),   server.arg("ssid"));
  if (server.hasArg("pass"))   copyField(cfg.pass,   sizeof(cfg.pass),   server.arg("pass"));
  if (server.hasArg("apssid")) { copyField(cfg.ap_ssid, sizeof(cfg.ap_ssid), server.arg("apssid"));
                                 if (!cfg.ap_ssid[0]) strcpy(cfg.ap_ssid, "DeyeFan-Config"); }
  if (server.hasArg("appass")) copyField(cfg.ap_pass, sizeof(cfg.ap_pass), server.arg("appass"));
  cfgMarkDirty();
  server.send(200, "application/json", "{\"ok\":1,\"redemarrage\":0}");
  wifiApply();                       /* AP et/ou client appliques immediatement */
}

static void handleRestart() {
  server.send(200, "application/json", "{\"ok\":1}");
  rebootTicker.once(2, []() { ESP.restart(); });
}

/* --------------------------- page web (PROGMEM) ---------------------- */
static const char PAGE[] PROGMEM = R"rawl(<!doctype html><html lang=fr><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Simulateur tach Deye / Noctua</title>
<style>
:root{--bg:#0f1318;--fg:#e6edf3;--mut:#8b98a5;--bd:#262e38;--ac:#3fb061;--ac2:#d9a441;--err:#e0524f;--inp:#171d25}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,"Segoe UI",Roboto,sans-serif}
main{max-width:700px;margin:0 auto;padding:14px}
h1{font-size:1.15em;margin:2px 0 6px}
.sub{color:var(--mut);font-size:.85em;margin-bottom:12px}
.card{background:#141a21;border:1px solid var(--bd);border-radius:10px;padding:12px;margin:10px 0}
.card h2{font-size:.95em;margin:0 0 8px;color:var(--ac2)}
table{width:100%;border-collapse:collapse}
td{padding:5px 6px;border-bottom:1px solid var(--bd);vertical-align:middle}
tr:last-child td{border-bottom:0}
.num{font-size:1.5em;font-weight:700}.sim{color:var(--ac)}.lu{color:var(--ac2)}
.st{font-size:.8em;color:var(--mut)}
.badge{display:inline-block;border-radius:4px;padding:2px 6px;font-size:.75em;border:1px solid var(--bd)}
.badge.ok{color:var(--ac);border-color:var(--ac)}.badge.no{color:var(--mut)}.badge.err{color:var(--err);border-color:var(--err)}
.ctl{display:flex;flex-wrap:wrap;gap:6px;align-items:center;margin-top:10px}
input[type=text],input[type=number],select{background:var(--inp);border:1px solid var(--bd);color:var(--fg);border-radius:6px;padding:7px 8px;font:inherit;min-width:0}
input.n6{width:86px}input.wide{width:170px}
button{background:var(--inp);color:var(--fg);border:1px solid var(--bd);border-radius:6px;padding:7px 11px;cursor:pointer;font:inherit}
button.hi{border-color:var(--ac);color:var(--ac)}
label{font-size:.88em;color:var(--mut);display:flex;flex-direction:column;gap:3px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px 10px}
footer{color:var(--mut);font-size:.78em;margin:16px 4px;line-height:1.5}
</style></head><body><main>
<h1>Simulateur tachymetre Deye &mdash; Noctua</h1>
<div class=sub>
 <span id=wifi>WiFi : ...</span> &middot; <span id=ip>-</span> &middot;
 <span id=up>-</span> &middot; simulation globale <span id=mst class="badge ok">ON</span>
</div>

<div class=card><h2>Canal A &mdash; Noctua NF-A9 (9 cm) &rarr; connecteur Deye 9 cm</h2>
<table><tr>
<td>RPM lus<br><span class="num lu" id=rin0>-</span></td>
<td>RPM simul&eacute;s<br><span class="num sim" id=rout0>-</span></td>
<td>p&eacute;riodes entr&eacute;e / sortie<br><span class=st id=per0>-</span></td>
<td>&eacute;tat<br><span id=st0 class="badge no">-</span></td>
</tr></table>
<div class=ctl>
<select id=m0 title="mode du canal"><option value=0>OFF</option><option value=1>SIMULER</option><option value=2>BYPASS (1:1)</option></select>
<button data-c=0 data-d=-5>-0,05</button><button data-c=0 data-d=5>+0,05</button>
<input type=number id=r0 class=n6 step=0.01 min=1 max=20.00 value=2.00>
<button data-c=0 data-set=1 class=hi>Appliquer ratio</button><button data-c=0 data-e=1 id=e0>Sortie ACTIVE</button>
</div></div>

<div class=card><h2>Canal B &mdash; Noctua NF-A6x25 (6 cm) &rarr; connecteur Deye 6 cm</h2>
<table><tr>
<td>RPM lus<br><span class="num lu" id=rin1>-</span></td>
<td>RPM simul&eacute;s<br><span class="num sim" id=rout1>-</span></td>
<td>p&eacute;riodes entr&eacute;e / sortie<br><span class=st id=per1>-</span></td>
<td>&eacute;tat<br><span id=st1 class="badge no">-</span></td>
</tr></table>
<div class=ctl>
<select id=m1 title="mode du canal"><option value=0>OFF</option><option value=1>SIMULER</option><option value=2>BYPASS (1:1)</option></select>
<button data-c=1 data-d=-5>-0,05</button><button data-c=1 data-d=5>+0,05</button>
<input type=number id=r1 class=n6 step=0.01 min=1 max=20.00 value=2.00>
<button data-c=1 data-set=1 class=hi>Appliquer ratio</button>
<button data-c=1 data-e=1 id=e1>Sortie ACTIVE</button>
</div></div>

<div class=card><h2>R&eacute;glages globaux</h2>
<div class=grid>
<label>Impulsions / tour du tach (Noctua = 2)<input type=number id=ppr class=n6 min=1 max=12 value=2></label>
<label>Simulation globale<select id=mstsel><option value=1>ON</option><option value=0>OFF</option></select></label>
<label>Cablage du tach c&#234;t&#233; onduleur<select id=dir><option value=0>Normal (J2 ouvert)</option><option value=1>Ventilateur aussi reli&#233; au Deye (J2 ferm&#233;)</option></select></label>
<div class=ctl style="align-self:end"><button id=apply class=hi>Appliquer</button><button id=restart>Redemarrer</button></div>
</div>
<div class=st style="margin-top:8px">Mode BYPASS : recopie 1:1 du tach du Noctua (sert &agrave; relever les RPM r&eacute;els, y compris ceux du ventilateur d'origine avant le remplacement).<br>Cablage &laquo;&nbsp;J2 ferm&eacute;&nbsp;&raquo; : le tach r&eacute;el du Noctua est aussi connect&eacute; au Deye (via D5) ; la p&eacute;riode &eacute;mise est alors calcul&eacute;e pour que chaque impulsion simulate soit vue (ratio utile plafonn&eacute; &agrave; ~x4-x5).</div>
</div>

<div class=card><h2>WiFi (persistant, AP + client simultan&eacute;s)</h2>
<div class=grid>
<label>Client &mdash; SSID<input type=text id=ssid class=wide placeholder="(vide = point d'acces seul)"></label>
<label>Cl&eacute; WPA<input type=text id=wpass class=wide></label>
<label>Point d'acces local &mdash; SSID<input type=text id=apssid class=wide></label>
<label>Cl&eacute; AP (8 car. min, vide = ouverte)<input type=text id=appass class=wide></label>
</div>
<div class=ctl><button id=applywifi class=hi>Appliquer WiFi</button>
<span class=st>Adresse : <b>http://deye-fan.local</b> &middot; passerelle AP : <span id=apip>-</span></span></div>
</div>

<footer>Sortie tach = collecteur ouvert NPN vers la masse du Deye : compatible 3,3&nbsp;V /
5&nbsp;V / 12&nbsp;V sans modification. Les fronts sont produits par le timer mat&eacute;riel
50&nbsp;&micro;s : le r&eacute;seau, le serveur web et les &eacute;critures flash n'ont aucun effet
sur le signal. Pr&eacute;voir les dominos J1/J2 d&eacute;crits dans le README (s&eacute;curit&eacute;
et secours).</footer>
</main>
<script>
var RMIN=100,RMAX=2000,RSC=100;   /* doit rester aligne avec RATIO_MIN/RATIO_MAX/RATIO_SCALE */
var st={c0:{},c1:{}};
function P(i){return document.getElementById(i)}
function api(p){return fetch(p,{cache:'no-store'}).then(function(r){return r.json()}).then(function(j){st=j;paint()})}
function ctrl(s){return fetch('/control',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:s,cache:'no-store'}).then(function(r){return r.json()}).then(function(j){st=j;paint()})}
function paint(){
  for(var i=0;i<2;i++){var c=st['c'+i];if(!c||c.ratio===undefined)continue;
    P('rin'+i).textContent=c.rin;P('rout'+i).textContent=c.rout;
    P('per'+i).textContent=(c.med?c.med:0)+' / '+(c.out?c.out:0)+' tick (50 \u00b5s)';
    var s=P('st'+i);
    if(c.trap){s.className='badge err';s.textContent='SIGNAL TRAPP\u00c9 (sortie coup\u00e9e)';}
    else if(c.lost){s.className='badge err';s.textContent='TACH ABSENT';}
    else if(c.mode===0){s.className='badge no';s.textContent='ARRET\u00c9';}
    else if(c.sim){s.className='badge ok';s.textContent=c.mode===2?'BYPASS ACTIF':'SIMULATION ACTIVE';}
    else{s.className='badge no';s.textContent='ATTENTE DE SIGNAL';}
    var r=P('r'+i);if(document.activeElement!==r)r.value=(c.ratio/RSC).toFixed(2);
    var e=P('e'+i);e.textContent=c.en?'Sortie ACTIVE':'Sortie OFF';e.className=c.en?'':'hi';
    var m=P('m'+i);if(document.activeElement!==m)m.value=c.mode;
  }
  P('mst').textContent=st.mst?'ON':'OFF';P('mst').className='badge '+(st.mst?'ok':'no');
  P('ppr').value=st.ppr;P('mstsel').value=st.mst;
  var d=P('dir');if(document.activeElement!==d)d.value=st.dir;
  P('up').textContent='marche '+Math.floor(st.up/1000)+' s';
  P('apip').textContent=st.apip;
  P('ip').textContent=(st.wif==1)?('client '+st.ip+' ('+st.rssi+' dBm)'):'client hors ligne';
  P('wifi').textContent=(st.wif==1)?'WiFi client OK':'point d\'acces seul';
}
document.addEventListener('click',function(ev){
  var b=ev.target.closest?ev.target.closest('button'):null;if(!b)return;
  if(b.dataset.c!==undefined){
    var i=+b.dataset.c,c=st['c'+i],v=Math.round(c.ratio);
    if(b.dataset.d!==undefined){v+= +b.dataset.d}
    else if(b.dataset.set){var f=parseFloat(P('r'+i).value);if(!isNaN(f))v=Math.round(f*RSC)}
    else if(b.dataset.e!==undefined){ctrl('e'+i+'='+(c.en?0:1));return}
    if(v<RMIN)v=RMIN;if(v>RMAX)v=RMAX;ctrl('r'+i+'='+v);return}
  if(b.id==='apply'){ctrl('ppr='+P('ppr').value+'&mst='+P('mstsel').value+'&dir='+P('dir').value+'&m0='+P('m0').value+'&m1='+P('m1').value);return}
  if(b.id==='restart'){if(confirm('Redemarrer le simulateur ?'))fetch('/restart',{cache:'no-store'});return}
  if(b.id==='applywifi'){
    fetch('/wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:'ssid='+encodeURIComponent(P('ssid').value)+'&pass='+encodeURIComponent(P('wpass').value)+
           '&apssid='+encodeURIComponent(P('apssid').value)+'&appass='+encodeURIComponent(P('appass').value),
      cache:'no-store'}).then(function(){setTimeout(function(){api('/state')},4000)});return}
});
['m0','m1'].forEach(function(id){P(id).addEventListener('change',function(){ctrl('m'+id[1]+'='+P(id).value)})});
setInterval(function(){api('/state')},1200);
api('/state');
</script></body></html>)rawl";

static void handleRoot() { server.send_P(200, "text/html", PAGE); }

/* =====================================================================
 * 6. SETUP / LOOP
 * ===================================================================== */

void setup() {
  /* Sorties tach d'abord, etat repos immediat (NPN bloque) des le boot. */
  for (int i = 0; i < NB_CANAL; i++) {
    pinMode(OUT_PINS[i], OUTPUT);
    digitalWrite(OUT_PINS[i], HIGH);   /* GPIO haut -> NPN bloque -> tach Deye a son niveau haut (0 RPM) */
  }
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);         /* LED eteinte (active LOW) */
  for (int i = 0; i < NB_CANAL; i++)
    pinMode(IN_PINS[i], INPUT);        /* pull-up 10k vers 3V3 realise par le schema */

  Serial.begin(115200);
  Serial.println();
  Serial.println(F("deye_fan -- simulateur tach Deye/Noctua : demarrage"));

  if (!LittleFS.begin()) Serial.println(F("ATTENTION : LittleFS monte mal (verifier 'Flash size 4M + LittleFS 1M')"));
  if (cfgLoad()) Serial.println(F("config chargee depuis la flash"));
  else         { Serial.println(F("config absente/validee par defaut (ratio x2,00 / mode SIMULER)")); cfgDefaults(); }

  memset((void *)cs, 0, sizeof(cs));
  for (int i = 0; i < NB_CANAL; i++) { cs[i].outPulse = CH_PULSE_TICKS; cs[i].mode = MODE_OFF; }
  applyChannelSettings();

  /* WiFi AP + STA et serveur web */
  WiFi.persistent(false);
  wifiApply();
  if (MDNS.begin("deye-fan")) MDNS.addService("http", "tcp", 80);
  server.on("/",          handleRoot);
  server.on("/state",     sendState);
  server.on("/control",   handleControl);
  server.on("/wifi",      handleWifi);
  server.on("/restart",   handleRestart);
  server.onNotFound([]() { server.send(404, "text/plain", "not found"); });
  server.begin();
  Serial.print(F("AP  : ")); Serial.print(cfg.ap_ssid);
  Serial.print(F("  http://")); Serial.println(WiFi.softAPIP());
  Serial.print(F("STA : ")); Serial.println(cfg.ssid[0] ? cfg.ssid : "(non configure)");

  /* Timer1 materiel : tick de 50 us, ISR hergee en IRAM. Le prescaler 16 MHz
   * (TIM_DIV16) donne 800 counts = exactement 50 us. Le timer est en mode
   * one-shot (TIM_SINGLE) : c'est l'ISR qui le re-arme a chaque entree, ce
   * qui supprime toute derive de periode. tickISR est declaree IRAM_ATTR,
   * donc executable depuis la RAM rapide meme quand la flash est occupee
   * (ecritures LittleFS, mises a jour WiFi). */
  timer1_init();
  timer1_attachInterrupt(tickISR);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
  timer1_write(TICK_CYCLES);
  Serial.println(F("timer1 actif : tick 50 us - signal tach pris en charge a 100 % par l'ISR"));
}

static uint32_t tWatchdog = 0;
static uint32_t lastUs    = 0;

void loop() {
  server.handleClient();
  dns.processNextRequest();
  wifiService();

  /* Choix du rythme LED (l'emission du rythme reste dans l'ISR : pas de
   * gigue). Le cycle est lu sur les drapeaux tenus par l'ISR. */
  bool anySim = false;
  for (int i = 0; i < NB_CANAL; i++) if (cs[i].simActive) anySim = true;
  if (!cfg.master_en) setLedBlinkMs(BLINK_CUT_MS);    /* tres lent : simulation coupee */
  else if (anySim)    setLedBlinkMs(BLINK_ACTIVE_MS); /* rapide : simulation en cours  */
  else                setLedBlinkMs(BLINK_IDLE_MS);   /* lent : attente de signal      */

  /* Enregistrement de la config : jamais pendant une transaction HTTP en
   * cours, force au bout de 15 s pour ne jamais perdre un reglage. */
  uint32_t now = millis();
  if (cfgDirty && ((millis() - lastClientMs > 1000 && now - cfgDirtyMs > 600) ||
                   now - cfgDirtyMs > 15000)) {
    cfgSave();
    Serial.println(F("config enregistree"));
  }

  /* Controle de vie du timer1 : si le nombre de ticks est incoherent
   * (core modifie, materiel detourne), on le signale serieusement. */
  if (now - tWatchdog > 2000) {
    tWatchdog = now;
    uint32_t expect = 2000000UL / TICK_US;           /* ticks attendus en 2 s */
    uint32_t du = isrUs - lastUs; lastUs = isrUs;
    if (du < expect / 2 || du > expect + expect / 4) {
      Serial.print(F("ANOMALIE timer1 : ticks=")); Serial.print(du);
      Serial.print(F(" attendu=")); Serial.println(expect);
    }
  }

  delay(1);   /* cede la main au SDK WiFi : ne concerne PAS le timer1, qui est materiel */
}
