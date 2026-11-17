# Deye / Noctua — gpt6.1sol medium

## Résultat et limite déterminante

Le sketch `deye_fan/deye_fan.ino` est une implémentation complète **ESP8266 seul, à précision non garantie sous WiFi**. Ce n'est pas une réalisation conforme à l'exigence d'absence de tout impact mesurable du réseau. Cette exigence est impossible à garantir par une architecture purement logicielle sur ce matériel : un timer matériel qui déclenche une ISR ne génère pas lui-même les fronts GPIO ; ceux-ci dépendent encore de la disponibilité du processeur. Les timestamps des entrées pris en ISR dépendent eux aussi de la latence de service des interruptions.

`IRAM_ATTR`, `volatile`, des sections critiques brèves et Timer1 réduisent les perturbations ; ils ne créent pas une ressource matérielle indépendante. Une NMI ne résout pas la capture GPIO ordinaire, l'arbitrage matériel ni toutes les contraintes du SDK. Il serait trompeur d'annoncer « zéro jitter WiFi » ici. Les écritures flash présentent une difficulté supplémentaire.

Pour respecter la contrainte stricte, ajouter un moteur temps réel matériel indépendant, présenté dans `ARCHITECTURE_STRICTE.md`. Cette variante exige du matériel et un firmware supplémentaires ; elle n'est pas implémentée dans le sketch livré. Si l'absence d'impact réseau est un critère impératif, **ne pas retenir la variante ESP8266 seule**.

## Livrables

- `deye_fan/deye_fan.ino` : sketch Arduino commenté ; deux captures tach, ratios, deux sorties, LED, interface autonome sans CDN, WiFi AP+STA, configuration persistante avec CRC.
- `SCHEMA.md` : schéma électrique détaillé sous forme de circuits et de netlist de raccordement, alimentation et nomenclature.
- `ARCHITECTURE_STRICTE.md` : changement d'architecture nécessaire pour l'isolation temporelle.
- `VERIFICATION.md` : compilation effectuée, limites des contrôles et protocole de banc.
- `verification/` : compte rendu de compilation, symboles ISR, empreintes SHA-256 et binaire compilé. La chaîne de compilation a été téléchargée exclusivement dans ce dossier de projet puis retirée après vérification, sans lecture des autres projets competitors.

## Compilation Arduino IDE

1. Dans les URL de gestionnaire de cartes supplémentaires, ajouter `https://arduino.esp8266.com/stable/package_esp8266com_index.json`.
2. Installer **esp8266 by ESP8266 Community 3.1.2**, version de référence de ce projet.
3. Ouvrir `deye_fan/deye_fan.ino`. Le dossier porte le même nom que le sketch pour Arduino IDE.
4. Choisir **LOLIN(WEMOS) D1 R2 & mini**, identifiant CLI `esp8266:esp8266:d1_mini`. CPU 80 MHz ; carte 4 Mo. Les bibliothèques employées sont incluses dans ce core : ESP8266WiFi, ESP8266WebServer, EEPROM.
5. Compiler et téléverser **sur établi, installation déconnectée de l'onduleur**. Ne pas injecter 12 V sur la broche 5 V, 3V3 ou un GPIO.

## Utilisation

Au premier démarrage, AP `Deye-Fan-XXXXXX`, clé `DeyeXXXXXX`, avec `XXXXXX` = identifiant de puce en hexadécimal majuscule sur six chiffres. Le SSID permet donc de retrouver la clé initiale : **la remplacer** dans l'interface. Le moniteur série à 115200 bauds fournit aussi le SSID et la clé initiale calculée.

Ouvrir `http://192.168.4.1/`, utilisateur **admin**, mot de passe = clé AP actuelle. Le WiFi STA peut être configuré en parallèle ; son adresse est affichée. Un réseau STA absent ne bloque ni l'AP ni la synthèse. AP et STA utilisent une seule radio et partagent le canal de la STA. Pas de portail captif ni de mDNS ; utiliser l'adresse IP.

La page affiche une fois par seconde les RPM lus et les RPM de sortie **théoriques**, la présence de tach, le plafonnement éventuel et des diagnostics de retard des ISR. Les changements de ratio prennent effet sur la synthèse sans recompilation ni recâblage. L'AP reste activé même si la STA se déconnecte. Les mots de passe enregistrés ne sont jamais renvoyés par l'API. Un champ mot de passe vide conserve la valeur précédente ; la case STA sans mot de passe efface sa clé. Changer le SSID STA vers un réseau différent nécessite aussi d'indiquer sa clé, sinon l'ancienne est conservée.

Les formulaires sont authentifiés et les écritures demandent un jeton de session. HTTP Basic est utilisé sur HTTP, sans chiffrement applicatif : accès réservé à un réseau local de confiance, aucune redirection de port Internet. La configuration, y compris les clés WiFi, réside en clair en flash.

LED intégrée D4 : allumée = au moins un canal synthétisé ; éteinte = aucune voie active. Elle ne vérifie pas le signal électrique reçu par le Deye. Pour réinitialiser les paramètres sans effacer le firmware, bouton facultatif D0 → GND maintenu trois secondes ; voir sa résistance de rappel dans le schéma. Les sorties tach deviennent alors au ratio 1, et le WiFi revient aux identifiants initiaux. La sauvegarde suit les mêmes règles ci-dessous.

## Persistance et coupure d'alimentation

Les ratios et identifiants sont appliqués d'abord en RAM. Le code sauvegarde automatiquement en EEPROM émulée uniquement après une période calme : aucun canal actif, pas d'impulsion récente, puis trois secondes supplémentaires de calme. Dans le cas normal, cela représente environ 4,5 secondes ou davantage après le dernier front, selon le régime et les appels de `loop()`.

**Attendre que la page confirme la fin de la sauvegarde avant de couper l'alimentation.** Si les ventilateurs fonctionnent continuellement, l'écriture reste en attente ; une coupure dans cet état restaure les anciennes valeurs sauvegardées. Pour sauvegarder sur établi, alimenter le Wemos séparément en USB avec l'installation Deye déconnectée et les deux entrées tach à l'arrêt. En exploitation, si les deux connecteurs s'éteignent simultanément, le Wemos peut perdre son alimentation avant d'avoir pu sauvegarder. Le condensateur proposé n'est pas une alimentation secourue et ne garantit pas une sauvegarde à l'extinction.

Ce compromis évite de lancer volontairement un effacement flash pendant la synthèse. Il ne garantit pas l'absence de perturbation si un ventilateur redémarre pendant l'écriture. La sauvegarde est un seul bloc avec CRC et version ; une coupure pendant l'effacement peut perdre les paramètres précédents. Un bloc invalide charge les valeurs par défaut : aucune configuration corrompue n'est utilisée silencieusement. Une FRAM externe ou un moteur tach séparé serait nécessaire pour une persistance immédiate sans ces effets.

## Traitement des tach

Les Noctua fournissent un collecteur ouvert, normalement deux impulsions par tour. Avec une période `T` en microsecondes entre deux fronts descendants, `RPM = 30 000 000 / T`. On suppose la même convention deux impulsions/tour côté Deye, à vérifier sur le ventilateur NMB d'origine. Plus généralement, `f_sortie / f_entrée = ratio_RPM × PPR_Deye / PPR_Noctua`.

- Entrées sur D1/GPIO5 (A9) et D2/GPIO4 (A6). Sorties vers bases NPN D5/GPIO14 et D6/GPIO12.
- Acquisition par interruptions GPIO, moyenne exponentielle entière de coefficient 1/4. Trois périodes valides nécessaires avant activation, soit quatre fronts depuis la première acquisition.
- Périodes acceptées de 2 000 à 500 000 µs, soit 60 à 15 000 RPM à deux impulsions/tour. Les parasites plus courts sont rejetés ; ce filtre ne prouve pas qu'un signal périodique provient d'un rotor.
- Multiplicateurs de 1 à 8, indépendants ; valeur initiale 1. Aucune vitesse NMB ou valeur de ratio n'a été inventée.
- Synthèse approximativement à 50 % par Timer1 à échéances absolues. La prochaine transition dépend de la période filtrée ; ce n'est pas un duplicateur de chaque impulsion, ni une PLL verrouillée en phase.
- Inverse du ratio arrondi en Q12 puis demi-période arrondie à la microseconde. Erreur de quantification distincte du jitter ; pour ratio 8, erreur relative de l'inverse au plus environ 0,098 %, à laquelle s'ajoute l'arrondi de période et l'erreur de l'horloge.
- Demi-période minimum 250 µs : sortie limitée à 2 000 Hz, soit 60 000 RPM conventionnels ; l'interface affiche le plafonnement et la fréquence théorique réellement programmée, et non simplement le produit demandé.
- Arrêt de la simulation après absence de fronts durant `max(300 ms, 3 × période_filtrée)`, donc au plus 1,5 s de maintien théorique, plus le délai de service du timer. Ce timeout n'est pas une garantie matérielle en cas de CPU figé.
- Pas de génération de fronts dans `loop()`, aucune allocation/réseau/flottant en ISR, accès GPIO par registres. ISR timer réveillée au plus toutes les 2 ms pour surveiller les arrêts. Sections critiques limitées aux copies et aux deux coefficients de ratio.
- En cas de retard supérieur à une demi-période, resynchronisation sans rafale de rattrapage. Le compteur de resynchronisations augmente. Les diagnostics ne mesurent ni les fronts d'entrée perdus ni les niveaux électriques et ne constituent pas une preuve de jitter nul.

## Compatibilité à établir sur le Deye réel

Le courant disponible, le brochage des connecteurs, leur mode de commande, la résistance de tirage tach et les seuils d'alarme de ce modèle Deye ne sont pas établis par les données fournies. **Mesurer ces points avant raccordement** ; le schéma ne présume pas l'ordre des broches NMB. Une tension tach de 12 V ne sera pas transmise à l'ESP grâce aux NPN de sortie, à condition qu'il s'agisse bien d'une entrée à pull-up limité en courant.

Deux ventilateurs en parallèle ne garantissent pas le débit ni la pression statique d'un NMB, particulièrement à travers un dissipateur. Le changement nécessite une validation thermique au régime et à la température ambiante défavorables. La simulation RPM masque la vitesse réelle à l'onduleur ; elle ne compense aucune insuffisance de refroidissement. Seul un ventilateur par paire est instrumenté ici : l'arrêt du second reste invisible. Ne jamais relier les deux fils tach d'une paire.

## Sources techniques consultées

Sources fabricants/projet officiel, consultées le 17 novembre 2026 :

- [ESP8266 Arduino — interruptions et limites](https://arduino-esp8266.readthedocs.io/en/stable/reference.html).
- [Implémentation Timer1 du core 3.1.2](https://github.com/esp8266/Arduino/blob/3.1.2/cores/esp8266/core_esp8266_timer.cpp) : un callback d'interruption est exécuté par le CPU ; ce n'est pas une sortie compare matérielle autonome.
- [Noctua — lecture RPM, collecteur ouvert et deux impulsions/tour](https://www.noctua.at/en/support/faqs/microcontroller-guide-pwm-setup-and-rpm-monitoring).
- [NF-A6x25 FLX — caractéristiques et brochage trois fils](https://www.noctua.at/en/products/nf-a6x25-flx/specifications).
- [NF-A9 FLX — fiche fabricant](https://cdn.noctua.at/media/cf89cea3/Noctua%20NF-A9%20FLX%20Specification.pdf?download=true).
- [onsemi — 2N3904, tension collecteur et saturation](https://www.onsemi.com/pdf/datasheet/2n3903-d.pdf).
- [Pololu S13V10F5 — convertisseur buck-boost 5 V](https://www.pololu.com/product/4083).
- [Espressif — recommandations d'alimentation](https://documentation.espressif.com/esp8266_hardware_design_guidelines_en.html).
- [Wemos — GPIO 3,3 V et famille D1 mini](https://www.wemos.cc/en/latest/d1/d1_mini.html). La carte réelle V2.3.0 doit être contrôlée ; ne pas présumer que le chemin USB est celui d'une révision récente.
