# Analyse — qwen3.6_prompt_from_claude

**Source :** `competitors/qwen3.6_prompt_from_claude/deye_fan.ino` + `SCHEMATIC.md`  
**Fichiers analysés :** 1 fichier source C++ (.ino), 1 fichier documentation technique (.md)  
**Score global : 8.2 / 10**  
**Score qualité de code : 7.5 / 10**

---

## ✅ Positifs techniques

### Catégorie 1 — Temps-réel / ISR
- **[POSITIF] `ICACHE_RAM_ATTR` sur les deux ISRs GPIO** (`tachISR_A`, `tachISR_B`) — le code sera servi depuis la RAM IRAM. Point critique maîtrisé, héritage correct de qwen3.6.
- **[POSITIF] Écriture directe registre GPIO (GPOS/GPOC) dans schedulerTick()** : Correction majeure par rapport à qwen3.6 qui utilisait `digitalWrite()` dans l'ISR. Les écritures `GPOS = (1UL << SIMCHA_PIN)` et `GPOC = (1UL << SIMCHB_PIN)` sont des accès directs au registre GPIO — 1 instruction machine (~0,05 µs), IRAM_ATTR. **Bug fatal de qwen3.6 résolu.**
- **[POSITIF] Architecture « deux mondes » clairement documentée** : Diagramme ASCII en entête séparant le monde temps-réel (ISR/Timer1) du monde best-effort (loop/web). Approche de conception consciente et bien pensée.
- **[POSITIF] Anti-rebound par timestamp micros()** : Comparaison avec `TACH_DEBOUNCE_MIN_US = 500 µs`. Rejet immédiat dans l'ISR, pas de post-traitement. Correct pour éliminer les rebons mécaniques/électriques tout en capturant tous les fronts valides Noctua.
- **[POSITIF] Validation plage période dans l'ISR** : `period >= PERIOD_MIN_US && period <= PERIOD_MAX_US` filtre les artefacts électriques hors bande (10 ms à 333 ms). Couvre la plage Noctua (~3 à ~200 Hz).
- **[POSITIF] Variables partagées uniquement `uint32_t volatile`** : Le code note explicitement que tous les types de données partagés ISR↔loop sont des `uint32_t` → accès ATOMIQUE sur ESP8266 (32-bit). Aucune section critique explicite nécessaire. Bonne analyse architecturale.

### Catégorie 2 — Démarrage / Boot
- **[POSITIF] Boot log structuré et détaillé** : En-tête ASCII, messages `[CONFIG]`, `[CFG]`, `[HW]`, `[WIFI]`, `[SYSTEM]`. IP affichée selon le mode. Excellent pour diagnostic.
- **[POSITIF] Fallback AP intelligent** : Timeout STA de 30 secondes dans `loop()`, puis bascule automatique en mode AP. Pas bloquant — l'appareil reste opérationnel pendant la tentative.
- **[POSITIF] LittleFS initialisé avec fallback robuste** : `LittleFS.begin(true)` + vérification fichier + parse JSON avec gestion d'erreur `DeserializationError`. En cas d'échec, chargement des valeurs par défaut + écriture automatique du fichier default. Aucune configuration manuelle requise pour un premier déploiement.
- **[POSITIF] Overclock CPU à 160 MHz** : `system_update_cpu_freq(160)` double la capacité de traitement WiFi/loop. Gain notable pour le serveur web asynchrone et le traitement OTA. Consommation +15% acceptable pour un appareil branché en continu.

### Catégorie 3 — Génération sortie / Timer1
- **[POSITIF] Sortie sur D2 ET D5 entièrement implémentée** : Deux sorties physiques gérées par le scheduler Ticker @20kHz. Contrairement à gemma4 (jamais basculé) et qwen35 (fréquence fixe 1 Hz), ici la fréquence est proportionnelle au RPM × ratio mesuré en temps réel.
- **[POSITIF] Approche pulse scheduler** : Le scheduler compare `now >= schedule[0].nextChangeUs` et programme le prochain tick en conséquence (`now + (period >> 1)`). Méthode robuste évitant l'accumulation de dérive temporelle.
- **[POSITIF] Fan stopped state correctement géré** : Quand `targetPeriodA_us == 0`, le scheduler ne bascule pas la sortie → état HIGH permanent maintenu par `digitalWrite(SIMCHA_PIN, HIGH)` au boot. Comportement correct pour un tach simulé.
- **[POSITIF] Limites de fréquence applicables** : `OUTPUT_PERIOD_MIN_US = 2500 µs` (= ~400 Hz max) borne les fréquences de sortie, évitant les dépassements physiologiquement impossibles pour des Noctua.

### Catégorie 4 — Fidélité protocole / Fail-safe
- **[POSITIF] Stall detection avec timeout configurable** : `STALL_TIMEOUT_MS = 2000` ms dans `checkStall()`. Si aucun front reçu pendant 2 secondes, le canal est mis à zéro (sortie forcée). Fail-safe fonctionnel.
- **[POSITIF] Moyenne glissante sur N dernières périodes** : Buffer de 4 échantillons (`avgBufA[AVG_SAMPLES]`) dans l'ISR. Moyenne arithmétique calculée correctement dans `calcMovingAverage()`. Contrairement à qwen35 qui lisait des compteurs cumulés, ici le buffer stocke des **périodes cycliques** → moyenne physiquement significative.
- **[POSITIF] Filtre exponentiel (exponential smoothing)** : `smoothTransition()` applique `α × target + (1-α) × current` avec `α = 0.3`. Lissage progressif évitant les sauts de fréquence brutaux sur la sortie simulée. Approche IIR réelle, contrairement au filtre faux de qwen35.
- **[POSITIF] Double lissage** : Moyenne glissante (4 échantillons) + filtre exponentiel (`α=0.3`) sur le smoothed + lissage du target entre appels (`α=0.5`). Triple protection contre les fluctuations turbulentes des Noctua.

### Catégorie 6 — WiFi / Réseau
- **[POSITIF] Mode AP+STA simultanée** : `WiFi.mode(WIFI_AP_STA)` avec tentative STA non-bloquante via `tryStaConnect()`, fallback AP après 30s de timeout. UX flexible.
- **[POSITIF] mDNS inclus** : `#include <ESP8266mMDNS.h>` — découverte réseau Zeroconf possible (ex: `DeyeTachSimulator.local`). Seul compétiteur avec ce support natif, bien que non configuré explicitement (`MDNS.begin()` n'est pas appelé dans le code visible).
- **[POSITIF] OTA (Over-The-Air) intégré** : `ArduinoOTA` configuré sur port 8266 avec mot de passe `"deyetach1"`. Mises à jour firmware sans câblage USB. Point fort absent chez tous les autres compétiteurs (gemma4, qwen35, gemini-free avaient OTA absent).
- **[POSITIF] Reconnexion WiFi non-bloquante** : La tentative STA ne bloque pas la loop. Le timeout de 30s dans `loop()` permet à l'appareil de continuer à fonctionner en mode AP pendant que le STA est tenté.

### Catégorie 7 — Gestion mémoire / HEAP
- **[POSITIF] FIX #3 — JSON sans allocation heap** : Les handlers web utilisent un buffer statique sur la pile (`char jsonBuf[CAP]`) + `serializeJson(doc, static_cast<uint8_t*>(jsonBuf), CAP)`. **Zéro allocation heap par requête JSON**. Bien supérieur à String concatenation de gemma4/qwen35 et même au DynamicJsonDocument (qui alloue en heap). Note : `DynamicJsonDocument` est utilisé pour le parsing (`deserializeJson`) ce qui est acceptable car limité à 2-3 appels par requête POST.
- **[POSITIF] HTML stocké en PROGMEM** : `const char INDEX_HTML[] PROGMEM = R"rawliteral(...)"` — la page web entière tient en flash, pas en SRAM. Économie significative de mémoire volatile (~5 KB). Contrairement à gemma4 et qwen35 qui had HTML en SRAM.
- **[POSITIF] DynamicJsonDocument pour le parsing uniquement** : Le parsing JSON (POST requests) utilise `DynamicJsonDocument` car la taille du body est variable. C'est acceptable car limité à quelques dizaines de requêtes par heure. La sérialisation GET utilise un buffer statique sur la pile (zéro heap).

### Catégorie 8 — Interface Web
- **[POSITIF] API JSON séparée et cohérente** : `/api/config` (GET/POST) + `/api/readings` (GET) avec parsing structuré. Les lectures RPM incluent input/output pour chaque canal + statut global. Format machine-readable compatible avec Home Assistant, Node-RED, etc.
- **[POSITIF] Formulaire de configuration via JavaScript fetch** : `saveAll()` envoie un POST JSON au serveur web. Reboot différé après sauvegarde (flag `deferredRestartRequested`). UX moderne et fluide.
- **[POSITIF] Auto-refresh AJAX des lectures** : `setInterval(loadReadings, 1000)` — polling toutes les secondes pour le monitoring temps réel. Pas de rechargement complet de page nécessaire.
- **[POSITIF] Mot de passe AP valide (9 caractères)** : `"deyetach1"` respecte le minimum WPA2 de 8 caractères. Bien supérieur aux `"12345678"` (trivial) et à l'AP ouvert de qwen3.6.
- **[POSITIF] Mot de passe OTA cohérent** : `ArduinoOTA.setPassword("deyetach1")` — même mot de passe que l'AP, pratique pour l'utilisateur mais potentiellement préoccupant d'un point de vue sécurité si OTA est exposé publiquement.
- **[positif mineur] Masquage password dans API config** : `doc["wifi_pass"] = "••••••••"` — le password STA n'est jamais renvoyé en clair depuis `/api/config`. Bonne pratique de sécurité.

### Catégorie 9 — Persistance / Stockage
- **[POSITIF] LittleFS pour configuration complète** : Ratios + WiFi STA/AP persistés dans `/config.json` sur le système de fichiers. Persistant au reboot, bien supérieur à EEPROM non utilisée de certains concurrents ou absente de gemma4.
- **[POSITIF] Structure `Config_t` typée avec tailles protégées** : `char wifi_ssid[33]`, `char wifi_pass[65]`, etc. — tailles respectant les limites WiFi (max 32 caractères SSID, max 63 caractères password). Aucune déborder possible.
- **[POSITIF] Fallback automatique aux valeurs par défaut** : Si LittleFS n'est pas monté, fichier absent, ou JSON corrompu → chargement des defaults + écriture automatique du fichier default. Aucun reset manuel nécessaire.
- **[POSITIF] Cooldown 5s entre écritures Flash** : `SAVE_COOLDOWN_MS = 5000UL` empêche les écritures EEPROM/LittleFS trop fréquentes. Protection contre l'usure rapide du flash (limite ~100 000 cycles). Empêche également un utilisateur mal intentionné de saturer le FS avec des requêtes POST répétées.

### Catégorie 10 — Documentation hardware
- **[POSITIF] Schéma électronique complet** : `SCHEMATIC.md` contient une vue d'ensemble ASCII, un schéma alimentation détaillé (diodes Schottky, buck MT3608, condensateurs), un schéma d'entrée tach (pull-up 4.7kΩ + filtre RC 1nF), un schéma de sortie NPN (BC547 avec résistance 10kΩ), et un schéma LED complet.
- **[POSITIF] Liste des composants (BOM) complète** : 15 composants listés avec références, valeurs, quantités et rôles. Inclut les composants additionnels (Schottky OR-ing, buck MT3608, condensateur réservoir). Bien plus détaillé que le simple diviseur résistif de gpt-free.
- **[POSITIF] Calculs de dimensionnement fournis** : Courant de base NPN (`Ib = 0.26mA`), chute tension buck, constante de temps filtre RC, courants LED. Chaque choix de composant est justifié par un calcul. Niveau professionnel.
- **[POSITIF] Brochage complet avec justifications** : Tableau détaillé GPIO ↔ fonction avec explication des broches exclues (D0/GPIO16 pas d'interruption, D3/GPIO0 boot pull-down, D8/GPIO15 tiré LOW). Aide considérable au câblage.
- **[POSITIF] Points de test et dépannage** : Section 9 avec procédure étape par étape et tableau de dépannage (symptômes → causes → remèdes). Très pratique pour un DIY.

### Catégorie 11 — Qualité de code générale
- **[POSITIF] Sections commentées clairement numérotées** : Délimitations par `// ━━━━━━━━━━` + descriptions textuelles. Lisibilité exceptionnelle. Architecture explicitement documentée.
- **[POSITIF] Structure `OutputSchedule` pour les sorties** : Regroupement logique des variables de sortie (`nextChangeUs`, `currentState`) dans une struct. Meilleure organisation que des variables globales dispersées.
- **[POSITIF] FIX #6 — Reboot différé non-bloquant** : Le handler POST ne fait pas `ESP.restart()` directement (qui bloquerait la réponse HTTP). Il pose un flag `deferredRestartRequested` qui est traité dans `loop()` après 200ms. Garantit que la réponse TCP arrive avant le reboot. Approche mature et bien pensée.
- **[POSITIF] FIX #5 — Overclock CPU documenté** : Commentaire explicitant le gain (`×2 WiFi/loop`) et le coût (`+15% consommation`). Transparency technique appréciée.
- **[POSITIF] Variables statiques pour la scope locale** : `static bool saveRequested`, `static OutputSchedule schedule[2]` — portée limitée aux fichiers. Bonne pratique C++.

---

## ❌ Négatifs techniques

### Catégorie 1 — Temps-réel / ISR
- **[majeur] Ticker @20kHz = appel de callback toutes les 50 µs** : Le `schedulerTick()` est appelé 20 000 fois par seconde via Ticker. À chaque appel, il fait `micros()`, des comparaisons et potentiellement des écritures GPOS/GPOC. C'est un overhead CPU constant d'environ 20 000 × ~5 µs = **100 ms de CPU par seconde** (≈10% du CPU à 80 MHz). Sur ESP8266, le Ticker utilise un timer interne qui appelle le callback dans un contexte ISR — cela signifie que `schedulerTick()` a la même priorité qu'une interruption. En pratique, ce n'est pas un bug fonctionnel mais c'est une consommation CPU non-négligeable qui pourrait interférer avec d'autres tâches (OTA, web server async). Une approche alternative serait de configurer directement le hardware timer avec `timerBegin()` / `timerAttachInterrupt()` pour un overhead réduit.
- **[mineur] `micros()` dans schedulerTick() @20kHz** : `micros()` est une macro/lutteinline sur ESP8266 qui lit le registre de temps, donc c'est efficace. Mais 20 000 appels/seconde à `micros()` créent un overhead constant. Sur l'ESP8266, `micros()` retourne la valeur brute du timer SysTick dérivé de la fréquence CPU — lire ce registre 20k fois par seconde est acceptable mais pas optimal.
- **[mineur] Pas de section critique pour les lectures multiples de volatile** : Dans `updateTargetPeriods()`, le code lit `smoothedPeriodA_us`, `avgBufA`, `countA` sans `noInterrupts()`/`interrupts()`. Comme ce sont des `uint32_t` et que l'ESP8266 fait des lectures 32-bit atomiques, c'est théoriquement sûr. Mais si deux lectures séquentielles (ex: `smoothedPeriodA_us` puis `targetPeriodA_us`) sont séparées par un interrupt ISR, on pourrait capturer un état incohérent partiellement. Le risque est faible mais réel.

### Catégorie 2 — Démarrage / Boot
- **[critique] Pas de watchdog (SW ou HW)** : Aucun hardware ou software watchdog implémenté. En cas de plantage WiFi, loop infinie, ou crash ISR, aucun reboot automatique. Point identique chez qwen3.6 et gpt-free, mais c'est un manque significatif pour un appareil industriel dédié à un onduleur.
- **[majeur] Reboot différé seulement 200ms** : `DEFERRED_RESTART_DELAY_MS = 200UL` donne 200 ms pour que la réponse HTTP soit envoyée avant le reboot. C'est théoriquement suffisant pour une petite réponse JSON/TEXT, mais dans des conditions de réseau chargé (WiFi bruité, faible RSSI), un ACK TCP peut prendre plus de 200ms. Risque que le client reçoive une erreur de connexion coupée plutôt qu'une réponse 200 OK. 500-1000 ms serait plus prudent.
- **[mineur] `ArduinoOTA` nécessite connexion STA** : OTA n'est fonctionnel qu'en mode STA (sur le réseau). En mode AP uniquement, ArduinoOTA ne peut pas recevoir de firmware car il n'y a pas de route IP vers un client OTA. L'utilisateur ne pourra faire OTA que si son routeur est opérationnel et que l'appareil y est connecté. Limitation pratique.

### Catégorie 3 — Génération sortie / Timer1
- **[majeur] Ticker (software) au lieu de hardware timer direct** : Le scheduler utilise `Ticker` pour appeler `schedulerTick()` @20kHz. Le Ticker est un wrapper Arduino qui s'appuie sur un timer interne, mais il passe par des appels de fonction C++ (virtualisation implicite). Contrairement à un hardware timer configuré directement avec `timerBegin()` / `timerAttachInterrupt()` + callback en C pur (comme qwen3.6 le tentait), cette approche introduit une latence supplémentaire (~2-3 µs par appel Ticker) et une variabilité de timing moindre. Pour une génération de signal tach précise, un hardware timer direct serait préférable.
- **[mineur] Période >> 1 dans scheduler** : `(period >> 1)` effectue une division entière par 2 (floor). Pour les périodes impaires, cela introduit une erreur de ~0.5% sur le duty cycle. Par exemple, pour une période de 3 µs → `3 >> 1 = 1`, donc HIGH=1µs + LOW=1µs = 2 µs total (au lieu de 3). C'est négligeable aux fréquences normales (>10 Hz) mais peut se voir à très haute fréquence simulée.

### Catégorie 4 — Fidélité protocole / Fail-safe
- **[majeur] Pas de validation des ratios après POST** : `handleConfigPost()` accepte n'importe quelle valeur flottante pour les ratios sans vérification de borne (min/max). Un ratio de -1.0, 0.0 ou 999.0 serait appliqué, produisant un signal de sortie aberrant (fréquence infinie, négative, ou extrêmement élevée). Contrairement à gpt-free qui utilisait `constrain()`.
- **[majeur] Formule RPM inversée dans `/api/readings`** : `rpmInputA = 30000000.0f / inputA_us` — cette formule suppose que le signal d'entrée Noctua a une période mesurée, mais c'est **incorrect** pour la conversion RPM. La formule correcte est `RPM = 60,000,000 / (period_us × pulses_per_rev)`. Avec `pulses_per_rev = 2`, on obtient `60,000,000 / (period_us × 2)` = `30,000,000 / period_us`. **En fait c'est correct** car `30,000,000 / period_us` équivaut bien à `60,000,000 / (period_us × 2)`. Donc ce point est **non applicable**. C'est mathématiquement juste.
- **[mineur] Pas de seuil minimum RPM pour activer la sortie** : Quand une période valide est détectée même infime, le target period est calculé immédiatement. Un fan à très bas régime (ex: 50 RPM) activerait le signal simulé avec un ratio ×2 → 100 RPM simulés. Peut-être desirable de mettre un seuil minimum pour éviter des activations transitoires lors du démarrage du fan.

### Catégorie 5 — Filtres / Signal
- **[majeur] Moyenne glissante de 4 échantillons est très courte** : `AVG_SAMPLES = 4U` signifie que la moyenne utilise seulement les 4 dernières périodes mesurées. Pour un fan à 2000 RPM avec 2 pulses/rev → ~67 Hz, cela représente une fenêtre d'environ 60 ms de lissage. C'est très court comparé aux filtres IIR (qui ont une mémoire infinie). Les fluctuations turbulentes des Noctua ne seront pas fortement amorties. Un filtre exponentiel avec `α=0.3` compense partiellement, mais le buffer de 4 échantillons apporte peu par rapport à un simple IIR.
- **[mineur] Triple lissage peut être excessif** : Moyenne glissante (4) + IIR sur smoothed (`α=0.3`) + IIR sur target (`α=0.5`). Trois niveaux de filtrage peuvent introduire une latence significative (~200 ms pour que le signal stabilisé atteigne 90% de la valeur finale). Pour un onduleur Deye qui attend un signal tach, cette latence est probablement acceptable (l'onduleur a ses propres timeouts), mais elle réduit la réactivité du système à un changement rapide de régime.

### Catégorie 6 — WiFi / Réseau
- **[majeur] Pas d'authentification web** : N'importe qui connecté au réseau (AP ou STA) peut accéder à `/api/config` et modifier les ratios, ou `/api/config` POST pour changer le WiFi. Aucune protection par mot de passe sur le serveur web. Identique à la plupart des concurrents mais reste un risque.
- **[majeur] `WiFi.softAPdisconnect(true)` dans `tryStaConnect()`** : Avant de tenter une connexion STA, le code déconnecte l'AP temporaire (`apStarted = false`). Si la tentative STA échoue rapidement (pas d'AP disponible), l'utilisateur perd immédiatement l'accès web via l'AP. Il faudra attendre 30s de timeout dans `loop()` pour que le fallback AP se réinitialise. Pire用户体验 que gemini-free qui maintient l'AP en parallèle du STA.
- **[mineur] mMDNS inclus mais non activé** : `#include <ESP8266mMDNS.h>` est présent mais `MDNS.begin("DeyeTachSimulator")` n'est jamais appelé dans le code visible (ni dans `setup()` ni ailleurs). Le header est inclus probablement pour être utilisé plus tard ou par ArduinoOTA, mais mDNS n'est pas fonctionnel tel quel.
- **[mineur] OTA avec mot de passe identique à l'AP** : `"deyetach1"` est utilisé à la fois comme mot de passe AP et mot de passe OTA. Si quelqu'un connaît le mot de passe WiFi (facile sur un réseau local), il peut aussi flasher du firmware malveillant sur l'appareil via OTA. Bon pour un usage domestique isolé, mais pas une bonne pratique de sécurité généraliste.

### Catégorie 7 — Gestion mémoire / HEAP
- **[majeur] `DynamicJsonDocument` en heap pour le parsing POST** : `handleConfigPost()` utilise `DynamicJsonDocument doc(capacity)` qui alloue en heap. Même si limité à quelques requêtes POST par heure, chaque parse crée une allocation temporaire. Sur ESP8266 avec fragmentation progressive, même des allocations temporaires peuvent laisser des fragments orphelins si la réponse error ou timeout intervient avant destruction du document. `StaticJsonDocument` serait préférable ici (body JSON est petit, capacité max connue).
- **[mineur] String dans handleConfigPost()** : `String body = request->getString()` alloue une heap string pour lire le body. Le body JSON est petit (< 256 B) donc acceptable. Mais un usage de `request->onBody()` callback avec écriture directe dans un buffer statique serait plus sûr anti-fragmentation.

### Catégorie 8 — Interface Web
- **[mineur] Design web clair au lieu de dark theme** : Contrairement à qwen3.6 et gemini-free qui avaient des interfaces dark theme (`#121212` / `#1a1a2e`), cette version utilise un fond blanc/gris (`#f5f5f5`). Moins moderne, mais pas fonctionnellement incorrect.
- **[mineur] Pas d'indicateur RSSI ou mode réseau** : L'interface n'affiche pas le RSSI WiFi ou le mode courant (AP/STA). Un petit texte indiquant la connectivité actuelle serait utile pour le diagnostic.
- **[majeur] Formulaire ratio sans borne HTML enforced côté POST serveur** : Les inputs HTML ont `min=.5 max=4.0` mais `handleConfigPost()` ne vérifie pas les bornes côté serveur. Un client malveillant peut envoyer un ratio de 999.0 via une requête brute et le firmware l'acceptera.

### Catégorie 9 — Persistance / Stockage
- **[mineur] LittleFS sans checksum/versioning** : Contrairement à qwen3.6 qui utilisait un magic number EEPROM pour valider l'intégrité de la configuration, ici le fichier `/config.json` est écrit directement sans vérification d'intégrité. Une coupure pendant l'écriture corromprait le fichier JSON. Le fallback "charger les defaults" compense partiellement mais perd toute configuration personnalisée. Moins robuste que qwen3.6 (EEPROM magic) pour la détection de corruption.
- **[mineur] `saveConfig()` écrit sans verification d'erreur** : Si l'écriture LittleFS échoue (filesystem plein, corruption), le code retourne `false` mais ne met pas à jour `saveRequested` → l'utilisateur peut relancer la sauvegarde indéfiniment sans succès. Pas de feedback UI sur l'échec de write.

### Catégorie 10 — Documentation hardware
- **[positif mineur : aucun négatif notable]** — La documentation est exceptionnelle, probablement la meilleure parmi tous les compétiteurs. Le `SCHEMATIC.md` contient des ASCII art détaillés, des calculs de dimensionnement, une BOM complète, un plan de brochage et une procédure de test.

### Catégorie 11 — Qualité de code générale
- **[critique] Macro `GPOS`/`GPOC` non définies dans le fichier** : Le code utilise `GPOS = (1UL << SIMCHA_PIN)` et `GPOC = (1UL << SIMCHB_PIN)` mais ces macros ne sont ni incluses ni définies explicitement dans le `.ino`. Sur ESP8266 Arduino core, `GPOS` est en fait un **alias de registre** (`#define GPOS (*(&GPIO_OUT_SET0) + 0)`) — il s'agit d'un symbole du SDK ESP8266 non-Arduino. Il devrait être accessible car `ESP8266WiFi.h` inclut les headers du SDK qui définissent ces symboles. **Cependant**, utiliser des symboles bare-metal du SDK sans include explicite n'est pas portable et peut causer des erreurs de compilation selon la version du core Arduino ESP8266 utilisée. Risque modéré.
- **[majeur] `DynamicJsonDocument` au lieu de `StaticJsonDocument` pour les GET handlers** : Les handlers GET (`handleConfigGet`, `handleReadings`) créent un `DynamicJsonDocument` en heap, puis sérialisent dans un buffer statique sur la pile. C'est une allocation heap intermédiaire évitable avec un `StaticJsonDocument<384>` qui ferait tout sur la stack (zéro heap). Le commentaire FIX #3 mentionne "zero heap alloc" mais `DynamicJsonDocument` alloue lui-même en heap. Contre-productif : le buffer final est zero-heap mais la doc JSON l'est pas.
- **[majeur] Coefficient d'exponential smoothing non paramétrable** : `SMOOTHING_FACTOR = 0.3f` et le second `0.5f` sont des constantes compilées en dur. Un utilisateur ne peut pas ajuster le lissage sans recompiler. Pour un onduleur Deye avec des comportements de tolérance variables selon les modèles, cette rigidité est limitante.
- **[majeur] Variable `restartTime` dans loop() non initialisée avant utilisation** : `static bool wasSet = false; if (wasSet && !deferredRestartRequested) restartTime = 0;` — la variable `restartTime` (déclarée plus haut comme `static unsigned long restartTime = 0;`) est utilisée ici mais c'est le même scope. En fait, elle est bien initialisée à 0 au début. Ce n'est PAS un bug. C'est correct.

---

## Synthèse

**Ce projet est la version améliorée de qwen3.6 avec les corrections principales apportées.** Le plus gros bug de qwen3.6 (`digitalWrite()` dans l'ISR Timer1 → crash ESP8266) a été corrigé en utilisant les registres GPIO directs (`GPOS`/`GPOC`). L'ajout d'OTA, mDNS, exponential smoothing et JSON zero-heap représente une évolution significative. La documentation hardware est exceptionnelle (schémas détaillés, BOM complète, calculs de dimensionnement).

**Cependant, plusieurs limitations persistent :**

1. **Ticker @20kHz en software** — overhead CPU constant (~10%), moins précis qu'un hardware timer direct comme chez qwen3.6
2. **Pas de validation des ratios côté serveur** — un POST brute peut envoyer n'importe quelle valeur
3. **LittleFS sans checksum/versioning** — corruption possible du fichier config
4. **Pas de watchdog** — aucun reboot automatique en cas de crash

### Points forts réels
- `GPOS`/`GPOC` directs dans l'ISR → correct sur ESP8266 (contrairement à qwen3.6 qui utilisait `digitalWrite()`)
- Architecture « deux mondes » clairement documentée (ISR temps-réel vs loop best-effort)
- Triple filtrage : moyenne glissante + IIR smoothed + IIR target → lissage réel sur périodes cycliques
- LittleFS + ArduinoJSON v7 pour persistance config complète
- JSON zero-allocation GET handlers via buffer stack + serializeJson vers char[]
- HTML en PROGMEM → économie ~5 KB SRAM
- Overclock CPU 160 MHz documenté et appliqué
- OTA intégré (port 8266) — absent chez tous les autres compétiteurs
- mMDNS inclus — découverte réseau Zeroconf possible
- Mot de passe AP valide WPA2 (`"deyetach1"`) + masquage password dans API config
- Cooldown 5s entre écritures Flash → protection usure LittleFS
- Reboot différé non-bloquant (fix #6) → garantie réponse HTTP avant reboot
- Documentation hardware exceptionnelle (BOM, schémas, calculs, plan de test)

### Faiblesses critiques
- Ticker @20kHz en software → overhead CPU ~10%, variabilité timing > hardware timer direct
- Pas de validation serveur des ratios POST → valeur abusive acceptée
- LittleFS sans checksum/versioning → corruption config possible
- Pas de watchdog (SW ou HW)
- `DynamicJsonDocument` en heap pour les GET handlers (contradiction avec claim "zero heap")
- Reboot différé seulement 200ms → risque timeout WiFi chargé
- API web non authentifiée
- mDNS inclus mais `MDNS.begin()` non appelé → pas fonctionnel tel quel
- Triple lissage → latence ~200 ms pour stabilisation du signal

### Comparaison avec les autres compétiteurs

| Aspect | gemma4 (3.2) | qwen35 (4.8) | qwen3.6 (7.2) | gemini-free (5.3) | gpt-free (7.5) | **qwen3.6_prompt_from_claude (8.2)** |
|--------|-------------|-------------|---------------|-------------------|----------------|--------------------------------------|
| ISR IRAM_ATTR | ❌ Absent | ✅ Présent | ✅ Présent | ✅ Sur 3 ISRs | ✅ Sur 2 ISRs | ✅ Sur 2 ISRs |
| GPIO écriture ISR | N/A (jamais basculé) | ⚠️ GPOS non défini | ⚠️ digitalWrite() **CRITIQUE** | ⚠️ digitalWrite() **CRITIQUE** | ⚠️ External header missing | ✅ **GPOS/GPOC direct** |
| Timer/Sortie | ❌ Ticker SW | ⚠️ Sortie fixe 1 Hz | ⚠️ Hardware timer 1 µs | ✅ Waveform gen 50 µs | ⚠️ Depende external header | ⚠️ **Ticker @20kHz (software)** |
| Signal D5/D6 | ❌ Jamais | ⚠️ 1 Hz fixe | ✅ Proportionnel RPM | ✅ Proportionnel RPM | ✅ Proportionnel RPM | ✅ Proportionnel RPM |
| Filtres signal | ❌ Aucun | ⚠️ Fenêtre (faux) | ⚠️ Anti-rebound ISR | ❌ Aucun | ✅ IIR 7/8+1/8 | ✅ **Moyenne glissante + double IIR** |
| Persistance | ❌ Aucune | ❌ SPIFFS cassé | ✅ EEPROM magic | ✅ LittleFS JSON | ✅ EEPROM magic | ✅ **LittleFS JSON** |
| Ratio Web UI | ❌ Inopérant | ✅ Fonctionnel | ✅ Formulaire POST | ✅ Formulaire POST | ✅ AJAX fetch | ✅ AJAX fetch + validation client |
| Docs hardware | ✅ Correcte | ⭐ Exceptionnelle | ⭐ Exceptionnelle | ❌ Aucune | ❌ Très limitée | ⭐ **Exceptionnelle** |
| OTA | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ **Intégrée** |
| mDNS | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ **Inclus** (non activé) |
| AP password | ⚠️ 12345678 | ⚠️ 12345678 | ❌ Ouvert | ⚠️ 12345678 | ✅ DeyeFan123 | ✅ **deyetach1 (WPA2)** |
| JSON HEAP safe | ❌ String concat | ❌ String 10Hz | ✅ OK | ✅ StaticJsonDocument | ❌ String concat | ⚠️ DynamicJson + char[] mixte |
| Bugs critiques | 6+ | 3 | 2 | 2 | 1 (header) | **~1** (GPOS bare-metal SDK symbol) |

### Verdict
**Version la plus aboutie de la série qwen3.6.** Les corrections principales apportées par Claude (`GPOS`/`GPOC` au lieu de `digitalWrite`, OTA, mDNS, exponential smoothing, JSON zero-allocation GET) transforment un projet avec 2 bugs critiques en un projet fonctionnel à quasi-production-ready. Le seul bug technique persistant est l'utilisation des symboles bare-metal SDK (`GPOS`/`GPOC`) sans include explicite — risque de compilation selon la version du core Arduino ESP8266. L'approche Ticker @20kHz (software) au lieu d'un hardware timer direct est un compromis acceptable mais introduit ~10% d'overhead CPU et une variabilité timing supérieure à un timer matériel configuré directement. **Avec l'ajout d'un watchdog et la validation serveur des ratios, ce projet atteindrait un score de 8,5-9,0/10.**
