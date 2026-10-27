# Simulateur tachymétrique Deye — conception

## Objectif

Mesurer un ventilateur Noctua NF-A6x25-FLX et un NF-A9-FLX, puis présenter
à l'onduleur deux signaux tachymétriques indépendants dont les fréquences
sont multipliées par des ratios configurables. Les valeurs initiales sont
respectivement 2,50× et 2,00×.

## Architecture retenue

- Les sorties tachymétriques Noctua, de type collecteur ouvert et à deux
  impulsions par tour, sont tirées à 3,3 V puis mesurées sur D1/GPIO5 et
  D2/GPIO4.
- Les interruptions d'entrée ne font qu'horodater et accumuler les périodes.
  Elles résident en IRAM et n'appellent aucune fonction réseau.
- Le moteur d'ondes du coeur Arduino ESP8266 pilote D5/GPIO14 et D6/GPIO12.
  Il utilise Timer1 et le compteur de cycles pour conserver la fréquence
  moyenne et éviter toute dérive pendant le traitement Wi-Fi.
- Deux BC337 reproduisent des sorties collecteur ouvert. Les pull-up internes
  de l'onduleur peuvent ainsi être à 3,3 V, 5 V ou 12 V.
- Le réseau fonctionne en `WIFI_AP_STA`. Une interface HTTP configure les
  ratios et le réseau STA et affiche les quatre valeurs RPM.

L'ESP8266 ne possède pas de PWM matériel. Cette architecture minimise
fortement le jitter et isole le chemin critique des traitements HTTP, mais
elle ne constitue pas une garantie absolue de jitter nul face aux sections
internes non interruptibles du SDK Wi-Fi. Cette limite a été acceptée.

## Alimentation

Chaque alimentation positive de connecteur Deye traverse son propre fusible
réarmable et sa propre diode Schottky. Les cathodes sont réunies avant une
protection TVS, un condensateur réservoir et un convertisseur 5 V. Cette
combinaison empêche tout retour de courant d'un connecteur vers l'autre.

Le convertisseur doit accepter au minimum 6–36 V en entrée, fournir 5,0 V
régulés sous 1 A et tolérer l'usage automobile/industriel. Si l'onduleur
abaisse ses sorties sous la tension minimale du convertisseur, un modèle
buck-boost 3–30 V doit être employé. Le Wemos s'éteint nécessairement quand
les deux connecteurs sont hors tension.

Les masses des deux connecteurs doivent être communes. Cette continuité doit
être vérifiée hors tension avant montage. Des masses non communes imposeraient
une conception galvanisée avec optocoupleurs et alimentation isolée.

## Comportement logiciel

- Une sortie démarre après la mesure d'une période tachymétrique valide.
- Une moyenne glissante amortit les variations du ventilateur avant mise à
  jour de la fréquence produite.
- Après deux secondes sans front valide, la sortie concernée est arrêtée et
  son transistor reste bloqué.
- La LED externe est allumée lorsqu'au moins une sortie simule un signal.
- Les ratios acceptés vont de 0,10× à 10,00×.
- Les paramètres sont validés par CRC et conservés dans l'EEPROM émulée.
- Le point d'accès reste actif, même lorsque la station est connectée.

## Brochage

- D1 / GPIO5 : tach Noctua 6 cm
- D2 / GPIO4 : tach Noctua 9 cm
- D5 / GPIO14 : commande tach simulé Deye 6 cm
- D6 / GPIO12 : commande tach simulé Deye 9 cm
- D7 / GPIO13 : LED externe active à l'état haut

Ces broches n'imposent aucun niveau de démarrage susceptible d'empêcher
l'amorçage normal de l'ESP8266.

## Critères de validation

1. Le sketch compile pour `LOLIN(WEMOS) D1 R2 & mini`.
2. Les sorties restent inactives en l'absence de tachymètre.
3. À 3 000 tr/min en entrée 6 cm, la sortie indique environ 7 500 tr/min.
4. À 1 600 tr/min en entrée 9 cm, la sortie indique environ 3 200 tr/min.
5. Les ratios et identifiants Wi-Fi survivent à un redémarrage.
6. Une charge HTTP répétée ne modifie pas la fréquence moyenne produite.
7. Aucun connecteur Deye n'alimente l'autre lorsque leurs tensions diffèrent.
