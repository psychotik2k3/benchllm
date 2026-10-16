# Schéma électronique — Simulateur tach ventilateurs Deye

Onduleur : **Deye SUN-8K-SG05LP1-EU-AM2-P2**
Ventilateurs d'origine remplacés :
- 1× NMB 06025VE-12N-CL (6 cm, 12 V) → **2× Noctua NF-A6x25-FLX** (canal B)
- 1× NMB 09225VE-12N-CU (9 cm, 12 V) → **2× Noctua NF-A9-FLX** (canal A)

Microcontrôleur : **Wemos D1 mini V2.3.0** (ESP-12S / ESP8266, 80 MHz, 3.3 V logique).

---

## 1. Vue d'ensemble

```
              ONDULEUR DEYE
        +-----------------------+
        | Connecteur FAN 9 cm   |  GND  +12V  TACH_in_9
        | Connecteur FAN 6 cm   |  GND  +12V  TACH_in_6
        +-----------------------+
                 |        |          |
                 v        v          v
   +--------------------------------------------------+
   |                CARTE SIMULATEUR                   |
   |  [Alim 12V -> 5V]   [ESP8266 Wemos D1 mini]       |
   |  (OR-ing diodes)    - 2 entrees tach Noctua       |
   |                     - 2 sorties tach (NPN O.C.)   |
   |                     - serveur web AP+STA          |
   |                     - LED d'etat                 |
   +--------------------------------------------------+
        |          |          |
        v          v          v
   +-----------------------------------------+
   | 2x Noctua NF-A9-FLX  (alim 12V)         |
   | 2x Noctua NF-A6x25-FLX (alim 12V)       |
   +-----------------------------------------+
```

Chaque ventilateur Noctua est alimenté **directement** par le +12 V du
connecteur onduleur correspondant (les Noctua ne passent PAS par le Wemos).
Seul le signal tach Noctua est lu par le Wemos, et seul le signal tach
simulé est renvoyé à l'onduleur.

---

## 2. Alimentation (deux connecteurs 12 V, pas toujours alimentés ensemble)

**Contrainte** : les deux connecteurs ventilateur de l'onduleur ne sont pas
forcément alimentés en même temps, ni à la même tension. Le Wemos doit
pouvoir être alimenté dès que l'un OU l'autre est présent.

**Solution** : OR-ing par diodes Schottky + unique buck 12 V → 5 V.

```
 Connecteur 9 cm  +12V ────┬──[D1 Schottky 1N5819]──┐
                           │   (anode -> cathode)    │
                           │                         ├── +Vin ─► [Buck LM2596]
 Connecteur 6 cm  +12V ────┴──[D2 Schottky 1N5819]──┘            (reglable 5V)
                                                                      │
                                                                      └─► +5V pin Wemos D1 mini
 GND (commun onduleur) ────────────────────────────────────► GND Wemos D1 mini
```

- **D1, D2** : diodes Schottky **1N5819** (1 A, Vf ~0.3 V, 40 V). Le courant
  ne peut remonter d'un connecteur vers l'autre : si seul le 9 cm est
  alimenté, le 6 cm ne reçoit rien sur sa ligne +12 V via la carte.
- **Buck** : module **LM2596** (entrée jusqu'à 40 V, sortie réglée à
  **5.0 V**). On alimente le Wemos sur sa pin **5V** (le régulateur
  embarqué ME6211 produit le 3.3 V). Ne pas utiliser la pin 3V3 directement
  depuis le buck (le régulateur interne du Wemos est plus sûr).
- **C1** : condensateur 220 µF / 25 V sur l'entrée du buck (découplage).
- **C2** : condensateur 100 µF / 10 V sur la sortie 5 V du buck.
- Le LM2596 accepte une entrée 7 V → 35 V : couvre tous les cas
  (12 V nominal, 13.2 V max ventilateur, et même 5 V si l'onduleur baisse).

> Note : si un connecteur est à 5 V et l'autre à 12 V, l'OR-ing prend
> toujours la tension la plus haute (la diode côté 12 V conduit). Le buck
> régule en sortie 5 V dans tous les cas. Aucune modification matérielle.

---

## 3. Entrées tach Noctua → ESP8266

Les Noctua NF-A9-FLX et NF-A6x25-FLX sont des ventilateurs **3 broches**
(GND, +12V, TACH). Le signal tach est **open-collector** et produit
**2 impulsions par révolution** (source : Noctua PWM white paper). Il ne
fournit donc aucune tension propre : on tire la ligne vers la tension
logique du Wemos (**3.3 V**), ce qui élimine tout problème de niveau.

```
 Noctua NF-A9 (canal A)  TACH ──┬──[R_pull 4.7kΩ]──► +3.3V (Wemos pin 3V3)
                                │
                                ├──[R_série 1 kΩ]──► D6 / GPIO12  (entrée, pull-up ON)
                                │
 GND commun ─────────────────────┴────────────────► GND Wemos

 Noctua NF-A6x25 (canal B) TACH ─┬──[R_pull 4.7kΩ]──► +3.3V
                                 │
                                 ├──[R_série 1 kΩ]──► D7 / GPIO13 (entrée, pull-up ON)
                                 │
 GND commun ──────────────────────┴────────────────► GND Wemos
```

- **R_pull 4.7 kΩ** (externe) : pull-up vers 3.3 V. Le pull-up interne du
  ESP8266 (~50 kΩ) est trop faible pour un front propre à fréquence
  élevée ; on ajoute donc une résistance externe de 4.7 kΩ (compatible
  avec la spec Noctua : pull-up ≥ 2.7 kΩ, courant < 5 mA).
- **R_série 1 kΩ** : protection de l'entrée GPIO (jamais nécessaire en
  fonctionnement normal, mais protège contre les erreurs de câblage).
- Le tach Noctua est open-collector : quand le transistor interne du
  ventilateur est saturé, il tire la ligne vers GND ; sinon la ligne est
  à 3.3 V via R_pull. On lit donc un signal 0/3.3 V parfaitement adapté
  au GPIO de l'ESP8266. **Aucun diviseur de tension nécessaire côté entrée.**

> Les **deux** Noctua d'un même canal (les 2× NF-A9 pour le canal A, les
> 2× NF-A6x25 pour le canal B) ont leurs sorties tach mises en parallèle
> sur la même entrée ESP (wired-OR open-collector). En pratique les deux
> Noctua identiques tournent à la même vitesse, le signal reste lisible.
> Si on préfère ne lire qu'un seul Noctua par canal, ne câbler que le
> tach d'un des deux.

---

## 4. Sorties tach ESP8266 → onduleur Deye

**Contrainte** : la tension que l'onduleur applique sur sa broche tach est
inconnue (3.3 V, 5 V ou 12 V). La solution doit fonctionner dans tous les
cas sans modification matérielle.

**Solution** : sortie **collecteur ouvert via transistor NPN**. La broche
tach de l'onduleur est tirée vers sa propre tension (inconnue) par sa
résistance de pull-up interne ; notre transistor ne fait que tirer la ligne
vers GND. La tension côté onduleur ne nous concerne pas, tant que le
transistor la supporte (Vceo ≥ 30 V pour tous les candidats ci-dessous).

```
                           +Tach_onduleur (3.3 / 5 / 12 V, inconnu)
                                  │  (pull-up interne onduleur)
                                  │
                                  ├── Collecteur  Q1 (NPN)
                                  │                │
                                  │                Emetteur ──► GND commun
                                  │                │
   D5 / GPIO14 (sortie canal A) ──┴──[R_base 1 kΩ]──┤ Base
                                                    │
                                              [R_be 10 kΩ]
                                                    │
                                                   GND

   D2 / GPIO4 (sortie canal B) ────[R_base 1 kΩ]────┤ Base  Q2 (NPN)
                                                    │
                                              [R_be 10 kΩ]
                                                    │
                                                   GND
                                  │
                                  ├── Collecteur  Q2
                                  │
                                  Emetteur ──► GND commun
                                  │
                           +Tach_onduleur (canal B)
```

### Choix du transistor

Tous les NPN listés conviennent (Vceo ≥ 30 V, Ic ≥ 100 mA). On choisira
plutôt un transistor "généraliste robuste" pour la marge de courant :

| Référence | Vceo | Ic  | hFE min | Commentaire |
|-----------|------|-----|---------|-------------|
| **BC337** | 45 V | 800 mA | 100 | **Recommandé** : robuste, courant élevé |
| 2N2222    | 40 V | 600 mA | 100 | Bon choix, boîtier TO-92 |
| S8050 / SS8050 | 25 V | 1.5 A | 85 | Vceo un peu juste si pull-up 12 V + transitoires |
| 2N5551   | 160 V | 600 mA | 100 | Très haute tension, surdimensionné |
| BC547/548 | 45 V | 100 mA | 110 | OK mais marge de courant faible |
| 2N3904   | 40 V | 200 mA | 100 | OK |
| S9013/9014, C1815, C945, S9018 | 20–45 V | 100–700 mA | — | Vérifier Vceo ≥ 20 V |

**Recommandation finale** : **Q1 = Q2 = BC337** (ou 2N2222). Vceo = 45 V
couvre largement les 12 V même avec transitoires ; Ic = 800 mA couvre
n'importe quelle valeur de pull-up de l'onduleur (même 220 Ω à 12 V
= 55 mA).

- **R_base 1 kΩ** : limite le courant de base. Avec 3.3 V GPIO :
  Ib ≈ (3.3 − 0.7) / 1000 = 2.6 mA → saturé pour Ic jusqu'à ~260 mA
  (hFE_forced ~100), largement assez.
- **R_be 10 kΩ** : pull-down de base, garantit que le transistor est
  bloqué quand la broche ESP est en reset / haute-impédance (au boot,
  les GPIO peuvent flotter brièvement).

> **Inversion logique** : quand la broche ESP est HIGH, le transistor
> sature et tire la ligne onduleur vers GND (niveau bas). Quand la
> broche ESP est LOW, le transistor est bloqué et la ligne est tirée
> vers sa tension de pull-up par l'onduleur (niveau haut). Le signal est
> donc **inversé**, mais la **fréquence est conservée** — or l'onduleur
> ne compte que des flancs (rising edges), l'inversion n'a aucun effet
> sur la mesure de RPM.

---

## 5. LED d'état

La LED intégrée du Wemos D1 mini (GPIO2, **active low**) est utilisée :

- **Simulation active** (signal présent et sortie générée) → LED **allumée fixe**
- **Attente** (aucun signal tach) → LED **clignote lentement** (~2 Hz)

Aucun composant externe nécessaire.

---

## 6. Bouton de reset config (optionnel)

- **D1 / GPIO5** → bouton poussoir vers GND, pull-up interne activée.
- Appui **long ≥ 3 s** → remet la config WiFi/ratios par défaut et redémarre.

---

## 7. Récapitulatif des broches ESP8266

| Wemos | GPIO | Rôle | Sens |
|-------|------|------|------|
| D6 | GPIO12 | Entrée tach Noctua canal A (9 cm) | IN (pull-up) |
| D7 | GPIO13 | Entrée tach Noctua canal B (6 cm) | IN (pull-up) |
| D5 | GPIO14 | Sortie tach simulé canal A → base Q1 | OUT |
| D2 | GPIO4  | Sortie tach simulé canal B → base Q2 | OUT |
| D4 | GPIO2  | LED d'état intégrée (active low) | OUT |
| D1 | GPIO5  | Bouton reset config (optionnel) | IN (pull-up) |
| 5V | —      | Alim 5 V depuis buck LM2596 | PWR |
| GND | —     | Masse commune | PWR |

---

## 8. Liste des composants (BOM)

| Repère | Valeur / Référence | Qté | Rôle |
|--------|--------------------|-----|------|
| U1 | Wemos D1 mini V2.3.0 (ESP-12S) | 1 | MCU |
| U2 | Module buck LM2596 (réglé 5 V) | 1 | Alim 12 V → 5 V |
| D1, D2 | 1N5819 (Schottky 1 A / 40 V) | 2 | OR-ing alimentation |
| Q1, Q2 | BC337 (ou 2N2222) NPN | 2 | Sortie tach open-collector |
| R_pull1, R_pull2 | 4.7 kΩ 1/4 W | 2 | Pull-up tach Noctua vers 3.3 V |
| R_s1, R_s2 | 1 kΩ 1/4 W | 2 | Série protection entrée GPIO |
| R_base1, R_base2 | 1 kΩ 1/4 W | 2 | Limitation courant base NPN |
| R_be1, R_be2 | 10 kΩ 1/4 W | 2 | Pull-down base NPN |
| C1 | 220 µF / 25 V | 1 | Découplage entrée buck |
| C2 | 100 µF / 10 V | 1 | Découplage sortie 5 V |
| SW1 | Bouton poussoir | 1 | Reset config (optionnel) |

---

## 9. Câblage complet (résumé)

```
  ONDULEUR DEYE
  ├─ Connecteur FAN 9 cm
  │   ├─ GND    ──► GND commun (et Q1 emetteur, et Noctua GND)
  │   ├─ +12V   ──► D1 anode  /  Noctua NF-A9 #1 + #2 (pin +12V)
  │   └─ TACH   ──► Q1 collecteur  (sortie simulée canal A)
  │
  ├─ Connecteur FAN 6 cm
  │   ├─ GND    ──► GND commun (et Q2 emetteur, et Noctua GND)
  │   ├─ +12V   ──► D2 anode  /  Noctua NF-A6x25 #1 + #2 (pin +12V)
  │   └─ TACH   ──► Q2 collecteur  (sortie simulée canal B)
  │
  └─ (les GND des deux connecteurs sont reliés en commun)

  NOCTUA (alimentés par l'onduleur)
  ├─ NF-A9 #1 et #2  : GND → commun, +12V → +12V connecteur 9 cm,
  │                   TACH → (parallèle) → R_pull 4.7k → 3.3V, R_s 1k → D6
  └─ NF-A6x25 #1 #2 : GND → commun, +12V → +12V connecteur 6 cm,
                      TACH → (parallèle) → R_pull 4.7k → 3.3V, R_s 1k → D7

  WEMOS D1 MINI
  ├─ 5V   ←─ buck LM2596 sortie 5V
  ├─ GND  ──► commun
  ├─ D6   ←─ tach Noctua canal A (via R_s 1k)
  ├─ D7   ←─ tach Noctua canal B (via R_s 1k)
  ├─ D5   ──► R_base 1k → base Q1
  ├─ D2   ──► R_base 1k → base Q2
  ├─ D4   = LED intégrée (état)
  └─ D1   ←─ bouton reset config (optionnel)
```

---

## 10. Notes de conception et conformité au cahier des charges

1. **Tension tach onduleur inconnue (3.3/5/12 V)** : résolue par sortie
   **open-collector NPN**. Aucune modification matérielle selon la tension.
2. **Alimentation depuis 2 connecteurs pas toujours alimentés ensemble** :
   résolue par **OR-ing Schottky** + buck unique.
3. **Signal de sortie stable et sans jitter malgré WiFi actif** : résolu
   par `startWaveform()` qui utilise **Timer1 en NMI** (priorité > WiFi),
   synchronisé sur le compteur de cycles CPU 80 MHz. Voir commentaires en
   tête de `deye_fan.ino`.
4. **Isolation lecture/génération vs WiFi/HTTP** : les ISR d'entrée
   horodotent avec `ESP.getCycleCount()` (insensible au jitter WiFi) ;
   le serveur web ne touche jamais aux broches de signal, seulement à
   des variables volatiles protégées par `noInterrupts()`.
5. **Ratio réglable sans recompiler ni recâbler** : via interface web
   (POST `/setRatio`), persisté en LittleFS.
6. **WiFi AP+STA simultanés, paramètres persistants** : `WIFI_AP_STA` +
   `WiFi.persistent(true)` (STA) + LittleFS (AP, ratios).
7. **LED d'état** : allumée fixe en simulation, clignotante en attente.
