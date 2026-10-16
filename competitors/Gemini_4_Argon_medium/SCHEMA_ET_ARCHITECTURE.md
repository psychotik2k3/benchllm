# Simulateur de Tachymètre (RPM) Noctua pour Onduleur Deye SUN-8K-SG05LP1-EU-AM2-P

Ce dossier contient :
1. **`deye_fan.ino`** : Le firmware complet, commenté et prêt à compiler pour **LOLIN(WEMOS) D1 mini** sous Arduino IDE.
2. **Le dossier technique et les schémas électroniques complets** ci-dessous, utilisant **exclusivement** le Wemos D1 Mini V2.3.0, des résistances et les transistors NPN de la liste fournie.

---

## 1. Analyse des ventilateurs (Datasheets NMB vs Noctua)

| Canal | Ventilateur d'origine (Deye) | Vitesse / Courant d'origine | Remplacement (×2 en parallèle) | Vitesse / Courant Noctua | Ratio nominal à appliquer |
| :--- | :--- | :--- | :--- | :--- | :---: |
| **Canal 1 (60 mm)** | **NMB 06025VE-12N-CL** (12 V, 3 fils, collecteur ouvert) | **7 000 RPM** (233,3 Hz) — 0,22 A | 2× **Noctua NF-A6x25-FLX** (12 V, 3 fils) | **3 000 RPM** (100,0 Hz) — 2× 0,08 A = 0,16 A | **× 2,33** *(ou × 2,92 avec L.N.A 2400 RPM)* |
| **Canal 2 (92 mm)** | **NMB 09225VE-12N-CU** (12 V, 3 fils, collecteur ouvert) | **5 600 RPM** (186,7 Hz) — 0,72 A | 2× **Noctua NF-A9-FLX** (12 V, 3 fils) | **1 600 RPM** (53,3 Hz) — 2× 0,07 A = 0,14 A | **× 3,50** *(ou × 4,48 avec L.N.A 1250 RPM)* |

> **Note importante sur le doublage des ventilateurs** :
> Sur chaque canal, les deux ventilateurs Noctua ont leurs fils **Noir (`GND`)** et **Rouge (`+12V`)** câblés en parallèle sur le connecteur de l'onduleur, mais **un seul des deux ventilateurs Noctua** de chaque paire a son fil **Jaune (`TACH`)** relié à l'entrée du Wemos. Le fil jaune du second ventilateur de la paire doit rester **non connecté (isolé)** : il ne faut jamais relier deux sorties tachymétriques collecteur ouvert ensemble, sous peine de créer des battements de phase qui fausseraient la lecture.

---

## 2. Schémas Électroniques Complets

Tous les circuits ci-dessous respectent strictement l'inventaire imposé : **uniquement des résistances et des transistors NPN de la liste** (`BC337`, `2N2222`, `BC547` / `2N3904`).

### 2.1. Alimentation Double Source Auto-Isolée (OR-ing Actif + Pré-Régulateur 4,4 V)

#### Problématique matérielle à résoudre
1. Les deux connecteurs ventilateur de l'onduleur (`+12V_CH1` et `+12V_CH2`) peuvent être alimentés indépendamment (l'un à 12 V et l'autre à 0 V) ou à des tensions différentes (ex. régulation linéaire/PWM entre 7 V et 13,5 V). Il est interdit de les relier directement sous peine de réinjecter du courant dans le canal éteint.
2. La broche `5V` du **Wemos D1 Mini V2.3.0** attaque son régulateur LDO 3,3 V interne (`RT9013-33` / `ME6211C33`) dont la tension d'entrée maximale absolue est de **6,0 V** (et qui régule parfaitement dès **3,6 V** en entrée).
3. En l'absence de diodes discrètes ou de diode Zener dans la liste de composants, nous réalisons pour chaque voie :
   - **Une référence de tension active par régulateur shunt à $V_{BE}$ multiplié (`Q_REG1`, `Q_REG2` — `BC547` ou `2N3904`)** : le diviseur $R_{1}/R_{2}$ ($6,8\text{ k}\Omega / 1\text{ k}\Omega$) asservit la base du transistor ballast à :
     $$V_{\text{BASE}} = V_{BE(\text{Q\_REG})} \times \left(1 + \frac{6,8\text{ k}\Omega}{1,0\text{ k}\Omega}\right) \approx 0,65\text{ V} \times 7,8 \approx 5,07\text{ V}$$
     Quelle que soit la tension du connecteur ventilateur entre **7 V et 14 V**, la base reste verrouillée à **5,07 V**.
   - **Un étage ballast suiveur (émetteur suiveur) en configuration OR-ing actif (`Q_PWR1`, `Q_PWR2` — `BC337`)** :
     Les émetteurs de `Q_PWR1` et `Q_PWR2` sont reliés ensemble sur la broche **`5V`** du Wemos :
     $$V_{\text{5V\_WEMOS}} = V_{\text{BASE}} - V_{BE(\text{Q\_PWR})} \approx 5,07\text{ V} - 0,67\text{ V} = 4,40\text{ V}$$
   - **Protection contre le courant inverse (OR-ing sans diode)** :
     Si le Canal 1 est à 12 V ($V_{\text{5V\_WEMOS}} = 4,40\text{ V}$) et le Canal 2 est à 0 V :
     Sur `Q_PWR2` (`BC337`), le collecteur est à 0 V, la base est à 0 V, et l'émetteur est à $+4,40\text{ V}$.
     - Aucun courant ne peut remonter de l'émetteur vers le collecteur (la jonction Base-Collecteur est à $0\text{ V}$).
     - La tension inverse Émetteur-Base est de $4,40\text{ V}$, ce qui reste inférieur à la limite $V_{EBO(\max)} = 5,0\text{ V}$ du `BC337` (ou $6,0\text{ V}$ pour `2N2222`/`2N5551`), et même en cas de micro-courant de fuite, $R_{\text{pull2}}$ ($1,5\text{ k}\Omega$) + $R_{1B}$ ($6,8\text{ k}\Omega$) empêchent toute réinjection vers le connecteur arrêté.
   - **Répartition thermique ($R_{\text{drop}} = 33\,\Omega$)** :
     Une résistance série de $33\,\Omega$ (réalisée en mettant **3 résistances de $100\,\Omega$ 1/4 W en parallèle**) en amont du collecteur de chaque `BC337` absorbe environ $2,5\text{ V}$ ($185\text{ mW}$) sous les $75\text{ mA}$ moyens de l'ESP8266, limitant la dissipation du `BC337` à seulement $380\text{ mW}$ (bien en dessous de ses $625\text{ mW}$ max) tout en laissant assez de marge lors des pics TX WiFi de $170\text{ mA}$.

#### Schéma de l'alimentation (Connecteurs +12V_CH1 et +12V_CH2 $\to$ Wemos 5V)

```text
  CONNECTEUR DEYE 1 (60 mm)
  +12V_CH1 o----+-----> Vers fil Rouge (+12V) des 2× Noctua NF-A6x25-FLX
                |
                +---[ R_drop1 : 33 Ω (3× 100 Ω //) ]---+
                |                                      |
                |                                  [Collecteur]
                +---[ R_pull1 : 1.5 kΩ ]---+-----> [Base]   Q_PWR1 (BC337)
                                           |       [Émetteur]
                                           |           |
                                           |           +-------------------+===> Broche [5V]
                                   [Collecteur]        |                   |     Wemos D1 Mini
                       Q_REG1      [Base]--+           |                   |     (Régulé ~4.4V)
                     (BC547/2N3904)[Émetteur]  [ R_1A : 6.8 kΩ ]           |
                                       |       |       |                   |
                                       |       +-------+                   |
                                       |       |                           |
                                       |   [ R_2A : 1.0 kΩ ]               |
                                       |       |                           |
                                      ===     ===                          |
                                      GND     GND                          |
                                                                           |
  CONNECTEUR DEYE 2 (92 mm)                                                |
  +12V_CH2 o----+-----> Vers fil Rouge (+12V) des 2× Noctua NF-A9-FLX      |
                |                                                          |
                +---[ R_drop2 : 33 Ω (3× 100 Ω //) ]---+                   |
                |                                      |                   |
                |                                  [Collecteur]            |
                +---[ R_pull2 : 1.5 kΩ ]---+-----> [Base]   Q_PWR2 (BC337) |
                                           |       [Émetteur]              |
                                           |           |                   |
                                           |           +-------------------+
                                   [Collecteur]        |
                       Q_REG2      [Base]--+           |
                     (BC547/2N3904)[Émetteur]  [ R_1B : 6.8 kΩ ]
                                       |       |       |
                                       |       +-------+
                                       |       |
                                       |   [ R_2B : 1.0 kΩ ]
                                       |       |
                                      ===     ===
                                      GND     GND

  MASSE COMMUNE OBLIGATOIRE :
  GND_CH1 (Deye 60mm) ---+--- GND Noctua 60mm (×2)
                         +--- GND Noctua 92mm (×2)
  GND_CH2 (Deye 92mm) ---+--- Broche [G] (GND) du Wemos D1 Mini
```

---

### 2.2. Circuits d'Entrée Tachymètre (Noctua $\to$ ESP8266)

Les ventilateurs Noctua possèdent une sortie tachymétrique à **collecteur ouvert** (mise à la masse 2 fois par tour).
- On tire le signal au **`3V3`** du Wemos via une résistance de pull-up de **$4,7\text{ k}\Omega$** ($I_{\text{sink}} \approx 0,7\text{ mA}$, très inférieur aux $5\text{ mA}$ max du capteur Hall Noctua).
- On insère une résistance série de protection **$R_{\text{prot}} = 2,2\text{ k}\Omega$** avant la broche GPIO (`D5` ou `D6`) afin de protéger l'entrée de l'ESP8266 contre tout transitoire électromagnétique ou erreur de câblage.

```text
  ================================================================================
  CANAL 1 (60 mm — Noctua NF-A6x25-FLX n°1) :
  ================================================================================
                        Wemos [3V3]
                             |
                      [ R_pu1 : 4.7 kΩ ]
                             |
  Fil Jaune (Pin 3) ---------+---[ R_in1 : 2.2 kΩ ]---> Wemos [D5] (GPIO14)
  Noctua 60mm #1
  (Note : Fil Jaune du Noctua 60mm #2 laissé NON CONNECTÉ / isolé)

  ================================================================================
  CANAL 2 (92 mm — Noctua NF-A9-FLX n°1) :
  ================================================================================
                        Wemos [3V3]
                             |
                      [ R_pu2 : 4.7 kΩ ]
                             |
  Fil Jaune (Pin 3) ---------+---[ R_in2 : 2.2 kΩ ]---> Wemos [D6] (GPIO12)
  Noctua 92mm #1
  (Note : Fil Jaune du Noctua 92mm #2 laissé NON CONNECTÉ / isolé)
```

---

### 2.3. Circuits de Sortie Tachymètre Universels 3,3 V / 5 V / 12 V (ESP8266 $\to$ Onduleur Deye)

#### Pourquoi l'étage Collecteur Ouvert NPN fonctionne automatiquement en 3,3 V, 5 V et 12 V ?
D'après les datasheets NMB (`06025VE-12N-CL` et `09225VE-12N-CU`), la sortie tachymètre d'origine est un **transistor NPN à collecteur ouvert** ($V_{CE(\max)} = 15\text{ V}$, $I_{C(\max)} = 5\text{ mA}$). L'onduleur Deye possède donc sa propre résistance de tirage (pull-up) interne vers sa tension logique interne ($3,3\text{ V}$, $5\text{ V}$ ou $12\text{ V}$).

En utilisant un transistor NPN **`2N2222`** (ou `BC547` / `2N3904` / `BC337`, tous supportant $V_{CEO} \ge 30\text{ V}$ à $45\text{ V}$) monté en **collecteur ouvert** :
1. **Lorsque la sortie du Wemos est à `LOW` ($0\text{ V}$)** : le transistor `Q_OUT` est bloqué ($I_C = 0$). La ligne `TACH` de l'onduleur remonte naturellement à la tension exacte de la pull-up interne de l'onduleur (**qu'elle soit de 3,3 V, 5 V ou 12 V**), sans aucune réinjection de tension vers l'ESP8266.
2. **Lorsque la sortie du Wemos est à `HIGH` ($3,3\text{ V}$)** : la résistance de base $R_B = 4,7\text{ k}\Omega$ injecte $I_B = \frac{3,3 - 0,7}{4,7\text{ k}\Omega} \approx 0,55\text{ mA}$, ce qui sature franchement le transistor NPN ($V_{CE(\text{sat})} < 0,1\text{ V}$ jusqu'à $15\text{ mA}$). La résistance série de protection $R_{C} = 100\,\Omega$ limite les appels de courant capacitif tout en garantissant un niveau bas $< 0,25\text{ V}$ parfaitement reconnu en 3,3 V, 5 V et 12 V.
3. **Au démarrage / reset de l'ESP8266** : la résistance de rappel à la masse $R_{BE} = 47\text{ k}\Omega$ maintient `Q_OUT` bloqué tant que les GPIO sont en haute impédance.

```text
  ================================================================================
  SORTIE SIMULÉE CANAL 1 (Vers Entrée TACH Connecteur 60 mm de l'onduleur Deye) :
  ================================================================================

                                      +---[ R_c1 : 100 Ω ]---> Broche TACH_CH1
                                      |                        Onduleur Deye (60 mm)
                                 [Collecteur]                  (Pull-up interne Deye :
  Wemos [D1] ---[ R_b1 : 4.7 kΩ ]---+-[Base]   Q_OUT1           3.3V, 5V ou 12V)
   (GPIO5)                          | [Émetteur] (2N2222 / BC547)
                                    |     |
                         [ R_be1 : 47 kΩ ]|
                                    |     |
                                   ===   ===
                                   GND   GND

  ================================================================================
  SORTIE SIMULÉE CANAL 2 (Vers Entrée TACH Connecteur 92 mm de l'onduleur Deye) :
  ================================================================================

                                      +---[ R_c2 : 100 Ω ]---> Broche TACH_CH2
                                      |                        Onduleur Deye (92 mm)
                                 [Collecteur]                  (Pull-up interne Deye :
  Wemos [D2] ---[ R_b2 : 4.7 kΩ ]---+-[Base]   Q_OUT2           3.3V, 5V ou 12V)
   (GPIO4)                          | [Émetteur] (2N2222 / BC547)
                                    |     |
                         [ R_be2 : 47 kΩ ]|
                                    |     |
                                   ===   ===
                                   GND   ===
```

---

## 3. Nomenclature Complète des Composants (BOM)

| Réf. Schéma | Quantité | Composant / Valeur | Rôle |
| :--- | :---: | :--- | :--- |
| **U1** | 1 | **Wemos D1 Mini V2.3.0** | Microcontrôleur ESP8266 (ESP-12S) + LED d'état intégrée (`D4`) |
| **Q_PWR1, Q_PWR2** | 2 | **BC337** *(ou 2N2222 / S8050)* | Transistors NPN ballast et isolation OR-ing d'alimentation ($I_C = 800\text{ mA}$) |
| **Q_REG1, Q_REG2** | 2 | **BC547** *(ou 2N3904 / C1815)* | Transistors NPN de régulation shunt ($V_{\text{base}} \approx 5,07\text{ V}$) |
| **Q_OUT1, Q_OUT2** | 2 | **2N2222** *(ou BC547 / 2N3904)* | Transistors NPN de sortie collecteur ouvert vers l'onduleur Deye |
| **R_drop1, R_drop2** | 2× (3× 100 Ω) | **33 Ω** *(3× 100 Ω 1/4W en //)* | Délestage thermique et limitation de courant d'alimentation |
| **R_pull1, R_pull2** | 2 | **1,5 kΩ** (1/4 W) | Polarisation des collecteurs de régulation `Q_REG1` / `Q_REG2` |
| **R_1A, R_1B** | 2 | **6,8 kΩ** (1/4 W) | Pont diviseur haut de référence shunt ($5,07\text{ V}$) |
| **R_2A, R_2B** | 2 | **1,0 kΩ** (1/4 W) | Pont diviseur bas de référence shunt ($0,65\text{ V}$) |
| **R_pu1, R_pu2** | 2 | **4,7 kΩ** (1/4 W) | Pull-up au `3V3` des signaux tachymètres Noctua |
| **R_in1, R_in2** | 2 | **2,2 kΩ** (1/4 W) | Protection série des entrées GPIO `D5` et `D6` |
| **R_b1, R_b2** | 2 | **4,7 kΩ** (1/4 W) | Limitation de courant de base de `Q_OUT1` et `Q_OUT2` |
| **R_be1, R_be2** | 2 | **47 kΩ** (1/4 W) | Rappel à la masse des bases au démarrage (anti-glitch boot) |
| **R_c1, R_c2** | 2 | **100 Ω** (1/4 W) | Protection série des collecteurs de sortie vers Deye |
