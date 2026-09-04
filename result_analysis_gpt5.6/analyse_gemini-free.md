# Analyse — gemini-free

Sources analysées (liste exhaustive du dossier) :
- `competitors/gemini-free/gemini-code-1787264142602.cpp`

## ✅ Points techniques positifs

### Temps-réel / ISR

- Les deux ISR d’entrée et l’ISR Timer1 sont marquées `IRAM_ATTR`.
- La génération utilise le Timer1 matériel de l’ESP8266, pas un `Ticker` ni une bascule par polling dans `loop()`.
- Les entrées sont capturées par interruptions `FALLING`, ce qui est adapté à un tachymètre open collector.
- Les variables 32 bits échangées avec l’ISR de sortie sont déclarées `volatile`.

### Démarrage / Boot readiness

- Les GPIO de sortie sont configurés puis forcés à `LOW` avant le démarrage du WiFi.
- Les cibles de demi-période sont initialisées à zéro, donc aucune émission volontaire ne commence avant une mesure.
- AP, STA et serveur asynchrone sont initialisés sans boucle d’attente bloquante.

### Génération sortie / Timer1

- La formule visée pour la demi-période est correcte : `15 000 000 / RPM` pour un signal à deux impulsions par tour et deux transitions par période.
- Les deux canaux ont des compteurs et consignes indépendants.
- Le mode `TIM_SINGLE` est réarmé dans l’ISR, ce qui exprime clairement l’intention de cadence périodique.

### Fidélité protocole tachymétrique / Fail-safe

- Le calcul d’entrée `60 000 000 / (intervalle × 2)` respecte explicitement deux impulsions par tour.
- Un timeout d’une seconde remet RPM et consigne de sortie à zéro lorsque la configuration reste valide.
- Un seuil anti-rebond de 5 ms rejette les fronts très rapprochés.

### WiFi / Réseau

- Le mode `WIFI_AP_STA` fournit un accès local permanent tout en permettant une connexion STA.
- Le serveur asynchrone évite d’appeler `handleClient()` dans la boucle.
- Le SSID et le mot de passe STA sont persistés et appliqués au redémarrage.

### Gestion mémoire / HEAP

- HTML en `PROGMEM` et documents ArduinoJson statiques bornent une partie importante de l’usage RAM.
- Les identifiants WiFi sont conservés dans des tableaux `char` et copiés avec `strlcpy`, qui garantit la terminaison NUL.
- Aucune allocation dynamique n’est effectuée dans les ISR.

### Interface Web

- La page présente les RPM réels et simulés des deux voies.
- Ratios et WiFi disposent de formulaires séparés.
- Le statut est rafraîchi automatiquement chaque seconde.
- L’interface HTML est compacte, lisible et adaptée à un écran mobile.

### Persistance / Stockage

- LittleFS et JSON rendent la configuration lisible et extensible.
- Les documents JSON sont de taille fixe et les fichiers sont correctement fermés.

### Qualité de code générale (positif)

- **Structure :** sections distinctes pour pinout, ISR, persistance, interface et boucle.
- **Lisibilité :** variables explicites et commentaires courts rendent le flux facile à suivre.
- **Commentaires :** unités et formules RPM sont indiquées près des calculs.
- **Robustesse :** timeout, anti-rebond, copies bornées et valeurs par défaut couvrent quelques erreurs usuelles.
- **DRY :** la structure générale des deux canaux est identique, donc facile à comparer.
- **Conventions C++/Arduino :** usage correct de `IRAM_ATTR`, `PROGMEM`, `digitalPinToInterrupt` et types Arduino usuels.
- **Maintenabilité :** faible nombre de fonctions et interface simple facilitent une première prise en main.
- **Sécurité basique :** l’AP exige au moins un mot de passe par défaut.
- **Testabilité :** les formules sont visibles et pourraient être extraites facilement dans des fonctions pures.

## ❌ Points techniques négatifs

### Exactitude de compilation probable

- [majeur] Le fichier porte l’extension `.cpp` et définit `setup()/loop()` sans `#include <Arduino.h>`. Contrairement à un `.ino`, un `.cpp` n’est pas automatiquement prétraité avec les déclarations Arduino ; impact : `pinMode`, `micros`, `LOW`, `IRAM_ATTR`, Timer1 et autres symboles peuvent ne pas être déclarés selon les inclusions transitives, donc compilation non garantie.
- [majeur] Aucune version de core ou des dépendances `ESPAsyncTCP`, `ESPAsyncWebServer`, ArduinoJson n’est fournie ; impact : build non reproductible et incompatibilités possibles avec les forks récents.
- [mineur] Aucun manifeste PlatformIO ou instructions Arduino IDE ne précisent le type de carte et la partition LittleFS ; impact : compilation ou montage FS dépendant de réglages implicites.

### Temps-réel / ISR

- [majeur] L’ISR Timer1 appelle `digitalWrite()` jusqu’à quatre fois par passage puis `timer1_write()` ; impact : latence et jitter supérieurs à des écritures directes `GPIO_W1TS/W1TC`, avec dépendance à l’emplacement IRAM des fonctions du core.
- [majeur] L’interruption Timer1 est normale, pas NMI ; impact : sections critiques WiFi pouvant retarder des bascules.
- [majeur] Les ratios et RPM sont des `float` partagés entre callbacks asynchrones et `loop()` sans synchronisation ; impact : lecture incohérente ou changement en plein calcul selon le contexte d’exécution de la bibliothèque.
- [mineur] Quand une cible est nulle, `digitalWrite(LOW)` est exécuté à chaque interruption ; impact : travail ISR inutile.

### Génération sortie / Timer1

- [critique] Timer1 est configuré avec `TIM_DIV16`, donc il compte à 5 ticks/µs, mais l’ISR recharge `80 × 50 = 4000` ticks en supposant 80 ticks/µs. L’interruption arrive toutes les 800 µs et le compteur logiciel n’ajoute que 50 µs ; impact : le tach simulé est environ 16 fois plus lent que demandé.

```61:69:competitors/gemini-free/gemini-code-1787264142602.cpp
void IRAM_ATTR onTimerISR() {
  static unsigned long counter1 = 0;
  static unsigned long counter2 = 0;
  const unsigned long timerStepUs = 50;

  if (targetOutHalfPeriod1 > 0) {
    counter1 += timerStepUs;
```

```87:92:competitors/gemini-free/gemini-code-1787264142602.cpp
  } else {
    digitalWrite(PIN_TACH_OUT_2, LOW);
  }

  timer1_write(80 * timerStepUs);
}
```

- [critique] Le même `timer1_write(4000)` lance initialement une période de 800 µs, confirmant que l’erreur n’est pas limitée à un commentaire ; impact : fonction principale techniquement incorrecte.
- [majeur] Les compteurs sont remis à zéro au dépassement au lieu de conserver le reliquat ; impact : arrondi systématique au tick supérieur, dérive de fréquence et erreur pouvant atteindre un tick par demi-période.
- [majeur] Aucun rattrapage n’existe après une ISR retardée ; impact : perte de phase et impulsions manquantes sous charge.
- [majeur] `digitalWrite` traite les voies séquentiellement ; impact : fronts simultanés séparés par le temps d’appel de la fonction.

### Démarrage / Boot readiness

- [majeur] Le premier front utilise `lastPulseTime = 0`; s’il arrive plus de 5 ms après le démarrage, le temps boot→front est accepté comme période ; impact : RPM initial faux et émission transitoire arbitraire avant la deuxième impulsion.
- [majeur] LittleFS et le réseau sont initialisés avant les interruptions d’entrée et Timer1 ; impact : délai supplémentaire sans tach au boot et risque d’alarme Deye avant première mesure.
- [majeur] Aucun délai maximal de boot, comportement de brown-out ou stratégie de readiness n’est documenté ; impact : impossibilité d’évaluer le risque d’alarme lors de l’alimentation depuis un connecteur ventilateur.
- [mineur] La LED est forcée `LOW` au boot sans préciser sa polarité électrique sur GPIO13 ; impact : état utilisateur ambigu.

### Fidélité protocole tachymétrique / Fail-safe

- [critique] Un ratio nul, négatif ou non numérique n’efface pas la dernière `targetOutHalfPeriod`. Le test `if (rpmOut > 0)` est simplement sauté ; impact : la sortie peut continuer indéfiniment à simuler l’ancienne vitesse malgré une configuration invalide.
- [majeur] Aucun bornage des ratios n’est appliqué au chargement ou à l’API ; impact : fréquence hors plage, overflow de conversion flottant→entier ou état fail-safe incohérent.
- [majeur] Le seuil `diff > 5000` limite les fronts à moins de 200 Hz, soit moins de 6000 RPM à deux impulsions/tour, et non « ~12000 RPM » ; impact : régimes supérieurs rejetés et RPM figé sur une ancienne valeur.
- [majeur] `lastPulseTime` n’est mis à jour que pour un front accepté. Une entrée réellement plus rapide que 6000 RPM est sous-échantillonnée en agrégeant plusieurs périodes ; impact : RPM mesuré sous-estimé.
- [majeur] Le timeout fixe d’une seconde n’est pas lié à la période ; impact : arrêt détecté lentement à régime normal et impossibilité de représenter proprement moins de 30 RPM.
- [mineur] Les deux impulsions/tour sont codées en littéraux `2.0` et `15000000.0`, pas dans une constante commune ; impact : divergence facile lors d’un changement de protocole.

### Filtres / Signal

- [majeur] Aucun filtre moyenne, médiane, EMA ou limitation de pente n’est appliqué ; impact : chaque variation ou parasite accepté change immédiatement la sortie simulée.
- [majeur] `delay(50)` impose une mise à jour des consignes à environ 20 Hz seulement ; impact : réponse tardive de 50 ms et constante variable avec les callbacks.
- [mineur] Il n’existe pas de validation de cohérence entre plusieurs périodes successives ; impact : un outlier isolé produit une excursion de RPM.

### WiFi / Réseau

- [critique] Aucune authentification Web ni protection CSRF ; impact : tout client AP/STA peut modifier les ratios et redémarrer l’équipement.
- [majeur] Le SSID/mot de passe AP sont fixes (`Deye-Fan-Simulator` / `12345678`) et non configurables ; impact : accès trivial par une personne connaissant le code.
- [majeur] Aucun mécanisme de reconnexion STA, timeout ou retour d’état n’est implémenté ; impact : perte du mode STA jusqu’au redémarrage après incident réseau.
- [majeur] Les saisies HTTP ne sont pas validées en longueur, plage ou format au niveau métier ; impact : état persistant invalide.
- [mineur] Aucun mDNS, indication d’adresse IP ou nombre de clients n’aide au diagnostic ; impact : maintenance réseau difficile.

### Gestion mémoire / HEAP

- [majeur] Chaque route JSON construit une `String response` dynamique ; avec un polling toutes les secondes, cela fragmente progressivement le heap ESP8266 ; impact : instabilité après fonctionnement prolongé.
- [majeur] Les callbacks manipulent aussi les `String` retournées par `value()` ; impact : pics d’allocation lors des sauvegardes.
- [mineur] Aucun suivi du heap libre/minimal ou gestion explicite des erreurs de sérialisation ; impact : panne mémoire silencieuse.

### Interface Web

- [majeur] Le formulaire accepte visuellement tout ratio sans `min`/`max` et le backend ne valide rien ; impact : configuration dangereuse facilement saisie.
- [majeur] Les requêtes `fetch` n’ont aucune gestion d’erreur ; impact : l’interface peut afficher des données anciennes sans avertissement.
- [majeur] L’état stall/live et la connectivité WiFi ne sont pas affichés ; impact : zéro RPM impossible à distinguer d’une panne de communication.
- [mineur] La LED ne suit pas le commentaire : elle clignote rapidement quand un canal tourne et reste basse sinon, au lieu d’être fixe en activité et clignotante en attente ; impact : diagnostic physique inversé par rapport à l’intention.

### Persistance / Stockage

- [majeur] `LittleFS.begin()` n’est pas vérifié ; impact : le firmware continue après échec de montage et les réglages semblent sauvegardés alors qu’ils ne le sont pas.
- [majeur] Le fichier JSON n’a ni version, ni CRC, ni écriture atomique temporaire + rename ; impact : corruption complète après coupure pendant écriture.
- [majeur] Les ratios chargés ne sont pas validés ; impact : une corruption JSON sémantiquement valide peut immédiatement produire une commande dangereuse.
- [mineur] Les erreurs d’ouverture et de sérialisation ne sont jamais remontées à l’interface ou au port série ; impact : échec silencieux.

### Documentation hardware/code

- [critique] Aucune documentation hardware n’est fournie. Les sorties sont montrées uniquement comme GPIO dans le code, sans NPN/MOSFET collecteur ouvert ; impact : une connexion directe à une pull-up Deye 5 V ou 12 V peut détruire l’ESP8266 et ne reproduit pas le protocole open collector.
- [critique] Aucun schéma d’entrée n’établit la compatibilité avec tach open collector, pull-up 3,3 V ou une éventuelle source 5/12 V ; impact : entrée flottante, surtension GPIO ou lecture non fiable selon le câblage.
- [critique] Il n’existe aucun schéma d’OR-ing des deux connecteurs 12 V ; impact : relier directement les alimentations peut provoquer un backfeed entre sorties de l’onduleur.
- [critique] Aucun buck 12 V → 5 V n’est spécifié ; impact : risque d’appliquer 12 V au Wemos ou de surchauffer un régulateur linéaire non prévu pour cette dissipation.
- [majeur] Masse commune, résistances de base/pull-up, transistor, découplage et niveaux de repos ne sont pas documentés ; impact : réalisation matérielle non reproductible et potentiellement dangereuse.
- [majeur] Aucune instruction de compilation, BOM, procédure oscilloscope ou test de jitter ; impact : impossibilité de valider le montage et les affirmations temps-réel.

### Qualité de code générale (négatif)

- **Structure :** logique des deux canaux entièrement dupliquée ; [majeur], impact : corrections susceptibles de n’être appliquées qu’à une voie.
- **Lisibilité :** le code est court, mais le commentaire Timer1 erroné donne une fausse confiance ; [critique], impact : erreur fonctionnelle centrale difficile à repérer sans recalcul.
- **Commentaires :** anti-rebond « 12000 RPM » et comportement LED ne correspondent pas au code ; [majeur], impact : diagnostic et maintenance trompeurs.
- **Robustesse :** ratios non bornés, erreurs FS ignorées, premier front faux et consigne obsolète ; [critique], impact : sortie tach incorrecte ou persistante.
- **DRY :** ISR et boucle sont dupliquées par canal au lieu d’utiliser tableaux/fonctions ; [majeur], impact : maintenabilité médiocre.
- **Conventions C++/Arduino :** fichier `.cpp` sans inclusion Arduino explicite, flottants globaux et `digitalWrite` en ISR ; [majeur], impact : build et déterminisme fragiles.
- **Maintenabilité :** constantes magiques et absence de manifeste/dépendances ; [majeur], impact : évolution risquée.
- **Sécurité basique :** AP connu et API non authentifiée ; [critique], impact : commande distante non autorisée.
- **Testabilité :** aucun test unitaire, scénario de stall, compilation CI ou mesure timer ; [critique], impact : l’erreur ×16 atteint la version livrée.

## ⭐ Note globale : 2,7/10 — Prototype d’interface exploitable, mais Timer1 produit environ 1/16 de la fréquence demandée et aucun hardware sûr n’est spécifié.

Les bases AP+STA, LittleFS et deux impulsions/tour sont présentes, toutefois la fonction tach principale et la compatibilité électrique ne sont pas opérationnelles en l’état.

## ⭐ Note qualité de code : 3,6/10 — Code lisible en surface, mais non reproductible, dupliqué, non sécurisé et dépourvu de validations et tests essentiels.

La simplicité facilite la reprise, mais plusieurs erreurs critiques montrent qu’aucune validation de compilation, de timer ou de fail-safe n’a été menée.
