# Analyse — composer 2.5 prompt from claude

**Sources analysées** : `competitors/composer 2.5 prompt from claude/deye_fan.ino`, `competitors/composer 2.5 prompt from claude/schema_electronique.md`

---

## ✅ Points techniques positifs

### Temps-réel / ISR
- Capture d'entrée par interruptions GPIO matérielles (`attachInterrupt` sur front `FALLING`) minimales et exécutées en RAM (`IRAM_ATTR`).
- Ordonnanceur logiciel cadencé par le Timer1 matériel à 50 µs (`TIMER_TICK_US = 50`), assurant une indépendance vis-à-vis des tâches de la boucle principale.
- Variables échangées entre monde temps-réel et monde best-effort déclarées `volatile` et de type `uint32_t`.

### Démarrage / Boot readiness
- Broches configurées avec sorties à l'état bas (`digitalWrite(..., LOW)`) avant le démarrage des interruptions.
- Configuration LittleFS chargée avec initialisation de valeurs par défaut saines en cas d'absence du fichier.

### Génération sortie / Timer1
- Architecture de type *pulse scheduler* basée sur le Timer1 matériel configuré en `TIM_DIV16`, `TIM_EDGE`, `TIM_LOOP`.
- Limitation de la pente de variation de période (`slewLimit`, max 15 % par cycle) pour éviter les à-coups brutaux face à l'onduleur Deye.

### Fidélité au protocole tachymétrique / Fail-safe
- Règle de 2 impulsions par tour respectée (`RPM = 30 000 000 / periodUs`).
- Timeout d'inactivité de 1,5 s (`INPUT_TIMEOUT_US`) coupant la génération et relâchant la ligne au repos.

### Filtres / Signal
- Moyenne glissante sur 4 échantillons (`smoothPeriod`) pour filtrer les variations mécaniques.
- Limiteur de pente (`slewLimit`) préservant la stabilité de la consigne transmise.
- Validation des bornes de période entre 2 500 µs (~7 200 tr/min) et 500 000 µs (~60 tr/min).

### WiFi / Réseau
- Serveur asynchrone non-bloquant utilisant `ESPAsyncWebServer` et `ESPAsyncTCP`.
- Connexion Station en arrière-plan avec machine à états non-bloquante (`serviceWifi`).

### Gestion mémoire / HEAP
- Interface web embarquée en Flash (`INDEX_HTML[] PROGMEM`).
- Sérialisation JSON directe avec `StaticJsonDocument`.

### Interface Web
- Interface dynamique rafraîchie en JavaScript asynchrone (`fetch('/api/status')`) sans rechargement de page.
- Modification des ratios et des paramètres réseau via requêtes POST dédiées.

### Persistance / Stockage
- Persistance au format JSON dans le système de fichiers `LittleFS` (`/config.json`) avec bibliothèque `ArduinoJson`.
- Sauvegarde déclenchée uniquement sur requête explicite de l'utilisateur.

### Documentation hardware/code
- Document `schema_electronique.md` de très bonne qualité : synoptique ASCII clair, alimentation avec OR-ing par diodes Schottky (SS14/1N5819), condensateur réservoir de 680 µF, convertisseur buck 12V→5V, et découplage pour absorber les pics WiFi.
- Calcul détaillé de la résistance de base du transistor NPN de sortie (BC547, 1 kΩ pour saturation profonde).

### Qualité de code générale (positif)
- **Structure / organisation** : Séparation claire entre les mondes temps-réel et best-effort.
- **Lisibilité / nommage** : Identificateurs bien choisis et style uniforme.
- **Commentaires / documentation inline** : Présentation claire des contraintes physiques et logicielles.
- **Gestion d'erreurs / robustesse** : Validation systématique des ratios avec `constrain()`.
- **Duplication / DRY** : Boucles d'itération sur `NUM_CHANNELS`.

---

## ❌ Points techniques négatifs

### Temps-réel / ISR (majeur)
- [majeur] : Appel de `digitalWrite()` directement à l'intérieur de l'ISR Timer1 (`onTimer1Tick`) :
  ```cpp
  digitalWrite(ch == 0 ? PIN_TACH_OUT_CH0 : PIN_TACH_OUT_CH1, g_outLevel[ch]);
  ```
  Sur ESP8266, `digitalWrite()` exécute plusieurs instructions de conversion de broche et de masquage (environ 5 à 8 µs). Exécuté toutes les 50 µs, cela monopolise entre 10 % et 20 % du CPU uniquement dans l'interruption, au lieu d'une écriture instantanée sur `GPOS`/`GPOC` — impact : charge CPU excessive sous ISR pouvant causer des conflits de latence.
- [majeur] : L'échéance suivante dans l'ordonnanceur est calculée par `now + half` au lieu d'accumuler la période (`next += half`) :
  ```cpp
  g_nextToggleUs[ch] = now + half;
  ```
  Si l'interruption a quelques microsecondes de retard à cause d'une section critique WiFi, ce retard s'accumule à chaque front, entraînant une dérive de fréquence cumulée systématique — impact : dérive continue de la vitesse simulée.

### Filtres / Signal (majeur)
- [majeur] : Dans `updateRealtimeTargets()`, la fonction `smoothPeriod(ch, raw[ch])` est invoquée à chaque cycle de la boucle `loop()` sans vérifier si un nouveau front a effectivement été mesuré. En conséquence, la même période brute est réinjectée des dizaines de fois dans le buffer de moyenne circulaire — impact : le lissage sur 4 échantillons perd son utilité dynamique en régime stationnaire.

### Boot readiness / Fail-safe (mineur)
- [mineur] : `g_channelActive` est mis à jour avant la validation de la fraîcheur du signal d'entrée dans certaines branches de la machine à états — impact : risque de glitch fugitif lors de la bascule en arrêt.

### Qualité de code générale (négatif)
- [mineur] : L'affectation de sortie dans l'ISR utilise un opérateur ternaire redondant au lieu d'un masque de registre précalculé — impact : micro-gaspillage de cycles sous interruption.

---

## ⭐ Note globale : 6.8/10 — Excellente conception générale pénalisée par des détails en ISR
Le livrable bénéficie d'une excellente documentation matérielle et d'un serveur web asynchrone performant, mais souffre de l'appel de `digitalWrite()` dans l'ISR à 20 kHz et d'une dérive cumulée sur le calcul des demi-périodes.

## ⭐ Note qualité de code : 7.2/10 — Code propre et structuré
Architecture bien découpée, utilisation judicieuse de LittleFS et d'ArduinoJson, avec seulement quelques lacunes d'optimisation bas niveau dans le coeur de l'ISR.
