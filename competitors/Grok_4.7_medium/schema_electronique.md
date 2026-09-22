# Schéma — simulateur tach Deye / Noctua

Firmware associé : `deye_fan/deye_fan.ino` (LOLIN Wemos D1 mini, ESP8266).

Le Wemos D1 Mini V2.3.0 porte un régulateur ME6211 dont l'entrée maximum est 6 V. Le 12 V des ventilateurs ne va jamais sur la broche 5 V. La broche tach de l'onduleur, dont la tension de rappel est inconnue (3,3 V, 5 V ou 12 V), ne touche jamais une broche de l'ESP : elle est seulement commutée vers la masse par un transistor.

## Rôle de chaque fil

Deux paires de Noctua. Dans chaque paire, les deux ventilateurs partagent le +12 V et la masse du connecteur d'origine. Un seul tach de la paire est lu. Le second tach est isolé (gaine), jamais relié au premier : deux collecteurs ouverts en parallèle se masquent.

Le tach du Noctua n'est pas relié à l'onduleur. La broche tach de l'onduleur va uniquement au collecteur du transistor de sortie.

Couleurs Noctua habituelles, à confirmer au multimètre : noir = masse, jaune = +12 V, vert = tach. Sur le connecteur de l'onduleur, le brochage n'est pas supposé : repérer la masse, le +12 V et le tach avant de souder. Un tach de ventilateur 3 broches est un collecteur ouvert, deux impulsions par tour.

```
  Connecteur onduleur 6 cm                         Paire NF-A6x25
  +12 V  -------------------------------+---------- jaune des deux 6 cm
  GND    -------------------------------+---------- noir des deux 6 cm
  tach   ---- collecteur Q10            +---------- vert d'UN seul 6 cm
                                         |
                                         +-- vert du second 6 cm : isolé

  Connecteur onduleur 9 cm                         Paire NF-A9
  +12 V  -------------------------------+---------- jaune des deux 9 cm
  GND    -------------------------------+---------- noir des deux 9 cm
  tach   ---- collecteur Q11            +---------- vert d'UN seul 9 cm
                                         |
                                         +-- vert du second 9 cm : isolé
```

Toutes les masses sont communes : GND des deux connecteurs, GND du Wemos, émetteurs des transistors, retours des condensateurs.

## Alimentation

Les deux +12 V peuvent être absents l'un sans l'autre, et ne sont pas forcément à la même tension. Chacun passe par un « diode OU » qui bloque vraiment le retour vers le rail éteint.

Un NPN diode classique (base+collecteur ensemble) utilise la jonction base-émetteur, qui part en Zener vers 5 V. Le rail vivant remonterait alors le rail éteint vers ~6 V et pourrait faire tourner l'autre paire. Ici la jonction utilisée est collecteur-base : le SS8050 tient 40 V en inverse (Vcbo), et l'émetteur n'est pas câblé.

```
 +12 V canal 6 cm
      |
      +--[ R1 1 Ω ]-- collecteur Q1 SS8050 --+
      |                    base Q1 ----------+-- VRAW
      |                    émetteur Q1 : non connecté, isolé
      |
      +--[ R2 1 Ω ]-- collecteur Q2 SS8050 --+
                           base Q2 ----------+
                           émetteur Q2 : isolé

 +12 V canal 9 cm
      |
      +--[ R3 1 Ω ]-- collecteur Q3 SS8050 --+
      |                    base Q3 ----------+-- VRAW
      |                    émetteur Q3 : isolé
      |
      +--[ R4 1 Ω ]-- collecteur Q4 SS8050 --+
                           base Q4 ----------+
                           émetteur Q4 : isolé

 VRAW ----+---- C1 100 µF 25 V ---- GND
          |
        [ R10 10 Ω 2 W ]
          |
        VMID ----+---- collecteurs Q5 et Q6 (SS8050, ballast)
                 |
               [ R11 470 Ω ]
                 |
               base commune de Q5 et Q6
                 |
               collecteur Q7 BC547
                 |
               émetteur Q7 ---- GND
                 |
               base Q7 ---- nœud F

 Q5 émetteur --[ R12 1 Ω ]--+---- V5
 Q6 émetteur --[ R13 1 Ω ]--+
                            |
                          [ R14 10 kΩ ]
                            |
                          nœud F
                            |
                          [ R15 1,5 kΩ ]
                            |
                           GND

 C2 100 nF entre la base commune de Q5/Q6 et GND
 C3 470 µF 10 V entre V5 et GND
 C4 100 nF entre la broche 5 V du Wemos et GND, contre la carte

 V5 -------- broche 5 V du Wemos
 GND ------- broche G du Wemos
```

Q7 compare une fraction de V5 à sa tension base-émetteur et vole le courant de base de Q5/Q6 dès que V5 est trop haut.

```
V5 ≈ 0,65 V × (R14 + R15) / R15 ≈ 0,65 × (10 + 1,5) / 1,5 ≈ 5,0 V
```

À froid la consigne monte d'environ 15 mV/°C de moins (la Vbe baisse quand il fait chaud, et inversement). Mesurer avant de brancher le Wemos.

Procédure :

1. Ne pas brancher le Wemos. Charger V5 avec 220 Ω ½ W vers GND.
2. Appliquer 12 V sur une entrée, puis sur l'autre, puis sur les deux.
3. V5 doit rester entre 4,6 V et 5,3 V dans les trois cas. Au-dessus de 5,4 V, ne pas connecter la carte (le ME6211 est spécifié jusqu'à 6 V).
4. Si V5 est trop haut, augmenter R15 (1,8 kΩ donne environ 4,2 V, trop bas ; 1,6 kΩ est un pas fin). Si V5 est trop bas, diminuer R15 vers 1,3 kΩ.
5. Retirer la charge 220 Ω, brancher le Wemos, remesurer V5 sous WiFi. Les boîtiers de Q5 et Q6 doivent rester dans le flux d'air des Noctua.

Conso typique du D1 mini en AP+STA : 80 à 150 mA, pointes courtes vers 300 mA. À 100 mA, R10 chute de 1 V (0,1 W) et Q5+Q6 dissipent environ 0,5 W à deux. Sans le flux d'air des ventilateurs, les TO-92 dépassent 80 °C.

Le 5 V USB de la carte et ce régulateur peuvent coexister le temps d'un flash : la diode USB de la carte empêche le 5 V régulé de remonter vers le PC. Premier flash de préférence avec le 12 V coupé.

## Entrées tach (Noctua → ESP8266)

Un exemplaire par canal. Le tach Noctua est un collecteur ouvert : le rappel 4,7 kΩ vers 3,3 V est obligatoire, sinon la ligne ne remonte pas. Le transistor d'entrée encaisse 3,3 V, 5 V ou 12 V sur le fil tach (le 10 kΩ limite le courant de base) et la broche ESP ne voit que 0 ou 3,3 V.

Le signal sur le GPIO est inversé. Le firmware mesure la période entre fronts descendants du GPIO, donc l'inversion ne change pas les RPM.

```
Canal 6 cm

tach Noctua 6 cm ----+----[ R20 4,7 kΩ ]---- 3V3 du Wemos
                     |
                   [ R21 10 kΩ ]
                     |
                    base Q8 BC547
                     |
                   [ R22 100 kΩ ]
                     |
                    GND -------- émetteur Q8
                    |
                    collecteur Q8 ----+---- D1 / GPIO5
                                      |
                                    [ R23 10 kΩ ]
                                      |
                                     3V3

Canal 9 cm : même circuit.
tach Noctua 9 cm, Q9 BC547, R24 4,7 kΩ, R25 10 kΩ, R26 100 kΩ, R27 10 kΩ
collecteur Q9 ---- D2 / GPIO4
```

## Sorties tach (ESP8266 → Deye)

Collecteur ouvert. À l'état bloqué, la tension de la broche tach est celle du rappel interne de l'onduleur, quelle qu'elle soit. À l'état passant, le transistor tire la ligne près de 0 V. Aucun cavalier à changer entre 3,3 V, 5 V et 12 V.

GPIO haut = transistor passant = tach onduleur à 0 V. Le firmware émet un carré 50 %. L'onduleur compte les fronts ; le rapport cyclique n'entre pas dans le calcul des RPM. Au reset, le GPIO est à 0 et R31 maintient le transistor bloqué : la ligne n'est pas forcée pendant le boot.

Le 2N2222 (Vceo 40 V) est choisi parce que le collecteur voit la tension de rappel. Un SS8050 (Vceo 25 V) est trop juste sur un rail 12 V.

```
Canal 6 cm

D5 / GPIO14 ----[ R30 2,2 kΩ ]----+---- base Q10 2N2222
                                  |
                                [ R31 47 kΩ ]
                                  |
                                 GND -------- émetteur Q10
                                 |
                                 collecteur Q10 ---- tach onduleur 6 cm
                                 (pas de rappel ajouté de notre côté)

Canal 9 cm : même circuit.
D6 / GPIO12, R32 2,2 kΩ, R33 47 kΩ, Q11 2N2222
collecteur Q11 ---- tach onduleur 9 cm
```

Courant de base ≈ (3,3 − 0,7) / 2,2 kΩ ≈ 1,2 mA. Un rappel onduleur de 1 kΩ vers 12 V demande 12 mA : le 2N2222 est saturé, le niveau bas reste sous 0,3 V.

## LED

LED bleue de la carte, GPIO2 (D4), allumée à l'état bas. Rien à câbler.

- deux canaux en simulation : allumée fixe
- un seul canal : clignotement rapide
- aucun tach reçu : un éclair bref chaque seconde

## Broches Wemos utilisées

| Broche carte | GPIO | Direction | Pourquoi celle-ci |
|---|---|---|---|
| D1 | 5 | entrée tach 6 cm | libre au boot |
| D2 | 4 | entrée tach 9 cm | libre au boot |
| D5 | 14 | sortie 6 cm | libre au boot, à 0 le transistor est bloqué |
| D6 | 12 | sortie 9 cm | idem |
| D4 | 2 | LED | doit être haut au boot, ce qui éteint la LED |
| 5 V | — | alimentation régulée | jamais le 12 V brut |
| 3V3 | — | rappels d'entrée | sort du ME6211 de la carte |
| G | — | masse commune | — |

GPIO0 (D3), GPIO2 en sortie de puissance et GPIO15 (D8) sont des broches de boot : elles ne portent ni un tach ni un transistor.

## Nomenclature

| Repère | Composant | Qté |
|---|---|---|
| Q1–Q4 | SS8050, diode OU collecteur-base, émetteur isolé | 4 |
| Q5 Q6 | SS8050, ballast série | 2 |
| Q7 | BC547, asservissement 5 V | 1 |
| Q8 Q9 | BC547, entrée tach | 2 |
| Q10 Q11 | 2N2222, sortie collecteur ouvert | 2 |
| R1–R4, R12, R13 | 1 Ω | 6 |
| R10 | 10 Ω 2 W | 1 |
| R11 | 470 Ω | 1 |
| R14, R21, R23, R25, R27 | 10 kΩ | 5 |
| R15 | 1,5 kΩ | 1 |
| R20, R24 | 4,7 kΩ | 2 |
| R22, R26 | 100 kΩ | 2 |
| R30, R32 | 2,2 kΩ | 2 |
| R31, R33 | 47 kΩ | 2 |
| C1 | 100 µF 25 V | 1 |
| C2, C4 | 100 nF | 2 |
| C3 | 470 µF 10 V | 1 |
| — | Wemos D1 Mini V2.3.0 | 1 |

Les condensateurs ne sont pas dans la liste de transistors, et le régulateur oscille ou décroche au pic WiFi sans eux.

## Mise sous tension

1. Flasher `deye_fan.ino` par USB, carte « LOLIN(WEMOS) D1 R2 & mini », 80 MHz, 4 Mo. Le moniteur série est à 115200.
2. Point d'accès `DeyeFan-XXXX`, mot de passe `deyefan1`, page `http://192.168.4.1`.
3. Couper l'USB, raccorder l'alimentation 12 V et les tach, remettre le 12 V.
4. Vérifier V5 une dernière fois, puis comparer les RPM lus et les RPM simulés sur la page. Les ratios de départ (2,35 et 2,80) viennent des vitesses nominales ; le 9 cm est à ajuster si le 09225VE-12N-CU ne tourne pas à 4500 RPM.
