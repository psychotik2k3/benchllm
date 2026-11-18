# Deye SUN-8K-SG05LP1-EU-AM2-P · Adaptateur tachymétrique à deux canaux

Le Wemos sert la page web. Une iCEBreaker **iCE40UP5K-SG48 à horloge 12 MHz** mesure les deux ventilateurs et produit les impulsions. Son horloge tourne même si le Wemos redémarre. La LED verte de l’iCEBreaker indique qu’au moins un canal simule une rotation. Les sorties restent ouvertes au démarrage jusqu’à réception des ratios et d’une période d’entrée valide.

## Alimentation et ventilateurs

Chaque prise conserve son propre circuit moteur. Dans les dessins, **X** vaut successivement **9** et **6**. Construire chaque circuit portant un X en deux exemplaires. G9, G6 et G_LOG sont trois réseaux distincts dans l’adaptateur. Identifier les broches Deye par mesure et documentation de l’appareil avant intervention. Les numéros et couleurs ci-dessous concernent les Noctua.

```text
JX Deye +vent ───┬──────────────── rouge (2) ventilateur X-A
                 ├──────────────── rouge (2) ventilateur X-B
                 └── FX ── VX ──── +Vin (1) UX
JX Deye retour ──┬──────────────── noir (1) X-A et X-B
                 └── GX ────────── −Vin (2) UX

UX = Traco TDN 5-0911WI, isolé, 5 V / 1 A
VX ─── 10 µF / 25 V X7R en parallèle avec 100 nF ─── GX
UX On/Off (4) ouvert ; 5 NC ; −Vout (6) vers G_LOG ; +Vout (7) vers 5VX

5V9 ────── VIN1 (3) ┐
                    │ U3 LM66200DRL        VOUT (2 et 7) ─── +5_LOG
5V6 ────── VIN2 (6) ┘                      GND (1,5), /ON (4) ─── G_LOG
5V9, 5V6 : chacun 1 µF vers G_LOG          ST (8) libre
+5_LOG ─── 47 µF / 10 V et 100 nF ─── G_LOG
+5_LOG ─┬─ 5V du Wemos                    G_LOG ─┬─ G du Wemos
        └─ 5V du header iCEBreaker               └─ GND iCEBreaker
```

U3 sélectionne automatiquement une source et bloque le courant inverse. Chaque convertisseur doit pouvoir alimenter les deux cartes seul. Les sorties 3,3 V des cartes restent distinctes. Les résistances côté FPGA utilisent exclusivement le 3,3 V de l’iCEBreaker. Programmer les cartes individuellement sur USB, faisceau débranché. En service, débrancher les alimentations USB.

Les UX acceptent **4,5 à 13,2 V continus**. Vérifier à l’oscilloscope différentiel la tension réelle entre les deux bornes de chaque prise, démarrage et régulation compris. Ce schéma suppose une alimentation continue lorsqu’un ventilateur est commandé. Une alimentation hachée peut produire de faux fronts et faire redémarrer l’électronique. Lorsque les deux sources sont coupées, le Wi-Fi et la conversion s’arrêtent. Un chevauchement de coupures plus long que la réserve capacitive peut également provoquer un redémarrage.

FX protège uniquement la dérivation électronique. Dimensionner chacun des deux fusibles selon la capacité vérifiée du connecteur, le câblage et le courant d’appel. À titre de dimensionnement, 0,50 A sous 5 V demandent environ 0,26 A sous 12 V à 80 % de rendement, en plus des deux moteurs et de l’entrée optique. Mesurer la pointe réelle sous trafic Wi-Fi et pendant le démarrage des deux ventilateurs. Une seule prise doit supporter cette charge. [Convertisseurs Traco](https://www.tracopower.com/sites/default/files/products/datasheets/tdn5wi_datasheet.pdf) · [Double diode idéale TI](https://www.ti.com/lit/ds/symlink/lm66200.pdf)

## Entrées tach · répéter pour chaque canal

```text
VX ── 6,8 kΩ ──●──────── jaune (3) Noctua X-A
               └─ 2,2 kΩ ── B QIX (2N3904)
                            B ── 47 kΩ ── GX
                            E ─────────── GX
VX ── 1 kΩ / ¼ W ── A (1) OIX K (2) ── C QIX

OIX = VO617A-3 DIP4
  E (3) ── G_LOG
  C (4) ──●──── P1A1 si X=9, P1A2 si X=6
          └──── 4,7 kΩ ── 3V3_FPGA
Jaune (3) du X-B : isoler séparément l’extrémité.
```

Le tach ouvert du Noctua laisse QIX conduire. Lorsque le ventilateur tire sa ligne au niveau bas, QIX s’éteint. La résistance de 6,8 kΩ limite le courant du tach à moins de 2 mA à 13,2 V. Le FPGA voit des niveaux de son propre domaine de tension. Il mesure un front descendant par impulsion. [Sortie tach et deux impulsions par tour, Noctua](https://www.noctua.at/en/support/faqs/microcontroller-guide-pwm-setup-and-rpm-monitoring)

## Sorties tach · répéter pour chaque canal

```text
P1A3 si X=9, P1A4 si X=6 ── 1,8 kΩ ── B QOX (2N3904)
                                       B ── 10 kΩ ── G_LOG
                                       E ─────────── G_LOG
3V3_FPGA ── 180 Ω ── A (1) OOX K (2) ── C QOX

OOX = VO617A-3 DIP4
  C (4) ─────────────────── broche tach de JX Deye
  E (3) ─────────────────── GX de cette même prise
```

Le phototransistor de sortie reproduit un collecteur ouvert. La résistance de rappel interne du Deye fixe le niveau haut à 3,3, 5 ou 12 V. Le câblage est identique pour les trois tensions. La résistance base-émetteur maintient la sortie ouverte pendant la configuration du FPGA. Vérifier le brochage physique des 2N3904 selon leur fabricant.

**Condition électrique à vérifier sur le Deye** — la broche doit être une entrée avec rappel résistif, dont le courant et les seuils permettent au phototransistor de la tirer au niveau bas. Dimensionnement visé ≤ 2 mA. Mesurer la tension à vide puis avec une résistance de test connue (par exemple 10 kΩ vers GX). En déduire la résistance de rappel par Rtest × (Vvide / Vchargée − 1), puis le courant par Vvide / Rrappel. Vérifier les niveaux et temps de montée/descente au connecteur. Éviter tout court-circuit de caractérisation. [Photocoupleur et courbes de saturation Vishay](https://www.vishay.com/docs/83430/vo617a.pdf)

## Liaison logique

| Signal | Wemos | iCEBreaker |
|---|---|---|
| /CS | D1, GPIO5 | P1B1, rappel 10 kΩ vers 3V3_FPGA |
| Horloge SPI | D5, GPIO14 | P1B2, rappel 10 kΩ vers G_LOG |
| MOSI | D7, GPIO13 | P1B3 |
| MISO | D6, GPIO12 | P1B4 |
| Référence SPI | G | G_LOG |
| Activité | — | LED verte intégrée, broche FPGA 37, active à zéro |

Liaison point à point en 3,3 V. Garder les fils courts et grouper chaque signal avec son retour. La page lit des instantanés matériels. Le FPGA valide la longueur, la signature, le contrôle XOR et les bornes de chaque commande avant de changer les deux registres de ratio. Une transaction interrompue peut rester en pause indéfiniment. Les compteurs tach continuent indépendamment. [Carte iCEBreaker](https://docs.icebreaker-fpga.org/hardware/icebreaker/) · [Brochage de référence](https://raw.githubusercontent.com/icebreaker-fpga/icebreaker-examples/main/icebreaker/icebreaker.pcf)

## Programmation et accès

1. Sur table, ouvrir `deye_fan/deye_fan.ino` dans Arduino IDE. Installer **esp8266 by ESP8266 Community 3.1.2**, choisir **LOLIN(WEMOS) D1 R2 & mini** et une partition flash avec LittleFS. Ouvrir le moniteur série à **115200** dès le premier démarrage pour relever le SSID et la clé générée. Si cette clé initiale est perdue, effacer toute la flash et téléverser à nouveau pour réinitialiser la configuration.
2. Programmer la flash de l’iCEBreaker par USB avec `iceprog tach.bin` depuis le répertoire `fpga`. Pour reconstruire le fichier avec Yosys, nextpnr et IceStorm, utiliser les commandes ci-dessous.
3. Assembler sur table. Se connecter au point d’accès indiqué puis ouvrir **http://192.168.4.1/**. Identifiant **admin**, mot de passe identique à la clé AP. Le ratio initial est **1,00** pour chaque canal. Le formulaire accepte de 1,00 à 8,00 par pas de 0,01. Le point d’accès reste disponible quand la station rejoint le routeur. L’ESP8266 partage un seul canal radio entre les deux modes. Utiliser un réseau de confiance pour cette interface HTTP.
4. Après enregistrement, relever l’adresse STA dans la page. Après changement de clé ou de SSID AP, reconnecter le navigateur avec les nouveaux paramètres. Les réglages sont écrits dans un fichier temporaire LittleFS puis renommés. Les mesures ne sont jamais écrites en flash.

```sh
yosys -p "synth_ice40 -top top -json tach.json" tach.v
nextpnr-ice40 --up5k --package sg48 --freq 12 --json tach.json --pcf icebreaker.pcf --asc tach.asc
icepack tach.asc tach.bin
```

## Comportement temporel et réception sur banc

Les deux canaux échantillonnent en parallèle, après synchronisation, à **8 µs**. Trois échantillons identiques qualifient un changement de niveau. Deux fronts espacés de 2 ms à moins de 500 ms permettent d’acquérir une période. Un accumulateur fractionnaire distribue les demi-périodes de sortie entre deux nombres entiers de ticks. À période mesurée et ratio constants, la durée numérique des demi-périodes varie d’au plus un tick. La précision sur le connecteur dépend aussi des photocoupleurs, des fronts d’entrée, de l’oscillateur et de la charge électrique. Aucune tâche logicielle ne participe à cette cadence.

La colonne « Simulé » affiche la période mesurée entre deux activations successives du transistor **dans le FPGA**. Elle permet de vérifier le générateur. Pour vérifier le faisceau et le niveau réellement reçu, mesurer au collecteur côté Deye. Le calcul en tr/min suppose deux impulsions par tour. Relever la fréquence des NMB d’origine en fonction de la commande Deye avant de choisir un ratio.

| Essai sur table | Résultat attendu |
|---|---|
| Entrée 9 cm à 50 Hz, ratio 2,50 | 1 500 tr/min lus, sortie à 125 Hz, 3 750 tr/min simulés |
| Simultanément, entrée 6 cm à 100 Hz, ratio 1,75 | 3 000 tr/min lus, sortie à 175 Hz, 5 250 tr/min simulés |
| Bloquer l’une des entrées à 0 puis à 1 | Son collecteur reste ouvert après environ 500 ms sans front. L’autre canal continue |
| Désactiver les deux entrées | LED verte éteinte après expiration des deux délais |
| Couper le Wemos seul en maintenant le FPGA alimenté, liaison SPI débranchée | Les deux canaux conservent les ratios chargés et continuent à suivre les entrées |
| Requêtes HTTP continues puis enregistrement du même ratio | Comparer les distributions de demi-périodes au scope, avec le même générateur et la même charge, à celles mesurées au repos |
| Alimenter chaque prise seule, puis les deux à tensions différentes | +5_LOG stable, aucune alimentation de l’autre paire de moteurs, sorties sans signal ouvertes après expiration |
| Rappel de banc successivement à 3,3 V, 5 V, 12 V avec 10 kΩ | Même fréquence, niveau haut égal au rappel, niveau bas compatible avec l’entrée |

Le doublement des ventilateurs ne garantit pas le débit à travers les dissipateurs. L’arrêt du second ventilateur de chaque paire reste invisible au tach surveillé. Valider le démarrage des quatre moteurs, le débit sous la perte de charge réelle et les températures au régime de charge prévu avant d’utiliser un ratio supérieur à 1. Effectuer le câblage de l’onduleur après consignation des sources réseau, PV et batterie et décharge selon la procédure constructeur. Les essais temporels et thermiques sur matériel restent à effectuer.

