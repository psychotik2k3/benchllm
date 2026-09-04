# Analyse — composer 2.5 prompt from claude

Sources analysées (liste exhaustive du dossier) :
- `competitors/composer 2.5 prompt from claude/deye_fan.ino`
- `competitors/composer 2.5 prompt from claude/schema_electronique.md`

## ✅ Points techniques positifs

### Temps-réel / ISR

- Les trois routines d’interruption sont explicitement marquées `IRAM_ATTR`, ce qui est indispensable sur ESP8266 lorsque le cache flash est indisponible.
- Les ISR d’entrée restent courtes : horodatage, contrôle de plage, puis stockage de la période. Aucun traitement WiFi, JSON ou stockage n’y est effectué.
- La copie groupée des mesures sous `noInterrupts()` évite de construire un instantané incohérent dans `loop()`.
- La génération reste matériellement déclenchée par Timer1 même pendant les traitements réseau ; ce n’est ni un `Ticker`, ni une temporisation par polling dans `loop()`.

### Démarrage / Boot readiness

- Les sorties sont configurées avant LittleFS et le WiFi, puis forcées à `LOW`, ce qui bloque les NPN et relâche les lignes tachymétriques.
- Aucun signal simulé n’est produit avant qu’une période d’entrée valide ait été reçue : `g_outHalfPeriodUs` démarre à zéro.
- Timer1 et les interruptions d’entrée sont actifs avant le lancement du WiFi et du serveur Web, ce qui réduit la fenêtre durant laquelle le réseau pourrait retarder la mise en service.

### Génération sortie / Timer1

- Timer1/FRC1 est bien le timer matériel de l’ESP8266. Avec `TIM_DIV16`, 250 ticks représentent correctement 50 µs à partir de l’horloge APB de 80 MHz.
- La période générée respecte l’identité tachymétrique à deux impulsions par tour : période entre impulsions `30 000 000 / RPM` en microsecondes et demi-période électrique divisée par deux.
- Les deux canaux possèdent des échéances indépendantes.
- Le ratio est appliqué directement à la période : `targetHalf = période_entrée / (2 × ratio)`, ce qui multiplie bien la fréquence et les RPM affichés.
- Une limite basse de 100 µs protège le timer contre des réglages produisant une cadence déraisonnable.

### Fidélité protocole tachymétrique / Fail-safe

- Le code et la documentation utilisent explicitement deux impulsions par tour ; `periodToRpm()` calcule correctement `30 000 000 / période_us`.
- Une absence de front pendant 1,5 s désactive la sortie au lieu de continuer à simuler un ventilateur en rotation.
- La plage de périodes acceptées rejette les fronts inférieurs à 2,5 ms et supérieurs à 500 ms.
- Au stall, le transistor est remis à l’arrêt (`LOW` côté GPIO), donc la sortie collecteur ouvert est relâchée.

### Filtres / Signal

- Une moyenne glissante sur quatre périodes réduit le bruit de mesure.
- La limitation de variation à 15 % par mise à jour évite des sauts brusques de fréquence simulée.
- Le schéma ajoute un filtre RC léger de 4,7 kΩ/1 nF sur chaque entrée ; sa coupure, très supérieure aux fréquences tach utiles, limite les parasites rapides sans déformer sensiblement le signal.

### WiFi / Réseau

- Le mode `WIFI_AP_STA` maintient un point d’accès local tout en permettant une connexion au réseau existant.
- La connexion STA est non bloquante et bornée à 20 s ; elle ne bloque donc pas la boucle de traitement.
- L’interface repose sur un serveur asynchrone, ce qui limite les longues attentes HTTP dans `loop()`.

### Gestion mémoire / HEAP

- Les documents ArduinoJson sont statiques et de taille bornée.
- La page HTML est placée en `PROGMEM`, ce qui évite de consommer plusieurs kilo-octets de RAM.
- La configuration utilise des tableaux `char` bornés plutôt que des `String` persistantes.
- Les instantanés temps-réel sont des structures et tableaux statiques, sans allocation dynamique dans les ISR.

### Interface Web

- L’interface affiche RPM d’entrée, RPM simulés, activité des canaux et état AP/STA.
- Les ratios sont modifiables séparément, bornés entre 1 et 4, puis sauvegardés.
- Les identifiants STA et AP sont configurables, et le redémarrage après changement WiFi rend le comportement compréhensible.

### Persistance / Stockage

- LittleFS est préférable à l’émulation EEPROM pour une configuration JSON évolutive et évite une structure binaire dépendante du compilateur.
- Une configuration absente ou un JSON illisible déclenche des valeurs par défaut sûres.
- Les ratios chargés sont à nouveau bornés avant utilisation.

### Documentation hardware/code

- Le document couvre le brochage, la BOM, les masses communes et les calculs de composants.
- L’entrée Noctua, réellement à collecteur ouvert, reçoit un pull-up local vers 3,3 V : elle est donc directement compatible avec le GPIO sans supposer un niveau logique 5 V ou 12 V.
- La sortie utilise un NPN BC547 en collecteur ouvert. Son Vceo de 45 V couvre des pull-up Deye de 3,3 V, 5 V ou 12 V, contrairement à une sortie GPIO directe.
- L’alimentation issue des deux connecteurs est correctement réunie par deux diodes Schottky : aucun backfeed d’un connecteur vers l’autre. Un buck 12 V → 5 V alimente la broche 5 V du Wemos, au lieu de dissiper la chute dans son régulateur.
- Le dimensionnement des condensateurs prend en compte les pointes de courant WiFi.

### Qualité de code générale (positif)

- **Structure :** séparation nette entre capture ISR, génération Timer1, filtrage, configuration, réseau, Web et `setup()/loop()`.
- **Lisibilité :** noms descriptifs, constantes centralisées, unités indiquées et commentaires en français cohérents avec le code.
- **Commentaires :** les formules, niveaux électriques et choix du timer sont expliqués, sans cacher l’architecture temps-réel.
- **Robustesse :** timeouts, bornes de ratio, valeurs par défaut et contrôle d’ouverture des fichiers couvrent plusieurs pannes usuelles.
- **DRY :** tableaux par canal et fonctions communes (`smoothPeriod`, `slewLimit`, `periodToRpm`) évitent l’essentiel de la duplication.
- **Conventions C++/Arduino :** types entiers de largeur fixe, `F()` pour les messages série, `PROGMEM`, `IRAM_ATTR` et APIs ESP8266 appropriées.
- **Maintenabilité :** constantes et brochage sont regroupés ; les responsabilités sont suffisamment isolées pour modifier filtre, ratio ou interface.
- **Sécurité basique :** le mot de passe AP est non vide par défaut et les ratios HTTP sont bornés.
- **Testabilité :** les conversions et filtres sont des fonctions pures ou presque, donc testables séparément sur hôte après une légère extraction.

## ❌ Points techniques négatifs

### Exactitude de compilation probable

- [majeur] Le projet ne fournit ni `platformio.ini`, ni versions verrouillées de `ESPAsyncWebServer`, `ESPAsyncTCP` et ArduinoJson. L’ancien couple ESPAsyncWebServer/ESPAsyncTCP et ses forks récents n’ont pas toujours les mêmes contraintes d’installation ; impact : compilation non reproductible et risque d’échec dès l’inclusion ou l’édition de liens.
- [mineur] Le code dépend implicitement des déclarations Timer1 exposées par la version du core ESP8266 sans inclure explicitement `core_esp8266_timer.h` ; impact : portabilité réduite entre versions du core, même si les versions Arduino ESP8266 courantes l’exposent généralement via `Arduino.h`.

### Temps-réel / ISR

- [majeur] L’ISR Timer1 appelle `micros()` et `digitalWrite()`. Même marquée `IRAM_ATTR`, elle ne garantit pas que tout le chemin appelé soit en IRAM pour toutes les versions du core ; impact : risque de crash si une routine appelée réside en flash pendant une désactivation du cache, et latence supérieure à des écritures `GPIO_REG_WRITE`.
- [majeur] Timer1 interrompt le CPU toutes les 50 µs, soit 20 000 ISR/s, y compris lorsqu’aucun canal n’est actif ; impact : charge CPU permanente, contention avec la pile WiFi et consommation accrue.
- [mineur] Le commentaire « génération isolée du WiFi » est trop absolu : une ISR Timer1 normale n’est pas une NMI et peut être retardée par des sections critiques ; impact : jitter ponctuel sous charge radio.

### Génération sortie / Timer1

- [majeur] Les sorties sont pilotées par `digitalWrite` plutôt que par les registres atomiques GPIO `W1TS/W1TC` ; impact : ISR plus longue et jitter intercanal, les deux sorties n’étant pas basculées au même instant lorsqu’elles expirent ensemble.
- [majeur] Chaque échéance est recalée avec `g_nextToggleUs[ch] = now + half` au lieu de progresser depuis l’échéance théorique précédente ; impact : toute latence d’ISR devient une dérive de phase permanente et la fréquence moyenne est légèrement trop basse.
- [majeur] La cadence fixe de 50 µs arrondit chaque demi-période au tick suivant. À 400 Hz, l’erreur peut approcher 4 % par demi-période dans le pire alignement ; impact : RPM simulés différents de la consigne et jitter quantifié.
- [mineur] Si une échéance a été manquée de plusieurs demi-périodes, une seule bascule est réalisée et aucun rattrapage de phase n’a lieu ; impact : impulsions perdues lors d’un blocage prolongé.

### Démarrage / Boot readiness

- [majeur] LittleFS est monté, éventuellement formaté, puis lu avant l’activation des interruptions et du timer. Le formatage peut prolonger le démarrage ; impact : l’onduleur peut observer plusieurs secondes sans tach et déclencher une alarme avant la première mesure.
- [majeur] Aucune stratégie de « boot readiness » n’est documentée ou mesurée : absence de délai d’alarme Deye, de signal temporaire contrôlé, ou de ligne ready ; impact : comportement incertain lors d’un cold boot alimenté par les connecteurs ventilateur.

### Fidélité protocole tachymétrique / Fail-safe

- [critique] L’ISR met `g_lastEdgeUs[ch]` à jour avant de valider la période. Une rafale de parasites trop rapprochés maintient donc le canal non expiré tandis que `g_rawPeriodUs` conserve une ancienne valeur valide ; impact : le système peut continuer à simuler un ventilateur en marche alors que le tach réel est perdu.

```76:84:competitors/composer 2.5 prompt from claude/deye_fan.ino
  const uint32_t now = micros();
  const uint32_t last = g_lastEdgeUs[ch];
  g_lastEdgeUs[ch] = now;

  if (last == 0) {
    return;
  }

  uint32_t period = now - last;
```

- [majeur] Une impulsion isolée invalide peut repousser le timeout de 1,5 s ; impact : détection de stall retardée et fail-safe moins honnête.
- [mineur] Le timeout fixe de 1,5 s n’est pas adapté à la période mesurée ; impact : arrêt détecté lentement à haut régime, tout en interdisant implicitement les régimes sous environ 60 RPM.

### Filtres / Signal

- [majeur] Le slew limiter est exécuté à chaque tour de `loop()` sans cadence fixe. Sa constante temporelle dépend donc fortement de la charge CPU et réseau ; impact : réponse du signal non déterministe et accélération artificiellement lente ou rapide.
- [majeur] La moyenne réinsère continuellement la même valeur `g_rawPeriodUs` à chaque passage de boucle, même sans nouveau front ; impact : les quatre échantillons ne représentent pas quatre périodes distinctes et le filtre converge presque instantanément vers la dernière mesure.
- [mineur] Une moyenne simple est sensible à un outlier valide ; impact : excursion temporaire des RPM simulés malgré la limitation de pente.

### WiFi / Réseau

- [critique] Aucune authentification HTTP ni protection CSRF n’est présente. Tout client AP ou STA peut modifier ratios et identifiants puis redémarrer l’ESP ; impact : sabotage du signal tach, déni de service et prise de contrôle de la configuration.
- [majeur] Les identifiants AP par défaut sont publics et constants ; impact : accès immédiat à l’interface si l’utilisateur ne les change pas.
- [majeur] Longueur et règles WPA du SSID/mot de passe AP ne sont pas validées ; impact : `WiFi.softAP()` peut échouer au redémarrage et supprimer le canal de récupération local.
- [mineur] Après l’échec STA à 20 s, aucune reconnexion périodique n’est tentée ; impact : perte durable du mode STA après une indisponibilité transitoire.

### Gestion mémoire / HEAP

- [majeur] `buildStatusJson()` construit une `String` dynamique à chaque requête et `softAPIP().toString()` ajoute des allocations ; impact : fragmentation progressive du petit heap ESP8266 sous polling prolongé.
- [majeur] Les quatre `strncpy(..., sizeof(buffer))` de chargement et des routes Web ne garantissent pas un terminateur NUL si la source atteint la taille du tampon ; impact : lectures hors limites dans `strlen`, WiFi ou sérialisation, avec crash ou fuite mémoire potentielle.
- [mineur] Aucun indicateur de heap libre/minimal n’est exposé ; impact : régressions de fragmentation difficiles à diagnostiquer.

### Interface Web

- [majeur] Les erreurs HTTP, d’analyse JSON et de sauvegarde ne sont pas affichées côté navigateur ; impact : l’utilisateur peut croire qu’un réglage critique est appliqué alors que l’écriture a échoué.
- [majeur] La page injecte des valeurs reçues avec `innerHTML`; les valeurs actuelles sont surtout numériques, mais la pratique est fragile ; impact : future extension avec texte non échappé susceptible d’introduire une injection HTML.
- [mineur] Le formulaire WiFi ne recharge pas les paramètres actuels ; impact : changement partiel peu ergonomique et risque d’écraser involontairement des secrets.

### Persistance / Stockage

- [majeur] Le JSON LittleFS n’a ni version, ni CRC, ni écriture atomique par fichier temporaire/rename ; impact : une coupure pendant l’écriture peut corrompre la configuration entière.
- [majeur] En cas d’échec de montage, le firmware formate automatiquement LittleFS sans vérifier le second `begin()` ; impact : perte silencieuse des réglages et poursuite avec un stockage potentiellement inutilisable.
- [mineur] `saveConfig()` est appelé même si aucun paramètre valide n’a été fourni ; impact : écritures flash inutiles et usure accrue.

### Documentation hardware/code

- [majeur] La documentation ne mesure ni ne spécifie le délai maximal du buck + boot ESP8266 + montage LittleFS avant le premier tach valide ; impact : impossibilité de conclure à l’absence d’alarme au démarrage.
- [majeur] Le dimensionnement de la base utilise le gain typique β=200 avant d’appliquer une marge empirique. La saturation devrait être démontrée avec un β forcé ; impact : justification électrique moins rigoureuse, même si 1 kΩ procure en pratique une marge très large.
- [mineur] Le schéma est uniquement ASCII et ne donne ni référence de connecteur/pinout Deye vérifiée, ni fichier CAO ; impact : risque de câblage inversé au montage.
- [mineur] Il n’y a pas de résistance base-émetteur assurant explicitement Q1/Q2 bloqués pendant la phase où les GPIO flottent au boot ; impact : impulsions parasites possibles selon fuites, longueur de câbles et transistor.

### Qualité de code générale (négatif)

- **Structure :** `updateRealtimeTargets()` mélange état métier, filtrage, programmation temps-réel et LED ; [mineur], impact : tests et évolutions plus difficiles.
- **Lisibilité :** la polarité `g_outLevel` désigne le niveau GPIO et non le niveau tach au collecteur ; [mineur], impact : risque d’erreur lors d’une maintenance.
- **Commentaires :** plusieurs affirmations (« isolée du WiFi », moyenne de quatre périodes) sont plus fortes que l’implémentation réelle ; [majeur], impact : faux sentiment de garantie temps-réel.
- **Robustesse :** le défaut de timestamp sur front invalide et les chaînes potentiellement non terminées compromettent le fail-safe ; [critique], impact : simulation mensongère ou crash.
- **DRY :** les snapshots des deux canaux et plusieurs ternaires de broches sont écrits manuellement ; [mineur], impact : extension à plus de canaux propice aux oublis.
- **Conventions C++/Arduino :** emploi de flottants dans la boucle de commande et de `digitalWrite` dans l’ISR au lieu de calcul fixe/registres ; [majeur], impact : déterminisme inférieur.
- **Maintenabilité :** dépendances et versions ne sont pas déclarées ; [majeur], impact : build futur non reproductible.
- **Sécurité basique :** absence d’authentification, validation incomplète et secret AP par défaut ; [critique], impact : modification distante de la fonction de sûreté.
- **Testabilité :** aucun test, simulateur de fronts, mesure de jitter ou procédure de validation automatisée ; [majeur], impact : les propriétés temps-réel annoncées ne sont pas démontrées.

## ⭐ Note globale : 6,8/10 — Bonne architecture matérielle et fonctionnelle, mais le fail-safe sur fronts invalides et le jitter du scheduler doivent être corrigés avant usage fiable.

Le projet couvre bien Timer1, AP+STA, deux impulsions/tour, collecteur ouvert et alimentation anti-backfeed, mais une entrée parasitée peut maintenir à tort une sortie tach active.

## ⭐ Note qualité de code : 6,5/10 — Code clair et bien découpé, pénalisé par des garanties temps-réel surestimées, des chaînes non sûres et l’absence de sécurité Web.

La base est maintenable, mais elle exige des corrections de concurrence, de validation, de persistance atomique et des tests reproductibles.
