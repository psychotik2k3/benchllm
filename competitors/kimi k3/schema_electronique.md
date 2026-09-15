# Schéma électronique — Simulateur de tachymétrie pour onduleur Deye SUN-8K-SG05LP1-EU-AM2-P

Carte : **Wemos D1 Mini V2.3.0 (ESP-12S / ESP8266)** — firmware : `deye_fan.ino`

---

## 1. Nomenclature (BOM)

| Repère | Composant | Valeur / modèle | Rôle |
|---|---|---|---|
| U1 | Wemos D1 Mini V2.3.0 | — | Microcontrôleur |
| Q1, Q2 | Transistor NPN | 2N2222 (ou BC547/BC337/S8050…) | Sortie collecteur ouvert vers Deye |
| R1, R2 | Résistance | 1 kΩ | Base de Q1 / Q2 |
| R3, R4 | Résistance | 1 kΩ | Série entrée tach (protection) |
| R5, R6 | Résistance | 10 kΩ | Pull-up 3,3 V des entrées tach |
| D1, D2 | Diode Schottky | 1N5819 (≥ 1 A) | OU à diodes de l'alimentation |
| U2 | Module buck 12 V → 5 V | MP1584 (mini module réglable) | Alimentation du Wemos |
| C1 | Condensateur électrolytique | 470 µF / 25 V | Filtrage entrée buck |
| C2 | Condensateur céramique | 100 nF | Découplage |
| — | 2× Noctua NF-A9-FLX | 12 V, 3 broches | Remplace le NMB 09225VE (9 cm) |
| — | 2× Noctua NF-A6x25-FLX | 12 V, 3 broches | Remplace le NMB 06025VE (6 cm) |

> Le module buck et les diodes sont les seuls composants hors « résistances + NPN » :
> ils sont **indispensables** — le Wemos n'accepte pas 12 V directement (son régulateur
> interne attend 5 V sur la broche 5V) et l'OU à diodes est la seule façon propre de
> gérer deux sources 12 V non simultanées.

---

## 2. Vue d'ensemble

```
                    ONDULEUR DEYE
  ┌──────────────────────────────────────────────────────────┐
  │  Connecteur ventil. 9 cm        Connecteur ventil. 6 cm  │
  │  +12V  GND  TACH_IN             +12V  GND  TACH_IN       │
  └──┬─────┬─────┬────────────────────┬─────┬─────┬──────────┘
     │     │     │                    │     │     │
     │     │     └──────────┐         │     │     └──────────┐
     │     │                │         │     │                │
  ┌──┴─┐ ┌─┴──┐          ┌──┴───────┐ │  ┌──┴─┐ ┌─┴──┐    ┌──┴───────┐
  │NF-A9│ │NF-A9│         │  Q1 NPN  │ │  │NF-A│ │NF-A│    │  Q2 NPN  │
  │ #1  │ │ #2  │         │ col.ouv. │ │  │6x25│ │6x25│    │ col.ouv. │
  └──┬─┘ └──┬─┘          └────┬─────┘ │  └──┬─┘ └──┬─┘    └────┬─────┘
     │      │ tach(jaune)     │ base  │     │      │ tach      │ base
     │      │                 │       │     │      │           │
     └──────┴───────┐   ┌─────┴───────┴─────┴──────┴───────────┘
                    │   │        WEMOS D1 MINI                  │
                    └───┤  D5=in1  D6=in2  D1=out1  D2=out2     │
                        └───────────────────────────────────────┘
```

---

## 3. Alimentation (depuis les connecteurs ventilateur)

Les deux connecteurs ventilateur du Deye ne sont **pas forcément alimentés en même
temps ni exactement à la même tension**. L'**OU à diodes Schottky** prélève le 12 V
sur le connecteur qui est actif, sans jamais réinjecter de courant dans l'autre :

```
 Deye +12V (conn. 9 cm) ───|>|───┬─────────── IN+  ┌──────────┐
                      D1 1N5819   │                 │  BUCK    │
 Deye +12V (conn. 6 cm) ───|>|───┤            ┌────►│ MP1584   ├─ OUT+ (5,0–5,2 V) ──► 5V du Wemos
                      D2 1N5819   │            │    │ 12V→5V   │
                                  │           C1    │ réglé à  │
 GND (les deux connecteurs) ──────┴──┬─────────────►│  5,1 V   ├─ OUT− ─────────────► GND du Wemos
                                     │ 470µF/25V    └──────────┘
                                     └────────────────────────────────► GND commun (étoile)
```

**Réglage préalable obligatoire** : avant de brancher le Wemos, ajuster le
potentiomètre du module MP1584 pour obtenir **5,0–5,2 V** en sortie (mesurer au
multimètre). Le régulateur 3,3 V interne du Wemos fait le reste.

Points clés :
- Diodes **Schottky** (chute ≈ 0,3 V seulement) : le buck voit ≈ 11,7 V, sans
  conséquence.
- Si un seul connecteur est alimenté, le Wemos démarre quand même ; l'autre canal
  affichera simplement 0 RPM.
- Les masses des deux connecteurs Deye sont communes dans l'onduleur : relier les
  deux GND au montage (étoile au niveau du buck).
- Les ventilateurs Noctua sont alimentés **directement** par les connecteurs Deye
  (2 en parallèle par connecteur : +12V rouge, GND noir) — ils ne passent pas par
  le buck.

---

## 4. Circuits d'entrée (Noctua → ESP8266)

Le signal tach Noctua est un **collecteur ouvert, 2 impulsions par tour**. Il ne
fournit jamais de tension : il tire simplement la ligne à la masse. On la tire donc
au repos vers **3,3 V** (et non 12 V) côté Wemos — la ligne ne dépassera jamais
3,3 V, quelle que soit l'installation :

```
                            3,3 V (Wemos)
                              │
                             ┌┴┐
                             │ │ R5 = 10 kΩ  (pull-up)
                             └┬┘
 Tach Noctua 9 cm ──── R3 ────┼──────────────► D5 (GPIO14)   canal 1
 (fil jaune, coll. ouvert)  1 kΩ
                              │
 Tach Noctua 6 cm ──── R4 ────┼──────────────► D6 (GPIO12)   canal 2
 (fil jaune, coll. ouvert)  1 kΩ      (R6 = 10 kΩ vers 3,3 V, idem)

 GND Noctua ──────────────────────────────────► GND Wemos (obligatoire)
```

- **R3/R4 (1 kΩ)** limitent le courant en cas de transitoire ou de court-circuit
  (protection des diodes de clamp internes de l'ESP8266).
- **R5/R6 (10 kΩ)** fixent le niveau haut à 3,3 V. Le pull-up interne de l'ESP
  est activé en secours dans le firmware.
- Un seul des deux ventilateurs de chaque paire fournit son tach au Wemos
  (les deux tournent à la même vitesse ; inutile de lire les deux).

---

## 5. Circuits de sortie (ESP8266 → Deye)

La tension que le Deye applique sur sa broche tach est **inconnue (3,3 / 5 / 12 V)**.
La sortie **collecteur ouvert NPN** est la solution universelle : le transistor ne
fait que tirer la ligne à la masse ; c'est le pull-up **interne du Deye** qui fixe
le niveau haut, quelle que soit sa tension. Aucune adaptation matérielle requise.

```
 D1 (GPIO5) ──── R1 = 1 kΩ ────►│ B   Q1 = 2N2222
                                 │ E ──────────────► GND
 TACH_IN du connecteur 9 cm ◄────┤ C (collecteur)
        (côté Deye)              │

 D2 (GPIO4) ──── R2 = 1 kΩ ────►│ B   Q2 = 2N2222
                                 │ E ──────────────► GND
 TACH_IN du connecteur 6 cm ◄────┤ C
        (côté Deye)              │
```

- GPIO haut → transistor saturé → ligne tach **basse** (impulsion). L'inversion
  est gérée dans le firmware (rapport cyclique 50 %, comme un vrai ventilateur).
- Courant de base ≈ (3,3 − 0,7) / 1 kΩ ≈ 2,6 mA : saturation garantie même pour
  un pull-up Deye fort (ex. 1 kΩ vers 12 V ⇒ 12 mA au collecteur, β forcé ≈ 5).
- N'importe quel NPN de la liste convient (2N2222, BC547, BC337, S8050, 2N3904…).
- **Vérification une fois installé** : ventilateur d'origine débranché, mesurer la
  tension sur la broche tach du Deye. Si elle est ≈ 0 V (pas de pull-up interne,
  cas très improbable), ajouter une résistance 10 kΩ entre cette broche et le rail
  mesuré sur le connecteur. Dans tous les autres cas, rien à ajouter.

---

## 6. Schéma complet (nets)

```
                        ┌─────────────────────────── WEMOS D1 MINI ───────────────────────────┐
                        │                                                                     │
  Tach NF-A9  ──1k──────┤ D5 (GPIO14)      5V ◄────────────────────── OUT+ buck MP1584        │
  (pull-up 10k→3V3)     │                                                                     │
  Tach NF-A6  ──1k──────┤ D6 (GPIO12)      GND ◄─────────┬──────────── OUT− buck MP1584       │
  (pull-up 10k→3V3)     │                                │                                    │
                 ──1k──►├─ B(Q1) D1 (GPIO5)  3V3 ──► pull-ups R5/R6                         │
                 ──1k──►├─ B(Q2) D2 (GPIO4)                                                │
                        │ D4 (GPIO2) = LED intégrée (rien à câbler)                          │
                        └────────────────────────────────┬───────────────────────────────────┘
                                                         │
   Q1 : C ──► TACH_IN conn. Deye 9 cm      E ────────────┤
   Q2 : C ──► TACH_IN conn. Deye 6 cm      E ────────────┤
   GND Noctua (×4) ──────────────────────────────────────┤
   GND connecteurs Deye (×2) ──► entrée buck ────────────┘   (masse commune)
```

---

## 7. Pourquoi ce design répond aux contraintes

| Contrainte | Solution |
|---|---|
| Tension tach Deye inconnue (3,3/5/12 V) | Sortie **collecteur ouvert NPN** : le niveau haut est fixé par le Deye lui-même |
| Deux sources 12 V non simultanées | **OU à diodes Schottky** 1N5819 + buck 12→5 V |
| Signal sans jitter malgré le WiFi | Génération par **timer matériel n°1** (ISR IRAM, accès GPIO directs) ; le web/WiFi ne touche jamais les sorties |
| Ratio réglable sans recompiler | Interface web (AP `DeyeFanSim` → http://192.168.4.1), ratios persistés en **EEPROM** |
| AP + STA simultanés | `WiFi.mode(WIFI_AP_STA)`, identifiants STA en EEPROM |
| Témoin visuel d'activité | LED intégrée (D4) : fixe = simulation active, clignotante = attente |

**Ordre de grandeur du jitter** : demi-période minimale ≈ 300 µs (canal 6 cm à
3000 RPM × ratio 10) ; latence ISR timer1 même sous charge WiFi < ~10 µs, soit
< 3 % dans le pire cas et < 0,3 % en régime nominal — indétectable par le Deye.
