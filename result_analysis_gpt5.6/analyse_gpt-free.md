# Analyse — gpt-free

Sources analysées (liste exhaustive du dossier) :

- `competitors/gpt-free/gpt_deye_fan.ino`

## ✅ Points techniques positifs

### Temps-réel / ISR

- Les deux ISR de capture portent `IRAM_ATTR` et se limitent au compteur CPU, à des entiers et à des écritures d’état `volatile`. Les calculs flottants, l’EEPROM, le WiFi et les `String` restent hors ISR.
- La mesure par `CCOUNT` offre une résolution d’un cycle CPU et la soustraction non signée gère correctement le wrap du compteur.
- Les données ISR sont copiées sous `noInterrupts()` avant traitement, évitant de lire un couple drapeau/période incohérent.

### Démarrage / Boot readiness

- Les GPIO de sortie sont configurés LOW avant le réseau ; avec un NPN externe correctement muni d’un pull-down, la ligne Deye devrait être relâchée au démarrage.
- Les interruptions d’entrée sont installées avant le WiFi, ce qui permet de capturer les pulses pendant l’association.

### Génération sortie / Timer1

- Le sketch ne génère pas les fronts dans `loop()`. Il appelle le générateur `core_esp8266_waveform`, lequel s’appuie sur Timer1/NMI du core ESP8266 ; ce n’est ni `Ticker`, ni du polling, ni une temporisation bloquante.
- Le générateur continue pendant le traitement HTTP/WiFi. La mise à jour de fréquence n’a lieu qu’à réception d’une nouvelle mesure ou d’un changement de ratio.
- La période complète puis les deux demi-périodes 50 % sont calculées explicitement à partir du RPM.

### Fidélité protocole tachymétrique / Fail-safe

- `TACH_PULSES_PER_REV = 2` est centralisé et utilisé dans les deux conversions. La formule est correcte : `RPM = fréquence × 60 / 2`.
- Un délai de 500 ms sans nouvelle période traitée arrête `startWaveform` via `stopWaveform()` et met la base NPN LOW.
- Un seuil minimum de période rejette les fronts trop rapprochés ; le premier front ne produit pas de période.

### Filtres / Signal

- Un IIR entier `7/8 + 1/8` lisse les fluctuations de période sans flottant dans l’ISR :

```cpp
uint64_t filtered =
  ((uint64_t)channel.filteredPeriodCycles * 7ULL +
   (uint64_t)period)
  / 8ULL;
channel.filteredPeriodCycles = (uint32_t)filtered;
```

- Le calcul 64 bits prévient le débordement pendant la somme pondérée.

### WiFi / Réseau

- Le mode AP+STA simultané est explicitement activé ; en l’absence de SSID station, l’AP reste utilisable.
- L’AP par défaut utilise un mot de passe WPA de longueur valide et le serveur expose des routes séparées pour statut, lecture et écriture de configuration.

### Gestion mémoire / HEAP

- Les grandes chaînes HTML sont ajoutées via `F()` et la capacité des `String` HTML/JSON est réservée.
- Les structures temps-réel et de configuration ont des tailles fixes ; l’ISR n’alloue pas.

### Interface Web

- L’interface est responsive, affiche RPM réel/simulé et état des deux canaux, puis actualise les données toutes les 500 ms.
- Les deux ratios et les paramètres AP/STA sont modifiables sans recompilation.
- Les limites HTML des ratios correspondent aux constantes 0,10–10,00 et le serveur applique également `constrain()`.

### Persistance / Stockage

- Les ratios et identifiants réseau sont conservés en EEPROM émulée.
- Un `magic` permet au moins de reconnaître une EEPROM non initialisée et de restaurer une configuration AP exploitable.

### Documentation hardware/code

- Le commentaire d’en-tête documente le brochage, le caractère 2-PPR et la nécessité d’une sortie NPN à collecteur ouvert.
- Le code explique précisément la formule RPM, le rôle du générateur du core et le fait que `loop()` ne fabrique pas les fronts.

### Qualité de code générale (positif)

- **Structure :** sections thématiques claires et fonctions distinctes pour capture, conversion, canal, WiFi, web et EEPROM.
- **Lisibilité :** noms explicites, mise en forme très aérée et constantes regroupées.
- **Commentaires :** architecture, formules et sections critiques sont abondamment expliquées.
- **Robustesse :** arrêt sur timeout, bornage des ratios et section critique autour de l’échange ISR/loop.
- **DRY :** `processChannel()` et `setTachOutputRPM()` sont partagés entre canaux.
- **Conventions C++/Arduino :** `constexpr`, références, types fixes et `IRAM_ATTR` sont correctement employés.
- **Maintenabilité :** les paramètres fonctionnels sont centralisés et les routes web sont séparées.
- **Sécurité basique :** AP protégé par défaut et mot de passe inférieur à huit caractères remplacé par une valeur valide.
- **Testabilité :** les conversions et le traitement par canal sont isolés, donc extractibles pour des tests unitaires, même si aucun test n’est fourni.

## ❌ Points techniques négatifs

### Temps-réel / ISR

- **[majeur]** La compilation probable dépend d’un header interne, `core_esp8266_waveform.h`, et de la signature non stable `startWaveformClockCycles(...)`. Aucune version précise du core ESP8266 n’est fixée ; une mise à jour peut casser la compilation ou modifier le timing.
- **[majeur]** Les seuils sont commentés pour 160 MHz (`40000 cycles = 250 µs`, `320 millions = 2 s`) alors que la carte peut être compilée à 80 MHz. À 80 MHz ils valent 500 µs et 4 s : le comportement anti-glitch dépend donc silencieusement du réglage CPU.
- **[mineur]** `noInterrupts()` puis `interrupts()` ne restaure pas l’état antérieur des interruptions, ce qui limite la réutilisabilité sûre de `processChannel()`.
- **[mineur]** La garantie d’IRAM ne couvre que les ISR écrites ici ; le fonctionnement interne du générateur waveform dépend du core et n’est ni inspecté ni verrouillé.

### Démarrage / Boot readiness

- **[critique]** Le signal de sortie ne démarre qu’après deux fronts d’entrée valides, le passage de `processChannel()` et l’appel au générateur. L’installation des interruptions avant WiFi ne fournit donc aucun tach anticipé ; le Deye peut déclencher son alarme avant la première mesure si sa fenêtre de boot est courte.
- **[majeur]** Le sketch contient `delay(100)` avant d’armer les interruptions. Cette attente agrandit encore la fenêtre sans capture ni sortie.
- **[majeur]** Aucun schéma d’alimentation depuis les deux connecteurs n’est fourni. Il manque l’OR-ing anti-backfeed et le convertisseur 12→5 V ; alimenter naïvement le Wemos depuis un connecteur ou réunir les rails peut endommager l’équipement.

### Génération sortie / Timer1

- **[majeur]** Le code délègue entièrement jitter, priorité, accès GPIO et interaction WiFi à une API privée du core. Il n’utilise pas lui-même les registres GPIO, et aucune mesure ne démontre le jitter ou la dérive sous charge.
- **[majeur]** Le code appelle `startWaveformClockCycles()` à chaque nouvelle période. Selon l’implémentation du core, ces réarmements fréquents peuvent introduire des sauts de phase ou des pulses tronqués ; aucune synchronisation à un front ni test oscilloscope n’est prévu.
- **[majeur]** Aucune limite réaliste ne protège `halfCycles` contre un dépassement de la plage supportée par l’API. Seul un minimum de deux cycles est imposé ; une très faible vitesse ou valeur corrompue peut être tronquée/refusée par le core.
- **[mineur]** `double` est coûteux sur ESP8266 et n’améliore généralement pas la précision par rapport à `float` sur cette toolchain. L’impact est hors ISR mais ajoute du temps de calcul.
- **[mineur]** Le RPM affiché n’est pas dérivé du signal effectivement accepté par le générateur ; saturation ou quantification interne ne sont pas visibles.

### Fidélité protocole tachymétrique / Fail-safe

- **[majeur]** Le timeout est basé sur `lastValidEdgeUs` mis à jour lorsque `loop()` consomme une mesure, pas sur le timestamp réel du front ISR. Sous blocage prolongé du `loop()`, la fraîcheur peut être artificiellement décalée et le signal continuer plus de 500 ms après le dernier pulse physique.
- **[majeur]** `MAX_PERIOD_CYCLES` varie avec la fréquence CPU supposée et n’est pas relié au timeout de 500 ms ; les constantes expriment des unités incohérentes.
- **[majeur]** Un seul intervalle valide active immédiatement la simulation, sans validation de plusieurs périodes ni hystérésis. Un parasite peut créer un tach simulé pendant 500 ms.
- **[mineur]** Après timeout, `filteredPeriodCycles` n’est pas remis à zéro. Lors d’un redémarrage du ventilateur à une vitesse différente, l’ancienne moyenne influence plusieurs mesures et produit temporairement une fréquence erronée.
- **[mineur]** Aucun état de faute ne distingue stall, entrée absente et glitchs rejetés.

### Filtres / Signal

- **[majeur]** L’IIR à coefficient 1/8 est lent lors d’un changement brutal : l’ancien régime persiste sur de nombreux pulses. Cela peut retarder l’adaptation du tach simulé à une vraie accélération ou décélération.
- **[majeur]** Il n’existe ni médiane ni rejet relatif d’outlier. Toute période dans la plage très large influence le filtre, même si elle est incohérente avec les mesures voisines.
- **[mineur]** Aucun filtrage matériel correctement spécifié n’accompagne le code ; le diviseur dessiné ne comporte ni condensateur ni trigger de Schmitt.

### WiFi / Réseau

- **[critique]** `/api/config` renvoie en clair les mots de passe STA et AP, puis JavaScript les place dans les champs :

```cpp
json += ",\"staPassword\":\"";
json += String(config.staPassword);
json += "\"";
json += ",\"apPassword\":\"";
json += String(config.apPassword);
json += "\"";
```

  Tout client réseau peut exfiltrer les identifiants.
- **[critique]** Aucune authentification ne protège la lecture ou l’écriture. Un client AP/STA peut changer ratios et WiFi, donc falsifier la vitesse rapportée ou exclure l’administrateur.
- **[majeur]** HTTP est en clair et aucune protection CSRF/origine n’est présente.
- **[majeur]** Il n’y a pas de reconnexion STA explicite ni de redémarrage après modification des identifiants. Les nouveaux paramètres sont persistés mais `WiFi.begin()`/`softAP()` ne sont pas rappelés : l’interface laisse croire à une application immédiate alors qu’un reboot est requis.
- **[majeur]** Les SSID/mots de passe sont concaténés dans du JSON sans échappement. Une quote ou un backslash rend la réponse invalide et peut injecter du contenu dans le contexte JavaScript client.

### Gestion mémoire / HEAP

- **[majeur]** `makeWebPage()` construit environ 9 ko dans le heap à chaque requête. `F()` économise la copie statique initiale, mais le `String` final doit tout de même occuper un gros bloc contigu ; risque de fragmentation ou d’échec sous plusieurs connexions.
- **[majeur]** Les identifiants sont convertis en `String` temporaires à plusieurs endroits et le démarrage WiFi crée encore deux copies dynamiques. Le heap ESP8266 est inutilement sollicité.
- **[mineur]** Aucun `ESP.getFreeHeap()`, minimum de heap ni traitement d’échec n’est fourni.
- **[mineur]** La page gagnerait à être une constante `PROGMEM` envoyée par `send_P()`, ce qui supprimerait sa construction dynamique.

### Interface Web

- **[critique]** Les secrets sont préchargés et visibles par l’API ; un champ de mot de passe masqué visuellement ne constitue pas une protection.
- **[majeur]** Les promesses de `fetch()` n’ont pas de gestion d’erreur ni contrôle de `response.ok`. L’UI affiche toujours « Configuration enregistrée » dès qu’un texte est reçu.
- **[majeur]** `toFloat()` transforme une chaîne non numérique en `0`, puis `constrain()` la force à `RATIO_MIN`. Une saisie invalide est donc acceptée silencieusement au lieu d’un HTTP 400.
- **[mineur]** L’état actif et les valeurs ne sont pas échappés côté client ; les valeurs numériques sont sûres aujourd’hui, mais l’API de configuration ne l’est pas.

### Persistance / Stockage

- **[critique]** La configuration n’a ni CRC, ni version, ni champ de taille. Un seul `magic` ne détecte pas corruption partielle, écriture interrompue ou évolution de structure ; des identifiants non terminés et des ratios NaN peuvent être chargés.
- **[majeur]** `constrain()` n’assainit pas sûrement un NaN : les comparaisons sont fausses et le NaN peut rester, puis produire une conversion de période indéfinie.
- **[majeur]** Les chaînes chargées depuis EEPROM ne sont jamais forcées à se terminer par `'\0'`. `strlen`, `String(char*)` ou WiFi peuvent lire au-delà du buffer si l’enregistrement est corrompu.
- **[mineur]** Chaque sauvegarde réécrit la structure sans comparaison de changement ni mécanisme double-slot ; usure flash et perte totale sur coupure restent possibles.
- **[mineur]** EEPROM est acceptable pour un petit enregistrement, mais l’absence de CRC rend cette implémentation nettement moins sûre qu’EEPROM+CRC ou un fichier LittleFS transactionnel.

### Documentation hardware/code

- **[critique]** Le « diviseur » d’entrée dessiné est électriquement incomplet pour une sortie Noctua à collecteur ouvert : il relie tach à la masse par 10 kΩ + 3,3 kΩ mais ne montre aucun pull-up vers 3,3 V. Sans pull-up externe, la ligne relâchée flotte/basse et aucun pulse fiable n’est mesuré.
- **[critique]** Si le dessin suppose au contraire une tach déjà tirée à 12 V, cette hypothèse n’est ni établie ni compatible avec le fonctionnement autonome typique Noctua. Le GPIO dépend alors d’un câblage externe non décrit.
- **[critique]** Aucun schéma complet de sortie collecteur ouvert n’indique type de NPN, résistance de base, pull-down de boot, Vceo ou raccordement de masse. La compatibilité réelle avec des pull-up Deye 3,3/5/12 V n’est donc pas démontrée.
- **[critique]** Aucun schéma d’OR-ing des deux connecteurs, protection anti-backfeed, masse commune vérifiée ou conversion 12→5 V n’est livré.
- **[majeur]** Il n’y a ni nomenclature, ni procédure de mise en service, ni mesure préalable des tensions Deye, ni avertissement sur les dangers de l’onduleur.
- **[majeur]** Aucun test scope n’est demandé pour vérifier fréquence, niveau bas collecteur, jitter et comportement sous WiFi.

### Qualité de code générale (négatif)

- **Structure : [majeur]** un unique fichier d’environ 1 570 lignes mélange logique temps-réel, HTML/CSS/JS, persistance et réseau.
- **Lisibilité : [mineur]** l’aération extrême et les commentaires répétitifs gonflent fortement le fichier et masquent les points de risque.
- **Commentaires : [majeur]** plusieurs commentaires sont inexacts à 80 MHz et donnent une confiance injustifiée dans les timings.
- **Robustesse : [critique]** secrets exposés, EEPROM sans intégrité, boot sans tach et matériel incomplet rendent l’ensemble impropre à un usage sûr sans reprise importante.
- **DRY : [mineur]** les deux ISR sont dupliquées au lieu d’un wrapper minimal commun ; le HTML est assemblé par centaines de fragments.
- **Conventions C++/Arduino : [majeur]** dépendance directe à une API interne non versionnée et emploi non nécessaire de `double`.
- **Maintenabilité : [majeur]** aucun manifeste de dépendance/version du core, aucune documentation séparée et paramètres liés à `F_CPU` par commentaires erronés.
- **Sécurité basique : [critique]** l’API sans auth expose explicitement tous les mots de passe.
- **Testabilité : [majeur]** aucune suite de tests, aucun banc matériel et aucune capture de jitter/compilation de référence.

## ⭐ Note globale : 4,2/10 — Architecture logicielle prometteuse, mais matériel incomplet, secrets exposés et readiness de boot non résolue sont rédhibitoires.

Le générateur Timer1 du core et la formule 2-PPR sont pertinents, mais ne compensent pas les défauts électriques et de sûreté.

## ⭐ Note qualité de code : 4,8/10 — Code lisible et abondamment commenté, mais fragile face aux versions, à la corruption et aux entrées réseau hostiles.

La base est réutilisable après refonte de la persistance, de l’interface web, du schéma hardware et des garanties de timing.
