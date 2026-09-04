# Schéma électronique — Simulateur de tachymètre pour Deye SUN-8K-SG05LP1-EU-AM2-P

Installation complète : alimentation, deux circuits d'entrée (Noctua → ESP8266) et deux
circuits de sortie (ESP8266 → Deye).

> **Sécurité.** Un onduleur hybride reste sous tension dangereuse après la coupure AC :
> les condensateurs du bus DC et la batterie haute tension y contribuent. Coupez l'AC,
> le PV et la batterie, consignez, attendez au moins 10 minutes, puis vérifiez
> l'absence de tension avant toute intervention. Ce document est fourni sans garantie.

---

## 1. Synoptique

```
   Connecteur ventilateur DEYE 92 mm            Connecteur ventilateur DEYE 60 mm
   ┌───────────────────────────┐                ┌───────────────────────────┐
   │  +12V    GND    TACH(in)  │                │  +12V    GND    TACH(in)  │
   └───┬───────┬────────┬──────┘                └───┬───────┬────────┬──────┘
       │       │        │                           │       │        │
       │       │        │   ┌───────────────────────┼───────┼────────┼──┐
       │       └────────┼───┼──── masse commune ────┘       │        │  │
       │                │   │                               │        │  │
       │  2× NF-A9-FLX  │   │                2× NF-A6x25-FLX│        │  │
       ├──► +12V        │   │                               ├──► +12V│  │
       │    GND ────────┼───┤                               │    GND─┤  │
       │    TACH ──┐    │   │                               │  TACH─┐│  │
       │           │    │   │                               │       ││  │
   ┌───┴───┐       │    │   │                           ┌───┴───┐   ││  │
   │ D1    │       │    │   │                           │ D2    │   ││  │
   │Schottky       │    │   │                           │Schottky   ││  │
   └───┬───┘       │    │   │                           └───┬───┘   ││  │
       └───────────┼────┼───┴──────── OU diode ─────────────┘       ││  │
                   │    │        │                                  ││  │
                   │    │   ┌────┴─────────────┐                    ││  │
                   │    │   │ Buck 12V → 5,0V  │                    ││  │
                   │    │   │ MP1584EN/LM2596  │                    ││  │
                   │    │   └────┬─────────────┘                    ││  │
                   │    │        │ 5 V                              ││  │
                   │    │   ┌────┴──────────────────────────────┐   ││  │
                   │    │   │        WEMOS D1 MINI V2.3.0       │   ││  │
                   │    │   │                                   │   ││  │
                   │ CH1 IN │ D5/GPIO14 ◄── étage entrée A ◄────┼───┘│  │
                   │        │ D6/GPIO12 ◄── étage entrée B ◄────┼────┘  │
                   │        │                                   │       │
                   │        │ D1/GPIO5  ──► étage sortie A ─────┼───────┘  (vers TACH 92 mm)
                   │        │ D2/GPIO4  ──► étage sortie B ─────┼──────────(vers TACH 60 mm)
                   │        │ D4/GPIO2  ──► LED intégrée        │
                   │        └───────────────────────────────────┘
```

Principe : **les fils tach des Noctua ne sont jamais reliés à l'onduleur.** Le Wemos les
lit, puis fabrique un signal tach entièrement synthétique qu'il présente au Deye.

---

## 2. Alimentation

### 2.1 Contrainte

Les deux connecteurs ventilateur ne sont pas forcément alimentés en même temps, ni à la
même tension (l'onduleur peut piloter le 60 mm et le 92 mm indépendamment, et un
connecteur inactif peut se retrouver à 0 V ou à une tension partielle). Il faut donc :

- prélever le 12 V sur **les deux** connecteurs pour que le Wemos ne redémarre pas quand
  un seul ventilateur est arrêté ;
- **interdire tout courant de retour** d'un connecteur vers l'autre, ce qui provoquerait
  une rotation parasite du ventilateur censé être à l'arrêt et fausserait la régulation
  de l'onduleur.

### 2.2 Solution : OU diode + convertisseur abaisseur

```
  DEYE 92mm +12V ──[F1 500 mA]──►│─────┐        D1 : 1N5819 ou SS34 (Schottky)
                                 D1    │
                                       ├──── VBUS (≈ 11,6 V quand 12 V présent)
                                 D2    │
  DEYE 60mm +12V ──[F2 500 mA]──►│─────┘        D2 : 1N5819 ou SS34 (Schottky)

  VBUS ──┬──────────────┬────────────► IN+ du module buck
         │              │
       C1 100 µF      C2 100 nF
       25 V él.       céram.
         │              │
  GND ───┴──────────────┴────────────► IN− du module buck


  Module buck  OUT+ ──┬────────┬──────────────► broche 5V du D1 mini
  (réglé à 5,00 V)    │        │
                    C3 220 µF C4 100 nF
                    10 V él.  céram.
                      │        │
               OUT− ──┴────────┴──────────────► broche G (GND) du D1 mini
```

**Pourquoi des diodes Schottky** — leur chute directe (0,3 à 0,45 V à ces courants)
limite la perte et laisse ≥ 11,5 V au buck, très au-dessus de son minimum. Une 1N4007
conviendrait fonctionnellement (0,7 V) mais chauffe davantage pour rien.

**Pourquoi un buck et non le régulateur du Wemos** — la broche `5V` du D1 mini attaque
directement le LDO RT9013 dont l'entrée est spécifiée à 5,5 V maximum. **Appliquer 12 V
sur la broche 5V détruit la carte.** La broche `VIN`/`RAW` que l'on trouve sur d'autres
modules n'existe pas ici. Un module MP1584EN (4,5–28 V en entrée, 3 A) ou LM2596 réglé à
5,00 V avant câblage est donc obligatoire.

**Réglage à faire avant de brancher le Wemos** : alimenter le module en 12 V à l'établi,
régler le potentiomètre à 5,00 V ± 0,05 V au multimètre, puis seulement ensuite raccorder
la carte.

**Bilan de consommation**

| Charge | Courant à 12 V |
|---|---|
| 2 × NF-A9-FLX (0,08 A chacun) | 0,16 A |
| 2 × NF-A6x25-FLX (0,12 A chacun) | 0,24 A |
| Wemos via buck (5 V / 150 mA crête, rendement 85 %) | 0,08 A |
| **Total crête** | **≈ 0,48 A** |

À comparer aux 0,50 A (60 mm) et 0,72 A (92 mm) que consommaient les NMB d'origine : le
câblage de l'onduleur est largement dimensionné, et le montage reste sous la charge
initiale même en additionnant tout sur un seul connecteur.

**Option confort** — un condensateur de 470 à 1000 µF / 25 V supplémentaire sur VBUS
permet de traverser une coupure brève des deux connecteurs sans redémarrer le Wemos.

### 2.3 Vérification impérative des masses

Le montage suppose que les broches GND des deux connecteurs sont **au même potentiel**
(masse châssis commune), ce qui est le cas usuel sur cette famille d'onduleurs.

**À vérifier au multimètre, onduleur hors tension, en mode continuité :** résistance
entre la broche GND du connecteur 60 mm et celle du connecteur 92 mm. On attend < 1 Ω.

Si les masses ne sont **pas** communes (mesure ouverte ou tension continue entre les
deux), le montage ci-dessus n'est pas applicable tel quel : il faut alors isoler
galvaniquement chaque canal avec un optocoupleur (PC817 / 4N35) en remplacement de
l'étage de sortie NPN, chaque émetteur de phototransistor étant relié à la masse de
*son* connecteur. Voir § 5.3.

---

## 3. Étage d'entrée : Noctua → ESP8266

Le tach d'un Noctua est une sortie **collecteur ouvert** (transistor interne vers la
masse du ventilateur), à raison de **2 impulsions par tour**. Il n'impose aucune tension :
c'est nous qui fournissons la résistance de tirage, donc le niveau haut est
intrinsèquement à 3,3 V et il n'y a aucun risque pour l'ESP8266.

### 3.1 Variante A — couplage direct filtré (recommandée, 3 composants par canal)

```
                        3V3 (broche 3V3 du D1 mini)
                          │
                        R1 4k7
                          │
  TACH Noctua ──[R2 470]──┼──────────────► D5/GPIO14   (canal 1)
  (fil jaune)             │
                        C5 22 nF
                          │
                         GND
```

Identique pour le canal 2 : `R3 4k7`, `R4 470`, `C6 22 nF` → `D6/GPIO12`.

**Niveaux logiques obtenus**

- Transistor du ventilateur conducteur (V<sub>CE(sat)</sub> ≈ 0,2 V) :
  V<sub>broche</sub> = 0,2 + (3,3 − 0,2) × 470 / (4700 + 470) = **0,48 V**
  → largement sous V<sub>IL(max)</sub> ≈ 0,25 × 3,3 = 0,83 V.
- Transistor bloqué : V<sub>broche</sub> = **3,3 V**
  → largement au-dessus de V<sub>IH(min)</sub> ≈ 0,75 × 3,3 = 2,48 V.

**Rôle du filtre RC** — le front descendant, celui qui sert d'horodatage dans le
firmware, voit τ = (4k7 ∥ 470) × 22 nF = **9,4 µs** : il reste net. Le front montant est
volontairement ralenti (τ = 4k7 × 22 nF = 103 µs, soit ≈ 143 µs pour franchir le seuil),
ce qui filtre les parasites captés dans un boîtier d'onduleur commutant plusieurs
kilowatts. À 3000 tr/min le tach a une demi-période de 5 ms : 143 µs représentent 2,9 %,
sans conséquence puisque la mesure porte sur les fronts descendants.

Le firmware active en plus le pull-up interne de l'ESP8266 (30–100 kΩ), négligeable
devant 4k7 : le montage fonctionne même si R1 est oubliée, avec une immunité moindre.

### 3.2 Variante B — tampon NPN inverseur (câbles longs, environnement bruyant)

Si les câbles tach dépassent une vingtaine de centimètres ou passent près des
inductances de puissance, un étage NPN par canal apporte un front franc de 0 à 3,3 V et
protège la broche contre une erreur de câblage (le 12 V ne provoquerait que 1,1 mA dans
la base).

```
                        3V3
                          │
                        R6 4k7
                          │
                          ├──────────────► D5/GPIO14
                          │
  TACH Noctua ──[R5 10k]──┤ B         C
                          │  ┌────────┐
                          └──┤ Q3     │
                        R7 10k│ 2N3904│
                          │  └───┬────┘ E
                         GND     │
                                GND
```

`R5 10k` (base), `R7 10k` (base–émetteur, garantit le blocage), `R6 4k7` (collecteur),
`Q3` = 2N3904 / BC547 / 2N2222 indifféremment. Ajoutez `C 10 nF` du collecteur à la
masse pour conserver le filtrage.

Cet étage **inverse** le signal. C'est sans effet : il y a toujours 2 fronts descendants
par tour sur la broche de l'ESP, et le firmware ne mesure que l'intervalle entre deux
fronts descendants consécutifs. Aucune modification logicielle n'est nécessaire.

### 3.3 Câblage des ventilateurs

Chaque connecteur du Deye alimente **deux** Noctua en parallèle. Un seul des deux fils
tach est exploité ; l'autre est coupé court et isolé (thermorétractable).

| Connecteur Deye | Ventilateurs | Fil tach lu | Étage d'entrée | Broche ESP |
|---|---|---|---|---|
| 92 mm | 2 × NF-A9-FLX | celui du ventilateur n° 1 | canal 1 | D5 / GPIO14 |
| 60 mm | 2 × NF-A6x25-FLX | celui du ventilateur n° 1 | canal 2 | D6 / GPIO12 |

Les deux ventilateurs d'une même paire étant identiques et alimentés en parallèle,
mesurer l'un revient à mesurer l'autre à quelques pour cent près. Si l'un se bloque,
l'onduleur n'en sera pas averti : c'est le compromis inhérent au doublement des
ventilateurs. Le compteur « fronts rejetés » de l'interface web permet toutefois de
détecter une entrée qui se dégrade.

---

## 4. Étage de sortie : ESP8266 → Deye

### 4.1 Contrainte de tension inconnue

L'onduleur applique sur sa broche tach une résistance de tirage vers une tension
**inconnue : 3,3 V, 5 V ou 12 V**. Aucun étage qui *impose* une tension ne peut satisfaire
les trois cas sans modification.

La réponse est de **reproduire exactement ce que fait le ventilateur d'origine** : un
collecteur ouvert. La datasheet du NMB 09225VE le confirme — sortie collecteur ouvert,
V<sub>CE(max)</sub> = 15 V, I<sub>C(max)</sub> = 5 mA, V<sub>CE(sat)</sub> ≤ 0,5 V. Un
transistor NPN qui ne fait que tirer la ligne à la masse est indifférent à la tension de
tirage : il fonctionne à l'identique en 3,3 V, 5 V ou 12 V, **sans aucune adaptation
matérielle**. C'est la seule topologie qui satisfait la contrainte.

### 4.2 Schéma (un exemplaire par canal)

```
                                     ┌──── TACH du DEYE 92 mm
                                     │     (tirée en interne vers 3,3 / 5 / 12 V
                                     │      par l'onduleur — peu importe)
                                     │
                                     │  ┌── C7 1 nF (option, anti-parasites)
                                     │  │
                                  C  │  │
                              ┌──────┴──┴──┐
  D1/GPIO5 ──[R8 2k2]──┬──────┤ B   Q1     │
                       │      │  2N2222A   │
                     R9 10k   └──────┬─────┘ E
                       │             │
                      GND ───────────┴──── GND (masse commune Deye / Wemos)
```

Canal 2 identique : `R10 2k2`, `R11 10k`, `Q2` → depuis `D2/GPIO4` vers le TACH du
connecteur 60 mm.

### 4.3 Dimensionnement

**Résistance de base R8 = 2k2.** I<sub>B</sub> = (3,3 − 0,75) / 2200 = **1,16 mA**, pour
un courant de sortie GPIO de l'ESP8266 spécifié à 12 mA : très confortable. Avec un
h<sub>FE</sub> minimal de 75 (2N2222A à I<sub>C</sub> = 150 mA), le transistor peut
absorber **≥ 85 mA**, donc saturer profondément quel que soit le tirage du Deye.

**Résistance base–émetteur R9 = 10k.** Indispensable, et pas seulement par principe :
pendant le reset et les ~300 ms de démarrage de l'ESP8266, GPIO5 est en haute impédance.
Sans R9 la base flotterait et le transistor pourrait conduire de façon erratique,
c'est-à-dire injecter des impulsions parasites dans le compteur de l'onduleur. R9 force
le blocage : au démarrage la ligne tach reste **relâchée**, ce que l'onduleur interprète
comme un ventilateur arrêté — état franc et transitoire, sans risque de valeur aberrante.
Le firmware met d'ailleurs la broche à l'état bas *avant* de la configurer en sortie,
pour la même raison.

**Choix du transistor.** V<sub>CEO</sub> ≥ 20 V et I<sub>C</sub> ≥ 100 mA suffisent.
Parmi votre stock, par ordre de préférence :

| Transistor | V<sub>CEO</sub> | I<sub>C(max)</sub> | Avis |
|---|---|---|---|
| 2N2222A | 40 V | 800 mA | idéal |
| BC337 | 45 V | 800 mA | idéal |
| S8050 / SS8050 | 25 V | 700 mA (1,5 A) | très bien |
| 2N5551 | 160 V | 600 mA | très bien |
| 2N3904, BC547, BC548, S9013, S9014, C1815, C945 | 30–45 V | 100–500 mA | convient largement |

Tous vos NPN passent, l'exigence réelle étant très basse. Évitez seulement de choisir un
BC548 si vous découvrez à la mesure un tirage anormalement raide.

**Faut-il une résistance série sur le collecteur ?** Non, et il vaut mieux s'en passer :
elle s'ajouterait à V<sub>CE(sat)</sub> et dégraderait le niveau bas vu par l'onduleur.
La seule raison de mettre 100 Ω serait un tirage très raide (par exemple 100 Ω vers 12 V,
soit 120 mA) — voir la mesure ci-dessous. Avec un 2N2222A même ce cas passe sans
résistance (V<sub>CE(sat)</sub> ≈ 0,3 V à 120 mA).

### 4.4 Mesure préalable de la broche tach du Deye (5 minutes, à faire)

Ventilateur d'origine débranché, onduleur en fonctionnement normal :

1. **Tension de tirage** — voltmètre entre la broche TACH du connecteur et GND. La
   lecture donne directement la tension de tirage : ≈ 3,3 V, 5 V ou 12 V.
2. **Raideur du tirage** — insérer une résistance connue R<sub>t</sub> = 1 kΩ entre TACH
   et GND, relever V<sub>m</sub>. La résistance de tirage vaut
   R<sub>pu</sub> = R<sub>t</sub> × (V<sub>tirage</sub> − V<sub>m</sub>) / V<sub>m</sub>.
   Le courant que devra encaisser le transistor est
   I<sub>C</sub> ≈ V<sub>tirage</sub> / R<sub>pu</sub>.
3. Vérifier que I<sub>C</sub> < 100 mA. En pratique on trouve 1 kΩ à 10 kΩ, soit 1 à
   12 mA : aucun problème.

Notez la tension trouvée : elle n'a **aucune** incidence sur le montage, mais elle
confirme que la broche est bien une entrée tirée et non une sortie.

---

## 5. LED d'état

Aucun composant : la LED bleue intégrée au D1 mini (GPIO2 / D4, active à l'état bas) est
utilisée.

| Comportement | Signification |
|---|---|
| Allumée en continu | les deux canaux simulent activement |
| Clignotement rapide (5 Hz) | un seul canal simule, l'autre attend un signal |
| Impulsion brève toutes les secondes | aucun signal d'entrée, le système attend |

Pour une LED déportée visible sans ouvrir l'onduleur : LED + résistance 330 Ω entre
`D0/GPIO16` et GND, en modifiant `PIN_LED` dans le firmware. GPIO16 convient pour ce
seul usage (il ne peut pas servir aux entrées ou sorties tach, car ses registres sont
distincts de GPI/GPOS et il n'est pas géré par le moteur de signal).

---

## 6. Nomenclature

### 6.1 Modules

| Réf. | Désignation | Qté | Remarque |
|---|---|---|---|
| U1 | Wemos D1 mini V2.3.0 (ESP-12S) | 1 | disponible |
| U2 | Module abaisseur MP1584EN ou LM2596 | 1 | **à ajouter**, réglé à 5,00 V |

### 6.2 Semi-conducteurs

| Réf. | Désignation | Qté | Remarque |
|---|---|---|---|
| Q1, Q2 | NPN 2N2222A (ou BC337 / S8050) | 2 | sorties tach, disponible |
| Q3, Q4 | NPN 2N3904 (ou BC547) | 0 ou 2 | seulement en variante B |
| D1, D2 | Diode Schottky 1N5819 ou SS34 | 2 | **à ajouter** pour le OU diode |

### 6.3 Résistances (1/4 W, 5 % suffisant) — toutes disponibles

| Réf. | Valeur | Qté | Fonction |
|---|---|---|---|
| R1, R3 | 4,7 kΩ | 2 | tirage des entrées tach vers 3V3 |
| R2, R4 | 470 Ω | 2 | limitation / filtre des entrées |
| R8, R10 | 2,2 kΩ | 2 | bases des NPN de sortie |
| R9, R11 | 10 kΩ | 2 | maintien au blocage base–émetteur |
| R5, R7 (var. B) | 10 kΩ | 0 ou 4 | tampon d'entrée |
| R6 (var. B) | 4,7 kΩ | 0 ou 2 | collecteur du tampon |

### 6.4 Condensateurs

| Réf. | Valeur | Qté | Fonction |
|---|---|---|---|
| C1 | 100 µF / 25 V électrolytique | 1 | entrée du buck |
| C2, C4 | 100 nF céramique | 2 | découplage HF |
| C3 | 220 µF / 10 V électrolytique | 1 | sortie du buck, pics WiFi |
| C5, C6 | 22 nF céramique | 2 | filtrage des entrées tach |
| C7, C8 | 1 nF céramique | 0 ou 2 | option anti-parasites en sortie |

### 6.5 Divers

| Désignation | Qté | Remarque |
|---|---|---|
| Fusible réarmable 500 mA (polyfuse) | 2 | fortement recommandé sur chaque +12 V |
| Connecteurs 3 broches compatibles Deye + rallonges | 2 | ou dérivation en Y |
| Plaque à trous / PCB, gaine thermo, colliers | — | |

---

## 7. Tableau de câblage complet

| De | Vers | Via |
|---|---|---|
| Deye 92 mm — +12V | VBUS | F1 (500 mA) puis D1 (anode → cathode) |
| Deye 60 mm — +12V | VBUS | F2 (500 mA) puis D2 (anode → cathode) |
| Deye 92 mm — +12V | +12V des 2 × NF-A9-FLX | direct |
| Deye 60 mm — +12V | +12V des 2 × NF-A6x25-FLX | direct |
| Deye 92 mm — GND | masse commune | direct |
| Deye 60 mm — GND | masse commune | direct (après vérification § 2.3) |
| VBUS | IN+ du buck | C1 100 µF + C2 100 nF vers GND |
| Buck OUT+ (5,00 V) | broche `5V` du D1 mini | C3 220 µF + C4 100 nF vers GND |
| Buck OUT− | broche `G` du D1 mini | direct |
| Tach NF-A9-FLX n° 1 | D5 / GPIO14 | R2 470 Ω ; R1 4k7 vers 3V3 ; C5 22 nF vers GND |
| Tach NF-A6x25-FLX n° 1 | D6 / GPIO12 | R4 470 Ω ; R3 4k7 vers 3V3 ; C6 22 nF vers GND |
| Tach des 2 ventilateurs n° 2 | — | coupés et isolés |
| D1 / GPIO5 | base de Q1 | R8 2k2 ; R9 10k base → GND |
| Collecteur de Q1 | Deye 92 mm — TACH | direct ; émetteur de Q1 → GND |
| D2 / GPIO4 | base de Q2 | R10 2k2 ; R11 10k base → GND |
| Collecteur de Q2 | Deye 60 mm — TACH | direct ; émetteur de Q2 → GND |

**Repérage des fils.** Sur les Noctua : jaune = tach, rouge = +12 V, noir = GND. Sur les
faisceaux du Deye, la convention usuelle est noir = GND, rouge = +12 V, jaune ou blanc =
tach — **à confirmer au voltmètre** avant tout raccordement, les faisceaux OEM variant
d'une série à l'autre.

---

## 8. Mise en service

1. **À l'établi, avant tout raccordement à l'onduleur.** Régler le buck à 5,00 V sur une
   alimentation de laboratoire à 12 V. Vérifier le OU diode : injecter 12 V sur une seule
   entrée à la fois et confirmer que l'autre entrée reste à 0 V (absence de retour).
2. **Flasher le firmware.** Arduino IDE, carte « LOLIN(WEMOS) D1 mini », CPU 80 ou
   160 MHz, `deye_fan/deye_fan.ino`.
3. **Vérifier le moteur de signal.** Sur le moniteur série (115200 bauds), la ligne
   `[engine] ... mode NMI` doit apparaître, et le champ « Réveils NMI / s » de
   l'interface web doit indiquer ≈ 25 000. Une valeur nulle signale que le Timer1 n'a pas
   démarré.
4. **Test à vide.** Alimenter un Noctua en 12 V à l'établi, le relier à une entrée :
   l'interface web (point d'accès `DeyeFan-xxxxxx`, mot de passe `deyefan123`,
   http://192.168.4.1 ou http://deye-fan.local) doit afficher ≈ 1600 tr/min pour le
   NF-A9-FLX ou ≈ 3000 tr/min pour le NF-A6x25-FLX. Contrôler la sortie à l'oscilloscope
   ou au fréquencemètre, avec une résistance de tirage de 4,7 kΩ vers 5 V sur le
   collecteur, et vérifier que la fréquence correspond bien à
   RPM<sub>simulés</sub> × 2 / 60.
5. **Test d'immunité au WiFi** — c'est le test qui valide l'architecture NMI. Pendant que
   la sortie est observée au fréquencemètre, saturer le module de requêtes (par exemple
   `while ($true) { curl -s http://192.168.4.1/api/status > $null }` en boucle, plus un
   scan WiFi ou un transfert). La fréquence de sortie ne doit pas bouger de manière
   mesurable. En basculant `ENGINE_USE_NMI` à `0` et en refaisant le même test, on voit
   au contraire apparaître la gigue : c'est la démonstration directe de l'intérêt du
   vecteur NMI.
6. **Relever les RPM des ventilateurs d'origine.** Avant de les retirer définitivement,
   brancher chaque NMB sur une entrée du montage (le tach NMB est aussi un collecteur
   ouvert, l'étage d'entrée l'accepte tel quel) et noter les RPM affichés à pleine
   vitesse. C'est la valeur cible.
7. **Calculer et saisir les ratios.**
   ratio = RPM<sub>cible</sub> ÷ RPM<sub>Noctua</sub>, dans l'interface web, sans
   recompilation ni recâblage.
8. **Installer, puis surveiller** l'absence d'alarme ventilateur et le comportement
   thermique de l'onduleur sur plusieurs cycles de charge.

### Ratios de départ

| Canal | Noctua | RPM max Noctua | NMB d'origine | RPM cible estimés | Ratio par défaut | RPM simulés |
|---|---|---|---|---|---|---|
| 1 | NF-A9-FLX | 1600 | 09225VE-12N-CU | ≈ 4500 | **2,80** | ≈ 4480 |
| 2 | NF-A6x25-FLX | 3000 | 06025VE-12N-CL | ≈ 7500 | **2,50** | ≈ 7500 |

Les catalogues documentent les variantes **-12Q** de ces deux corps (5600 tr/min pour le
09225VE, 9200 tr/min pour le 06025VE) mais pas les variantes **-12N** qui équipent votre
onduleur. Chez NMB la lettre de gradation désigne la vitesse et « N » est en dessous de
« Q » : les 4500 et 7500 tr/min du tableau sont des estimations. D'où l'étape 6 — c'est
la seule façon d'obtenir la vraie valeur, et c'est précisément pour cela que le ratio est
réglable à chaud.

---

## 9. Points de conception à retenir

**Le collecteur ouvert est la réponse à la tension inconnue.** Aucune détection, aucun
strap, aucune variante de montage : le transistor tire la ligne à la masse ou la relâche,
ce qui est valide de 3,3 V à 12 V et au-delà. C'est aussi, littéralement, ce que faisait
le ventilateur d'origine.

**Le OU diode est la réponse à l'alimentation intermittente.** Il autorise le prélèvement
sur les deux connecteurs, tolère des tensions différentes, et surtout empêche
d'alimenter par accident un ventilateur que l'onduleur a délibérément arrêté.

**Le vecteur NMI est la réponse à l'isolation vis-à-vis du WiFi.** Sur un ESP8266
mono-cœur, tout ce qui est de niveau 1 — timer Arduino, `attachInterrupt`, `tone()`,
`analogWrite()` — partage sa priorité avec la pile WiFi et se fait retarder de dizaines
de microsecondes sous trafic. Le Timer1 routé vers la NMI (niveau 3) préempte le WiFi et
ignore `noInterrupts()`. Combiné à un horodatage par compteur de cycles CPU et à des
dates de front accumulées plutôt que recalculées, cela donne un signal dont la
régularité ne dépend d'aucune autre tâche logicielle. Les détails et les contraintes
associées (code en IRAM, aucun flottant, aucun appel SDK dans l'ISR) sont documentés en
en-tête de `deye_fan.ino`.

**Conséquence à ne pas oublier :** le Timer1 étant monopolisé, `analogWrite()`, `tone()`
et la bibliothèque `Servo` ne sont plus utilisables dans ce sketch.
