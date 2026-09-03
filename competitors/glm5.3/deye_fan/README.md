# Deye Fan Tach — Multiplicateur de signaux tach pour onduleur Deye (remplacement des ventilateurs par des Noctua)

Ce projet remplace les ventilateurs NMB d'origine d'un onduleur **Deye SUN-8K-SG05LP1-EU-AM2-P** (NMB
09225VE-12N-CU 92 mm + NMB 06025VE-12N-CL 60 mm, 12 V, 3 fils) par des Noctua silencieux (2 × NF-A9-FLX 92 mm +
2 × NF-A6x25-FLX 60 mm), tout en **trompant proprement la surveillance tach de l'onduleur** : un Wemos D1 mini
(ESP8266) mesure la vitesse réelle des Noctua et lui renvoie un signal tach multiplié, réglable en interface web.

## Contenu du dossier

| Fichier | Rôle |
|---|---|
| `deye_fan.ino` | Firmware complet (carte **LOLIN(WEMOS) D1 R2 & mini**). Aucune bibliothèque externe : tout est dans le coeur ESP8266. |
| `schema_electronique.md` | Schéma électronique complet : alimentation, circuits d'entrée/sortie, câblage, BOM, contrôles, calibration. |

## Fonctionnalités

- 2 canaux indépendants (92 mm / 60 mm) ; tach des 2 ventilateurs de chaque taille câblés en parallèle ;
- **ratio multiplicateur par canal** réglable **sans recompilation** (interface web, mémorisé en EEPROM avec CRC) ;
- interface web : RPM mesurés et RPM simulés en temps réel, réglages, configuration Wi-Fi, scan des réseaux,
  redémarrage, portail captif ;
- Wi-Fi **AP + STA simultanés**, paramètres persistants, mDNS (`http://deye-fan.local` via la box) ;
- LED embarquée : **fixe = simulation active sur tous les canaux**, **clignotante = attente d'un signal tach** ;
- mode « cache-panne » optionnel par canal : si le tach d'entrée disparaît, le module simule une vitesse fixe
  configurable (désactivé par défaut — à activer en connaissance de cause, cela masque aussi les pannes réelles) ;
- **génération du signal 100 % dans une interruption matérielle** (timer FRC1 + compteur de cycles CPU) :
  aucune influence mesurable du WiFi, du serveur web ou des sauvegardes sur la fréquence/phase envoyée à l'onduleur.

## Installation logicielle (Arduino IDE)

1. Arduino IDE ≥ 2.x avec le coeur **esp8266** (Gestionnaire de cartes → « esp8266 by ESP8266 Community », ≥ 3.0
   recommandé ; testé jusqu'à 3.1.x).
2. Ouvrir `deye_fan/deye_fan.ino` (le dossier du croquis doit s'appeler `deye_fan` — c'est déjà le cas).
3. Carte : **LOLIN(WEMOS) D1 R2 & mini** ; réglages conseillés :
   - CPU : **80 MHz** (le firmware force 80 MHz pour garantir la base de temps) ;
   - Flash : 4 MB (FS: none ou FS: 1MB — l'EEPROM utilise le dernier secteur, les deux réglages conviennent) ;
   - Upload speed : 921600 ; port COM du module.
4. Téléverser. Aucune bibliothèque supplémentaire à installer.

## Premier démarrage

1. Câbler conformément à `schema_electronique.md` et vérifier les contrôles §6 avant de raccorder l'onduleur.
2. Le module crée un point d'accès permanent : SSID **DeyeFan-xxxxxx** (xxxxxx = id de la puce), mot de passe
   **noctua12v**. Ouvrir **http://192.168.4.1** (portail captif automatique sur téléphone).
3. Onglet Wi-Fi : renseigner SSID/mot de passe de la box → « Enregistrer & redémarrer ». Le module se reconnecte
   ensuite en STA tout en gardant son AP actif. Sur le réseau de la box : `http://deye-fan.local`.
4. Onglet canaux : régler les ratios (voir calibration ci-dessous), puis « Appliquer » (effet en < 100 ms).

## Calibration des ratios (important)

Le ratio fait correspondre la vitesse Noctua mesurée à la vitesse que l'onduleur attendait du NMB d'origine :

```
ratio = RPM attendus par l'onduleur (NMB d'origine) / RPM réels du Noctua
```

Valeurs constructeur de référence (à vérifier) : NF-A9-FLX ≈ **1600 tr/min**, NF-A6x25-FLX ≈ **3100 tr/min**.
Les RPM exacts des NMB d'origine n'étant pas publiés de façon fiable, les défauts sont des hypothèses à calibrer :

- canal 1 (92 mm) : défaut **2.5** (≈ NMB 4000 / Noctua 1600) ;
- canal 2 (60 mm) : défaut **2.3** (≈ NMB 7000 / Noctua 3100).

Procédure : l'interface web affiche les RPM réels des Noctua (`RPM ventilateur (mesuré)`) et les RPM simulés
(`RPM simulé → Deye`). Si l'onduleur remonte une alarme ventilateur, augmenter le ratio par pas de 0,25. Si
l'onduleur pilote la vitesse de ses ventilateurs en abaissant la tension d'alimentation, tout suit
proportionnellement : le signal simulé reste calé sur la vitesse réelle des Noctua, exactement comme le tach
d'un NMB ralenti.

## Architecture & isolation du signal (pourquoi le WiFi n'influence pas la sortie)

- **Sortie** : chaque front est généré dans l'interruption matérielle du timer FRC1 (timer1, mode EDGE, 80 MHz)
  et daté sur le **compteur de cycles CPU** : le front suivant est calculé en temps absolu. Une requête HTTP ou
  un burst WiFi ne peut au pire décaler un front de la durée de latence de l'interruption (~1 µs) ; la phase
  suivante repart exactement à l'heure (aucune dérive cumulée). Les changements de fréquence sont appliqués au
  front suivant, sans rupture de phase.
- **Entrée** : les fronts tach sont horodatés dans une interruption GPIO (anti-rebond 400 µs, buffer de 128
  timestamps) ; la mesure (comptage sur fenêtre + lissage exponentiel) se fait dans `loop()`, hors interruptions.
- Les réglages (EEPROM/flash) ne surviennent qu'à la demande de l'utilisateur ; un front éventuellement retardé
  par l'écriture flash est resynchronisé au cycle suivant.

## Dépannage

| Symptôme | Cause probable / action |
|---|---|
| « RPM ventilateur (mesuré) » reste à — | Tach Noctua non câblé ou pull-up 10 k absent ; vérifier fil tach (vert) et masse commune. |
| Le module redémarre en boucle | Buck mal réglé ou alimentation insuffisante : régler 5,0–5,2 V, contrôler C3/C4. |
| L'onduleur affiche une alarme ventilateur | Augmenter le ratio ; vérifier la tension au repos sur la broche tach de l'onduleur (§6 du schéma) ; activer éventuellement le mode cache-panne. |
| Impossible d'accéder à 192.168.4.1 | Attendre ~20 s après boot ; l'AP est permanent, le portail captif peut demander de « rester connecté ». |
| Ratios non appliqués | Cliquer « Appliquer » (les changements sont pris en compte dans les 100 ms). |

## Avertissement

Fourni « tel quel », sans garantie. Testez le montage sur bancale (alimentation de labo ou simple chargeur USB)
**avant** de le raccorder à l'onduleur, et ne modifiez jamais le câblage de l'onduleur lui-même : le module se
branche sur les connecteurs ventilateurs existants. La simulation du tach masque la surveillance de *vitesse*
des ventilateurs, pas la température : vérifiez le refroidissement réel de l'onduleur après pose.

