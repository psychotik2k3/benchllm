# Simulateur Tachémétrique Onduleur Deye (Architecture et Schéma)

Ce document décrit en détail l'architecture matérielle et logicielle du simulateur tachémétrique conçu pour l'onduleur Deye, permettant de le tromper avec des ventilateurs Noctua via un microcontrôleur Wemos D1 Mini.

---

## 1. Schéma Électronique Complet

Le montage s'articule autour des composants imposés. Afin de garantir l'intégrité électrique du système face à deux alimentations 12V asynchrones et une tension de pull-up onduleur inconnue, l'approche suivante est utilisée :

### A. Alimentation Redondante et Abaissement (Onduleur -> Wemos)
Les connecteurs ventilateur d'origine fournissent du 12V. Il est crucial d'unifier ces sources (sans retour de courant) et de les abaisser à la tension de fonctionnement de l'ESP8266 sans surchauffe.

1. **Création du bus 12V unifié :**
   - Connectez le pin +12V du connecteur de l'onduleur **9cm** à l'anode d'une diode (ex: 1N4148, 1N4007).
   - Connectez le pin +12V du connecteur de l'onduleur **6cm** à l'anode d'une seconde diode.
   - Reliez les cathodes de ces deux diodes ensemble. Ce nœud est votre source d'alimentation "12V-IN" ininterrompue.
   - *Note d'expertise matérielle :* Les diodes sont indispensables ici. Si vous n'en possédez pas, vous pouvez utiliser deux transistors NPN montés en "pseudo-diode" (Base soudée au Collecteur) pour remplir cette fonction avec les composants fournis.

2. **Abaissement 12V -> 5V :**
   - Le nœud "12V-IN" doit impérativement traverser un **convertisseur Buck Step-Down** (ex: module Mini-360, LM2596) réglé sur 5V. 
   - L'utilisation de résistances ou d'un NPN en régulateur série est fortement proscrite car l'ESP8266 peut consommer >300mA en transmission WiFi, ce qui exigerait de dissiper plus de 2 Watts et détruirait n'importe quel transistor TO-92 instantanément.
   - La sortie +5V du Buck se connecte à la broche **5V** du Wemos D1 Mini.
   - Les masses (GND des connecteurs 9cm et 6cm, GND IN/OUT du Buck, GND du Wemos) doivent toutes être communes.

### B. Entrées Tach (Ventilateurs Noctua -> Wemos)
Les ventilateurs Noctua respectent le standard Intel (broche Tach en collecteur ouvert).
1. Connectez la ligne GND et +12V des ventilateurs Noctua directement avant les diodes pour qu'ils profitent des pleins 12V.
2. Reliez le signal Tach (fil jaune) du Noctua 9cm à la broche **D1** du Wemos (via une résistance série de 1 kΩ pour protéger le microcontrôleur en cas de court-circuit).
3. Reliez le signal Tach du Noctua 6cm à la broche **D2** du Wemos (via une résistance série de 1 kΩ).
*Le code active les pull-ups internes (`INPUT_PULLUP`) en 3.3V, protégeant nativement l'ESP.*

### C. Sorties Tach Simulées (Wemos -> Onduleur)
L'onduleur possède sa propre tension de lecture (peut-être 3.3V, 5V, ou 12V). Nous devons impérativement la simuler par un montage en **collecteur ouvert (Open-Collector)**. C'est ici que les transistors NPN fournis entrent en jeu (ex: 2N2222, BC547).

1. **Sortie 9cm :**
   - Wemos broche **D5** -> Résistance de 1 kΩ -> **Base** du Transistor NPN 1.
   - **Émetteur** du Transistor NPN 1 -> Masse (GND) commune.
   - **Collecteur** du Transistor NPN 1 -> Pin de lecture Tach du connecteur 9cm de l'onduleur.
2. **Sortie 6cm :**
   - Wemos broche **D6** -> Résistance de 1 kΩ -> **Base** du Transistor NPN 2.
   - **Émetteur** du Transistor NPN 2 -> Masse (GND) commune.
   - **Collecteur** du Transistor NPN 2 -> Pin de lecture Tach du connecteur 6cm de l'onduleur.

*De cette manière, le Wemos ferme ponctuellement le circuit de l'onduleur sans jamais exposer ses broches à des tensions inconnues ou excessives.*

---

## 2. Architecture Logicielle (Isolation Avancée - Zéro Jitter)

La difficulté principale réside dans l'utilisation simultanée du protocole WiFi (qui met régulièrement le processeur en pause pendant plusieurs millisecondes) et de la génération d'un signal d'horloge de précision.

La solution employée pour satisfaire la contrainte "totalement isolée" ne repose pas sur `analogWrite`, `tone()` ni la boucle `loop()`, mais sur **le Timer1 matériel et les interruptions NMI-like** :

1. **Calcul des cibles asynchrone :**
   - La boucle `loop()` ne fait que calculer, une fois par seconde, les RPM des ventilateurs et les périodes de battement correspondantes qu'elle inscrit atomiquement en mémoire.
2. **Exécution Matérielle Stricte (`IRAM_ATTR timer1_isr`) :**
   - Le hardware interne (Timer1) est configuré pour lancer une interruption toutes les **50 microsecondes (20 kHz)** très précises.
   - La fonction d'interruption réside directement dans la RAM interne (`IRAM_ATTR`), évitant les latences d'accès à la mémoire Flash. 
   - Même si l'ESP8266 est en plein traitement d'une requête HTTP ou d'une renégociation réseau, l'interruption prend la main et bascule les transistors NPN. Le signal RPM simulé perçu par l'onduleur de réception sera d'une absolue stabilité métrologique.
3. **Persistance Douce :**
   - L'EEPROM n'est écrite que lors du clic sur le bouton "Enregistrer", moment où l'appareil valide le redémarrage. Ainsi, les micro-latences induites par l'écriture Flash (`EEPROM.commit`) n'interfèrent jamais pendant l'utilisation nominale.