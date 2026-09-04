# Deye Fan Simulator — schéma et installation

## Limites à connaître avant le montage

- Ne jamais appliquer 12 V à la broche `5V`, `3V3` ou à un GPIO du Wemos.
- Si les deux sorties ventilateur Deye sont hors tension, aucune alimentation
  dérivée de ces sorties ne peut maintenir le simulateur actif. C'est une
  impossibilité physique. Ajouter alors une alimentation 12 V auxiliaire à
  l'entrée d'OR-ing si l'onduleur exige un tach avant d'alimenter ses fans.
- Les sorties tach sont à collecteur ouvert. Elles acceptent donc un pull-up
  Deye à 3,3 V, 5 V ou 12 V sans changement de composant.
- L'ESP8266 est monocœur et partage le silicium avec la radio. Timer1 et une ISR
  en IRAM rendent la génération indépendante de `loop()`, du HTTP et des
  allocations, mais ne constituent pas une garantie métrologique absolue de
  jitter nul. Une telle garantie exige un générateur matériel séparé (CPLD,
  petit MCU sans radio ou multiplicateur de fréquence externe), vérifié à
  l'oscilloscope dans l'onduleur.

## Nomenclature

| Repère | Valeur / composant | Rôle |
|---|---|---|
| U1 | Wemos D1 mini V2.3.0 | Contrôleur |
| U2 | Buck-boost 3–18 V vers 5,0 V, au moins 1 A, avec UVLO | Alimentation U1 |
| D1, D2 | SS34, 3 A / 40 V Schottky | OR-ing des deux alimentations |
| F1, F2 | Fusible réarmable PTC 0,5 à 1 A | Protection des connecteurs |
| TVS1 | SMBJ18A | Écrêtage des transitoires 12 V |
| C1 | 470 µF / 25 V, faible ESR | Réservoir en entrée de U2 |
| C2 | 100 nF céramique | Découplage en entrée de U2 |
| C3 | 470 µF / 10 V, faible ESR | Réservoir 5 V |
| C4 | 100 nF céramique | Découplage 5 V |
| Qin9, Qin6 | 2N3904 ou BC547 | Adaptation tach Noctua vers 3,3 V |
| Rin9, Rin6 | 47 kΩ | Limitation du courant de base d'entrée |
| Rbe9, Rbe6 | 100 kΩ | Arrêt franc des transistors d'entrée |
| Rfan9, Rfan6 | 4,7 kΩ | Pull-up tach Noctua vers son propre +12 V |
| Respin9, Respin6 | 10 kΩ | Pull-up des entrées ESP vers 3,3 V |
| Qout9, Qout6 | BC337 ou 2N2222 | Sorties tach Deye à collecteur ouvert |
| Rbout9, Rbout6 | 2,2 kΩ | Résistance de base de sortie |
| Roff9, Roff6 | 47 kΩ | Maintien des sorties coupées au démarrage |

Les condensateurs exigés par le fabricant du module U2 restent à installer si
le module ne les contient pas déjà. Ne pas remplacer U2 par le régulateur
linéaire du Wemos : sa dissipation sous 12 V serait excessive.

## Schéma d'alimentation

```text
Connecteur Deye 9 cm, +V ---- F1 ----|>|----+
                                     D1     |
Connecteur Deye 6 cm, +V ---- F2 ----|>|----+---- VIN_OR ---- U2 ---- +5V_WEMOS
                                     D2     |     buck-boost           |
                                            |      5,0 V               +-- C3 470 µF -- GND
                                      TVS1 18 V                        +-- C4 100 nF -- GND
                                            |
                                      C1 470 µF / 25 V
                                      C2 100 nF
                                            |
GND Deye 9 cm ------------------------------+------------------------------- GND
GND Deye 6 cm ------------------------------+
GND des quatre Noctua ----------------------+
GND U2 et GND Wemos ------------------------+

+5V_WEMOS ---------------------------------------------- broche 5V de U1
```

D1 et D2 empêchent un connecteur d'alimenter l'autre. Le convertisseur doit
accepter sans dommage la plage réelle mesurée sur les connecteurs, y compris
les rampes et les coupures. Si la tension peut descendre sous la tension
minimale de U2, employer un vrai buck-boost à large plage et UVLO, pas un simple
module LM2596.

Les deux ventilateurs de même taille sont raccordés en parallèle pour
l'alimentation uniquement. Un seul tach par taille est lu :

```text
Deye +V 9 cm  ---> +12 V des deux NF-A9-FLX
Deye GND 9 cm ---> GND des deux NF-A9-FLX
Tach d'un seul NF-A9-FLX ---> circuit ENTREE 9 cm
Tach du second NF-A9-FLX ---> non connecté, isolé

Deye +V 6 cm  ---> +12 V des deux NF-A6x25-FLX
Deye GND 6 cm ---> GND des deux NF-A6x25-FLX
Tach d'un seul NF-A6x25-FLX ---> circuit ENTREE 6 cm
Tach du second NF-A6x25-FLX ---> non connecté, isolé
```

Ne jamais relier ensemble les deux fils tach de ventilateurs.

## Entrées tach Noctua vers ESP8266

Construire deux fois ce circuit. Les valeurs fonctionnent lorsque le fan est
alimenté entre 5 V et 13,2 V. Le pull-up 4,7 kΩ consomme environ 2,6 mA sous
12 V, inférieur à la limite de 5 mA annoncée par Noctua.

```text
                 +V du connecteur du fan concerné
                              |
                         Rfan 4,7 kΩ
                              |
TACH Noctua ------------------+---- Rin 47 kΩ ---- B
                                                   |\
                                      Rbe 100 kΩ   | > Qin 2N3904
                                      B ----/\/\---|/
                                             |      E
                                            GND     |
                                                    GND

3V3 Wemos ---- Respin 10 kΩ ----+---- D5/GPIO14 pour le fan 9 cm
                                |
                                C de Qin

Deuxième exemplaire : sortie collecteur vers D6/GPIO12 pour le fan 6 cm.
```

Le transistor inverse le signal, ce qui ne change ni sa fréquence ni le calcul
des RPM. Cette interface évite qu'une tension tach 5 V ou 12 V atteigne un
GPIO 3,3 V.

## Sorties ESP8266 vers entrées tach Deye

Construire deux fois ce circuit. Aucun pull-up externe n'est ajouté : le
simulateur utilise celui déjà présent dans l'onduleur, quelle que soit sa
tension entre 3,3 V et 12 V.

```text
D1/GPIO5 (canal 9 cm) ---- Rbout 2,2 kΩ ---- B
                                               |\
                                  Roff 47 kΩ   | > Qout BC337
                                  B ----/\/\---|/
                                         |      E
                                        GND     |
                                                GND commun

Entrée TACH du connecteur Deye 9 cm ------------ C de Qout

Deuxième exemplaire :
D2/GPIO4 -> Rbout -> base de Qout6
collecteur de Qout6 -> entrée TACH du connecteur Deye 6 cm
émetteur et Roff -> GND commun.
```

Quand le GPIO est bas ou que le Wemos redémarre, Qout est bloqué et la ligne
tach Deye est relâchée. Quand le GPIO est haut, Qout la tire à la masse. Le
BC337 supporte 45 V ; vérifier toutefois avant raccordement que la broche Deye
est bien une entrée tach avec pull-up et non une sortie de puissance.

## Affectation finale

| Wemos | Signal |
|---|---|
| `5V` | Sortie 5,0 V de U2 |
| `GND` | Masse commune |
| `3V3` | Pull-up 10 kΩ des deux entrées uniquement |
| `D5 / GPIO14` | Collecteur de Qin9 |
| `D6 / GPIO12` | Collecteur de Qin6 |
| `D1 / GPIO5` | Résistance de base de Qout9 |
| `D2 / GPIO4` | Résistance de base de Qout6 |
| `D4 / GPIO2` | LED intégrée : allumée pendant la simulation |

## Mise en service sûre

1. Débrancher complètement l'onduleur et attendre la décharge selon sa
   documentation de maintenance. Les tensions PV et batterie sont mortelles.
2. Sans Wemos, mesurer au multimètre puis à l'oscilloscope la tension
   d'alimentation de chaque connecteur et la tension de pull-up de chaque entrée
   tach Deye. Ne continuer que si cette dernière est comprise entre 0 et 12 V.
3. Régler U2 à **5,00 V avant** de connecter le Wemos.
4. Tester chaque entrée avec un fan et vérifier au moniteur série / à la page
   web que les RPM sont plausibles.
5. Tester les collecteurs Qout hors onduleur avec un pull-up 10 kΩ vers 12 V.
   Vérifier un niveau bas inférieur à 0,4 V et une fréquence égale à la
   fréquence d'entrée multipliée par le ratio.
6. Raccorder les entrées tach Deye, démarrer avec un ratio prudent, puis ajuster
   à partir des RPM des ventilateurs NMB d'origine réellement mesurés.
7. Vérifier à l'oscilloscope les deux sorties pendant un trafic Wi-Fi soutenu,
   puis contrôler les températures de l'onduleur en charge. Une absence
   d'alarme RPM ne prouve pas un refroidissement suffisant.

## Interface

Au premier démarrage :

- SSID : `Deye-Fan-Simulator`
- mot de passe : `deye-fan`
- URL : `http://192.168.4.1/`
- ratios initiaux : `2,00` sur chaque canal

Le mode AP+STA est permanent. Les ratios, SSID et mots de passe sont stockés
avec version et CRC dans l'EEPROM émulée. Les mots de passe ne sont pas
réaffichés dans le formulaire. Un changement Wi-Fi provoque un redémarrage ;
un changement de ratio seul prend effet sans redémarrage.

## Références électriques

- Noctua, guide microcontrôleur : sortie tach à collecteur ouvert, deux
  impulsions par tour.
- ESP8266 Arduino Core : les ISR GPIO doivent résider en IRAM ; Timer1 offre
  un compteur 23 bits avec préscalers 1, 16 et 256.

