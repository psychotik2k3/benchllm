# Schéma électrique et montage

## Avertissements indispensables

Cet appareil se raccorde à un onduleur contenant des tensions dangereuses.
Couper toutes les sources AC, photovoltaïques et batterie, attendre le temps
prescrit par Deye et vérifier l'absence de tension avant d'intervenir.

Le brochage physique du connecteur Deye doit être identifié à partir du
câblage d'origine et au multimètre. Les noms `+FAN`, `GND` et `TACH_DEYE`
ci-dessous désignent les fonctions électriques, pas une position supposée
sur le connecteur.

Hors tension, mesurer la résistance entre les masses des deux connecteurs :
elle doit être proche de 0 ohm. Ne pas réaliser ce montage si elles ne sont
pas communes. Il faudrait alors des optocoupleurs et une alimentation isolée.

## Nomenclature

- 1 × Wemos D1 Mini V2.3.0 / ESP8266.
- 2 × BC337 (Q1 et Q2). Vérifier le brochage C-B-E du fabricant.
- 2 × résistances 2,2 kΩ, bases de Q1/Q2.
- 2 × résistances 100 kΩ, rappel base-émetteur de Q1/Q2.
- 2 × résistances 4,7 kΩ, pull-up des tachymètres Noctua.
- 2 × résistances 1 kΩ, série sur les entrées tachymétriques.
- 2 × condensateurs céramique 1 nF, filtrage des entrées.
- 1 × LED et 1 × résistance 1 kΩ.
- 2 × diodes Schottky SS34 ou équivalent 3 A / 40 V (D1 et D2).
- 2 × fusibles réarmables PTC 0,5 A (PF1 et PF2), dédiés à l'alimentation
  électronique.
- 1 × TVS SMBJ18A unidirectionnelle, à adapter si la tension mesurée dépasse
  16 V en fonctionnement normal.
- 1 × convertisseur DC/DC régulé 5,0 V, 1 A minimum, entrée 6–36 V minimum.
  Choisir un buck-boost 3–30 V si le système doit rester actif lorsque la
  tension ventilateur descend sous 6 V.
- 1 × condensateur 470 µF / 35 V et 1 × 100 nF à l'entrée du convertisseur.
- 1 × condensateur 470 µF / 10 V et 1 × 100 nF à sa sortie.
- Connecteurs, gaine thermorétractable et plaque à pastilles ou PCB.

Les tensions maximales du convertisseur, des condensateurs, des diodes et de
la TVS doivent être réévaluées à partir des mesures réelles dans l'onduleur.

## Vue d'ensemble

```text
 Connecteur Deye 6 cm                         Paire Noctua 6 cm
 +FAN6 --------------------------------------+---- rouge ventilateur A
    |                                        +---- rouge ventilateur B
    +-- PF1 --|>|--+                         GND -- noirs A et B
             D1    |
                   +-- VOR -- protection -- convertisseur 5 V -- Wemos 5V
             D2    |
    +-- PF2 --|>|--+
    |
 +FAN9 --------------------------------------+---- rouges Noctua 9 cm A et B
 Connecteur Deye 9 cm                        GND -- noirs A et B

 Un seul fil tach jaune par paire va à une entrée Wemos.
 L'autre fil tach jaune de la paire est isolé séparément.

 TACH_DEYE6 <--------- collecteur Q1  BC337  émetteur -------- GND commun
 TACH_DEYE9 <--------- collecteur Q2  BC337  émetteur -------- GND commun
```

Les barres sur D1/D2 représentent des Schottky orientées de chaque connecteur
vers `VOR` : anode côté connecteur, cathode (côté bague) côté `VOR`. Elles
interdisent le retour de courant vers le connecteur dont la tension est plus
basse ou absente.

## Bloc d'alimentation

```text
 +FAN6 --- PF1 0,5 A --- anode D1 SS34 cathode ---+
                                                   +--- VOR ---+--- IN+ DC/DC
 +FAN9 --- PF2 0,5 A --- anode D2 SS34 cathode ---+           |
                                                               +--- cathode TVS
                                                               +--- + 470 µF
                                                               +--- 100 nF

 GND6 ---------------------------------------------------------+--- IN- DC/DC
 GND9 ---------------------------------------------------------+
                                                               +--- anode TVS
                                                               +--- - condensateurs

 DC/DC OUT+ 5,0 V --------+---------------- Wemos broche 5V
                          +--- + 470 µF / 10 V
                          +--- 100 nF
 DC/DC OUT- --------------+---------------- Wemos GND
```

Les ventilateurs restent directement alimentés par leur propre connecteur.
Seule la branche Wemos passe dans PF1/PF2 et les Schottky. Ne jamais relier
directement `+FAN6` et `+FAN9`.

Régler et vérifier le convertisseur à 5,0 V avant de brancher le Wemos. Ne pas
connecter simultanément le câble USB et l'alimentation de l'onduleur pendant
la programmation ou les essais.

## Entrée tachymétrique 6 cm

```text
 Noctua 6 cm A, fil tach jaune --- R1 1 kΩ ---+--- D1 / GPIO5
                                              |
                                     R2 4,7 kΩ
                                              |
                                         Wemos 3V3
                                              |
                                   C1 1 nF entre GPIO5 et GND

 Noctua 6 cm A, noir ------------------------- GND commun
 Noctua 6 cm B, fil tach jaune --------------- isolé, non connecté
```

## Entrée tachymétrique 9 cm

```text
 Noctua 9 cm A, fil tach jaune --- R3 1 kΩ ---+--- D2 / GPIO4
                                              |
                                     R4 4,7 kΩ
                                              |
                                         Wemos 3V3
                                              |
                                   C2 1 nF entre GPIO4 et GND

 Noctua 9 cm A, noir ------------------------- GND commun
 Noctua 9 cm B, fil tach jaune --------------- isolé, non connecté
```

La sortie tach Noctua est un collecteur ouvert et produit deux impulsions par
tour. Son pull-up doit donc être relié exclusivement au 3,3 V du Wemos. Les
fils `TACH_DEYE` ne sont jamais reliés à ces entrées.

## Sortie simulée vers le Deye, canal 6 cm

```text
 D5 / GPIO14 --- R5 2,2 kΩ --- base Q1 BC337
                                  |
                             R6 100 kΩ
                                  |
 GND commun ----------------------+--- émetteur Q1

 TACH_DEYE6 --------------------------- collecteur Q1
```

## Sortie simulée vers le Deye, canal 9 cm

```text
 D6 / GPIO12 --- R7 2,2 kΩ --- base Q2 BC337
                                  |
                             R8 100 kΩ
                                  |
 GND commun ----------------------+--- émetteur Q2

 TACH_DEYE9 --------------------------- collecteur Q2
```

Ne placer aucun pull-up externe sur ces sorties : l'onduleur fournit son
propre pull-up inconnu. Les BC337 présentent à l'onduleur un collecteur ouvert
et isolent les GPIO de sa tension, qu'elle soit de 3,3 V, 5 V ou 12 V.

## LED

```text
 D7 / GPIO13 --- R9 1 kΩ --- anode LED
                                  cathode LED --- GND
```

La LED s'allume dès qu'au moins un canal génère un signal. Elle s'éteint quand
les deux canaux attendent un tachymètre valide.

## Raccordement fonctionnel complet

Pour chaque taille :

1. Relier les deux fils rouges Noctua au `+FAN` du connecteur correspondant.
2. Relier les deux fils noirs au `GND` correspondant.
3. Relier le fil tach d'un seul Noctua à son circuit d'entrée Wemos.
4. Isoler individuellement le tach du second Noctua.
5. Relier le `TACH_DEYE` uniquement au collecteur du BC337 correspondant.

Avant de doubler les ventilateurs, comparer la somme de leurs courants
indiqués sur l'étiquette, plus la consommation de la branche Wemos ramenée à
12 V, avec le courant du ventilateur d'origine et la capacité documentée du
connecteur. La branche Wemos peut consommer des pointes importantes pendant
une émission Wi-Fi.

## Mise en service

1. Onduleur totalement hors tension, confirmer la masse commune et chaque
   fonction du connecteur.
2. Alimenter le montage sur une alimentation de laboratoire limitée en
   courant. Vérifier 5,0 V puis 3,3 V.
3. Vérifier que Q1/Q2 sont bloqués au démarrage et que leurs collecteurs ne
   présentent aucun court-circuit permanent vers la masse.
4. Faire tourner chaque ventilateur et vérifier son entrée : environ
   100 Hz à 3 000 RPM et 53,3 Hz à 1 600 RPM.
5. À l'oscilloscope, confirmer les fréquences de sortie attendues :
   250 Hz pour 3 000 RPM × 2,5 et 106,7 Hz pour 1 600 RPM × 2,0.
6. Brancher ensuite les collecteurs aux entrées Deye et vérifier que leur
   tension haute réelle ne dépasse pas la tension Vce admissible du BC337.
7. Soumettre l'interface web à des actualisations répétées et contrôler la
   fréquence produite à l'oscilloscope avant exploitation définitive.

## Accès et compilation

- Carte Arduino IDE : `LOLIN(WEMOS) D1 R2 & mini`.
- Coeur ESP8266 recommandé : version 3.1.x.
- Fréquence CPU : 80 MHz ou 160 MHz, détectée automatiquement.
- Point d'accès : `DeyeFan-XXXXXX`, où `XXXXXX` dépend de la puce.
- Mot de passe AP initial : `DeyeFanSetup`.
- Interface : `http://192.168.4.1/`.
- Bibliothèques : uniquement celles livrées avec le coeur ESP8266.

Le sommeil Wi-Fi est désactivé pour réduire la variabilité temporelle. Cela
augmente la consommation ; le convertisseur 5 V doit être dimensionné en
conséquence.
