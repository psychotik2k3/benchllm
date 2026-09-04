# Schéma électronique — Simulateur tach Deye

## Vue d'ensemble

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        ONDULEUR DEYE SUN-8K-SG05LP1                        │
│                                                                             │
│  Connecteur ventilateur 9 cm          Connecteur ventilateur 6 cm          │
│  ┌──────┬──────┬──────┐               ┌──────┬──────┬──────┐                │
│  │ +12V │ GND  │ TACH │               │ +12V │ GND  │ TACH │                │
│  └──┬───┴──┬───┴──┬───┘               └──┬───┴──┬───┴──┬───┘                │
└─────┼──────┼──────┼──────────────────────┼──────┼──────┼────────────────────┘
      │      │      │                      │      │      │
      │      │      │                      │      │      │
══════╪══════╪══════╪══════════════════════╪══════╪══════╪══════════════════
      │      │      │   MODULE SIMULATEUR  │      │      │
      │      │      │                      │      │      │
      ▼      ▼      ▼                      ▼      ▼      ▼
   [OR]   [COM]  [OUT0]                 [OR]   [COM]  [OUT1]
      │      │      │                      │      │      │
      │      │      │                      │      │      │
      ▼      ▼      ▼                      ▼      ▼      ▼
   VIN+   GND   Circuit                   (idem) (idem) Circuit
          │    sortie 0                          sortie 1
          │      │                                 │
          ▼      ▼                                 ▼
     ┌─────────────────┐                    ┌─────────────────┐
     │  WEMOS D1 MINI  │                    │  NOCTUA FANS    │
     │    (ESP8266)    │◄─── TACH IN ───────│  9cm + 6cm      │
     └─────────────────┘                    └─────────────────┘
```

---

## 1. Alimentation — OR-ing des deux connecteurs ventilateur

Les deux connecteurs ventilateur de l'onduleur fournissent chacun un +12 V indépendant.
On utilise des diodes Schottky pour combiner les deux sources (OR passif) : le Wemos
est alimenté dès qu'**au moins un** connecteur est sous tension.

```
Fan9 +12V ────┬───|>|───┬──── VIN+ (vers régulateur Wemos)
              │  D1     │
              │ 1N5819  │
Fan6 +12V ────┤         ├─── C1 100µF/25V ─── GND
              │  D2     │
              └───|>|───┘
                 1N5819

Fan9 GND ─────┬──── GND commun (masse étoile)
Fan6 GND ─────┤
Wemos GND ────┘
```

| Réf | Composant | Rôle |
|-----|-----------|------|
| D1, D2 | 1N5819 (Schottky) | OR passif des deux +12 V |
| C1 | 100 µF / 25 V électrolytique | Découplage alimentation |
| C2 | 100 nF céramique | Découplage HF près du Wemos |

**Branchement Wemos :** connecter VIN+ au pin **5V** du Wemos D1 mini.
Le régulateur AMS1117-3.3 embarqué sur la carte accepte jusqu'à ~15 V en entrée.
Consommation typique : 80–150 mA (WiFi actif).

> **Note thermique :** à 12 V d'entrée, le régulateur linéaire dissipe ~1 W.
> C'est acceptable en permanence pour un ESP8266. Si la tension d'entrée dépasse
> 12 V, ajouter un régulateur buck 12 V → 5 V en amont.

---

## 2. Circuits d'entrée — Noctua → ESP8266

Chaque ventilateur Noctua 3 broches (GND, +12V, TACH) est câblé ainsi :

```
Noctua +12V ──── Fan9/Fan6 +12V de l'onduleur (alimente le ventilateur)
Noctua GND  ──── GND commun
Noctua TACH ──┬── R1 4.7kΩ ──── 3.3V (pin 3V3 du Wemos)
              │
              ├── GPIO entrée (D2 ou D1)
              │
              └── C3 100nF ──── GND  (filtrage optionnel)
```

### Canal 0 — Ventilateur 9 cm (NF-A9-FLX)

| Signal | Broche Wemos | GPIO |
|--------|-------------|------|
| TACH IN | D2 | GPIO4 |

### Canal 1 — Ventilateur 6 cm (NF-A6x25-FLX)

| Signal | Broche Wemos | GPIO |
|--------|-------------|------|
| TACH IN | D1 | GPIO5 |

| Réf | Composant | Valeur | Rôle |
|-----|-----------|--------|------|
| R1, R2 | Résistance pull-up | 4.7 kΩ | Tirer le tach à 3.3 V (niveau logique ESP) |
| C3, C4 | Condensateur | 100 nF | Filtre anti-rebond (optionnel) |

**Pourquoi 4.7 kΩ vers 3.3 V :** le signal tach Noctua est open-collector.
La résistance de pull-up externe (4.7 kΩ) est plus robuste que le pull-up interne
de l'ESP (~40 kΩ) et garantit des fronts rapides, conformément aux recommandations
Noctua (≥ 2.7 kΩ pour ventilateurs 12 V).

---

## 3. Circuits de sortie — ESP8266 → Deye (open-collector)

Le signal tach de l'onduleur Deye est **open-collector** avec pull-up interne
de tension inconnue (3.3 V, 5 V ou 12 V). Notre sortie utilise un transistor NPN
en collecteur ouvert : nous ne faisons que tirer la ligne à GND, jamais la pousser
vers le haut. Compatible avec toute tension de pull-up sans modification.

```
                    +12V / 5V / 3.3V (pull-up INTERNE de l'onduleur Deye)
                         │
                         │  (résistance pull-up interne Deye, non représentée)
                         │
Deye TACH IN ────────────┤
                         │
                         ├── C_collecteur
                         │
                    ┌────┴────┐
                    │  NPN    │  (2N2222, BC547, S8050, etc.)
                    │ Q1 / Q2 │
                    └────┬────┘
                         │ Emetteur
                         │
                        GND

GPIO sortie ──── R3 1kΩ ──── Base (B)
```

### Canal 0 — Sortie vers Deye 9 cm

| Signal | Broche Wemos | GPIO | Transistor |
|--------|-------------|------|------------|
| TACH OUT | D6 | GPIO12 | Q1 (ex. BC547) |

### Canal 1 — Sortie vers Deye 6 cm

| Signal | Broche Wemos | GPIO | Transistor |
|--------|-------------|------|------------|
| TACH OUT | D7 | GPIO13 | Q2 (ex. BC547) |

| Réf | Composant | Valeur | Rôle |
|-----|-----------|--------|------|
| R3, R4 | Résistance de base | 1 kΩ | Limite le courant de base |
| Q1, Q2 | NPN | BC547 / 2N2222 / S8050 | Commutation open-collector |

**Fonctionnement :**
- GPIO HIGH (3.3 V) → transistor saturé → collecteur à ~0 V → Deye voit un front descendant
- GPIO LOW (0 V) → transistor bloqué → collecteur flottant → pull-up Deye tire la ligne haute

> **Aucune résistance de pull-up côté simulateur.** Le pull-up est fourni par
> l'onduleur. C'est ce qui rend la solution compatible 3.3 V / 5 V / 12 V.

---

## 4. LED d'état

La LED intégrée du Wemos D1 mini (broche D4 / GPIO2) est utilisée :

| État LED | Signification |
|----------|---------------|
| Clignotement lent (1 Hz) | En attente de signal tach d'entrée |
| Fixe allumée | Simulation active (au moins un canal) |
| Clignotement rapide (5 Hz) | Démarrage / mode configuration WiFi |

```
GPIO2 (D4) ──── LED intégrée Wemos ──── R_limit intégrée ──── 3.3V
(active LOW : LOW = LED allumée)
```

---

## 5. Câblage complet — Récapitulatif des connexions

```
WEMOS D1 MINI
┌────────────────────────────────┐
│ 5V  ◄──── VIN+ (via OR Schottky)│
│ GND ◄──── GND commun            │
│ 3V3 ──── R1 ──┬── TACH_IN_0    │
│               └── TACH_IN_1     │
│ D2 (GPIO4) ◄── TACH Noctua 9cm │
│ D1 (GPIO5) ◄── TACH Noctua 6cm │
│ D6 (GPIO12)── R3 ── B(Q1)     │
│ D7 (GPIO13)── R4 ── B(Q2)     │
│ D4 (GPIO2) ── LED status       │
└────────────────────────────────┘

Q1 collecteur ──► TACH Deye 9cm (connecteur ventilateur 9cm)
Q2 collecteur ──► TACH Deye 6cm (connecteur ventilateur 6cm)
Q1, Q2 émetteurs ──► GND commun

Noctua 9cm : +12V → Fan9 +12V, GND → GND, TACH → D2
Noctua 6cm : +12V → Fan6 +12V, GND → GND, TACH → D1
```

---

## 6. Schéma unifié (format texte)

```
                    ┌─── D1 (1N5819) ───┐
  Fan9 +12V ────────┤                   ├─── VIN+ ───┬── C1 100µF ── GND
                    └─── D2 (1N5819) ───┘              │
  Fan6 +12V ────────┤                                   │
                    (OR Schottky)                       ▼
                                              Wemos 5V pin
                                              Wemos GND ──── GND commun
                                              Wemos 3V3 ──┬── R1 4k7 ── TACH_N9 ── D2
                                                           └── R2 4k7 ── TACH_N6 ── D1

  Noctua 9cm +12V ── Fan9 +12V        D6 ── R3 1k ── B ─┐
  Noctua 9cm GND  ── GND commun                          │ Q1 (BC547)
  Noctua 9cm TACH ── D2 (via R1)                    C ───┼── TACH_Deye9
                                                         E ── GND

  Noctua 6cm +12V ── Fan6 +12V        D7 ── R4 1k ── B ─┐
  Noctua 6cm GND  ── GND commun                          │ Q2 (BC547)
  Noctua 6cm TACH ── D1 (via R2)                    C ───┼── TACH_Deye6
                                                         E ── GND
```

---

## 7. Notes de mise en service

1. **Ratios initiaux :** 2.5 par défaut. Ajuster via l'interface web
   (`http://192.168.4.1` en mode AP, ou l'IP STA une fois connecté).
   Ratio typique : Noctua ~1500 RPM → NMB d'origine ~3500 RPM → ratio ≈ 2.3.

2. **WiFi :** le point d'accès `DeyeFanSim` / `deye1234` est toujours actif.
   Configurer le WiFi domestique via la page web pour accès à distance.

3. **Vérification :** au démarrage, la LED clignote rapidement. Dès qu'un
   ventilateur Noctua tourne, la LED passe fixe allumée et les RPM simulés
   apparaissent sur la page web.

4. **Sécurité :** ne jamais connecter directement le TACH de l'onduleur à un
   GPIO ESP sans transistor — la tension de pull-up de l'onduleur peut dépasser
   3.3 V et endommager le microcontrôleur.
