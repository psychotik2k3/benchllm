# Schéma Électronique — Simulateur Tachymètre Ventilateurs Deye (ESP8266)

## 1. Vue d'ensemble du système

```
┌─────────────────────────────────────────────────────────────────────┐
│                    SIMULATEUR TACH Deye — ESP8266                     │
│                                                                     │
│  ┌──────────┐    ┌──────────────────────┐    ┌──────────┐          │
│  │ FAN CONN │───▶│   POWER SECTION       │───▶│ WEMOS    │          │
│  │ 12V A    │    │ Schottky OR-ing →     │    │ D1 MINI  │          │
│  └──────────┘    │ Buck 12V→5V + découp.│    │ (ESP8266)│          │
│  ┌──────────┐    └──────────────────────┘    └─────┬────┘          │
│  │ FAN CONN │                                       │              │
│  │ 12V B    │                                       ▼              │
│  └──────────┘                           ┌─────────────────┐        │
│                                          │   ESP8266       │        │
│  Noctua Tach IN A ────── D6/GPIO12 ─────▶│  Input Capture  │        │
│  Noctua Tach IN B ────── D7/GPIO13 ─────▶│  (ISR FALLING)  │        │
│                                          │                 │        │
│  SIM-TACH OUT A ◀────── D2/GPIO4 ───────◀│  Scheduler      │        │
│  SIM-TACH OUT B ◀────── D5/GPIO14 ──────◀│  (Timer1 @20kHz)│        │
│                                          │                 │        │
│  STATUS LED   ◀────── D4/GPIO2 ──────────▶│  (loop() only)  │        │
│                                          └─────────────────┘        │
│                                                                     │
│                    ↓ NPN Transistors ↓                              │
│          SIM-TACH → INVERTER TACH A   (collecteur ouvert)           │
│          SIM-TACH → INVERTER TACH B   (collecteur ouvert)           │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 2. Liste des composants (BOM)

| Réf. | Composant                  | Valeur / Référence    | Qty | Rôle                           |
|------|----------------------------|-----------------------|-----|--------------------------------|
| U1   | Module microcontrôleur     | LOLIN(WEMOS) D1 mini  | 1   | Cerveau du système (ESP8266)   |
| D1,D2| Diode Schottky OR-ing      | BAT54 / SS14          | 2   | Alimentation OR-ing ventilateurs|
| C1   | Condensateur réservoir     | 1000µF / 16V (électro.) | 1 | Tampon courant bascule alim    |
| U2   | Convertisseur buck         | MT3608 module 12V→5V  | 1   | Régulation DC/DC高效           |
| C2   | Découplage local ESP8266   | 100nF céramique       | 1   | Filtrage transitoires WiFi     |
| C3   | Découplage local ESP8266   | 220µF / 6.3V (tantale) | 1  | Tampon appels courant WiFi     |
| R1,R2| Pull-up entrée tach        | 4.7kΩ / 0.1W          | 2   | Tirage vers 3.3V Noctua        |
| C4,C5| Filtre RC entrée tach      | 1nF / 50V céramique   | 2   | Suppression bruit HF           |
| Q1,Q2| Transistor NPN sortie      | BC547                 | 2   | Étage collecteur ouvert        |
| R3,R4| Résistance de base NPN     | 10kΩ / 0.1W           | 2   | Limitation courant base Q      |
| D_LED| LED indicateur statut      | Rouge / vert          | 1   | Indicateur visuel              |
| R5   | Résistance série LED       | 470Ω / 0.1W           | 1   | Limitation courant LED         |

### Composants ajoutés (hors liste ventilateur d'origine)
- **Diodes Schottky D1,D2** : indispensables pour l'OR-ing d'alimentation, car les deux connecteurs ventilateurs ne sont pas garantis actifs simultanément ni à la même tension. Sans ces diodes, un connecteur alimenté pourrait injecter du courant dans le connecteur éteint → court-circuit / destruction.
- **Module buck MT3608** : indispensable car l'AMS1117 embarqué sur le Wemos ne peut pas dissiper 12V→3.3V (≈9.7W de dissipation théorique avec les pointes WiFi). Le buck externalise cette conversion avec un rendement >85%, limitant la dissipation à <0.5W.
- **Condensateur réservoir C1** : tamponne la bascule d'un connecteur ventilateur à l'autre (si l'un coupe brièvement avant que l'autre prenne le relais).
- **Découplage C2+C3 près de l'ESP8266** : absorption des pointes de courant transitoires du WiFi (jusqu'à 300-400 mA en émission), problème connu qui provoque des brown-outs et resets sur ESP8266.

---

## 3. Section Alimentation

```
                          ┌──────────────┐
                          │              │
       FAN-CONN-A (+12V) ──┤             ├───┐     BAT54 (D1)     ┌──────────────┐
                          │              │   ──|<|----------------─┤ VBUS_COMMON  ├────┐
       FAN-CONN-B (+12V) ──┤              └────────────────────────┤              │    │
                          │                                         │ RESERVOIR    │    │
       GND_A   ──────────┼───┐                                       │  C1          │    │
       GND_B   ──────────┼───┤   GND communs (à court-circuiter)     │ 1000µF/16V   │    │
                          └───┘                                       │              │    │
                                                                     └──────┬───────┘    │
                                                                            │           │
                                                                   +12V bus  │       GND
                                                                            ▼           │
                                                                      ┌───────────┐     │
                                                                      │  MT3608   │◀────┘
                                                                      │  BUCK     │
                                                                  +5V ◄─│  12V→5V   │
                                                                      └─────┬─────┘
                                                                            │
                                                                   +5V WEMOS │
                                                                            ▼
                                                                      ┌───────────┐
                                              C2 (100nF)       ─────▶│   5V      │
                                              C3 (220µT)             │           │◀── Alimentation
                                                                        │ ESP8266   │    WEMOS D1 mini
                                              GND ────────────────────▶│ VDD/3V3   │    (broche 5V,
                                                                        │           │    pas VIN!)
                                                                      └───────────┘
```

### Calculs de la section alimentation

**a) Diodes Schottky OR-ing (BAT54 / SS14)**

Les deux connecteurs ventilateurs peuvent avoir des tensions différentes :
- Si FAN_A = 12V et FAN_B = 0V (déconnecté), le courant ne doit pas circuler de A vers B.
- Diode Schottky BF54 : V_f ≈ 0.3V à 100mA (très basse chute)
- Si on utilisait une diode silicium classique (1N4001, V_f ≈ 0.7V), la chute serait moindre mais toujours acceptable pour un buck qui supporte jusqu'à ~15V d'entrée.

**Justification de l'ajout des diodes Schottky :**
> Les connecteurs ventilateurs de l'onduleur sont alimentés depuis le bus interne de l'onduleur. Il est possible (et documenté sur certains modèles Deye) qu'un seul connecteur soit actif à un instant donné, ou que leurs tensions fluctuent légèrement selon la charge thermique. Les diodes Schottky en OR-ing évitent tout courant de retour d'un connecteur vers l'autre et garantissent une tension unique au noeud commun.

**b) Condensateur réservoir C1 (1000µF / 16V)**

Dimensionnement pour absorber un basculement d'un connecteur à l'autre :
- Courant moyen consommé par ESP8266 ≈ 150 mA en repos, jusqu'à ~400 mA en émission WiFi
- Si le connecteur actif coupe pendant ~10ms (le temps que l'autre prenne le relais) :

```
ΔV = I × Δt / C = 0.4A × 0.01s / 0.001F = 4V
```

Avec un buck MT3608 supportant une entrée de ~7V à 28V, une chute de 4V (de 12V à 8V) est acceptable. Si on souhaite minimiser la chute :

```
C_ideal = I × Δt / ΔV_max = 0.4A × 0.01s / 2V = 2000µF
```

On choisit **1000µF** comme compromis taille/coût/répartition acceptable (chute de ~4V tolérable par le buck). Si on veut une chute <2V, utiliser **2200µF / 16V**.

**c) Convertisseur buck MT3608 (12V → 5V)**

- Entrée : 7V à 28V (satisfait la plage après diodes + réservoir)
- Sortie : réglable de 1.25V à 35V, on règle sur **5V**
- Courant max : 1.2A (suffisant pour ESP8266 qui tire ≤400mA)
- Rendement : >85%
- Dissipation au lieu de l'AMS1117 :
  - AMS1117 en direct 12V→3.3V : P = (12-3.3) × 0.3A ≈ **2.6W** → surchauffe garantie
  - Buck 12V→5V puis 5V→3.3V interne : P_buck + P_ams1117
    ≈ 0 (rendement 90%) + (5-3.3) × 0.3A ≈ **0.5W** → acceptable

**d) Découplage local C2 (100nF céramique) + C3 (220µF tantale)**

Problème bien connu de l'ESP8266 : en émission WiFi, le courant passe brutalement de ~20mA à ~350mA. Sans découplage adéquat, la chute de tension sur les traces d'alimentation provoque un brown-out et un reset.

- C2 (100nF) : absorbe les transitoires ultra-rapides (<1µs), placé le plus près possible des pins VDD/3V3
- C3 (220µF tantale) : tamponne les appels de courant plus longs (1-10ms), juste derrière C2

**e) Injection sur la broche 5V du Wemos (PAS VIN)**

Le Wemos D1 mini dispose d'une broche **5V** (alimentée directement par le jack/USB). Si on alimente cette broche en 5V externes :
- On bypass complètement l'AMS1117 interne → ZÉRO dissipation sur le régulateur
- Le régulateur interne 5V→3.3V travaille avec une chute de seulement 1.7V (très faible dissipation)

**Ne PAS brancher sur la broche VIN/RAW du Wemos**, car celle-ci va directement à l'AMS1117 qui serait alors soumis aux mêmes problèmes de surchauffe qu'évoqués en (c).

---

## 4. Étage d'entrée — Lecture tach Noctua → ESP8266

```
       CONNECTEUR NOCTUA TACH         ┌─────────┐
       (collecteur ouvert, open-drain)│         │
              │                       │ Pull-up  │    ┌──────────┐
      GND_Noq──┤                        │ R1=4.7kΩ ├───▶│ 3.3V rail │
              │          +───────┐     │ vers      │    └──────────┘
      Tach_Out──┼─────────┤│      │     │ 3.3V ESP  │
               C4 │ 1nF     │     └─────────────────┤            │
                     céramique                        │            │
                                                     │   NOSCH_A  │
              ───────────────────────────────────────▶│ D6/GPIO12  │
                                                      └────────────┘
                           (FALLING interrupt)

       Même circuit pour canal B : R2=4.7kΩ, C5=1nF → D7/GPIO13
```

### Calculs de l'entrée tach

**a) Résistance de pull-up R1/R2 (4.7kΩ)**

Le fil Tach du Noctua est en **collecteur ouvert** : le transistor NPN interne du ventilateur tire la ligne vers la masse lors d'une impulsion, et ne l'alimente jamais activement. Il faut donc un pull-up externe pour avoir un niveau haut.

- **Direction vers 3.3V ESP (jamais 12V)** : Noctua spécifie explicitement ≤5.25V sur la broche tach. Le 3.3V de l'ESP8266 est parfaitement en dessous.
- Valeur choisie : **4.7kΩ** (compromis entre courant de fuite et immunité au bruit)

```
Courant lors d'une impulsion LOW : I = 3.3V / 4700Ω ≈ 0.7mA
Puissance dissipée dans la pull-up : P = V²/R = 3.3²/4700 ≈ 2.3mW → résistor 1/8W (0.1W) suffisant
```

**Pourquoi pas 10kΩ ?** Plus grand = moins de courant mais front montant plus lent (RC formé par la résistance + capacité parasite de l'entrée ESP). 4.7kΩ donne un temps de montée :
- C_parasite ≈ 5pF (ESP8266) + C4(1nF) ≈ 1nF
- τ = R × C = 4700 × 1nF = 4.7µs → front montant < 3τ = 14µs, bien inférieur à la période minimale du signal (~12ms à ~80Hz)

**b) Condensateur de filtrage C4/C5 (1nF)**

Absorbe les rebonds et le bruit HF. Dimensionné pour que la constante de temps RC reste largement inférieure à la période minimale :

```
Fréquence max attendue sur l'entrée tach Noctua :
  NF-A9-FLX : ~2000 tr/min → 2 pulses/tour → 4000 pulses/min = 67 Hz
  NF-A6x25-FLX : ~3000 tr/min → 6000 pulses/min = 100 Hz (max)

Fréquence de coupure du filtre RC :
  fc = 1 / (2π × R × C) = 1 / (2π × 4700 × 1nF) ≈ 33.9 kHz
  
Cela donne un ratio fc/f_signal_max = 33.9kHz/100Hz ≈ **340x** → largement suffisant
(consigne ≥10x respectée sans déformer le front des impulsions)
```

Si on mettait C=10nF (au lieu de 1nF), fc serait de ~3.4kHz → toujours >10× mais front plus mou : acceptable si l'ESP8266 a une entrée schmitt clean (ce qui est le cas sur GPIO). **On garde 1nF** pour garantir des fronts nets.

### Pourquoi aucun transistor côté entrée ?

Le signal Noctua est déjà un simple transistor NPN interne qui tire à la masse → parfaitement compatible avec une interruption FALLING ESP8266 lorsqu'on ajoute un pull-up vers 3.3V. Aucun étage intermédiaire n'est nécessaire.

---

## 5. Étage de sortie — ESP8266 → Transistor NPN → Inverter Deye

```
              GPIO (ESP8266)                          Deye INVERTER TACH
       ┌───────────────┐           ┌──────────────┐        ───────────────
       │               │           │              │              │
  SIMCHA◄──┤ D2/GPIO4    ├───R3=10kΩ───┤ B   Q1 BC547 ├─── Collector ──▶│ TACH IN A
       │               │           │   /|\        │          (ferré à haut
       │               │           │    | NPN     │           par pull-up
       │               │           │    E         │           interne de l'inv.)
       │               │           │    |         │
       │               │           └────┼─────────┘
       │               │                │
       │               │          ──────┴── GND (côté émetteur)
       │
       +── 3.3V rail ESP8266

       Même schéma pour canal B : R4=10kΩ, Q2=BC547 → SIMCHB(D5/GPIO14)
```

### Calculs de l'étage sortie NPN

**a) Sélection du transistor : BC547**

Critères requis :
- Vceo (tension collecteur-émetteur en blocage) **> 12V** (car l'onduleur peut appliquer jusqu'à 12V sur sa broche tach)
- Ic max (> courant collecteur attendu, quelques mA max)
- Disponibles et courants

| Transistor     | Vceo   | Ic_max   | hFE typ. | Choix      |
|----------------|--------|----------|----------|------------|
| BC547          | 45V    | 100mA    | 100-600  | ✅ Retenu  |
| 2N3904         | 40V    | 200mA    | 100-300  | ✅ Alternative |
| S8050          | 25V    | 700mA    | 60-300   | ⚠️ OK      |
| C1815          | 50V    | 150mA    | 100-400  | ✅ Alternative |

Le **BC547** est retenu : Vceo=45W >> 12V, Ic_max=100mA >> besoin (~1mA max), très courant.

**b) Résistance de base R3/R4 (10kΩ)**

L'ESP8266 sort un niveau HIGH de 3.3V sur ses GPIOs. La chute Vbe du BC547 en saturation est typiquement 0.7V. Le courant de base maximal disponible par le GPIO est :

```
Ib_max_theorique = (Vgpio_high - Vbe) / Rb
```

Le transistor doit saturer avec un courant collecteur Ic bien que faible (quelques µA à mA, car c'est l'onduleur qui "tire" via son pull-up interne). Estimons le pire cas :

```
Pull-up interne inverter : supposé ~10kΩ (valeur typique)
Ic_max = 12V / 10kΩ = 1.2mA (pire cas, si l'onduleur tire vraiment ce courant)

Gain β du BC547 en saturation : prendre hFE(sat) ≈ 10 (faible car en saturation)
Courant de base minimum requis : Ib_min = Ic / hFE(sat) = 1.2mA / 10 = 120µA

Facteur de sursaturation ×5-10 pour marge : Ib_design = 120µA × 10 = 1.2mA

Résistance de base correspondante :
Rb = (Vgpio - Vbe) / Ib_design = (3.3V - 0.7V) / 1.2mA = 2.17kΩ → arrondi à 2.2kΩ minimum
```

**Mais attention** : le GPIO ESP8266 a un courant max par pin de **~12mA** (absolu max). Un Rb=2.2kΩ donnerait Ib = 2.6mA, acceptable mais proche de la limite si plusieurs pins sont sollicitées.

**Compromis retenu : 10kΩ**

```
Ib réel avec Rb=10kΩ = (3.3V - 0.7V) / 10kΩ = 0.26mA
hFE effectif = Ic/Ib = 1.2mA / 0.26mA ≈ 4.6 → le transistor est EN SATURATION profonde

✅ Tension Vce(sat) typique du BC547 à Ib=0.26mA, Ic=1.2mA : < 0.1V
   Le collecteur est correctement tiré vers la masse lors d'un HIGH GPIO.
```

**Pourquoi pas une résistance plus petite (ex: 2.2kΩ) ?**
- Plus de courant dans le GPIO (jusqu à ~1.2mA au lieu de 0.26mA)
- Risque si le pull-up interne de l'onduleur est bien plus faible que 10kΩ : Ic pourrait monter, Ib insuffisant pour saturer, Vce(sat) augmenterait, le signal serait déformé.

**Le choix de Rb=10kΩ garantit saturation même dans un scénario moderate (pull-up inverter ~20-47kΩ → Ic<0.6mA)**, avec une marge confortable tout en limitant le courant GPIO à <0.35mA/pin.

**Vérification puissance sur R3/R4 :**
```
P_Rb = Ib² × Rb = (0.26mA)² × 10kΩ ≈ 0.68mW → résistor 1/8W (0.1W) largement suffisant
```

**c) Émetteur à la masse, collecteur flottant côté haut**

C'est le point **critique** de l'isolation :
- L'émetteur est relié directement au GND commun ESP8266/inverter → pas de référence flottante.
- Le collecteur est relié au fil tach de l'onduleur, mais **ne tire jamais vers 3.3V** : c'est uniquement le pull-up interne de l'onduleur (qu'il soit à 3.3V, 5V ou 12V) qui fixe le niveau haut.
- Ainsi, quel que soit le domaine tension de l'onduleur, le transistor NPN travaille correctement en commutation collecteur ouvert et l'ESP8266 est isolé.

---

## 6. Indicateur LED

```
          D4/GPIO2 (STATUS_LED)              Wemos D1 mini built-in LED
                    │                              (bleu, active LOW sur GPIO2)
                    ├───┬── R5=470Ω ───|>|── GND  (LED externe rouge/verte)   │
                    │                                  ▲                       │
                    │        Built-in                  │ (optionnel, en         │
                    │        LED (bleu)                │  parallèle avec la     │
                    │        active LOW                │  LED externe via une   │
                    └──────────────────────────────────┘  résistance séparée). 
                                                       
   État HIGH GPIO  → LED éteinte (active LOW)
   État LOW GPIO   → LED allumée
```

### Calcul de R5

Courant cible pour la LED : ~10mA

```
R5 = (Vgpio_low - Vf_LED) / If_LED

Pour une LED rouge (Vf ≈ 2.0V) :
R5 = (3.3V - 2.0V) / 10mA = 130Ω → valeur standard proche: 150Ω ou 180Ω
Pour une LED verte (Vf ≈ 2.2V) :
R5 = (3.3V - 2.2V) / 10mA = 110Ω → valeur standard proche: 120Ω ou 150Ω

Choix retenu : **470Ω** pour un courant plus doux (~3-4mA), LED assez brillante,
dissipation minimale et marge de sécurité sur le GPIO.
```

### Signification des états LED

| État LED         | Signification                                        |
|------------------|------------------------------------------------------|
| Allumée fixe     | Au moins un canal Noctua reçoit un signal valide et la simulation est active |
| Clignotante (~1 Hz)  | Aucun signal valide détecté (timeout >2s sur les deux canaux), mode attente/repli |

La LED est pilotée **uniquement depuis loop()** (monde best-effort), jamais de l'ISR, conformément à l'architecture d'isolation stricte.

---

## 7. Schéma global complet (vue d'ensemble)

```
┌───────────────────────────────────────────────────────────────────────────────────────┐
│                                                                                       │
│   [FAN CONNECTOR A]                     [FAN CONNECTOR B]                             │
│    +12V_A  ──────┐                        +12V_B  ──────┐                            │
│       │          │                          │            │                            │
│     BAT54│ D1                  BAT54│ D2                         │                     │
│       │   │<│──────────┐                 │   │<│──────────┐    │                    │
│       └───┘          │                 └───┘          │    │                    │
│                      ├──────── VBUS_COMMON ────────────┤    │                    │
│                      │                                │    │                    │
│     GND_A  ──────────┼──────────┐                     │    │   GND_B  ──────────┼─────┐
│                      │          │                     │    │                   │     │
└──────────────────────┴──────────┤   C1: 1000µF/16V   ├────┘                   └─────┘
                                  │  (réservoir)       │
                                  └──────────┬─────────┘
                                             │ +12V bus
                                    MT3608 Buck Module
                                              │ +5V sortie
                                 ┌────────────┼────────────┐
                                 │            │            │
                               C2: 100nF   C3: 220µT     GND
                               (découpage)  (tampon)      (common)
                                 │            │            │
                                 ▼            ▼            ▼
                          ┌───────────────────────────────────────┐
                          │       LOLIN(WEMOS) D1 MINI            │
                          │         (ESP8266-WROOM-02)             │
                          │                                       │
     Noctua TACH A ──────▶│ D6 / GPIO12  ←────────── INPUT      │
     Pull-up 4.7kΩ       │  Interrupt FALLING                    │
     C4: 1nF filter      └──────────────────┬────────────────────┘
                                              │ ISR capture temps-réel
                                          TIMER1 @20kHz
                                              │ ordonnanceur
                                              ▼
                          ┌───────────────────────────────────────┐
     SIM-TACH A ◀────────│ D2 / GPIO4   →────────── OUTPUT      │
     R3: 10kΩ + BC547    │ (Scheduler ISR, Timer1)               │
                          │                                       │
     SIM-TACH B ◀────────│ D5 / GPIO14  →────────── OUTPUT      │
     R4: 10kΩ + BC547    └──────────────────┬────────────────────┘
                                              │
     Inverter TACH A                         ▼   Q1,Q2 BC547 NPN Transistors
     (pull-up interne inverter)          Collecteur ouvert → compatible 3.3V/5V/12V
     Inverter TACH B
                                             
                          ┌───────────────────────────────────────┐
     STATUS LED ◀────────│ D4 / GPIO2   ←────────── BEST-EFFORT  │
     R5: 470Ω + LED      │ (piloté depuis loop(), millis())       │
                          │                                       │
                          │         WiFi AP/STA                   │
                          │         ESPAsyncWebServer             │
                          │         LittleFS /config.json         │
                          └───────────────────────────────────────┘

                          [ESP8266 WROOM-02 internal: 5V→3.3V AMS1117]
                                    (alimenté depuis la broche 5W du Wemos)
```

---

## 8. Plan de brochage complet (Wemos D1 Mini)

| Broche   | GPIO   | Fonction           | Mode              | Justification                                     |
|----------|--------|--------------------|-------------------|---------------------------------------------------|
| D0       | GPIO16 | ❌ Non utilisé     | —                 | Ne supporte PAS les interruptions (hardware limit)|
| D1       | GPIO5  | ⚠️ Libre          | —                 | Pas utilisé dans ce design                          |
| D2       | GPIO4  | ✅ SIMCHA         | OUTPUT            | Sortie vers base transistor Q1                    |
| D3       | GPIO0  | ❌ Non utilisé     | —                 | Pullup interne au boot (configurateur)             |
| D4       | GPIO2  | ✅ STATUS_LED     | OUTPUT            | LED indicateur, active LOW                          |
| D5       | GPIO14 | ✅ SIMCHB         | OUTPUT            | Sortie vers base transistor Q2                    |
| D6       | GPIO12 | ✅ NOSCH_A        | INPUT_PULLUP + INT│ Entrée tach Noctua A, attachInterrupt FALLING     |
| D7       | GPIO13 | ✅ NOSCH_B        | INPUT_PULLUP + INT│ Entrée tach Noctua B, attachInterrupt FALLING     |
| D8       | GPIO15 | ❌ Non utilisé     | —                 | Tiré LOW au boot (non usable sans résistance)      |

### Broches exclues pour usage critique et pourquoi

| Broche   | Raison d'exclusion                              |
|----------|--------------------------------------------------|
| D0/GPIO16| Absence de support matériel des interruptions sur ESP8266. L'ISR `tachISR_A`/`tachISR_B` nécessiterait impérativement attachInterrupt() qui échoue silencieusement sur ce GPIO. |
| D3/GPIO0 | Nécessite un pulldown (~10kΩ vers GND) pour booter normalement. Sans ce pulldown, l'ESP8266 refuse de démarrer (boot loop). Utiliser cette broche ajouterait une résistance de rappel permanente qui interférerait avec le signal. |
| D8/GPIO15 | Tiré activement LOW par le circuit de boot. Toute tentative d'utiliser cette broche comme entrée/sortie normale provoquerait un court-circuit entre le GPIO et la masse, ou empêcherait le démarrage si configurée en input avec pull-up interne. |

### Broches recommandées vs alternatives

**Entrées tach (interruption FALLING) :** D6(GPIO12), D7(GPIO13), D5(GPIO14) — toutes trois supportent les interruptions matérielles sur ESP8266.

**Sorties (simple OUTPUT) :** D2(GPIO4), D5(GPIO14), D1(GPIO5) — n'importe quelle broche peut servir de sortie numérique simple.

Dans ce design, on privilégie :
- **D6 + D7** pour les entrées tach car GPIO12/13 sont les pins interrupt-capable les plus éloignées des contraintes de boot (GPIO0, GPIO2, GPIO15).
- **D2 + D5** pour les sorties car elles n'ont aucune contrainte de boot et sont facilement accessibles sur le breakout.

---

## 9. Points d'attention fabrication & tests

### Avant première mise sous tension
1. Vérifier la polarité des condensateurs électrolytiques/tantalums (C1, C3).
2. S'assurer que les deux GND (ESP8266 et inverter) sont bien reliés (GND commun).
3. Mesurer la résistance entre VBUS_COMMON et GND : doit être >1kΩ (pas de court-circuit).
4. Mesurer la tension buck : 5V ±5% sur la broche 5W du Wemos.

### Test étape par étape
1. **Alimentation seule** : connecter uniquement les 12W, vérifier 5V stable sur le Wemos → la LED intégrée (bleue) devrait s'allumer faiblement si le WiFi tente de se connecter.
2. **Entrées tach simulées** : relier D6 et D7 à la masse via un interrupteur momentané, vérifier que les ISR réagissent (log Serial).
3. **Sorties tach** : brancher un oscilloscope sur SIMCHA/D2 → vérifier une onde carrée correcte (50% duty cycle, fréquence = RPM_input × ratio).
4. **Connexion inverter** : connecter les sorties aux fils tach de l'onduleur → l'onduleur ne devrait plus signaler d'erreur ventilateur.
5. **Interface web** : naviguer vers l'IP AP (192.168.4.1 par défaut) → configurer les ratios et vérifier les lectures RPM en temps réel.

### Dépannage courant

| Symptôme                         | Cause probable                    | Remède                              |
|----------------------------------|-----------------------------------|-------------------------------------|
| Reset aléatoires ESP8266         | Insuffisance courant WiFi        | Ajouter C3(220µF tantale) + vérifier 5V stable |
| Signal tach sortie irrégulier    | Buffer de config corrompu       | Réinitialiser LittleFS, recharger defaults |
| LED clignote mais ventilateur tourne | Ratio trop faible            | Augmenter ratio via interface web   |
| Pas de connexion WiFi            | SSID/MDP erronés en config     | Reset AP → configurer depuis l'interface web |
| Overheat AMS1117                 | Alimentation sur VIN au lieu de 5W | Vérifier branchement sur broche 5V (pas VIN/RAW) |
