# Schéma électronique — Simulateur tach pour onduleur Deye

Projet : remplacement des ventilateurs NMB par des Noctua silencieux sur
l'onduleur **Deye SUN-8K-SG05LP1-EU-AM2-P**, avec génération de signaux tach
(RPM) simulés via un **LOLIN Wemos D1 mini** (ESP8266).

Le firmware correspondant est `deye_fan.ino` (même dossier).

---

## 1. Vue d'ensemble (schéma bloc)

```
     CONNECTEUR VENTILO 1 (90 mm)                CONNECTEUR VENTILO 2 (60 mm)
     ┌───────────────┐                           ┌───────────────┐
+12V◄─┤GND   +12V     │                           │GND   +12V     │
     │      tach  o◄───┼── coupé ──► vers Deye 90 │      tach  o◄──┼── coupé ──► vers Deye 60
     │    (alim 12V)  │                           │    (alim 12V)  │
     └───────────────┘                           └───────────────┘
        │  │                                          │  │
        │  ├── tach Noctua 90 ──────────────────► IN0 (GPIO5)  OUT0 (GPIO14) ──► Deye 90
        │  └── tach Noctua 60 ──────────────────► IN1 (GPIO4)   OUT1 (GPIO12) ──► Deye 60
        │
+12V1 ──► 2 diodes OR (1N5822) ──► buck 12V→5V ──► VIN du Wemos D1 mini
+12V2 ──►                                   GND commun ◄── GND fan + GND Deye
                                     LED interne (GPIO2) = état simulation
```

Points clés :

- **Alimentation** : les deux connecteurs 12 V ne sont pas forcément alimentés
  en même temps ni à la même tension. Deux **diodes Schottky en OR** (1N5822
  ou SS34) prélèvent le 12 V disponible, puis un **module buck 12 V→5 V**
  (MP1584/LM2596) alimente le Wemos via **VIN**. Dès qu'au moins un
  ventilateur est alimenté, le système démarre.
- **Entrées tach** : résistance série 4k7 + **clamp 3,3 V** (BAT54S ou 1N4148).
  Le GPIO ne voit jamais plus de ~3,6 V quelle que soit la tension source
  (3,3 / 5 / 12 V) : aucune modification matérielle nécessaire.
- **Sorties** : transistors **NPN en collecteur ouvert**. Le collecteur est
  branché sur le fil tach d'origine retournant à l'onduleur : exactement comme
  le transistor Hall des ventilateurs NMB, quelle que soit la pull-up de
  l'onduleur (3,3 / 5 / 12 V).
- **LED** : la LED on-board (GPIO2/D4, active basse) indique l'état de la
  simulation. Une LED externe optionnelle peut être ajoutée sur D7 (GPIO13)
  via R 330 Ω.

---

## 2. Tableau de câblage

| Borne Wemos D1 mini | Signal                 | Destination |
|---------------------|------------------------|--------------------------------------------|
| VIN                 | +5 V                   | sortie du buck 12V→5V |
| GND                 | masse                  | masse commune (ventilateurs + Deye) |
| D1 = GPIO5          | tach entrée 90 mm      | R série 4k7 + clamp 3V3 ← tach Noctua 90 |
| D2 = GPIO4          | tach entrée 60 mm      | R série 4k7 + clamp 3V3 ← tach Noctua 60 |
| D5 = GPIO14         | tach simulée 90 mm     | R base 1k → base Q90 ; C collecteur → fil tach Deye 90 |
| D6 = GPIO12         | tach simulée 60 mm     | R base 1k → base Q60 ; C collecteur → fil tach Deye 60 |
| D4 = GPIO2 (interne)| LED d'état (active basse) | (on-board) — extensible D7=GPIO13 + R 330 Ω |
| 3V3                 | référence clamp        | relier au noeud milieu des diodes de clamp |

> Rappel connecteur Noctua 3 broches : **1 = GND (noir)** · **2 = +12 V (jaune)**
> · **3 = tach/sens (vert)**.

## 3. Sous-schéma : alimentation

```
  +12V conn.1 o────┬──► D_OR1 (1N5822 / SS34) ──o───  V_ALIM
                   │                            │
  +12V conn.2 o────┴──► D_OR2 (1N5822 / SS34) ──┤     (Schottky 3A/40 V :
                   │                            │      faible chute de tension)
                   │   V_ALIM ──┬── C1 470µF/25V ──┴── C2 100µF/25V ──┴── C3 100nF ──┴── GND
                   │            │                                                      │
                   │            └────────────────► module buck MP1584 / LM2596         │
                   │                                    (réglé 12 V → 5 V,             │
                   │                                     courant ≥ 400 mA)             │
                   │                                     │                             │
                   │                                     ▼                             │
                   │                                +5V ────────► VIN du Wemos         │
                   └──────────────────────────── GND ──────────► GND communal          │
```

- Consommation typique : Wemos ≈ 70–170 mA (WiFi actif), soit ~0,1–0,2 A sous
  12 V : le buck travaille très confortablement.
- Les diodes OR garantissent le bon fonctionnement si un seul connecteur est
  alimenté, ou si les deux sont alimentés (la plus haute tension l'emporte,
  aucun courant de retour d'un connecteur vers l'autre).
- **La masse** : joindre impérativement le GND du Wemos au GND des ventilateurs
  Noctua et au GND de l'onduleur (même réseau qu'à l'origine sur les
  connecteurs).

---

## 4. Sous-schéma : entrée tach (ventilateur → GPIO)

```
    tach Noctua 90
    (collecteur ouvert,      R_serie 4k7
     pull-up interne 12V) o────┬──/\/\/───┬──────o  GPIO5 (Wemos D1)
                               │          │
                               │          ├────  ◄  D_HIGH  (BAT54S ou 1N4148)
                               │          │      anode = noeud / cathode = +3,3 V
                               │          └──► D_LOW  (BAT54S ou 1N4148)
                               │                anode = GND / cathode = noeud
                               └── (optionnel) C 1nF ──► GND  (anti-parasites)
```

La conversion est **insensible à la tension de la source** :

| Niveau source | Noeud tach | Tension vue par GPIO |
|---------------|------------|-----------------------|
| 3,3 V  | 3,3 V      | 3,3 V (normal) |
| 5 V    | 5 V        | 3,3 V + ~0,3 V (clamp BAT54S) ≈ 3,6 V — ok |
| 12 V   | 12 V       | 3,3 V + ~0,3 V (clamp) ≈ 3,6 V — ok |
| 0 V    | 0 V        | 0 V — ok |

Courant dans le clamp en pic : (12 − 3,6)/4,7k ≈ 1,8 mA (négligeable pour le
rail 3,3 V du Wemos). En cas de surtension jusqu'à 24 V : (24 − 3,6)/4,7k ≈
4,3 mA — toujours sans danger pour la BAT54S/1N4148 et le GPIO. Le firmware
applique en outre un anti-rebond de 400 µs.

---

## 5. Sous-schéma : sortie simulée (GPIO → onduleur)

```
   GPIO14 (Wemos D5) o──── R_b 1kΩ ────┬──► base Q90
                                        │     émetteur ───────────────► GND commun
                                        │     collecteur ──┴─────┬─────► fil tach d'origine
                                        │                        │       (coupé du ventilateur,
                                        │                        │        remonte vers Deye)
                                        │                        └── (optionnel) R_pu 10k ──► +3,3 V
                                        │                             si la ligne n'a aucune pull-up
   (idem D6/GPIO12 pour la voie 60 mm avec Q60)
```

- **Collecteur ouvert** : quand le firmware veut une impulsion, le transistor
  conduit et tire la ligne vers ~0 V ; sinon il est bloqué et la pull-up de
  l'onduleur ramène la ligne haute. Ce comportement est identique au transistor
  Hall des ventilateurs d'origine, pour n'importe quelle tension de pull-up de
  l'onduleur (3,3 / 5 / 12 V).
- Courant de collecteur : pull-up typique 10 kΩ → ~1,2 mA à 12 V (très faible).
- Courant de base : (3,3 − 0,7)/1k ≈ 2,6 mA → saturation sûre pour 2N2222 /
  2N3904 / BC547 / S8050.
- À la mise sous tension, les GPIO sont en entrée flottante, les transistors
  restent bloqués : aucune impulsion parasite avant l'initialisation du
  firmware.

## 6. Liste des composants (BOM)

| Qté  | Référence          | Valeur / type                          | Usage |
|------|--------------------|----------------------------------------|-------|
| 1    | U1                 | Wemos D1 mini V2.3.0 (ESP-12S)         | microcontrôleur |
| 1    | U2                 | Module buck MP1584 ou LM2596 (12V→5V)  | alimentation |
| 2    | D_OR1, D_OR2       | 1N5822 ou SS34 (Schottky 3 A / 40 V)   | OR des 12 V |
| 2    | D_HIGH/D_LOW ×2    | BAT54S (ou 4 × 1N4148)                 | clamp 3,3 V entrées |
| 2    | Q90, Q60           | 2N2222A / 2N3904 / BC547 / S8050       | sorties collecteur ouvert |
| 2    | R_b ×2             | 1 kΩ 1/4 W                             | base des transistors |
| 2    | R_serie ×2         | 4,7 kΩ 1/4 W                           | série entrées tach |
| 0-2  | R_pu (optionnel)   | 10 kΩ                                  | pull-up de secours lignes de sortie |
| 1    | R_serie_LED (opt.) | 330 Ω                                  | LED externe D7 |
| 1    | C1                 | 470 µF / 25 V                          | lissage V_ALIM |
| 1    | C2                 | 100 µF / 25 V                          | lissage V_ALIM |
| 3    | C3, C5, ...        | 100 nF céramique                       | découplages 5 V / 3,3 V |
| 1    | C4                 | 10 µF céramique                        | sortie buck |
| 2    | F1, F2 (optionnels)| Fusible lent 1 A                       | protection des +12 V |
| 4    | Connecteurs        | headers 3 broches 2,54 mm / cosses     | raccordement des ventilateurs |
| —    | Fil                | 0,5 mm² ou nappe multibrins            | câblage |

---

## 7. Raccordement à l'onduleur (réversible)

- Le fil **tach** de chaque ventilateur d'origine est **coupé** et inséré dans
  le montage : tach Noctua → entrée ESP8266 → sortie simulée → fil retournant à
  l'onduleur.
- Les **+12 V et GND** des connecteurs restent branchés sur les ventilateurs
  Noctua (en parallèle) et alimentent le circuit du Wemos (via les diodes OR).
- Aucune modification de l'onduleur : le montage est réversible.
- **Vérification** : à l'oscilloscope sur la sortie, on doit mesurer un signal
  carré à la fréquence attendue (f = RPM_simulé / 60 × 2 pules/tour), identique
  à celui d'un tach fan, avec un jitter « quasi nul » même pendant un transfert
  réseau (téléchargement + polling web simultanés).

---

## 8. Choix du ratio et mise au point

RPM vues par l'onduleur = RPM réels Noctua × ratio.

Ratio recommandé : `ratio = RPM(ventilateur d'origine) / RPM(Noctua réel)`.

Ordres de grandeur (à vérifier sur la page web, les valeurs réelles dépendent
de la tension d'alimentation appliquée par l'onduleur) :

| Voie | Ventilo d'origine | Noctua de remplacement | Ratio typique |
|------|-------------------|------------------------|---------------|
| 90 mm | NMB 09225VE-12N-CU  | NF-A9-FLX (≈ 3 000 tr/min) | 1,3 – 2,0 |
| 60 mm | NMB 06025VE-12N-CL  | NF-A6x25-FLX (≈ 3 000 tr/min) | 1,3 – 1,8 |

Procédure de mise au point :
1. Alimenter l'onduleur (les ventilateurs sont entraînés par lui).
2. Ouvrir `http://192.168.4.1` (AP « DeyeFanAP ») ou l'IP STA.
3. Lire les RPM réels de chaque voie, régler le ratio pour retrouver les RPM
   d'origine, puis « Enregistrer » (persistant en EEPROM).
4. Si l'onduleur signale encore une ventilation insuffisante, augmenter le
   ratio (jusqu'à 20) ou vérifier les RPM mesurés (valeur cible = RPM d'origine
   ± 10 %).
5. Contrôler que la LED est allumée (simulation active) une fois l'onduleur
   sous tension.

> Rappel : le choix **2 impulsions par tour** est celui des ventilateurs Noctua
> (et des NMB d'origine). Il est paramétrable dans `deye_fan.ino`
> (`#define PULSES_PER_REV`).