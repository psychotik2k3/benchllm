# Simulateur de tachymetre pour Deye SUN-8K-SG05LP1-EU-AM2-P (ventilateurs Noctua)

Ce dossier contient :

| Fichier | Role |
|---|---|
| `deye_fan/deye_fan.ino` | Code Arduino complet (carte **LOLIN(WEMOS) D1 R2 & mini**), tout est dans un seul fichier, aucune bibliotheque externe |
| `README.md` | Schema electronique complet (alimentation, entrees, sorties), nomenclature, cablage, reglages, mise a l'epreuve |

---

## 1. Principe general

```
  ONDULEUR DEYE                      BOITIER DU SIMULATEUR                 VENTILATEURS DE REMPLACEMENT
 ┌───────────────┐                ┌──────────────────────────┐           ┌──────────────────────────┐
 │ Connecteur 9cm│  12V ─┬────────│ Alim: 2 diodes OR +      │           │ Noctua NF-A9  #1 (+12V)  │
 │ GND +12V TACH │  GND ─┼────────│ abaisseur 12V->5V ->     │           │ Noctua NF-A9  #2 (+12V)  │
 │               │  TACH─┼──┐     │ Wemos D1 mini (5V USB)   │           │ Noctua NF-A6x25 #1       │
 │ Connecteur 6cm│       │  ├──┐  │                          │           │ Noctua NF-A6x25 #2       │
 │ GND +12V TACH │       │  │  ├──│  entrees tach (NPN)      │           │  -> 12V commun, GND      │
 │               │       │  │  │  │  sorties tach (NPN       │           │  -> tach vers le Deye    │
 │ (tach = entree│       └──┘  └──│  collecteur ouvert)      │           │    via le commutateur    │
 │  du controleur│                 │  LED = etat simulation   │           │    BYPASS / SIMULE       │
 │  de ventilos) │                 └──────────────────────────┘           └──────────────────────────┘
 └───────────────┘
```

Deux canaux complets et independants :

* **Canal A** : tach Noctua NF-A9 (9 cm) -> lecture ESP8266 -> re-emission vers le connecteur 9 cm du Deye.
* **Canal B** : tach Noctua NF-A6x25 (6 cm) -> lecture ESP8266 -> re-emission vers le connecteur 6 cm du Deye.

Un canal ne fait que **saturer/un blocage un NPN en parallele sur le fil tach d'origine** :
le NPN sature = le tach du Deye est tire a la masse (creneau bas, comme le faisait le ventilateur d'origine) ;
le NPN bloque = le tach du Deye revient a son niveau haut interne (repos).
C'est exactement ce que fait le signal tach d'origine, donc le circuit du Deye n'est jamais
attaque en tension : le montage fonctionne que son rappel interne soit a 3,3 V, 5 V ou 12 V.

Deux choix materiels volontaires :

* **Entrees a NPN** (et non diviseur de tension + diode de protection) : le tach d'un Noctua est un
  collecteur ouvert rappelé en interne vers son +12 V (~1 k). Un diviseur vers l'ESP dependrait de
  cette tension et la diode de protection ferait absorber ~9 mA en permanence ; le petit NPN, lui,
  commute proprement quelle que soit la tension de rappel (3,3/5/12 V) avec ~1 mA dans le ventilateur.
* **Sortie a NPN collecteur ouvert** : compatible 3,3/5/12 V sans aucun changement de materiel
  (exigence de l'enonce).

---

## 2. Schema electronique complet

### 2.1 Alimentation (les 2 connecteurs peuvent etre a des tensions differentes, ou non alimentes en meme temps)

```
 DEYE 9 cm                DEYE 6 cm
 ┌──── TACH               ┌──── TACH          (non utilises ici : ils vont aux circuits de signal 2.2/2.3)
 ├──── +12V               ├──── +12V
 └──── GND                └──── GND
      │                        │
      │ 1N5819 (Anode)         │ 1N5819 (Anode)      <-- OR "ET" de source : chaque source
      ▼                        ▼                          peut etre absente, plus faible ou
     ┌──┐                     ┌──┐                        plus elevee sans jamais debiter
     │ 1N5819                 │ 1N5819                    dans l'autre (exigence de l'enonce)
     └──┘                     └──┘
      └───────────┬────────────┘
                  │  (le 12 V le plus haut des deux, ou les deux si identiques)
                  │
        +---------+---------+
        |  100 uF / 25 V    |          (electrolytique + 100 nF ceramique au plus pres)
        +---------+---------+
                  │
          ┌───────┴────────┐
          │ Abaisseur DC-DC│   module "Mini360 / MT3608" regle a 5,0 V (ou LM2596/HLK-PM1205)
          │  entree 4,5-28V│   sortie 5 V >= 1 A, rendement > 85 %
          └───────┬────────┘
                  │
        +---------+---------+
        |  220 uF / 10 V    |          (gros condensateur de sortie : le pic WiFi du ESP8266
        +---------+---------+           atteint 300-400 mA en < 1 ms)
                  │
                  │  USB 5 V
                  ▼
             Wemos D1 mini
```

Regles importantes :

* **Une diode de redressement Schottky par prelevage de 12 V.** C'est elle qui remplit l'exigence
  « les deux connecteurs ne sont pas forcement alimentes en meme temps ni a la meme tension » :
  le rail DC-DC suit la source la plus haute, et aucune source ne recoit de courant de l'autre.
* Le prelevage se fait **en parallele sur le +12 V ventilateurs** (ne pas couper ce 12 V : les
  Noctua en ont besoin pour tourner). Le DC-DC, lui, ne tire que ~25-45 mA (Wemos + 2 Noctua ne sont
  pas alimentes par lui).
* **Masse unique obligatoire** : GND du Wemos, GND des DC-DC, GND des ventilateurs et GND des
  connecteurs du Deye sont tous relie ensemble (le montage collecteur ouvert de sortie en depend ;
  c'est deja le cas dans l'onduleur, il suffit de ne pas le separer).
* Un condensateur de 100 nF ceramique en parallele avec l'entree et la sortie du DC-DC, au plus pres.
* Le DC-DC est un convertisseur a decoupage : il genere du bruit. On l'eloigne des fils tach
  (quelques centimetres suffisent), et les fils tach restent en twisted pair avec leur GND.
* Pas de fusible necessaire en pratique, mais un 500 mA rapide sur le 12 V preleve est un bonus.

### 2.2 Circuit d'entree (identique sur les 2 canaux) — Noctua -> ESP8266

```
   Noeud T = fil tach cote Deye (recoit le tach reel via D5, et le collecteur de Q2 : voir 2.3)
        │
        ├───────────────────┐
        │                   │
       [R1] 10 k           [R0] 10 k vers +12 V ventilateurs   <-- rappel du noeud T (voir note)
        │                   │
        ├─────┐             │
        │     │             │
        │     └─────┐  base │
        │           ├─ C    │        Q1 : 2N2222 / 2N3904 / BC337 / BC547 / S8050 / C1815 / C945
        │           ├─ E ───┴─ (masse)   (tout NPN de votre liste convient ; BC337, S8050 ou 2N2222
        │           B                     preferables pour leur gain)
        │                                 │
        │                                [R3] 10 k vers 3V3
        │                                 │
        │                                 └──────────→ broche ESP : GPIO4 (A) / GPIO5 (B)
```

**Note sur le rappel R0 (essentielle)** : Noctua decrit sa sortie tach comme un collecteur ouvert
« qui ne porte pas de tension et qu'il faut rappeler au niveau voulu » : la valeur (et parfois
l'existence) du rappel interne depend des modeles. On ajoute donc un rappel externe R0 = 10 k vers
le +12 V des ventilateurs. Sil existe deja un rappel interne ailleurs, R0 ne change rien (il se met
en parallele) ; sil ny en a pas, cest lui qui cree les creneaux haut du signal. Le rappel interne du
Deye, sur le meme noeud T, y contribue egalement : le circuit fonctionne donc que la tension de
rappel soit 3,3, 5 ou 12 V. Grace a la diode D5 (2.3), ce noeud T porte le tach reel du Noctua et
**jamais** le signal emis par Q2 : la mesure reste vraie en toutes circonstances.

Fonctionnement et dimensionnement :

| Etat du tach Noctua | Noeud N | Q1 | Broche ESP |
|---|---|---|---|
| impulsion (transistor interne du ventilateur sature) | ~0,2 V | bloque | ~3,3 V (pull-up R3) — niveau **HAUT** |
| repos (collecteur ouvert flottant) | remonte vers +12 V par R0 (10 k) | sature par R1 (~1,1 mA) | ~0,1 V — niveau **BAS** |

* **Le signal lu par l'ESP est inverse** (le firmware en tient compte : la logique globale
  Noctua → Deye reste directe).
* R1 = 10 k vers R0 = 10 k vers 12 V : courant dans la base ≈ (12 - 0,7)/20k ≈ 0,56 mA → Q1 sature
  largement pour tirer le pull-up R3 (0,33 mA) avec un gain de saturation > 50. Courant de sortie
  du capteur tach pendant l'impulsion : seulement 12 V/10k = 1,2 mA (les sorties tach sont
  dimensionnees 20-30 mA).
* Si le rappel est assure par le Deye lui-meme (1 k « dur » a 12 V, cas extreme) : le courant de
  base de Q1 vaut (12-0,7)/(1k+10k) ≈ 1,1 mA, toujours tres au-dessus du necessaire, et le courant
  debite par le tach du ventilateur reste < 12 mA : acceptable pour un capteur tach.
* R2 n'est pas necessaire ici (la base est deja ramenee a la masse par l'emetteur de Q1) ; on
  n'ajoute que R3 = 10 k vers 3V3 : la broche ESP ne voit jamais plus de 3,3 V, quelle que soit la
  tension de rappel. Aucune diode de protection, aucun diviseur dependant de la tension :
  l'exigence 3,3/5/12 V est tenue.
* Cable tach : torsade avec le GND, < 50 cm, a l'ecart des etages de puissance.

**Point cle : le tach reel du Noctua ne doit JAMAIS etre relie directement au meme fil que la sortie
simulee Q2.** Sinon, quand Q2 tire le fil tach a la masse (son impulsion), l'entree de mesure verrait
en permanence une impulsion : le tach reel du Noctua serait court-circuite et la mesure devient fausse.
Deux cablages sont possibles, choisis par le jumper J2 :

* **J2 ouvert (cablage normal, recommande)** : le tach du Noctua n'attaque que le circuit de mesure,
  le fil tach du Deye n'est attaque que par Q2. Mesure et emission parfaitement independantes,
  precision maximale du ratio (jusqu'a x20). Le regle du firmware « Cablage du tach cote onduleur »
  doit etre laisse sur **Normal**.
* **J2 ferme (etat de secours, avec la diode D5 = 1N4148 en serie)** : le tach reel du Noctua est
  egalement relie au Deye au travers de D5 (anode cote ventilateur). Avantage : si le simulateur est
  debranche, eteint ou en panne, le Deye recoit quand meme le tach reel des Noctua (le NPN etant
  bloque, R5 le maintenant bloque au demarrage) — aucune alarme « ventilateur » due a une panne du
  Wemos. Inconvenient : les creneaux du ventilateur et ceux du simulateur se chevauchent sur le fil
  tach ; le firmware le sait et allonge la periode emise pour que chaque impulsion simulate soit vue
  (regle « Ventilateur aussi relie au Deye »), mais le ratio utile est alors plafonne a environ
  x4-x5 et la precision retombe a quelques pour cent.

### 2.3 Circuit de sortie (identique sur les 2 canaux) — ESP8266 -> Deye

```
  tach Noctua ────────┐  (toujours : attaque le circuit de mesure 2.2 via R1)
  (bleu, sortie       │
   tach du capteur)   └─|>|── D5 : 1N4148 ──┤ J2 ├──┐  (montee seulement en mode
                     │   anode cote              (ouvert = cablage normal)   │     secours, J2 ferme)
                     │   ventilateur                                        │
                     ├──────────────────────────────────────────────────────┘
                     │
                     ├──────────► fil TACH vers le connecteur Deye   (= noeud T)
                     │                 │
                     │                [R0] 10 k vers +12 V      (rappel du noeud, voir 2.2)
                     │                 │
                     │                [R1] 10 k → base de Q1    (circuit d'entree 2.2 : en cablage
                     │                 │                          normal il ne voit que le tach reel
                     │                 │                          du Noctua, jamais le signal de Q2)
                     │                 │
                     │            C ───┘   Q2 : BC337 / 2N2222 / 2N5551 / S8050
  Broche ESP          │            │
  GPIO13 (canal A)   R4 4,7k     E ───────► GND commun (celui du Deye)
  GPIO15 (canal B) ────── base   │
                        │     (le collecteur est "en parallele" sur le tach du Deye : il ne fait que
                   R5 10k        │       le tirer a la masse, jamais il n'y envoie de courant)
                   base─┤─ emetteur
                        │
   3V3 (Wemos) ──|>|──┐ │  D3 : 1N5819 (cathode vers 3V3, anode vers la base)
                      └─┘  plafond de securite de la broche ESP
```

Fonctionnement et dimensionnement :

| Etat de la broche ESP | Q2 | Tach du Deye (noeud T) |
|---|---|---|
| HIGH (repos, logique inversee par Q2) | bloque | remonte par le rappel interne du Deye (et/ou R0) ; voit aussi le tach reel du Noctua si J2 est ferme (via D5) |
| LOW (impulsion emulatee) | sature (~0,55 mA de base) | tire a ~0,2-0,3 V au-dessus du GND du Deye |

* **Compatibilite 3,3 / 5 / 12 V** : un collecteur ouvert n'a pas d'avis sur la tension du rail qu'il
  tire. Courant de collecteur a prevoir : 12 V / 1 k (rappel interne le plus « dur » possible d'un
  controleur de ventilateur) = 12 mA → avec 0,55 mA de base et un gain de saturation de 30-100 pour
  les transistors de votre liste (2N2222 : specifie hFE ≥ 30 a Ic = 150 mA), Q2 est sature avec une
  marge de facteur 5 a 20. Si par exception le Deye debitait beaucoup plus qu'un simple rappel,
  un second parallele de Q2 (deux NPN en parallele) ou R4 = 2,2 k le resout.
* **1N5819 entre base et 3V3** : plafond de securite. Si la broche tach du Deye etait animee
  (quelques milliamperes a 12 V), la broche ESP serait maintenue a 3,3 + 0,3 = 3,6 V au lieu de
  recevoir 12 V. Cette diode ne passe jamais de courant en fonctionnement normal.
* **R5 = 10 k base-emetteur** : Q2 reste bloque tant que l'ESP n'a pas initialise la broche
  (pendant le boot, GPIO15/GPIO13/GPIO2 sont flottants) → aucun parasite emis sur le tach du Deye,
  et respect de la contrainte de demarrage de GPIO15 (le 10 k sert de pull-down).
* **Ne jamais relier le collecteur de Q2 au +12 V des ventilateurs.** Le collecteur va
  uniquement sur le fil tach du Deye. C'est ce qui rend la mise en parallele du tach reel possible
  sans conflit : quand le simulateur est arrete, son NPN bloque et ne gene pas un signal emis par
  ailleurs (le tach du Noctua, en mode secours via D5/J2).
* Pas de pull-up ajoute cote Deye : son rappel interne existe deja (c'est lui qui fait monter le
  tach quand le ventilateur ne tire pas).

### 2.4 Cablage du tach : ce qui est relie a quoi, et debranchement de securite

**Cablage par defaut (recommande)** : le tach du Noctua n'attaque **que** le circuit de mesure (2.2),
le fil tach du Deye n'est attaque **que** par le collecteur de Q2 (2.3). D5 et le jumper J2 ne sont
montes que **facultativement** (J2 ouvert = cette position par defaut). Le Deye voit exactement le
signal emis par Q2 — creneaux bas de 2,25 ms, periode = RPM lues / ratio — avec la precision
theorique du pas de 50 µs, et le ratio reglable va jusqu'a x20.

**Position J2 ferme (tach reel du Noctua aussi relie au Deye, via D5)** : etat de secours — si le
simulateur est debranche, eteint ou en panne, le Deye recoit quand meme le tach reel des Noctua.
Dans cette position les creneaux du Noctua et ceux du simulateur se **chevauchent** sur le fil tach :
le Deye ne voit un front montant que la ou les deux sorties sont flottantes a la fois. Le firmware en
tient compte : regle web « **Cablage du tach cote onduleur = Ventilateur aussi relie au Deye** », il
impose

```
periode_emise  >=  periode_du_ventilateur + creneau_emis (2,25 ms)
```

ce qui garantit qu'a la fin de chaque creneau eme le ventilateur est au repos : chaque creneau eme
produit exactement un front vu par le Deye, et aucun front parasite ne s'ajoute. Le ratio utile est
alors plafonne a `periode_ventilateur / (2,25 ms + largeur du creneau du ventilateur)`, soit
environ **x4 a x6** selon la vitesse — largement sufficient (les ratios utiles ici sont x2-x3) mais
le reglage devient moins fin, la periode emise etant plancher par cette regle et non par le ratio.

**Consequence pratique : choisir son cablage et regler le firmware en consequence.** En cas de doute,
laisser J2 ouvert et le regle web sur « Normal » : c'est le plus precis ; le secours n'est alors
assure que par le domino J1 (qui rebranche le tach reel du Noctua) — d'ou l'interet de monter D5+J2.
Les deux dominos restent accessibles sans outil.

1. **J1 = deconnexion du collecteur de Q2** : coupe la simulation. Avec J2 ferme, le Deye voit le tach
   reel des Noctua : c'est l'etat de securite.
2. **J2 = liaison du tach Noctua au Deye** (secours, ou mesure a l'oscilloscope / verification des
   RPM d'origine avec le NMB encore en place via un adaptateur en Y).

Le harnais d'origine (NMB) est a conserver tel quel : il se rebranche a la place des Noctua.
Le mode **BYPASS** du firmware (recopie 1:1 du tach lu, largeur de creneau reproduite comprise) sert a
la verification logicielle des RPM.

### 2.5 LED d'etat

Aucun composant : la **LED integree du Wemos (GPIO2, active LOW)** est pilotee par le firmware.

| Signification | Comportement de la LED |
|---|---|
| Au moins un canal emet un tach simule | clignotement rapide (~2,5 allumages par seconde) |
| Aucun front emis (signal ventilateur absent ou mode OFF) | clignotement lent (1 s allumee / 1 s eteinte) |
| Simulation globalement coupee (interrupteur web) | eclats tres espaces (2 s par cycle) |

### 2.6 Brochage ESP (resume)

| Fonction | Broche ESP | Broche Wemos | Contraintes |
|---|---|---|---|
| Entree tach canal A (9 cm) | GPIO4 | D2 | aucune |
| Entree tach canal B (6 cm) | GPIO5 | D3 | aucune |
| Sortie tach canal A (9 cm) | GPIO13 | D7 | broche SCK de la flash SPI : inutilisee en mode flash normal ; ne pas la partager avec un peripherique SPI |
| Sortie tach canal B (6 cm) | GPIO15 | D8 | broche HS : ne pas y mettre de pull-up externe (le R5 10 k du circuit de sortie sert de pull-down) |
| LED | GPIO2 | D4 | active LOW, pull-up 10 k interne pendant le boot : le R4 4,7 k de Q2 laisse passer ~0,55 mA, le boot se passe bien |

Si vous voulez eviter les broches flash : sortie A → **GPIO1 (TX)** en debranchant le pont USB-TX,
sortie B → **GPIO16 (D0)** (attention : GPIO16 n'a pas de pull-up interne, le R5 10 k du schema fait
l'affaire).

---

## 3. Nomenclature complete

| Ref | Valeur / type | Quantite | Role |
|---|---|---|---|
| Q1, Q3 | NPN petit signal : 2N2222, 2N3904, BC337, BC547, S8050, C1815, C945… | 2 | niveau-shift d'entree (Noctua → ESP) |
| Q2, Q4 | NPN : BC337, 2N2222, 2N5551, S8050, SS8050… | 2 | sortie collecteur ouvert vers le tach du Deye |
| R0, R7 | 10 k | 2 | rappel du noeud tach cote ventilateur (vers +12 V) |
| R1, R6 | 10 k | 2 | resistance de base des entrees |
| R3, R8 | 10 k | 2 | pull-up des entrees vers 3V3 |
| R4, R9 | 4,7 k | 2 | resistance de base des sorties |
| R5, R10 | 10 k | 2 | base-emetteur des sorties (pull-down au boot) |
| D1, D2 | 1N5819 (ou MBRS340, 1N5822) | 2 | OR des deux prelevages 12 V |
| D3, D4 | 1N5819 (ou BAT54, 1N4148 en secours) | 2 | plafond 3,6 V des broches ESP (sorties) |
| C1 | 100 uF 25 V + 100 nF | 1 | filtrage entree DC-DC |
| C2 | 220 uF 10 V + 100 nF | 1 | filtrage sortie 5 V (pics WiFi) |
| U1 | Wemos D1 mini V2.3.0 | 1 | ESP8266 + regulateur 5V→3V3 |
| U2 | Mini360/MT3608 regle a 5,0 V (ou LM2596, HLK-PM1205) | 1 | alimentation depuis le 12 V ventilateurs |
| SW1 (tres conseille) | commutateur 3 positions 2 rangees (SPDT x2) ou 2 dominos | 1 | bypass materiel des tach simules |
| — | Connecteurs 3 broches (JST/PH/Molex selon l'onduleur), fil torsade, cosse | — | harness |

Tout est dans la liste de composants que vous avez indiquee (resistances de toutes valeurs +
les NPN citees), plus 4 diodes, 2 condensateurs et le convertisseur abaisseur.

---

## 4. Cablage, resume des connexions

| Du cote | Vers | Fil |
|---|---|---|
| Connecteur Deye 9 cm (12V) | Noctua NF-A9 #1 et #2 en parallele (+12V) | +12 V |
| Connecteur Deye 9 cm (GND) | GND commun | GND |
| Connecteur Deye 9 cm (TACH) | broche tach du Noctua NF-A9 (via SW1 position secours) **et** collecteur de Q2 (position simulateur) | tach |
| Connecteur Deye 6 cm (12V) | Noctua NF-A6x25 #1 et #2 en parallele | +12 V |
| Connecteur Deye 6 cm (TACH) | tach des NF-A6x25 en parallele + collecteur de Q4 | tach |
| +12 V de chaque connecteur | anode de D1 / D2 → entree du Mini360 | prelevage d'alimentation (paralleles, pas de coupure) |
| Sortie Mini360 5 V | Vpin (5 V) du Wemos | alimentation |
| tach Noctua 9 cm | noeud N = tach Noctua + tach Deye + R0(10k→+12V) + R1(10k)→base Q1 ; collecteur Q1 + R3(10k→3V3) → GPIO4 | entree A |
| tach Noctua 6 cm | idem avec R7/R6/Q3/R8 → GPIO5 | entree B |
| GPIO13 | R4(4,7k) → base de Q2 (R5 10k base-emetteur, D3 base→3V3) ; collecteur Q2 → tach Deye 9 cm | sortie A |
| GPIO15 | R9 → base de Q4 (R10, D4 idem) ; collecteur Q4 → tach Deye 6 cm | sortie B |

Sur les deux canaux : les tach de plusieurs ventilateurs en parallele **ne se branchent qu'avec le
commutateur en position secours** (ils sont en collecteur ouvert, donc aucun conflit). En position
simulateur, un seul tach de Noctua par canal est utilise pour la mesure (les autres restent
debranches ou relies a la masse).

---

## 5. Compilation et televersement

1. Arduino IDE 2.x + paquet **esp8266** (menu Tableau des cartes → Gestionnaire de paquets → "esp8266" par ESP8266 Community).
2. Carte : **LOLIN(WEMOS) D1 R2 & mini** ; Flash Size : **4M (1M LittleFS)** ou « 4M and 1M LittleFS » ; CPU : **80 MHz** ; lwIP : **V2 HH**.
3. Ouvrir `deye_fan/deye_fan.ino` (le dossier s'appelle comme le sketch : obligatoire).
4. Brancher le Wemos en USB, **toutes les broches tach deconnectees du Deye pendant le televersement**.
5. Compiler / televerser. Serial 115200 pour voir les messages de demarrage.

---

## 6. Mise en service et reglages (sans recompiler)

1. Alimentation : le Wemos demarre des que le rail 12 V ventilateurs est present.
2. **Point d'acces** : `DeyeFan-Config` (ouverte par defaut → mettez une cle dans l'onglet WiFi).
   Passerelle de l'AP : **19.0.0.1**. Ou, si vous avez configure le client WiFi, **http://deye-fan.local**.
3. **Etape de calibration** : commuter le canal en position 1 (2.4) et le mettre en mode **BYPASS** :
   l'entree lit le tach reel du ventilateur en place et la sortie le re-emet a l'identique.
   Avec le ventilateur **d'origine (NMB)** encore monte (via l'adaptateur en Y), noter les « RPM lues »
   ; avec les Noctua montes, relever leurs « RPM lues ». Consigner les 4 valeurs.
4. Passer en mode **SIMULER** et regler le ratio pour que « RPM simules » ≈ RPM d'origine (a ±10 %).
   Valeurs indicatives (verifiez sur votre machine) :
   * NF-A9 FLX ≈ 1450 tr/min (le raccord anti-bruit NAEN le bride a ~1050 : **retirez-le**),
   * NF-A6x25 FLX ≈ 1800 tr/min,
   * NMB 09225VE ≈ 2600-3300 tr/min → ratio ≈ **x1,8 - x2,3**,
   * NMB 06025VE ≈ 4500-5300 tr/min → ratio ≈ **x2,5 - x3,0**.
   Le ratio est enregistre en flash immediatement : il survit aux coupures d'alimentation.
5. Boutons utilles : `OFF` / `SIMULER` / `BYPASS` par canal, `Sortie ACTIVE/OFF` par canal,
   `Simulation globale ON/OFF` (coupe tout, l'onduleur voit son niveau haut = 0 RPM),
   `Redemarrer`, champ `Impulsions/tour` (le tach Noctua delivre **2 impulsions par tour**).
6. Verifier la LED : elle doit clignoter vite des que la simulation emet.

---

## 7. Pourquoi le signal ne peut pas etre perturbe par le logiciel

| Couche | Ce qui se passe | Effet sur le tach |
|---|---|---|
| Timer1 materiel du ESP8266 | compteur libre, prescaler 16 MHz, re-arme a l'entree de l'ISR (800 cycles = exactement 50 us) | periode du tick sans derive |
| ISR (`IRAM_ATTR`, executee depuis la RAM rapide) | echantillonne les entrees, mesure les periodes (median de 8 + moyenne glissante), emet les fronts des 2 sorties. Aucun appel SDK, aucun flottant, aucun acces flash | un front ne peut bouger que de ±1 tick = **±50 us** |
| Boucle Arduino (WiFi, serveur HTTP, JSON, page web) | ne lit/ecrit que des variables 32 bits et ne fait que recalculer des periodes cibles | **aucun** : elle ne fabrique aucun front |
| Ecritures flash (sauvegarde de la config) | deplacees dans la boucle, jamais dans l'ISR ; le core les execute depuis l'IRAM sans couper les interruptions | **aucun** |
| WiFi | `setSleepMode(WIFI_NONE_SLEEP)` : aucune phase de calibration RF bloquante | **aucun** (le timer1 est materiel) |

Resolution de la periode de sortie : 50 us sur des periodes de 3 a 80 ms → erreur de periode
inferieure a 1,7 % dans le pire cas (ventilateur tres rapide), et 0,1 a 0,3 % aux regimes usuels.
Le jitter cumule entree+sortie reste < 100 us. Un controleur de ventilateur moyenne le tach sur
plusieurs revolutions : c'est tres en dessous de son seuil de detection.

**Comment le verifier vous-meme** : oscilloscope sur le collecteur de Q2, mesure de la periode ;
lancer en parallele plusieurs dizaines de requetes web (rafraichir la page en continu, changer le
ratio). La periode doit rester identique au tick pres. Le firmware tient aussi un controle de vie
du timer1 (message « ANOMALIE timer1 » sur le port serie si le nombre de ticks devient incoherent).

Garde-fous prevus dans le code :

* periode de sortie minimale 2 ms (pas d'emitation irrealiste meme avec ratio x20),
* signal « trappe » : 8 periodes < 300 ms (200 tr/min equivalent) consecutives → sortie du canal
  coupee, defaut affiche sur la page (reglable),
* perte de signal > 1 s : le dernier RPM connu continue d'etre emis (volontaire : un ventilateur
  arrete doit rester visible par l'onduleur, pas disparaitre),
* sortie forcee a l'etat repos (NPN bloque) des la mise sous tension : les broches de sortie sont
  cablées en pull-down par R5/R10, donc rien n'est emis avant que le programme ne le demande.

---

## 8. Securite

* Travail **hors tension**, onduleur eteigne (DC et AC), condensateurs defiltrage decharges. Le circuit
  ne touche **pas** aux tensions de puissance : seul le circuit 12 V des ventilateurs est atteint.
* Ouvrir l'onduleur peut annuler la garantie. Fixez le Wemos a l'exterieur du boitier de puissance
  (boite etanche a cote) et faites entrer uniquement les 6 fils (2 x {12V, GND, tach}) par un passe-fil.
* Ne jamais ponter le tach avec le +12 V : le tach est une entree du controleur.
* Le commutateur de bypass (2.4) est la protection contre une panne du simulateur : a cabler en priorite.
* Les Noctua doubles (2 par emplacement) doivent etre branches en parallele electriquement sur le
  meme 12 V : verifier que le connecteur du Deye supporte le courant cumule (~0,4 A au total pour
  4 Noctua, les NMB d'origine tiraient plus : c'est dans les clous).
