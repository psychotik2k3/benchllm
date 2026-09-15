# Schéma électronique — Simulateur de tach ventilateurs pour Deye SUN-8K-SG05LP1-EU-AM2-P

Remplacement des ventilateurs d'origine (NMB 09225VE-12N-CU 9 cm + NMB 06025VE-12N-CL 6 cm)
par 2× Noctua NF-A9 FLX (1600 tr/min) + 2× Noctua NF-A6x25 FLX (3000 tr/min).
Le Wemos D1 mini lit le tach réel d'un Noctua par voie et régénère vers l'onduleur un
signal tach multiplié par un ratio configurable (voir `deye_fan.ino`).

---

## 1. Synoptique général

```
 ┌─────────────────────────── Onduleur Deye ───────────────────────────┐
 │                                                                     │
 │  Connecteur 9 cm            Connecteur 6 cm                         │
 │  ┌──────────────┐           ┌──────────────┐                        │
 │  │ +12V  GND  TACH│         │ +12V  GND  TACH│                       │
 │  └──┬────┬────┬──┘           └──┬────┬────┬──┘                       │
 └─────┼────┼────┼────────────────┼────┼────┼──────────────────────────┘
       │    │    │                │    │    │
       │    │    │ Q1 (NPN)       │    │    │ Q2 (NPN)
       │    │    └───────┐        │    │    └───────┐
       │    │            │        │    │            │
       │    │   ┌────────┼────────┼────┼────────────┼───────────┐
       │    │   │  WEMOS D1 mini  │    │            │           │
       │    │   │                D1 ───┘ (2,2k base) │           │
       │    │   │                D2 ─────────────────┘           │
       │    │   │  D5 ◄── tach Noctua 9 cm (1k série, 10k pull-up 3,3V)
       │    │   │  D6 ◄── tach Noctua 6 cm (1k série, 10k pull-up 3,3V)
       │    │   │  5V ◄── buck 12→5V ◄── D1/D2 (1N5819) ◄── +12V des 2 connecteurs
       │    │   │  GND ─── commun partout ───┘
       │    │   └───────────────────────────────────────────────┘
       │    │
       │    └────► GND  des 2× Noctua 9 cm        └────► GND  des 2× Noctua 6 cm
       └─────────► +12V des 2× Noctua 9 cm        └────► +12V des 2× Noctua 6 cm

  ⚠ Le fil tach des Noctua ne va JAMAIS à l'onduleur : seul le tach simulé
    (collecteur de Q1/Q2) est raccordé à la broche TACH du connecteur Deye.
```

---

## 2. Alimentation (depuis les connecteurs ventilateur)

Contraintes : le Wemos est alimenté par les +12 V des connecteurs ventilateur ;
les deux connecteurs ne sont **pas forcément alimentés en même temps ni à la même
tension** (l'onduleur peut moduler la tension ventilateur, typiquement 7–12 V).

```
 +12V (conn. 9 cm) ──[D1 1N5819]──┐
                                   ├── +VRAW ──► IN+ ┌─────────────┐ OUT+ ──► broche 5V du Wemos
 +12V (conn. 6 cm) ──[D2 1N5819]──┘                  │ MODULE BUCK │
                                                     │ MP1584      │
 GND (conn. 9 cm) ──┬── GND (conn. 6 cm) ──► IN− ────┤ réglé 5,0 V │ OUT− ──► broche GND du Wemos
                    └───────────────────────────────────────────────┴──────► GND commun
```

**Principe :**
- **D1/D2 (1N5819, Schottky 1 A)** : OR-ing à diodes. Le rail le plus élevé alimente
  le buck ; si un connecteur est hors tension, l'autre prend le relais sans
  discontinuité ni retour de courant vers le connecteur inactif. Chute ~0,3 V,
  sans conséquence (le buck accepte 4,5–28 V en entrée).
- **Module buck MP1584** (ou LM2596 mini) réglé à **5,0 V** : alimente le Wemos par
  sa broche `5V` (le régulateur 3,3 V embarqué, ME6211, accepte 4,3–6 V).
  ⚠ **Régler la sortie à 5,0–5,1 V au multimètre AVANT de raccorder le Wemos.**
- Budget courant : Wemos ≈ 80 mA moyen, pics WiFi ≈ 350 mA → côté 12 V, le buck ne
  tire que ~150 mA en pic. Les 1N5819 (1 A) sont largement dimensionnées.
- Le GND est commun à toute l'installation (connecteurs, Wemos, Noctua, NPN) :
  il l'est déjà de fait puisque tout est alimenté par les mêmes connecteurs.

---

## 3. Circuits d'entrée (Noctua → ESP8266) — ×2 voies

Le tach Noctua est une sortie **collecteur ouvert, 2 impulsions par tour**
(courant de sink max 5 mA ; Noctua recommande un pull-up ≥ 2,7 kΩ). La ligne ne
porte donc jamais le 12 V : c'est notre pull-up vers 3,3 V qui fixe le niveau haut.
L'entrée est protégée par construction.

```
 3,3 V (Wemos)
    │
   [R1 10 kΩ]  pull-up
    │
    ├─────────── fil tach Noctua (vert) ── collecteur ouvert interne au ventilateur
    │
   [R2 1 kΩ]   série (limitation courant de clamp / ESD)
    │
    ▼
 GPIO D5 (voie 1, 9 cm)  /  GPIO D6 (voie 2, 6 cm)   — front descendant, interruption
```

- Niveau haut ≈ 3,3 V (> VIH de l'ESP8266), niveau bas ≈ 0,1 V. Courant de sink du
  ventilateur : 3,3 V / 10 kΩ ≈ 0,33 mA ≪ 5 mA ✔
- R2 (1 kΩ) protège l'entrée contre les transitoires ; la constante de temps avec
  les capacités parasites (~1 µs) est négligeable devant la demi-période minimale
  du signal (5 ms à 3000 tr/min).
- Le pull-up interne de l'ESP (`INPUT_PULLUP`) est activé en secours : fil tach
  débranché ⇒ ligne haute fixe ⇒ aucune impulsion ⇒ état « attente » propre.
- Le tach du **second** Noctua de chaque paire n'est pas câblé (une seule mesure
  par voie, conformément au besoin).

---

## 4. Circuits de sortie (ESP8266 → Deye) — ×2 voies

La tension du pull-up interne de l'onduleur sur sa broche tach est **inconnue**
(3,3 V, 5 V ou 12 V). La sortie **collecteur ouvert** s'adapte d'elle-même :
le NPN ne fait que tirer la ligne à la masse ; le niveau haut est toujours fixé
par le pull-up interne du Deye (les NMB d'origine sont eux-mêmes à collecteur
ouvert, donc ce pull-up existe forcément). **Aucun réglage matériel requis.**

```
 GPIO D1 (voie 1) / D2 (voie 2)
    │
   [R3 2,2 kΩ]  base
    │
    ├──► base  Q1/Q2  2N2222 (NPN)
    │           émetteur ──► GND
    │           collecteur ──► broche TACH du connecteur Deye (9 cm / 6 cm)
   [R4 10 kΩ]  base-émetteur (maintien bloqué pendant le boot / Hi-Z)
    │
   GND
```

- **Q1/Q2 : 2N2222** recommandé (Vceo 40 V > 12 V + transitoires). Tout NPN de la
  liste avec Vceo ≥ 25 V convient (2N3904, BC547/548, 2N5551, S8050, C1815…).
- Courant de base ≈ (3,3 − 0,7) / 2,2 kΩ ≈ 1,2 mA → saturation franche même si le
  pull-up de l'onduleur est costaud (ex. 12 V / 1 kΩ = 12 mA à drainer : bêta forcé
  ≈ 10, très inférieur au hFE min → Vce(sat) < 0,2 V, niveau bas propre).
  Si le niveau bas n'était pas net sur une installation exotique, passer R3 à 1 kΩ.
- R4 (10 kΩ) garantit un transistor bloqué tant que le GPIO est en haute impédance
  (reset, boot) : la ligne tach reste relâchée, aucun glitch vers l'onduleur.
- Logique inversée (GPIO haut = ligne tirée bas) sans conséquence : le logiciel
  génère un signal carré symétrique, seule la fréquence compte pour le Deye.

---

## 5. Table de câblage

| Signal | Broche Wemos | Composants associés | Destination |
|---|---|---|---|
| Tach Noctua 9 cm (fil vert) | D5 / GPIO14 | R1 10 kΩ → 3,3 V ; R2 1 kΩ série | entrée voie 1 |
| Tach Noctua 6 cm (fil vert) | D6 / GPIO12 | R1 10 kΩ → 3,3 V ; R2 1 kΩ série | entrée voie 2 |
| Tach simulé voie 9 cm | D1 / GPIO5 | R3 2,2 kΩ base ; Q1 2N2222 ; R4 10 kΩ B-E | collecteur → TACH conn. Deye 9 cm |
| Tach simulé voie 6 cm | D2 / GPIO4 | R3 2,2 kΩ base ; Q2 2N2222 ; R4 10 kΩ B-E | collecteur → TACH conn. Deye 6 cm |
| LED intégrée (état) | D4 / GPIO2 | — (LED sur carte) | fixe = simulation active ; clignotant = attente |
| Alim 5 V | 5V | sortie OUT+ du buck MP1584 | — |
| Alim 12 V | — | D1/D2 1N5819 depuis +12V des 2 connecteurs → IN+ buck | — |
| Masse | G | GND commun : connecteurs, Noctua, émetteurs NPN, IN−/OUT− buck | — |
| +12 V Noctua 9 cm (×2, parallèle) | — | direct depuis +12V conn. 9 cm | fils jaunes |
| +12 V Noctua 6 cm (×2, parallèle) | — | direct depuis +12V conn. 6 cm | fils jaunes |
| GND Noctua (×4) | — | direct depuis GND des connecteurs | fils noirs |

Repères couleurs Noctua (3 broches) : **noir = GND, jaune = +12 V, vert = tach**.
Sur les connecteurs d'origine de l'onduleur, identifier +12 V et GND au multimètre
avant tout raccordement ; le 3ᵉ fil est le tach. Récupérer les fiches des NMB
d'origine pour se repiquer proprement sur la carte de l'onduleur.

---

## 6. Nomenclature (BOM)

| Qté | Référence | Rôle |
|---|---|---|
| 1 | Wemos D1 mini V2.3.0 (ESP-12S) | contrôleur |
| 1 | Module buck MP1584 (ou LM2596 mini) | abaisseur 12 V → 5,0 V |
| 2 | 1N5819 (Schottky 1 A) | OR-ing alimentation |
| 2 | 2N2222 (ou autre NPN de la liste, Vceo ≥ 25 V) | sorties collecteur ouvert |
| 2 | Résistance 10 kΩ | pull-up entrées tach |
| 2 | Résistance 1 kΩ | série entrées tach |
| 2 | Résistance 2,2 kΩ | base NPN |
| 2 | Résistance 10 kΩ | base-émetteur NPN |
| — | Fils, fiches récupérées des ventilateurs NMB, platine d'essai / PCB | câblage |

Seuls ajouts hors liste « résistances + NPN » de l'énoncé : les 2 diodes 1N5819 et
le module buck (indispensables pour l'alimentation propre depuis les deux
connecteurs ; aucun équivalent réalisable avec les seuls composants listés).

---

## 7. Mise en service

1. **Régler le buck à 5,0–5,1 V** (entrée 12 V, sortie à vide) **avant** de le
   raccorder à la broche 5V du Wemos.
2. Identifier +12 V / GND / TACH sur chaque connecteur ventilateur de l'onduleur
   (multimètre, onduleur sous tension, ventilateurs d'origine débranchés).
3. Câbler en suivant la table §5. Vérifier que **seuls** les collecteurs de Q1/Q2
   vont aux broches TACH de l'onduleur (jamais les tach des Noctua).
4. Mettre sous tension : la LED clignote (attente) puis devient fixe dès que les
   ventilateurs tournent (simulation active).
5. Se connecter à l'AP WiFi **`DeyeFanSim`** (mot de passe `deye1234`) →
   http://192.168.4.1 → vérifier que « RPM lu » ≈ 1600 (9 cm) / 3000 (6 cm),
   ajuster les ratios pour que « RPM simulé » ≈ le régime des NMB d'origine,
   et configurer le WiFi STA si souhaité (persistant, l'AP reste actif).

## 8. Remarques

- **Isolation WiFi/web** : la lecture et la génération tach sont entièrement en
  interruptions matérielles (GPIO + timer1) en IRAM ; le serveur web tourne en
  tâche de fond. Gigue résiduelle < quelques µs, sans dérive — non mesurable par
  le comptage RPM de l'onduleur (détails en tête de `deye_fan.ino`).
- Si l'onduleur module la tension de ses sorties ventilateur (7–12 V), le buck la
  tolère ; le simulateur reste alimenté tant qu'**au moins un** connecteur l'est.
- Ventilateur réellement à l'arrêt ⇒ le simulateur cesse d'émettre (l'onduleur
  voit 0 tr/min, comportement fidèle) et la LED repasse en clignotant.
