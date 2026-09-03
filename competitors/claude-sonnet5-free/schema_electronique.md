# Schéma électronique — Spoofer tach ventilateurs Deye

## 1. Vue d'ensemble

```
 Connecteur ventil. 9cm (Noctua NF-A9-FLX)      Connecteur ventil. 6cm (Noctua NF-A6x25-FLX)
  GND  +12V  TACH                                GND  +12V  TACH
   |    |     |                                   |    |     |
   |    |     +---[Etage entree A]---> D2 (GPIO4)  |    |     +---[Etage entree B]---> D7 (GPIO13)
   |    +-----------------+                        |    +-----------------+
   |                      |                        |                      |
  GND               [D1 anode]                    GND               [D2 anode]
                          |                                                |
                          +--------------------- VIN_RAW ------------------+
                                                       |
                                                 [C1 330uF/25V]
                                                       |
                                                      GND
                                              (VIN_RAW alimente aussi
                                               l'entree du module buck)

  VIN_RAW ---> [Module buck 12V->5V, reglé sur 5V] ---> 5V du Wemos D1 mini
  GND commun -----------------------------------------> GND du Wemos D1 mini

  Wemos D1 mini :
   D5 (GPIO14) --[Etage sortie A]--> fil TACH vers connecteur onduleur (ex 9cm)
   D6 (GPIO12) --[Etage sortie B]--> fil TACH vers connecteur onduleur (ex 6cm)
   D1 (GPIO5)  --[R + LED]--> GND
```

Les fils `+12V` et `GND` originaux des connecteurs ventilateurs de l'onduleur
alimentent directement les Noctua (en parallèle, 2 Noctua par connecteur
d'origine puisque vous doublez le nombre de ventilateurs). Seul le fil
`TACH` de chaque connecteur d'origine est intercepté et remplacé par la
sortie simulée du Wemos ; le fil TACH des Noctua eux-mêmes part vers les
entrées du Wemos.

---

## 2. Nomenclature (BOM)

| Réf. | Composant | Valeur / réf. | Rôle | Dispo dans votre stock |
|---|---|---|---|---|
| Q1, Q2 | NPN | BC547 (ou 2N3904/S9014...) | Adaptation niveau entrée (canaux A/B) | ✅ |
| Q3, Q4 | NPN | BC547 (ou 2N3904/S9014...) | Driver open-collector sortie (canaux A/B) | ✅ |
| Rb1, Rb2 | Résistance | 10 kΩ | Base Q1/Q2 (limite courant de base) | ✅ |
| Rc1, Rc2 | Résistance | 4.7 kΩ | Pull-up collecteur Q1/Q2 vers 3.3V | ✅ |
| Rprot1, Rprot2 | Résistance | 220 Ω | Protection série vers GPIO d'entrée | ✅ |
| C_filt1, C_filt2 | Céramique | 1 nF | Filtrage HF sur les entrées | à prévoir |
| Rb3, Rb4 | Résistance | 1 kΩ | Base Q3/Q4 | ✅ |
| Rbe3, Rbe4 | Résistance | 10 kΩ | Base-émetteur Q3/Q4 (sécurité anti-glitch au boot) | ✅ |
| Rled | Résistance | 330–470 Ω | Limitation courant LED | ✅ |
| LED | LED 3-5mm | couleur au choix | Indicateur d'état | à prévoir |
| D1, D2 | Diode Schottky | 1N5819 / SS14 (1A, 40V) | OU-câblage des deux alimentations 12V | **à ajouter** (non listée dans votre stock) |
| C1 | Électrolytique | 330–470 µF / 25V | Lissage / tampon lors des commutations d'alimentation | **à ajouter** |
| — | Module buck (ou régulateur linéaire) | 12V→5V, ≥1A (ex. type "Mini-360"/MP1584, ou LM2596) | Alimentation du Wemos | **à ajouter** |
| C2 | Céramique | 100 nF | Découplage près du 5V du Wemos | à prévoir |
| — | Wemos D1 mini v2.3.0 | ESP-12S | Contrôleur | ✅ |

> Les diodes Schottky et le module d'alimentation ne figuraient pas dans
> votre liste de composants déclarés — je les ajoute car ils sont
> indispensables et sont des composants standards, très courants et bon
> marché. Dites-moi si vous préférez une variante 100% "résistances +
> transistors NPN" pour l'un de ces blocs (au prix d'un rendement moins bon,
> par ex. régulateur linéaire 7805 à la place du module buck).

---

## 3. Bloc alimentation

Vos deux connecteurs ventilateurs d'origine ne sont pas forcément alimentés
simultanément ni à la même tension exacte. On les combine avec un
**OU câblé à diodes Schottky** : chaque `+12V` attaque l'anode de sa propre
diode, les deux cathodes sont reliées ensemble sur un nœud commun `VIN_RAW`.
Ainsi, quelle que soit la ou les source(s) réellement alimentée(s),
`VIN_RAW` reste toujours alimenté correctement, sans court-circuiter les
deux sources l'une dans l'autre.

```
+12V (connecteur 9cm) ---|>|---+
                          D1    |
                                +--- VIN_RAW ---+--- [C1 330uF/25V] --- GND
+12V (connecteur 6cm) ---|>|---+                |
                          D2                     |
                                            IN+ --+
                                      [Module buck 12V -> 5V]
                                            IN-  ---- GND commun
                                            OUT+ ---- broche "5V" du Wemos
                                            OUT- ---- broche "GND" du Wemos
```

- `C1` (électrolytique 330–470 µF) absorbe les transitoires lorsque
  l'alimentation bascule d'un connecteur à l'autre.
- Le module buck (réglé précisément sur 5.0V) alimente la broche **5V**
  du Wemos D1 mini, qui alimente elle-même le régulateur 3.3V embarqué sur
  la carte (ne pas injecter 12V directement sur cette broche : c'est bien
  du 5V régulé qu'il faut fournir ici).
- Alternative 100% linéaire si vous préférez éviter un module à
  découpage : régulateur 7805 (avec condensateurs 100nF/100µF en entrée et
  sortie + petit dissipateur, car il dissipera environ 1 W à ~150 mA).
- Prévoir un module ≥1A pour garder de la marge lors des pics de
  consommation WiFi (~300 mA côté 3.3V, soit ~500 mA équivalent côté 5V).

---

## 4. Étage d'entrée (canal A et canal B, circuit identique x2)

Le tach Noctua est un signal **collecteur ouvert**, tiré au niveau haut par
une résistance interne du ventilateur vers son propre +12V (repos ≈ 12V,
impulsions vers 0V, 2 impulsions/tour). On l'adapte au niveau logique 3.3V
du Wemos avec un transistor NPN monté en inverseur/adaptateur de niveau :

```
TACH Noctua (repos ~12V, impulsions vers 0V)
        |
      [Rb 10k]
        |
        +----- Base Q1 (BC547)
                Emetteur Q1 ------------------------ GND
                Collecteur Q1 --+--[Rc 4.7k]-------- 3.3V (rail regulee du Wemos)
                                |
                                +--[Rprot 220R]-----> D2 / D7 (GPIO, entree)
                                |
                             [C_filt 1nF]
                                |
                               GND
```

Fonctionnement :
- Tach au repos (~12V) → base de Q1 polarisée → Q1 saturé → collecteur
  proche de 0V → **GPIO lit un niveau BAS**.
- Tach en impulsion (~0V) → Q1 bloqué → collecteur tiré à 3.3V par Rc →
  **GPIO lit un niveau HAUT**.

Le firmware déclenche donc sur front **montant** (RISING) pour détecter le
début de chaque impulsion. Ce montage isole totalement le GPIO ESP8266
(non tolérant au 5V) de la tension réelle du signal tach (jusqu'à 12V),
quel que soit son niveau exact.

---

## 5. Étage de sortie (canal A et canal B, circuit identique x2)

L'onduleur applique sur sa broche TACH un pull-up interne dont la tension
est inconnue (3.3V, 5V ou 12V selon modèle). La solution qui fonctionne
**dans tous les cas sans aucune modification matérielle** est de reproduire
exactement le comportement d'un vrai ventilateur : une sortie
**collecteur ouvert**, qui ne fait que tirer la ligne vers le bas, sans
jamais chercher à la tirer vers le haut (c'est l'onduleur, via son propre
pull-up, qui gère le niveau haut, quelle que soit sa tension).

```
D5 / D6 (GPIO, sortie) ---[Rb 1k]--- Base Q3 (BC547)
                                      |
                          [Rbe 10k] --+-- Emetteur Q3 ------- GND
                                      |
                                Collecteur Q3 ---------------> fil TACH
                                                                vers connecteur
                                                                onduleur Deye
```

Fonctionnement :
- GPIO HAUT → Q3 saturé → tire la ligne TACH vers le bas (0V) → imite
  l'impulsion active-bas d'un vrai ventilateur, quelle que soit la tension
  de pull-up de l'onduleur (le transistor ne fait que "avaler" du courant,
  jamais en fournir).
- GPIO BAS → Q3 bloqué → la ligne est libre, tirée au niveau haut par le
  pull-up interne de l'onduleur (3.3V, 5V ou 12V, peu importe).
- `Rbe` (10 kΩ base-émetteur) garantit que Q3 reste bien bloqué si le GPIO
  se retrouve en haute impédance pendant le démarrage/flashage de l'ESP8266
  (évite toute impulsion parasite pendant le boot).
- Choisir un transistor NPN classique (BC547, 2N3904, S9013…) : leur
  Vceo (40–45V) couvre largement un pull-up 12V, et le courant en jeu est
  très faible (quelques mA, dicté par la résistance de pull-up interne de
  l'onduleur).

---

## 6. LED d'état

```
D1 (GPIO5) ---[Rled 330-470R]--- Anode LED --- Cathode LED --- GND
```

- **Allumée fixe** : les deux canaux reçoivent un tach valide et le
  Wemos émet activement un signal simulé vers l'onduleur sur les 2 canaux.
- **Clignotante (2 Hz)** : au moins un canal n'a pas (ou plus) de signal
  tach valide depuis 3 secondes (ventilateur non alimenté, déconnecté, ou
  démarrage en cours) — la sortie correspondante reste alors au repos
  (niveau haut côté onduleur) plutôt que de simuler un régime inventé.

---

## 7. Table des broches Wemos D1 mini

| Broche carte | GPIO | Fonction | Remarque |
|---|---|---|---|
| D1 | GPIO5 | LED d'état | libre, sans contrainte de boot |
| D2 | GPIO4 | Entrée tach canal A (9cm) | libre |
| D5 | GPIO14 | Sortie tach canal A vers Deye | libre |
| D6 | GPIO12 | Sortie tach canal B vers Deye | libre |
| D7 | GPIO13 | Entrée tach canal B (6cm) | libre |
| 5V | — | Alimentation depuis module buck | voir §3 |
| GND | — | Masse commune | voir §3 |
| D0, D3, D4, D8, RX, TX | GPIO16,0,2,15,3,1 | **non utilisées** | fonctions de boot/flash — à ne pas câbler |

---

## 8. Points d'attention / mise en service

- **Choix du ratio** : commencez avec un ratio proche de
  `RPM_typique_NMB / RPM_typique_Noctua` à tension équivalente, puis
  affinez via l'interface web en comparant au RPM que l'onduleur affichait
  avec les ventilateurs d'origine.
- **Connecteurs** : utilisez des connecteurs Y ou des dérivations sur les
  fils `+12V`/`GND` d'origine pour alimenter 2 Noctua par connecteur
  d'origine ; n'interceptez que le fil `TACH` pour l'entrée/sortie du
  Wemos.
- **Sécurité fonctionnelle** : ce montage désactive de fait la fonction
  de détection de panne de ventilateur basée sur le RPM réel (puisque
  l'onduleur ne voit plus que des RPM artificiellement gonflés). Il est
  recommandé de surveiller occasionnellement le fonctionnement réel des
  Noctua (bruit, rotation visible) puisque l'onduleur ne le fera plus à
  votre place pour vous.
