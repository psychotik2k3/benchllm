# Analyse — qwen3.6_prompt_from_claude

Sources analysées (liste exacte) :
- `competitors/qwen3.6_prompt_from_claude/deye_fan.ino`
- `competitors/qwen3.6_prompt_from_claude/SCHEMATIC.md`

## ✅ Points techniques positifs

### Temps-réel / ISR
- Les ISR d'entrée et de sortie portent `ICACHE_RAM_ATTR`/`IRAM_ATTR`, n'effectuent ni log ni allocation dynamique, et utilisent des écritures GPIO directes `GPOS/GPOC`.
- La capture par front descendant et la soustraction non signée de `micros()` sont adaptées au tach Noctua.

### Démarrage / Boot readiness
- Les GPIO de capture 12/13 évitent les broches de strap 0/2/15 et GPIO16, qui ne convient pas à `attachInterrupt`.
- La connexion STA est conçue comme non bloquante, ce qui limite l'attente réseau avant le fonctionnement nominal.

### Génération sortie / Timer1
- Deux échéanciers distincts permettent en principe des fréquences indépendantes.
- Les bascules par registres réduisent fortement la durée du chemin critique par rapport à `digitalWrite()`.

### Fidélité protocole tachymétrique / Fail-safe
- Le code utilise correctement **2 impulsions/tour** et `RPM = 30 000 000 / période_us`.
- La période cible suit la bonne relation `période_sortie = période_entrée / ratio`.
- Un timeout de 2 s coupe la consigne de chaque canal.
- Le NPN collecteur ouvert proposé en sortie est le bon principe pour accepter un pull-up Deye à 3,3/5/12 V.

### Filtres / Signal
- Plage de périodes, anti-rebond de 500 µs, moyenne glissante de quatre périodes et lissage exponentiel sont prévus.
- Un RC 4,7 kΩ/1 nF donne une coupure très supérieure aux 100 Hz attendus tout en filtrant les parasites HF.

### WiFi / Réseau
- L'intention AP+STA, le serveur asynchrone et l'OTA sont fonctionnellement riches.
- L'AP par défaut et l'OTA disposent d'un mot de passe.

### Gestion mémoire / HEAP
- La page statique est placée en `PROGMEM`.
- Les réponses JSON utilisent des buffers bornés sur la pile plutôt que de longues concaténations de `String`.
- La configuration emploie des tableaux `char` de taille fixe.

### Interface Web
- L'interface est responsive, actualise les mesures sans recharger la page et sépare configuration et lecture.
- Les mots de passe STA sont masqués dans la réponse de configuration.

### Persistance / Stockage
- LittleFS/JSON est plus évolutif qu'un adressage EEPROM brut.
- Les défauts sont restaurés si le fichier manque ou si le JSON est invalide; les écritures sont différées hors ISR.

### Documentation hardware/code
- Le schéma couvre alimentation issue des deux connecteurs, anti-backfeed, conversion 12→5 V, découplage, entrées tach et sorties collecteur ouvert.
- La BOM, les calculs et une procédure de test à l'oscilloscope sont fournis.

### Qualité de code générale (positif)
- **Structure :** sections et responsabilités bien séparées.
- **Lisibilité :** noms descriptifs et flux global compréhensible.
- **Commentaires :** les contraintes ISR, les formules et le matériel sont largement explicités.
- **Robustesse :** défauts de configuration, timeout, bornes de fréquence et redémarrage différé sont envisagés.
- **DRY :** plusieurs traitements sont factorisés par boucle sur les deux canaux.
- **Conventions C++/Arduino :** tailles fixes, `static`, `const` et `PROGMEM` sont employés.
- **Maintenabilité :** configuration structurée et API séparée.
- **Sécurité basique :** AP et OTA protégés par mot de passe, mais HTTP reste vulnérable.
- **Testabilité :** API de lectures et étapes de test matériel facilitent la validation.

## ❌ Points techniques négatifs

### Exactitude de compilation probable
- [critique] Plusieurs conversions retirent illégalement `volatile`, par exemple `const uint32_t* avgBuf = avgBufA` et `uint32_t* target = &targetPeriodA_us`. **Impact : erreurs de compilation C++ (`invalid conversion from volatile ...`).**
- [critique] `restartTime` est déclaré `static` dans le bloc `if`, puis référencé dans le bloc `else`, où il est hors portée. **Impact : erreur de compilation certaine.**
- [critique] `request->getString()` et le handler `handleConfigPost(AsyncWebServerRequest*, int)` ne correspondent pas à l'API usuelle d'ESPAsyncWebServer; un POST body requiert un callback de body distinct. **Impact : compilation impossible ou corps jamais reçu.**
- [critique] `LittleFS.begin(true)` correspond à une signature ESP32, pas au LittleFS ESP8266 standard (`begin()`). **Impact : échec de compilation selon le core annoncé.**
- [majeur] Le cast d'`IPAddress` en `const char*` et plusieurs surcharges de `request->send(..., jsonBuf, len)` ne sont pas portables entre versions d'ESPAsyncWebServer. **Impact : build dépendant d'une version non spécifiée et probablement cassé.**
- [majeur] Le code se dit ArduinoJson v7 tout en utilisant `DynamicJsonDocument`, API v6 dépréciée/remaniée. **Impact : avertissements ou incompatibilité selon version réellement installée.**

### Temps-réel / ISR
- [critique] `Ticker` sur ESP8266 est un timer logiciel/os_timer, pas un Timer1 matériel dédié; un callback à 20 kHz est irréaliste et peut affamer le WiFi/watchdog. **Impact : jitter, resets et pertes réseau.**
- [critique] L'objet `Ticker scheduler` est local à `setup()` et est détruit au retour. **Impact : l'ordonnanceur est détaché; aucune sortie tach durable.**

```940:945:competitors/qwen3.6_prompt_from_claude/deye_fan.ino
// Attache la fonction schedulerTick au Timer1 @20 kHz via Ticker
Ticker scheduler;
scheduler.attach(
  1.0f / (float)SCHEDULER_HZ, schedulerTick);
```

- [majeur] `micros()` dans un callback à 20 kHz et le partage de structures multichamps sans snapshot/section critique introduisent incohérences et charge CPU. **Impact : période instable.**
- [majeur] Les buffers et index sont lus pendant que l'ISR les modifie. **Impact : moyenne pouvant combiner index et échantillons de générations différentes.**

### Démarrage / Boot readiness
- [critique] Le code initialise les sorties à HIGH en affirmant que le NPN est bloqué; c'est l'inverse avec une base reliée par résistance. **Impact : ligne Deye forcée LOW dès l'initialisation, avec risque d'alarme.**
- [majeur] LittleFS peut monter, formater et écrire avant que les sorties et interruptions soient configurées. **Impact : fenêtre de boot sans tach pouvant durer assez pour le diagnostic Deye.**

### Génération sortie / Timer1
- [critique] Ce n'est pas un Timer1 matériel, contrairement aux commentaires et au schéma. **Impact : absence d'isolation temporelle réelle vis-à-vis du WiFi/flash.**
- [majeur] Le tick de 50 µs quantifie chaque demi-période; à 2,5 ms, l'erreur de phase peut atteindre 2 %. **Impact : jitter de fréquence mesurable.**
- [majeur] La prochaine échéance est fixée à `now + demi_période`, et non à `ancienne_échéance + demi_période`. **Impact : retard de callback converti en dérive permanente.**
- [majeur] La comparaison `now >= nextChangeUs` n'est pas sûre au débordement de `micros()`. **Impact : interruption de génération autour de 71,6 minutes.**
- [majeur] Le « second lissage » est nul : `prevTarget` est lu après l'affectation de `*target`, puis la valeur est lissée avec elle-même. **Impact : comportement différent de celui documenté.**

### Fidélité protocole tachymétrique / Fail-safe
- [critique] En cas de stall, `targetPeriod=0`, mais aucun code ne force la sortie dans l'état transistor bloqué. Elle reste figée HIGH ou LOW au hasard. **Impact : consommation permanente du NPN ou niveau Deye bas indéfini.**
- [critique] La polarité de sortie est inversée dans les commentaires et l'état fail-safe devrait être GPIO LOW pour bloquer un NPN base-résistance. **Impact : fail-safe matériel incorrect.**
- [majeur] `lastPulse_ms` est mis à jour même pour une période hors plage; du bruit régulier peut empêcher le stall tout en conservant une ancienne cible. **Impact : ancienne vitesse simulée maintenue sur signal invalide.**
- [mineur] `OUTPUT_PERIOD_MIN_US=2500` correspond à 400 impulsions/s, soit 12 000 RPM à 2 impulsions/tour, pas aux 600 Hz commentés. **Impact : spécification et limite incohérentes.**

### Filtres / Signal
- [majeur] La moyenne des périodes n'est pas robuste aux outliers; médiane ou rejet relatif seraient plus adaptés. **Impact : une période aberrante autorisée perturbe quatre calculs.**
- [majeur] Le pull-up interne est activé en plus du pull-up externe 4,7 kΩ. **Impact : valeur effective inutilement modifiée, même si elle reste électriquement acceptable.**

### WiFi / Réseau
- [critique] Les routes de lecture et d'écriture HTTP n'ont aucune authentification ni protection CSRF. **Impact : tout client AP/STA peut reconfigurer ratios et WiFi ou déclencher un reboot.**
- [majeur] Mot de passe AP et OTA identiques, fixes et publiés dans le source. **Impact : accès trivial sans personnalisation.**
- [majeur] `tryStaConnect()` coupe l'AP; après connexion STA, `loop()` met seulement `apStarted=true` sans redémarrer l'AP. **Impact : le fonctionnement réel n'est pas AP+STA simultané.**
- [majeur] `Serial.println("[WIFI] STA connected")` s'exécute à chaque boucle. **Impact : saturation série, charge CPU et jitter.**
- [majeur] Les mots de passe WiFi sont stockés en clair dans LittleFS. **Impact : extraction facile depuis la flash.**

### Gestion mémoire / HEAP
- [majeur] `DynamicJsonDocument` alloue toujours sur le heap, malgré les commentaires « zéro allocation ». **Impact : fragmentation possible sous requêtes répétées.**
- [majeur] Un serveur asynchrone peut exécuter plusieurs handlers simultanément, chacun avec document JSON et buffers. **Impact : pression mémoire et concurrence sur `cfg`.**
- [mineur] Aucun suivi du heap minimal ou rejet de requêtes concurrentes n'est prévu. **Impact : diagnostic difficile en production.**

### Interface Web
- [critique] `/api/config` renvoie `ap_pass` en clair alors qu'il masque seulement `wifi_pass`. **Impact : secret AP exposé à tout client HTTP.**
- [critique] Les ratios POST ne sont ni testés avec `isfinite()` ni bornés. **Impact : zéro, négatif, NaN ou valeur énorme peut arrêter ou saturer la sortie.**
- [majeur] Le navigateur renvoie les placeholders `••••••••` comme mot de passe STA si l'utilisateur ne le ressaisit pas. **Impact : remplacement involontaire du vrai secret.**
- [majeur] Les erreurs de `fetch` sont presque toutes avalées. **Impact : l'utilisateur croit une configuration appliquée alors qu'elle a échoué.**

### Persistance / Stockage
- [majeur] Aucun CRC, schéma/version, fichier temporaire ni renommage atomique. **Impact : coupure pendant l'écriture pouvant corrompre `config.json`.**
- [majeur] `saveConfig()` renvoie `true` même si `serializeJson` échoue. **Impact : succès mensonger et perte de configuration.**
- [majeur] Les valeurs JSON restaurées ne sont pas validées (ratios, longueur/minimum WPA2, types/null). **Impact : configuration invalide persistante.**

### Documentation hardware/code
- [critique] Le MT3608 est un convertisseur **boost** (élévateur), pas un buck 12→5 V. **Impact : la conversion annoncée ne fonctionne pas; risque d'alimenter le Wemos à une tension dangereuse.**
- [majeur] Les dessins de diodes OR-ing sont ambigus/inversés et emploient BAT54 sans vérification de courant de pointe. **Impact : backfeed ou chute/surchauffe possibles.**
- [majeur] Le schéma affirme à tort qu'alimenter la broche 5 V « bypass complètement l'AMS1117 », puis dit que le régulateur 5→3,3 V travaille : contradiction. **Impact : décision de conception fondée sur une explication erronée.**
- [majeur] Les capacités sont parfois notées `220µT`, les broches `5W/12W`, et le dessin LED met une LED externe en parallèle de manière incohérente avec la LED intégrée active-low. **Impact : documentation difficile à câbler sans interprétation.**
- [mineur] Le 220 µF tantale à 6,3 V a peu de marge sur un rail 5 V et supporte mal les surtensions. **Impact : fiabilité réduite.**

### Qualité de code générale (négatif)
- [critique] **Compilation/robustesse :** plusieurs erreurs certaines et dépendances/API non figées empêchent un build probable.
- [majeur] **Structure :** bonne apparence, mais trop d'état global partagé entre ISR, boucle et callbacks asynchrones.
- [majeur] **Lisibilité/commentaires :** des revendications fausses (« Timer1 matériel », « zéro heap », polarité NPN) diminuent la confiance.
- [majeur] **DRY :** factorisation partielle réussie, mais les ISR restent dupliquées.
- [majeur] **Conventions C++/Arduino :** retraits de `volatile`, portée incorrecte et callbacks API non conformes.
- [majeur] **Maintenabilité :** aucune version de bibliothèques ni configuration PlatformIO/Arduino CLI.
- [critique] **Sécurité basique :** HTTP non authentifié, secrets fixes/en clair et AP password exposé.
- [majeur] **Testabilité :** aucune mesure automatique de jitter, aucun test de débordement, stall, boot ou corruption.

## ⭐ Note globale : 3,0/10 — Conception ambitieuse et mieux ciblée, mais non compilable et non sûre en temps-réel comme en alimentation.

La bonne formule tach, les filtres et l'usage des registres ne compensent pas le faux Timer1, le Ticker détruit, le fail-safe inversé et le faux buck MT3608.

## ⭐ Note qualité de code : 4,0/10 — Organisation solide en surface, exactitude d'API et assertions techniques insuffisantes.

Le code est lisible et documenté, mais ses erreurs de portée, de types, de sécurité et de concurrence empêchent une maintenance fiable.
