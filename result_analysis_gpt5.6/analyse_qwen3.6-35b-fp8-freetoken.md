# Analyse — qwen3.6-35b-fp8-freetoken

**Sources analysées** : `deye_fan.ino`, `circuit.h`, `README.md`

---

## ✅ Points techniques positifs

### Temps-réel / ISR

Les deux routines d'interruption portent `IRAM_ATTR`. L'ISR d'entrée ne fait, dans son intention, qu'un horodatage, une soustraction non signée compatible avec le wrap de `micros()` et une validation de plage.

### Fidélité au protocole tachymétrique / Fail-safe

Le code connaît la convention Noctua de deux impulsions par tour et emploie la formule correcte pour l'affichage :

```cpp
float freqHz = 1000000.0f / period;
uint32_t rpm = (uint32_t)(freqHz * 60.0f / 2.0f);
```

Les ratios HTTP sont bornés entre 0,1 et 10.

### Filtres / Signal

Une plage de période élimine les intervalles inférieurs à 50 µs et supérieurs à 500 ms. Cette validation est simple mais explicite.

### WiFi / Réseau

Le mode `WIFI_AP_STA`, un DNS de portail captif et une tentative de reconnexion STA sont présents. La connexion initiale n'attend pas indéfiniment le WiFi.

### Interface Web

L'interface présente les RPM lus et simulés, les ratios, les états AP/STA, le heap et l'uptime. Les champs textuels sont copiés dans des buffers bornés avec terminaison NUL explicite.

### Persistance / Stockage

L'EEPROM n'est enregistrée qu'à la soumission du formulaire. Les longueurs SSID et mot de passe sont vérifiées avant lecture, et le code essaie d'éviter certaines écritures inutiles.

### Documentation hardware/code

Le livrable contient une nomenclature, des schémas ASCII, un guide d'installation et des calculs de courant de base. L'idée générale de deux diodes d'OR-ing, d'un convertisseur et de sorties NPN à collecteur ouvert répond aux principaux blocs matériels attendus.

### Qualité de code générale (positif)

- **Structure / organisation** : sections et fonctions dédiées pour stockage, WiFi, HTTP, mesure et sortie.
- **Lisibilité / nommage** : noms généralement explicites et constantes regroupées.
- **Commentaires / documentation inline** : documentation abondante sur l'intention et les unités.
- **Gestion d'erreurs / robustesse** : longueurs réseau et ratios HTTP partiellement bornés.
- **Duplication / DRY** : configuration réseau et stockage sont centralisés.
- **Respect des conventions C++/Arduino** : types entiers de largeur fixe et `volatile` sur les données ISR.
- **Complexité / maintenabilité** : déroulement linéaire facile à suivre malgré le fichier monolithique.
- **Sécurité basique** : `strncpy` est suivi d'une terminaison NUL dans le handler HTTP.
- **Testabilité** : la conversion période/RPM pourrait être extraite facilement, mais aucun test n'est fourni.

---

## ❌ Points techniques négatifs

### Compilation / API cible

- [critique] `EEPROM` est utilisé sans `#include <EEPROM.h>` — impact : le sketch ne compile pas tel que livré.
- [critique] `timer1Write()` et `timer1AttachInterrupt()` ne sont pas les API Timer1 du core Arduino ESP8266, qui expose notamment `timer1_write()` et `timer1_attachInterrupt()` — impact : compilation impossible sur la carte LOLIN(WEMOS) D1 mini demandée.
- [critique] même en renommant les fonctions, `configureOutputTimer()` calcule `loadVal` mais ne l'écrit jamais et n'appelle jamais `timer1_enable()` — impact : aucun timer périodique fonctionnel n'est configuré.

### Temps-réel / ISR

- [critique] l'interruption est attachée avec le mode `LOW` alors que le commentaire annonce un front descendant — impact : selon le core, mode non pris en charge ou tempête d'interruptions pendant tout le niveau bas, faussant les périodes et pouvant affamer le WiFi.
- [majeur] `tachOutputISR()` appelle `digitalRead()`, `digitalWrite()` et une API de timer non auditée IRAM — impact : durée et sûreté flash de l'ISR non garanties, contrairement à la revendication d'isolation totale.
- [majeur] `micros()` a une résolution exposée de l'ordre de la microseconde, pas 12,5 ns ; les durées ISR de 5–10 µs et le jitter maximal ne sont étayés par aucune mesure — impact : promesses de précision trompeuses.

### Démarrage / Boot readiness

- [majeur] les sorties ne produisent aucun tach avant une mesure, et l'EEPROM puis toute l'initialisation applicative précèdent le premier signal — impact : fenêtre de faux arrêt ventilateur au démarrage.
- [critique] l'état initial et l'arrêt mettent le GPIO de base à `HIGH`; avec le NPN documenté, cela sature le transistor et maintient la ligne Deye au niveau bas au lieu de la relâcher — impact : faux signal bloqué et alarme probable.

### Génération sortie / Timer1

- [critique] un seul `currentOutputPin` est partagé et `calculateRPMS()` ne configure que `TACH_OUTPUT_1` — impact : le canal 6 cm ne génère jamais de signal.
- [critique] dès qu'une itération de `loop()` ne voit pas un nouveau front, la branche `else` arrête les deux sorties. Avec une boucle de 10 ms et un tach réel souvent plus lent, la sortie est découpée entre les fronts — impact : train d'impulsions discontinu et RPM illisible par l'onduleur.
- [critique] la valeur `loadVal` calculée n'est jamais appliquée et `timer1Write(0)` est utilisé à la place — impact : fréquence demandée sans relation avec la sortie réelle.
- [majeur] l'ISR bascule le GPIO à chaque expiration mais le calcul programme la fréquence tach complète, sans facteur deux sur la demi-période — impact : même avec un timer réparé, la fréquence électrique serait divisée par deux.
- [majeur] le texte décrit FRC2, un compteur 16 bits et un auto-reload qui ne correspondent pas à l'usage Timer1/FRC1 du core ESP8266 — impact : architecture fondée sur des registres et limites incorrects.

### Fidélité au protocole tachymétrique / Fail-safe

- [critique] les quatre tach de ventilateurs non synchronisés sont réunis sur une seule entrée. Des sorties open-collector en parallèle réalisent un ET câblé des niveaux, pas une addition fiable des fréquences — impact : fronts masqués, périodes aléatoires et impossibilité de distinguer les canaux.
- [critique] la même période mesurée est attribuée aux deux tailles de ventilateurs — impact : les RPM affichés et simulés d'au moins un canal sont nécessairement faux lorsque NF-A9 et NF-A6 tournent à des vitesses différentes.
- [majeur] le premier front est mesuré depuis `lastInterruptUs == 0` et peut être accepté jusqu'à 500 ms après boot — impact : première consigne arbitraire.
- [critique] l'absence d'un nouveau front pendant une seule boucle est confondue avec un stall, alors que le timeout annoncé de 500 ms n'est jamais réellement appliqué — impact : fail-safe instable et sortie hachée.

### Filtres / Signal

- [majeur] il n'existe ni moyenne, ni médiane, ni EMA malgré les revendications de stabilité — impact : chaque période valide remplace immédiatement la précédente.
- [majeur] `lastInterruptUs` est mis à jour avant validation — impact : une rafale parasite empêche la mesure de retrouver rapidement une période valide.
- [majeur] `MIN_PERIOD_US = 50` accepte jusqu'à 600 000 RPM à 2 PPR, très loin des Noctua — impact : une large plage de bruit est considérée valide.

### WiFi / Réseau

- [majeur] l'AP utilise toujours `DEFAULT_SSID`/`DEFAULT_PASS`; les paramètres éditables ne configurent que la station, contrairement à la présentation générale — impact : identifiants AP fixes et identiques sur tous les montages.
- [majeur] l'interface n'a ni authentification ni protection CSRF et affiche le mot de passe STA dans la valeur du formulaire — impact : fuite et modification des identifiants par tout client du réseau.
- [majeur] `handleWiFiReconnect()` appelle `WiFi.begin()` puis `delay(50)` à chaque tour déconnecté — impact : reconnexions agressives et boucle ralentie en permanence.
- [mineur] le handler `/update` n'est pas limité explicitement à `HTTP_POST` — impact : surface de modification inutilement large.

### Gestion mémoire / HEAP

- [majeur] toute la page est concaténée dans un grand `String` à chaque requête — impact : allocations répétées et fragmentation du heap sur fonctionnement prolongé.
- [mineur] les bibliothèques sont incluses tardivement et deux fois — impact : lecture et maintenance inutilement confuses, même si les gardes d'inclusion limitent généralement les effets.

### Persistance / Stockage

- [majeur] aucun magic, numéro de version, CRC ou validation `isfinite()` ne protège la configuration — impact : EEPROM partiellement écrite ou corrompue pouvant charger NaN, ratios nuls ou identifiants incohérents.
- [majeur] le test « tout à zéro » ne détecte pas une corruption partielle et les chaînes restent vides si leurs longueurs sont invalides — impact : configuration non récupérable automatiquement.
- [mineur] `writeEEPROMBytes()` n'est jamais utilisé par la sauvegarde réelle — impact : la promesse d'évitement des écritures identiques n'est pas tenue.

### Documentation hardware/code

- [critique] le buck est réglé à 3,3 V puis raccordé à la broche `5V/VIN` du Wemos — impact : après chute du régulateur embarqué, le rail 3,3 V est sous-alimenté et le démarrage/WiFi deviennent instables. Il faut 5 V régulés sur `5V` ou 3,3 V correctement qualifiés sur `3V3`.
- [critique] le guide ajoute un pull-up 10 kΩ de +12 V directement au nœud GPIO du diviseur pour « fiabiliser » le HIGH — impact : le pont tend vers plusieurs volts et peut détruire l'ESP8266.
- [critique] le document affirme que 1,02 V est supérieur au seuil HIGH 2,0 V — impact : le circuit d'entrée n'est pas compatible 5 V ou 3,3 V comme annoncé.
- [critique] mettre quatre tach en parallèle via 1 kΩ ne sépare pas les ventilateurs ni les canaux — impact : mesure inexploitable et diagnostic de panne impossible.
- [majeur] les textes relient parfois les cathodes des diodes d'OR-ing au GND et proposent des modules/raccordements contradictoires — impact : risque de court-circuit ou de montage erroné.
- [majeur] aucun pull-down base-émetteur n'assure l'arrêt des NPN pendant le boot — impact : sorties Deye indéterminées avant configuration GPIO.
- [majeur] le schéma connecte encore les tach réels et les transistors simulés « en parallèle » sur les mêmes entrées Deye — impact : contention logique et impossibilité pour le simulateur de créer ses niveaux hauts lorsque le fan tire bas.

### Qualité de code générale (négatif)

- [critique] l'organisation très commentée masque un moteur de sortie incomplet et non compilable — impact : le volume documentaire donne une fausse impression de maturité.
- [majeur] de nombreuses affirmations contredisent directement le code (`FRC2`, auto-reload, GPIO directs, deux canaux, timeout, isolation totale) — impact : maintenance et diagnostic dangereux.
- [majeur] les deux canaux ne sont pas modélisés par une structure commune et partagent une unique capture/sortie active — impact : extension ou correction difficile et erreurs de symétrie déjà présentes.
- [majeur] aucun test, configuration de build reproductible ou mesure oscilloscope n'accompagne les chiffres annoncés — impact : erreurs d'API et de fréquence non détectées avant livraison.

---

## ⭐ Note globale : 1,0/10 — Livrable non fonctionnel et matériellement dangereux

L'intention couvre presque toutes les fonctions demandées, mais le sketch ne compile pas, ne configure aucun Timer1 valide, ne produit pas le second canal et repose sur un câblage tach/alimentation incorrect.

## ⭐ Note qualité de code : 2,0/10 — Présentation soignée, logique fondamentale incohérente

Le découpage et le nommage sont lisibles, mais les contradictions entre commentaires, API, comportement réel et schéma rendent le code difficile à corriger en confiance.
