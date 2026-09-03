# ────────────────────────────────────────────────────────────────
#  Schéma Électronique Complet — Deye TachSim
#  Remplacement ventilateurs NMB → Noctua sur onduleur Deye SUN-8K-SG05LP1-EU-AM2-P
#  Carte : Wemos D1 Mini V2.3.0 (ESP-12S / ESP8266)
# ────────────────────────────────────────────────────────────────

## SOMMAIRE
1. [Vue d'ensemble](#1-vue-densemble)
2. [Alimentation](#2-alimentation)
3. [Circuits d'entrée Tach (Noctua → ESP8266)](#3-circuits-dentrée-tach-noctua--esp8266)
4. [Circuits de sortie Tach (ESP8266 → Deye)](#4-circuits-de-sortie-tach-esp8266--deye)
5. [État de statut LED](#5-état-de-statut-led)
6. [Table des correspondances de broches](#6-table-des-correspondances-de-broches)
7. [Liste du matériel](#7-liste-du-matériel)
8. [Procédure d'assemblage](#8-procédure-dassemblage)

---

## 1. Vue d'ensemble

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Wemos D1 Mini V2.3                          │
│                                                                      │
│  ┌──────────┐    ┌──────────┐   ┌──────────┐   ┌──────────────┐   │
│  │ Aliment. │→──→│ Entrée   │→──│ Trait-   │→──│ Sortie      │→──│ Tach       │
│  │ (12V)    │    │ Tach Noct│    │ ment     │    │ NPN Switch │    │ Deye Inverter│
│  └──────────┘    └──────────┘   │ (ISR HW) │   └──────────────┘    └──────────────┘
│                                 └──────────┘                          ▲
│                                        ▲                              │
│                                    ┌─────────┐                      │
│                                    │ WiFi /  │←──(pas d'impact sur│
│                                    │ Web Serve│── les ISR matériel)│
│                                    └─────────┘                      │
└─────────────────────────────────────────────────────────────────────┘
```

Principe : le signal tach des ventilateurs Noctua est lu par des interruptions
matérielles GPIO (priorité max, non impactées par le WiFi). Un Timer1 matériel
génère ensuite les signaux simulés à la fréquence désirée. Le serveur web et
le WiFi tournent dans la boucle principale sans jamais interférer avec les ISR.

---

## 2. Alimentation

### 2.1 Schéma de puissance

```
                    ┌──────────┐         ┌─────────────┐    ┌──────────┐
  Fan Connector 1   │Schottky  │──+──→   │     470µF   │    │          │
  (+12V)            │BAT54/SS  │   │    ║   16V       │    │ Wemos    │
                    └──────────┘   │    ║             │    │ D1 Mini  │
                                     │    ║             │    │          │
  Fan Connector 2   ┌──────────┐   │    ╚═════════════╧═══>│  VIN     │
  (+12V)            │Schottky  │──>+                    │    │          │
                    │BAT54/SS  │   │                    │    │ GND      │─┼───────── GND commun
                    └──────────┘   │                    │    │          │
                                     │                    │    │ AMS1117  │
                                     │                    │    │ → 3.3V   │
                                     │                    │    │          │
                                     +────────────────────┘    └──────────┘
```

### 2.2 Description

| Élément | Valeur | Rôle | Notes |
|---------|--------|------|-------|
| D1, D2 | BAT54S (dual Schottky) ou SS34 | OR logique des deux alimentations 12V | Permet que l'onduleur alimente le Wemos depuis n'importe quel connecteur. Si les deux sont actifs, celui au potentiel le plus élevé fournit le courant. Chute de tension ~0.3V (Schottky). |
| C1 | 470µF / 16V électrolytique | Réservoir d'énergie | Compense les variations brutales de charge quand un ventilateur démarre ou s'arrête. Placez-le le plus près possible du connecteur d'entrée. |

### 2.3 Remarques importantes sur l'alimentation

- **Les deux connecteurs peuvent être alimentés simultanément** (si les deux ventilateurs sont installés) → la diode OR gère automatiquement le partage de courant
- **Les tensions peuvent différer légèrement** (quelques centaines de mV) → la chute Schottky de ~0.3V rend cette différence négligeable pour l'AMS1117
- **Le Wemos D1 Mini intègre un AMS1117-3.3** qui fournit 3.3V stable au ESP8266 à partir de 5V+ sur VIN (avec ~2V de dropout, minimum ~5V requis sur VIN)
- **L'AMS1117 dissipe de la chaleur** : avec deux ventilateurs Noctua draw ~0.2A + Wemos ~0.3A max → ~1.5W dissipés. Prévoyez une ventilation suffisée si le boîtier est fermé.

---

## 3. Circuits d'entrée Tach (Noctua → ESP8266)

### 3.1 Principe de fonctionnement du tach Noctua

Le signal tach des ventilateurs Noctua est un **open-drain** :
- Le transistor interne du fan tire la ligne à GND alternativement
- La fréquence des pulses indique la vitesse (RPM)
- Le niveau HAUT est fourni par le pull-up externe (ici l'onduleur Deye, avec sa résistance interne de pull-up à une tension inconnue — 3.3V, 5V ou potentiellement plus)

### 3.2 Schéma d'entrée

```
                    Fan Noctua                          Wemos D1 Mini
                  ┌─────────┐                           ┌──────────────┐
Tach Pin ────────┤ TACH OUT├──────────┬───────────────>│ GPIO         │
                  └─────────┘          │               │              │
                                        │               │     R1      │
                                       R1 4k7          ├╌╌╌╌╌╌╌╌╌╌╌╌┤ D7 (GPIO13) Fan 9cm
                                        │               │              │
                                       GND             │     D1      │
                        Pull-down 10k   │               │ ╭──|│──┐     │
                                            ║            │ │ BAT54 │    │
                                           C2           ─┘ Anode  │     │
                                        100pF             │ Cath.    │    │
                                            │             │     │     │
                        Pull-up interne   │               │     ▼     │
                        de l'onduleur     └───────────────┼── VCC3.3 (provenant  │
                                                   Ligne   │    du pull-up     │
                                                   vers    │    onduleur)      │
                                                   Deye   │                    │
                                                   Inverter│                    │
                                             Tach Input │                    │
                                             Pin       └────────────────────┘
```

### 3.3 Fonctionnement détaillé

1. **État HAUT (fan tach OFF)** : La ligne est tirée par le pull-up de l'onduleur
   - Si le pull-up est à 12V → courant limité par R1(4.7k) et D1(Schottky) :
     `(12V - 3.7V) / 4.7k = ~1.8mA` → entièrement safe pour le circuit
   - L'ESP8266 voit ~3.7V (3.3V + forward voltage Schottky) → interprété comme **HAUT**

2. **État BAS (fan tach ON)** : Le transistor du fan tire la ligne à GND
   - L'ESP8266 voit GND via C2(100pF) et R1(4.7k) → interprété comme **BAS**

3. **Protection Schottky D1 (BAT54)** :
   - Clamp de surtension si le pull-up de l'onduleur dépasse ~3.7V
   - Le courant est limité à <2mA par R1, totalement safe

### 3.4 Composants d'entrée (par canal)

| Élément | Valeur | Type | Notes |
|---------|--------|------|-------|
| R1 | 4.7kΩ ±5% | Résistance série | Limite le courant de clamp Schottky et forme un filtre RC avec C2 |
| D1 | BAT54S (dual) ou SS14 | Diode Schottky | Clamp de surtension vers VCC3.3. BAT54S est compact (SOT-23). |
| C2 | 100pF / 50V | Condensateur céramique X7R | Filtre HF anti-résonance et anti-couplage capacitif. Pas critique, peut être omis. |

**Note sur le pull-up/pull-down :** Le schéma ci-dessus **ne nécessite pas** de résistance de tirage externe. Les deux côtés (onduleur en pull-up et transistor fan en open-drain) suffisent. Le code utilise `INPUT` (sans pullup interne ESP8266).

---

## 4. Circuits de sortie Tach (ESP8266 → Deye)

### 4.1 Principe

L'onduleur Deye attend un signal tach en **open-collector** (comme un PC standard).
Le transistor NPN en émetteur commun crée un open-collector parfaitement isolé :
- ESP8266 pilote la base du transistor (niveau 3.3V TTL)
- Le collecteur va vers l'entrée tach de l'onduleur
- L'émetteur est à la masse

L'onduleur fournit son propre pull-up interne au collecteur, quelle que soit sa tension (3.3V, 5V ou plus).

### 4.2 Schéma de sortie

```
                    Wemos D1 Mini                  Deye Inverter
                  ┌─────────────┐                       ┌───────────┐
Tach Output ──┬──>│ GPIO(D6)    │                      │ Tach Input│
              │   │ Fan 9cm     │                      │ Pin (fan 1)│
              │   │ (GPIO12)    │                      │           │
              │   └─────────────┘                      │           │
              │                                        │           │
             R3 4k7                                     │    (pull-up interne      │
     Fan 1 path:                                          │     de l'onduleur)      │
              │                                           │                       │
              +──╌╌╌╌╌╌╌┬─────────────┐                  │                   │
                       ║│ Base        │                 ┌─┤─────────────────┘
                      Q1│ 2N2222      │                T1│ (câble vers   ▲
                     NPN│ C           │                  │ inverter)     │
                       ║│ E           │                  │               │
              ┌────────┴╌─────────────┤──────────────────┘               │
              │       Fan 2 path:                                       │
              │                                                         │
              │   Wemos D1 Mini                                         │
              │ ┌─────────────┐                                          │
              +─>│ GPIO(D5)    │                                          │
              │   │ Fan 6cm     │                                          │
              │   │ (GPIO14)    │                                          │
              │   └─────────────┘                                          │
              │                                                            │
             R4 4k7                                                       │
              │                                                            │
              +──╌╌╌╌╌╌┬─────────────┐                                     │
                       ║│ Base        │                                    │
                      Q2│ BC547       │                                   ┌─┤───────────┐
                     NPN│ C           │     (pull-up interne)              │ Tach Input│
                       ║│ E           │                                  │ Pin (fan 2)│
              ┌────────┴╌─────────────┤──────────────────────────────────┤           │
              │                        │ GND                               │           │
              │   Wemos D1 Mini        │                                   │           │
              │  ──────────────────────┘                                   └───────────┘
              │
    GND du Wemos → GND commun (tous les GND connectés)
```

### 4.3 Fonctionnement détaillé

**Étape par étape pour un cycle complet :**

| Étape | GPIO | Transistor | Collecteur | Ligne Deye | Signification |
|-------|------|------------|------------|------------|---------------|
| 1 | HIGH (3.3V) | NPN ON (saturation) | ≈0V | BAS (0V) | Impulse tach présente |
| 2 | LOW (0V) | NPN OFF | → Pull-up interne Deye → HAUT | HAUT (3.3V/5V/12V) | Pause entre pulses |

**Le cycle répété à fréquence F crée un train de pulses dont la période encode les RPM simulés.**

### 4.4 Composants de sortie (par canal)

| Élément | Valeur/Type | Rôle | Notes |
|---------|-------------|------|-------|
| R3, R4 | 4.7kΩ ±5% | Résistance de base | Limite le courant de base du NPN : `(3.3V - 0.7V) / 4.7k = ~0.55mA`. Suffisant pour saturer un 2N2222/BC547. |
| Q1 | 2N2222 ou BC547 | NPN Switch | Commutation rapide, Vce_sat <0.3V. Le 2N2222 est plus robuste. |
| Q2 | BC547 ou 2N2222 | NPN Switch | Mêmes caractéristiques que Q1. Peut être le même type que Q1 pour simplicité. |

### 4.5 Fréquence maximale supportée

Les transistors utilisés (2N2222, BC547) ont des temps de commutation typiques de ~10-30ns, bien en-deçà des fréquences nécessaires :
- **NF-A9 max 3000 RPM × ratio 2.5× = 7500 RPM simulés** → fréquence pulses ≈ 250 Hz → période 4ms
- **NF-A6x25 max 3000 RPM × ratio 2.5× = 7500 RPM simulés** → même fréquence

Aucun problème de vitesse avec ces composants.

---

## 5. État de statut LED

```
GPIO2 (D4) ───────────────────────────┐
                              │        │
                             LED       │ ← Résistance limitatrice interne
                             intégrée   │   du module Wemos D1 Mini
                              (actif-LOW)│
                                        │
                                       GND
```

### 5.1 Signification des états

| État LED | Signification |
|----------|---------------|
| **ON permanente** | Au moins un ventilateur tourne, simulation active |
| **Clignote lentement (2s cycle)** | Mode AP uniquement — pas connecté à un réseau WiFi STA |
| **Clignote court (1s cycle)** | Connecté en STA mais aucun ventilateur ne tourne encore |
| **Éteinte** | État par défaut au boot, pendant les ~500ms d'initialisation |

---

## 6. Table des correspondances de broches

### 6.1 Carte Wemos D1 Mini V2.3.0 (ESP-12S)

| Broche physique | GPIO | Fonction | Direction | Notes |
|-----------------|------|----------|-----------|-------|
| D7 | GPIO13 | Tach entrée Fan 9 cm (NF-A9) | Entrée | Interruption matériel FALLING |
| D8 | GPIO15 | Tach entrée Fan 6 cm (NF-A6x25) | Entrée | Interruption matériel FALLING |
| D6 | GPIO12 | Sortie simulée Fan 9 cm → NPN Q1 | Sortie | Timer1 ISR, toggle dynamique |
| D5 | GPIO14 | Sortie simulée Fan 6 cm → NPN Q2 | Sortie | Timer1 ISR, toggle dynamique |
| D4 | GPIO2 | LED intégrée Wemos | Sortie | Active LOW — statut système |
| VIN | — | Alimentation 12V (de connecteur fan) | Entrée | Via AMS1117 → 3.3V ESP8266 |
| GND | — | Masse commune | — | Relie tous les GND |

### 6.2 Mapping Noctua → Deye

```
Fan Noctua      Wemos         Deye Inverter    NPN Switch
─────────────   ─────────     ────────────     ──────────
+12V            ┌── D1/D2   → │ AMS1117       │
                │ Schottky OR │ → 3.3V        │
GND             └─────────────┴───────────────┘

TACH Fan 9cm    ─── R1(4k7) + D1(Schottky) → GPIO13 (D7) [INT0]
                 C2(100pF) GND

TACH Fan 6cm    ─── R1(4k7) + D1(Schottky) → GPIO15 (D8) [INT1]
                 C2(100pF) GND

GPIO12 (D6)     ─── R3(4k7) → Base Q1(2N2222) → Collecteur → TACH Inverter Fan 1
GPIO14 (D5)     ─── R4(4k7) → Base Q2(BC547)  → Collecteur → TACH Inverter Fan 2

GPIO2 (D4)      ─── LED intégrée Wemos
```

---

## 7. Liste du matériel

### Composants principaux

| Réf. | Quantité | Désignation | Valeur / Type | Boîtier | Prix estimé |
|------|----------|-------------|---------------|---------|-------------|
| PCB/WEMOS | 1 | Wemos D1 Mini V2.3.0 (ESP-12S) | ESP8266 @ 80MHz | Module | ~5 € |
| Q1, Q2 | 2 | Transistor NPN | 2N2222 ou BC547 | TO-92 / SMD | ~0.50 € |
| R1-R4 | 4 | Résistance série/clamp | 4.7kΩ ±5% | 0805 / Axial | ~0.10 € |
| D1-D4 | 4 | Diode Schottky | BAT54S (dual) ou SS14 | SOT-23 / DO-41 | ~0.40 € |
| C1 | 1 | Condensateur tampon | 470µF / 16V | Radial électrolytique | ~0.50 € |
| C2×2 | 2 | Filtre HF entrée | 100pF / 50V | 0805 céramique X7R | ~0.10 € |
| —— | — | Connecteurs ventilateur | Dupont 3-pin femelle (mâle pour connexion fans) | — | ~2 € |

### Outils nécessaires
- Soudure étain-plomb (60/40) ou sans plomb
- Fer à souder, pompe à dessouder
- Multimètre
- Pinces coupantes et ébavureuses
- Câbles brins fins (0.3mm² minimum pour le 12V)

---

## 8. Procédure d'assemblage

### Étape 1 — Préparation du Wemos D1 Mini
1. Vérifier que le module fonctionne (brancher en USB, tester avec un blink basique)
2. Noter les broches VIN et GND sur le connecteur externe

### Étape 2 — Circuit d'alimentation
1. Souder les diodes BAT54S : cathode vers le rail commun +12V (point de jonction)
2. Souder le condensateur 470µF entre le rail +12V commun et GND
3. Vérifier la polarité du condensateur électrolytique (+ → +12V, – → GND)

### Étape 3 — Circuits d'entrée tach
1. Pour chaque canal (Fan 9cm sur D7/GPIO13, Fan 6cm sur D8/GPIO15) :
   - Souder la résistance 4.7kΩ entre le fil tach du fan et la broche GPIO correspondante
   - Souder la diode Schottky (BAT54S) : **cathode** côté GPIO, **anode** côté VCC3.3 (ou VIN du Wemos via un jump court vers le régulateur)
2. Pas de résistance de pull-down nécessaire (le circuit interne fan + pull-up onduleur suffit)

### Étape 4 — Circuits de sortie tach
1. Pour chaque canal :
   - Collector → Q1/Q2 → Collecteur relié au fil qui va vers l'entrée tach correspondante du Deye
   - Base → R3/R4 (4.7kΩ) → GPIO12 (Fan 9cm) et GPIO14 (Fan 6cm) respectivement
   - Émetteur → GND commun
2. Vérifier que les collecteurs ne sont PAS connectés directement au GPIO — seulement via le transistor

### Étape 5 — Branchement des ventilateurs Noctua
1. Brancher chaque fan Noctua sur son connecteur d'entrée :
   - Rouge (+) → +12V du Deye (via les diodes OR)
   - Noir (–) → GND
   - Jaune/Orange (tach) → circuit d'entrée tach décrit ci-dessus

### Étape 6 — Branchement vers l'onduleur Deye
1. Les fils de sortie des transistors Q1 et Q2 vont vers les **entrées tach d'origine** sur le connecteur ventilateur du Deye
2. Le +12V et GND du Deye alimentent à la fois les fans Noctua et le Wemos

### Étape 7 — Flash et vérification
1. Compiler `deye_fan.ino` dans Arduino IDE :
   - Carte : LOLIN(WEMOS) D1 mini
   - CPU Frequency : 80MHz
   - Upload Speed : 921600
2. Brancher le Wemos en USB, flasher le firmware
3. Vérifier sur le moniteur série (115200 bauds) les messages de boot :
   ```
   Deye TachSim — Démarrage
   [EEPROM] Ratios chargés: Fan1=2.5  Fan2=2.5
   [Hardware] Entrées/sorties configurées.
   [Timer1] Génér. tach active (résolution=1 µs)
   AP IP=192.168.4.1  |  (mode AP uniquement)
   [Web] Serveur web démarré sur le port 80.
   ```
4. Se connecter au réseau WiFi `Deye-TachSim` (si mode AP) et accéder à `http://192.168.4.1`
5. Configurer les ratios et le WiFi STA si nécessaire

---

## ANNEXE A — Schéma électrique complet (notation net)

```
NETLIST DETAILLÉE :

[VIN_FAN1] -- Anode D1(BAT54S cath#2) --> [VIN_COM] --> VIN Wemos
[VIN_FAN2] -- Anode D2(BAT54S cath#1) --> [VIN_COM] --> VIN Wemos
[VIN_COM]  --+--> C1(470µF/16V +)
             |
[3.3V]       --> AMS1117-3.3 output

[NOCTUA_FAN9_TACH] -- R1(4.7k) --> [GPIO13_D7]
                              |     D1_BAT54(cath#1) -> Anode D1_BAT54 -> VIN_COM
                           C2(100pF) GND

[NOCTUA_FAN6_TACH] -- R1'(4.7k) --> [GPIO15_D8]
                               |     D1'_BAT54(cath#2) -> Anode D1'_BAT54 -> VIN_COM
                            C2'(100pF) GND

[GPIO12_D6]  -- R3(4.7k) --> Base Q1(2N2222)
                              Emetteur Q1 --> GND
                              Collecteur Q1 --> [DEYE_FAN1_TACH_IN]

[GPIO14_D5]  -- R4(4.7k) --> Base Q2(BC547)
                              Emetteur Q2 --> GND
                              Collecteur Q2 --> [DEYE_FAN2_TACH_IN]

[GPIO2_D4]   --> LED intégrée Wemos (actif LOW)
[GND]        --> Tous les émetteurs NPN, C1(-), C2s(-), GND Wemos
```

## ANNEXE B — Comportement du Timer1 ISR (vérification formelle)

Le Timer1 ISR `tachOutputAllISR()` fonctionne comme une machine à états :

```
Départ: fanNextTick = 0, simStateFan = true, GPIO = HIGH

À chaque tick Timer1:
  
  SI fanNextTick <= temps_actuel:
    SI fanActif == FAUX:
      → digitalWrite(GPIO, HIGH)          // Force stopped state
      → fanNextTick = UINT32_MAX           // Ne plus scheduler
    SINON:                                 // Fan tourne
      → simStateFan = !simStateFan         // Bascule d'état
      SI simStateFan == VRAI:
        → digitalWrite(GPIO, HIGH)         // NPN OFF → tach HIGH
      SINON:
        → digitalWrite(GPIO, LOW)          // NPN ON  → tach LOW (pulse)
      
      delta = simPeriodUs / US_PER_TICK    // Convertir en ticks Timer1
      SI delta <= 0 ou delta > MAX_TACH_PERIOD_US:
        → fanNextTick = UINT32_MAX          // Période invalide → stop
      SINON:
        → fanNextTick = now + delta        // Prochain tick = maintenant + demi-période
  
  Répéter pour l'autre canal (fan 6cm)

Le prochain interrupt Timer1 est programmé au minimum de tous les nextTick.
```

Ce schéma garantit que le signal tach simulé est **totalement indépendant** du WiFi, du serveur web et de toute autre tâche logicielle, car il est généré par un timer matériel dont l'ISR n'accède à aucune variable partagée avec la boucle principale hormis `simPeriodUsFanX` (lecture seule, mise à jour dans l'autre ISR d'entrée).
