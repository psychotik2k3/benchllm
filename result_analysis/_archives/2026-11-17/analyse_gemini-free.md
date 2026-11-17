# Analyse — gemini-free

**Source :** `competitors/gemini-free/gemini-code-1787264142602.cpp`  
**Fichiers analysés :** 1 fichier source C++ (.cpp)  
**Score global : 5.3 / 10**  
**Score qualité de code : 5.0 / 10**

---

## ✅ Positifs techniques

### Catégorie 1 — Temps-réel / ISR
- **[POSITIF] `IRAM_ATTR` sur les trois ISRs** (`handleTachIn1`, `handleTachIn2`, `onTimerISR`) — tous les handlers d'interruption seront servis depuis la RAM IRAM. Contrairement à gemma4 qui manquait cet attribut, gemini-free le respecte pour toutes les ISR.
- **[POSITIF] Anti-rebound par timestamp micros()** : Comparaison avec un intervalle minimum de `5000 µs` (5 ms). Les fronts trop rapprochés sont rejetés dans l'ISR elle-même. Approche correcte, bien que le seuil soit assez large (voir négatifs).
- **[POSITIF] Handlers ISR minimaux et non-bloquants** : Un calcul de différence de timestamp et une assignment conditionnelle. Pas de Serial.print ni allocations.

### Catégorie 2 — Démarrage / Boot
- **[POSITIF] LittleFS initialisé avant usage** : `LittleFS.begin()` appelé en début de `setup()`, puis `loadConfig()` lit la configuration persistée. Ordre correct des opérations.
- **[POSITIF] Configuration WiFi lue depuis LittleFS** : SSID et mot de passe récupérés du fichier JSON au démarrage. Fallback élégant (si pas de config, mode AP uniquement).

### Catégorie 3 — Génération sortie / Timer1
- **[POSITIF] Sortie sur D5 ET D6 entièrement implémentée** : Deux sorties physiques (GPIO5/D1 et GPIO4/D2) gérées par un seul ISR Timer1. Contrairement à gemma4 (jamais basculé) et qwen35 (fréquence fixe 1 Hz), ici la fréquence de toggle est proportionnelle au RPM × ratio mesuré en temps réel.
- **[POSITIF] Calcul dynamique dans `loop()`** : `targetOutHalfPeriod1` et `targetOutHalfPeriod2` sont recalculés à chaque itération de boucle depuis les RPM réels × ratios. Le signal simulé répond dynamiquement aux changements de régime.
- **[POSITIF] Arrêt de sortie quand fan à l'arrêt** : Quand `pulseInterval == 0`, le `targetOutHalfPeriod` est mis à 0 et la sortie forcée LOW. Comportement correct pour un tach simulé : niveau bas statique = ventilateur arrêté.

### Catégorie 4 — Fidélité protocole / Fail-safe
- **[POSITIF] Détection d'arrêt de fan** : `if (now - lastPulseTime > 1000000)` détecte l'absence de pulse pendant 1 seconde et remet tout à zéro (RPM, ratio, target). Le système passe automatiquement en mode "fan stopped" sans intervention externe.
- **[POSITIF] Formule RPM correcte** : `rpmIn = (60.0 * 1000000.0) / (pulseInterval * 2.0)` suppose 2 pulses par tour de ventilateur, ce qui est standard pour les tach Noctua. Formule physiquement correcte.
- **[POSITIF] FORMULE DE RATIO appliquée correctement** : `targetOutHalfPeriod = (15000000.0 / rpmOut)` où `rpmOut = rpmIn * ratio`. La relation mathématique est correcte pour un signal carré 50% duty cycle.

### Catégorie 6 — WiFi / Réseau
- **[POSITIF] Mode AP+STA simultanée** : `WiFi.mode(WIFI_AP_STA)` avec softAP configuré en priorité, puis tentative de connexion STA si credentials disponibles. Flexibilité de déploiement.
- **[POSITIF] Bibliothèque ESPAsyncWebServer utilisée** : Web serveur asynchrone qui ne bloque pas le thread principal pour le traitement des requêtes HTTP. Meilleure réactivité qu'un serveur synchrone classique.

### Catégorie 7 — Gestion mémoire / HEAP
- **[POSITIF] ArduinoJson StaticJsonDocument utilisé** : `StaticJsonDocument<512>` pour la config et `StaticJsonDocument<256>` pour les réponses JSON. **Zéro allocation dynamique heap** lors du sérialisation/désérialisation. C'est un excellent choix anti-fragmentation sur ESP8266, bien supérieur aux String concatenations de gemma4 et qwen35.
- **[POSITIF] `serializeJson()` direct dans variables locales** : Même pattern dans `/data` et `/config` endpoints — une doc locale créée et détruite par requête, pas de concatenation String cumulative.

### Catégorie 8 — Interface Web
- **[POSITIF] Design dark theme moderne** : Fond `#121212`, cartes `#1e1e1e`, accents cyan `#00bcd4`, valeurs RPM en vert `#4caf50`. Rendu visuel soigné et cohérent. CSS responsive avec viewport meta tag.
- **[POSITIF] Formulaire ratio fonctionnel** : `<form action="/set-ratios" method="POST">` avec inputs number step 0.1. Endpoint POST `/set-ratios` lit les paramètres, met à jour ratios, appelle `saveConfig()` puis redirect. Flux complet et fonctionnel.
- **[POSITIF] Formulaire WiFi séparé** : `/set-wifi` avec SSID + password, reconnexion automatique via `ESP.restart()` après sauvegarde. UX soignée.
- **[POSITIF] AJAX JSON pour le monitoring** : `setInterval(fetch('/data'), 1000)` rafraîchit les valeurs RPM côté client sans recharger la page. Flux de données en temps réel fluide.

### Catégorie 9 — Persistance / Stockage
- **[POSITIF] LittleFS pour configuration complète** : Ratios + WiFi credentials persistés dans `/config.json` sur le système de fichiers. Persistant au reboot/redémarrage, bien supérieur à EEPROM non utilisée de qwen35 ou absente de gemma4.
- **[POSITIF] `StaticJsonDocument<512>` pour chargement** : Lecture asynchrone avec vérification d'erreur `DeserializationError`. Lecture robuste qui ne crash pas si le fichier est corrompu.

---

## ❌ Négatifs techniques

### Catégorie 1 — Temps-réel / ISR
- **[critique] `digitalWrite()` dans l'ISR Timer1** : `onTimerISR()` appelle `digitalWrite(PIN_TACH_OUT_1, ...)` et `digitalWrite(PIN_TACH_OUT_2, ...)`. Sur ESP8266, `digitalWrite()` n'est **pas** marquée `IRAM_ATTR` — elle effectue un lookup de cache pour mapper le numéro de pin vers le registre GPIO. Si la page flash contenant `digitalWrite` est évictée pendant que l'ISR Timer1 tourne, cela provoque un **Guru Meditation Fault**. C'est un bug critique exactement comme chez qwen3.6. Solution : utiliser les registres GPIO directs (`GPIO_OUT_W1TS` / `GPIO_OUT_W1TC`).
- **[majeur] Pas de section critique pour les variables partagées ISR↔loop** : `loop()` lit `pulseInterval1`, `lastPulseTime1`, `targetOutHalfPeriod1` etc. qui sont modifiés par ISRs sans `noInterrupts()`/`interrupts()`. Sur ESP8266, une lecture 4-byte est théoriquement atomique, mais pas garantie pour les lectures multiples séquentielles (ex: lire `lastPulseTime1` puis `pulseInterval1` pourrait capturer un interrupt entre les deux lectures).
- **[mineur] Anti-rebound trop large (5 ms = ~12 RPM minimum)** : Le seuil de `diff > 5000 µs` signifie que tout pulse arrivant dans les 5 ms suivant le précédent est ignoré. Pour un fan à 1000 RPM avec 2 pulses/rev, la période est ~300 000 µs — largement au-dessus du seuil. Mais à très bas régime (< 12 RPM), les pulses réels seraient perdus. Un seuil de 80-100 µs (comme qwen3.6) serait plus précis et n'affecterait aucun fan réel.

### Catégorie 2 — Démarrage / Boot
- **[critique] Pas de LittleFS dans `setup()` avant utilisation** : La fonction `loadConfig()` vérifie `LittleFS.exists("/config.json")`, mais il n'y a **aucun appel à `LittleFS.begin()` dans `setup()`**. Attend... en fait, si — `LittleFS.begin();` est bien appelé dans `setup()`. Donc ce n'est pas un bug. Cependant :
- **[majeur] Mot de passe AP faible par défaut** : `WiFi.softAP("Deye-Fan-Simulator", "12345678")` — mot de passe trivial, même problème que gemma4 et qwen35.
- **[mineur] Pas de watchdog (SW ou HW)** : Aucun hardware ou software watchdog implémenté. En cas de plantage WiFi ou boucle infinie, aucun reboot automatique.
- **[critique] `setup()` n'appelle pas `LittleFS.begin()` AVANT `loadConfig()`** : Attendez, j'ai relus le code — `LittleFS.begin();` est bien présent dans `setup()`, avant `loadConfig()`. Donc ce point est **non applicable**. C'est corrigé.
- **[mineur] Pas de boot log sur Serial** : Impossible de diagnostiquer le démarrage en absence d'accès réseau (pas de message Serial.print pour confirmer les étapes d'initialisation).

### Catégorie 3 — Génération sortie / Timer1
- **[critique] Résolution du timer très faible (50 µs par tick)** : Le pas de compteur est `timerStepUs = 50` µs. À haute vitesse (ex: 4000 RPM, ratio 2.0 → rpmOut ≈ 8000), la demi-période cible est de ~1875 µs = **37,5 ticks**. Le rounding à 37 ou 38 ticks introduit une erreur de ±1.3%. À plus basse vitesse (ex: 1000 RPM × 2,0 → rpmOut 2000), la demi-période est ~7500 µs = **150 ticks**, erreur réduite à ±0.3%. Le problème principal est aux hautes vitesses où l'erreur relative devient significative. **Comparé à qwen3.6 qui utilisait un timer 1µs de résolution, la précision est nettement inférieure.**
- **[critique] Formule du target half-period ne gère pas les cas extrêmes** : `targetOutHalfPeriod = (15000000.0 / rpmOut)` — si rpmOut atteint un très haute valeur (fan Noctua à ~3000 RPM × ratio 4,67 → ~14000 RPM simulés), la demi-période descend à ~1071 µs = **21 ticks**. En dessous de ~50 µs (1 tick), le timer ne peut plus représenter la fréquence. Aucun clamp maximum sur rpmOut n'est appliqué.
- **[majeur] `timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE)` avec mode SINGLE** : Le mode `TIM_SINGLE` signifie que le timer génère **un seul interrupt** puis s'arrête. Pour un comptage continu, il faudrait `TIM_SINGLE` n'est pas correct ici — on veut un mode périodique (comme `TIM_LOOP` ou `TIM_EDGE`). Cependant, `timer1_write(4000)` dans setup() et `timer1_write(80 * timerStepUs)` dans l'ISR pourraient compenser en reprogrammant manuellement le timer à chaque interruption. Si c'est le cas, cela fonctionne mais est fragile (voir race condition ci-dessous).

### Catégorie 4 — Fidélité protocole / Fail-safe
- **[majeur] Détection d'arrêt de fan basée sur un seuil fixe arbitraire** : `now - lastPulseTime > 1000000` (1 seconde) est le seul critère. Si un fan tourne à ~60 RPM, la période entre pulses serait de ~500 ms (avec 2 pulses/rev). En 1 seconde, il y a normalement 2 pulses. Le seuil de 1 seconde est donc raisonnable pour détecter un arrêt mais pourrait déclencher faux-positivement si le fan tourne très lentement (~30 RPM et moins). Pas critique mais pas robuste face à tous les cas d'usage.
- **[mineur] Pas de validation des ratios** : Les ratios peuvent être envoyés n'importe quelle valeur via `/set-ratios` (pas de borne min/max). Un ratio de 0,001 ou 1000 serait accepté sans vérification, entraînant des fréquences de sortie absurdes.
- **[mineur] Pas de validation du WiFi password** : Le champ password dans `/set-wifi` accepte n'importe quelle chaîne vide ou très courte. Un SSID/passepartout pourrait rendre l'appareil inutilisable si les credentials sont invalides et bloquent la reconnexion STA.

### Catégorie 5 — Filtres / Signal
- **[majeur] Pas de filtre temporel sur les RPM mesurés** : Contrairement à qwen35 (fenêtre glissante) ou qwen3.6 (anti-rebound ISR), gemini-free calcule `rpmIn1` directement à partir du dernier intervalle mesuré sans aucun lissage temporel. Les variations de régime d'un fan réel (turbulence, démarrage/arrêt) se traduiront directement par des sauts de fréquence sur la sortie simulée → jitter audible et potentiellement problématique pour l'onduleur Deye.
- **[majeur] Pas de filtre IIR ou EMA** : Aucun algorithme de lissage progressif. Chaque variation de période tach est immédiatement répercutée sur la sortie via le recalcul `targetOutHalfPeriod` dans `loop()`. Moins smooth que qwen35 (même si mal implémenté) et qwen3.6 (qui avait au moins l'anti-rebound ISR).
- **[mineur] Anti-rebound seul comme mécanisme de filtrage** : Le seuil de 5 ms est le seul filtre sur les signaux d'entrée. Les rebonds à plus de 5 ms ou les spikes électriques entre pulses seraient comptés comme valides.

### Catégorie 6 — WiFi / Réseau
- **[majeur] Pas d'authentification web** : N'importe qui connecté au réseau (AP ou STA) peut accéder à `/set-ratios` et modifier les ratios, ou `/set-wifi` pour changer le WiFi. Aucune protection par mot de passe sur le serveur web.
- **[majeur] Reconnexion bloquante via `ESP.restart()` dans `/set-wifi`** : Après avoir sauvegardé un nouveau SSID/password, le code fait `ESP.restart()`. Pendant le redémarrage (~3 secondes), la sortie simulée s'arrête complètement (GPIO LOW). Si l'onduleur Deye détecte ce gap comme un "fan stopped" (timeout < 3s), il pourrait entrer en mode erreur ou coupure. C'est moins pire que qwen3.6 qui bloque pendant ~10s de reconnexion WiFi sans restart, mais le restart lui-même crée une interruption brutale du signal.
- **[majeur] Reconnexion STA non bloquante mais pas de retry avec backoff** : `WiFi.begin(wifiSSID, wifiPass)` est appelé une seule fois au boot. Si la connexion échoue (mauvais password, AP indisponible), le device reste en mode AP uniquement sans nouvelle tentative automatique.
- **[mineur] Pas de support mDNS / Zeroconf** : Aucun nom de domaine local (ex: `deye-fan.local`). L'utilisateur doit connaître l'IP du STA ou aller sur 192.168.4.1 en mode AP.
- **[mineur] Pas d'OTA (Over-The-Air)** : Mises à jour firmware uniquement par câblage USB.

### Catégorie 7 — Gestion mémoire / HEAP
- **[positif mineur] Aucune fragmentation String** : L'utilisation de `StaticJsonDocument` garantit zéro allocation heap pour le JSON. La seule allocation dynamique restante provient de la page HTML (stockée en PROGMEM, donc pas d'impact) et des buffers TCP du web server asynchrone.
- **[mineur] String locale dans les handlers JSON** : `String response; serializeJson(doc, response);` — une allocation String par requête JSON, puis destruction immédiate. Acceptable mais moins optimal qu'un passage direct au client via callbacks.

### Catégorie 8 — Interface Web
- **[positif mineur : aucun négatif majeur]** — L'interface est fonctionnelle, moderne et cohérente. Les seuls points faibles sont mineurs.
- **[mineur] Pas de bouton "Refresh" manuel** : Le monitoring se fait uniquement via le `setInterval` automatique (1s). Pas de contrôle utilisateur pour rafraîchir manuellement les données.
- **[mineur] Pas d'indicateur de statut WiFi** : L'interface n'affiche pas le RSSI, l'IP ou le mode (AP/STA) en cours. L'utilisateur ne sait pas si l'appareil est connecté au réseau STA.

### Catégorie 9 — Persistance / Stockage
- **[positif mineur : aucun négatif majeur]** — LittleFS est bien implémenté avec chargement/sauvegarde JSON robustes. Point fort de ce projet par rapport à gemma4 (aucune persistance) et qwen35 (SPIFFS non initialisé).
- **[mineur] `saveConfig()` n'utilise pas de checksum** : Le fichier `/config.json` est écrit directement sans vérification d'intégrité. Une coupure pendant l'écriture corromprait le fichier JSON, rendant la configuration illisible au prochain boot. Aucun mécanisme de versioning ou checksum (comme le magic number 0xDE 0xAD de qwen3.6) pour détecter la corruption.

### Catégorie 10 — Documentation hardware
- **[critique] Aucune documentation hardware** : Le dossier ne contient qu'un seul fichier source C++. Pas de README, pas de schéma électronique, pas de liste de composants, pas d'instructions de montage. Contrairement à tous les autres compétiteurs (gemma4, qwen35, qwen3.6) qui avaient au minimum un README ou un schéma, gemini-free n'a **aucune documentation**. C'est un manque important pour un projet hardware DIY.
- **[critique] Aucune explication du PINOUT dans le code** : Les définitions `#define` donnent les numéros de pin mais aucune indication sur la logique du circuit (NPN, pull-up, etc.). Un utilisateur ne saurait pas comment câbler physiquement les signaux vers l'onduleur Deye.

### Catégorie 11 — Qualité de code générale
- **[critique] `digitalWrite()` dans ISR Timer1** : Exactement le même bug que chez qwen3.6. Remplacer par accès directs aux registres GPIO (`GPIO_OUT_W1TS` / `GPIO_OUT_W1TC`) est nécessaire pour un fonctionnement fiable sur ESP8266.
- **[majeur] Formule RPM divise par 2 pour "pulses par tour"** : `rpmIn = (60.0 * 1000000.0) / (pulseInterval * 2.0)` — le code suppose toujours 2 pulses par tour de ventilateur, ce qui est cohérent pour les tach Noctua standard. Cependant, cette constante "2" n'est pas nommée (ex: `#define PULSES_PER_REV 2`) et serait facilement erronée si un fan différent était utilisé.
- **[majeur] Timer step hardcoded en dur** : `timerStepUs = 50` est défini dans l'ISR comme une variable locale `static`. Ce n'est ni const, ni #define, ni global. Rend le paramétrage difficile pour un futur mainteneur.
- **[mineur] Variables globales sans scope** : Toutes les variables sont au scope global (acceptable pour un projet ESP8266 de cette taille).
- **[critique] Pas de `#include <Arduino.h>`** : Bien que `<ESP8266WiFi.h>` l'inclue implicitement, ce n'est pas standard ni portable. La macro `IRAM_ATTR` provient d'Arduino et sans include explicite, la portabilité vers d'autres plateformes (esp32, avr) serait cassée.
- **[mineur] LED de statut basée sur millis()** : `digitalWrite(PIN_LED_STATUS, (millis() / 250) % 2)` — le clignotement est calculé directement dans le code sans utilisation de Ticker ou ISR dédiée. Correct fonctionnellement mais moins élégant qu'un gestionnaire dédié.

---

## Synthèse

**Ce projet est fonctionnel sur le plan architecturel mais souffre de limitations techniques notables.** La structure de base (ISR entrée + Timer1 sortie + web server asynchrone) est correcte, la persistance LittleFS avec ArduinoJson est bien implémentée, et l'interface web dark theme est moderne et fonctionnelle. Cependant, **trois bugs majeurs empêchent un déploiement production-ready** :

1. **`digitalWrite()` dans l'ISR Timer1** → crash ESP8266 aléatoire (exactement comme qwen3.6).
2. **Aucune documentation hardware** → pas de schéma, pas de liste de composants, impossible de monter physiquement l'appareil sans deviner le câblage.
3. **Résolution du timer trop faible (50 µs)** → précision médiocre à haute vitesse, jitter sur la sortie simulée.

### Points forts réels
- Timer1 matériel pour génération dynamique proportionnelle au RPM × ratio
- ArduinoJson StaticJsonDocument → zéro fragmentation HEAP (anti-string concatenation)
- LittleFS avec chargement/sauvegarde JSON robustes + persistence au reboot
- Interface web dark theme moderne et fonctionnelle (AJAX, formulaires POST)
- Formules RPM physiquement correctes (2 pulses/rev standard Noctua)
- Détection d'arrêt de fan automatique (1s timeout)
- ESPAsyncWebServer → non-bloquant pour le traitement HTTP

### Faiblesses critiques
- `digitalWrite()` dans ISR Timer1 → crash aléatoire ESP8266
- **Aucune documentation hardware** (README, schéma, composants) — dossier vide de tout document technique
- Résolution timer 50 µs (vs 1 µs chez qwen3.6) → erreur significative à haute RPM
- Anti-rebound 5 ms trop large pour des applications basse fréquence
- Pas de filtre temporel IIR/EMA sur les RPM mesurés → jitter sortie
- Pas d'authentification web, pas de mDNS, pas d'OTA
- Mot de passe AP faible par défaut ("12345678")
- `saveConfig()` sans checksum → fichier corrompu possible

### Comparaison avec les autres compétiteurs

| Aspect | gemma4 (3.2) | qwen35 (4.8) | qwen3.6 (7.2) | **gemini-free (5.3)** |
|--------|-------------|-------------|---------------|----------------------|
| ISR IRAM_ATTR | ❌ Absent | ✅ Présent | ✅ Présent | ✅ Présent sur 3 ISRs |
| Timer1 matériel | ❌ Ticker SW | ⚠️ Sortie fixe 1 Hz | ✅ Hardware 1 µs | ✅ Hardware 50 µs |
| Signal D5/D6 | ❌ Jamais implémenté | ⚠️ Fréquence fixe 1 Hz | ✅ Proportionnel RPM | ✅ Proportionnel RPM |
| Persistance | ❌ Aucune | ❌ Aucune (SPIFFS cassé) | ✅ EEPROM mag number | ✅ LittleFS JSON |
| Ratio Web UI | ❌ Inopérant | ✅ Fonctionnel | ✅ Formulaire fonctionnel | ✅ Formulaire POST fonctionnel |
| Filtres signal | ❌ Aucun | ⚠️ Fenêtre glissante (faux) | ⚠️ Anti-rebound ISR | ⚠️ Anti-rebound 5 ms seulement |
| Docs hardware | ✅ Correcte | ⭐ Exceptionnelle | ⭐ Exceptionnelle | ❌ **Aucune** |
| Mémoire HEAP | ❌ String concat | ❌ String concat 10Hz | ✅ OK (server.send direct) | ✅ StaticJsonDocument |
| Bugs critiques | 6+ | 3 | 2 | 2+ |
| WiFi password AP | ⚠️ 12345678 | ⚠️ 12345678 | ❌ Ouvert | ⚠️ 12345678 |

### Verdict
**Architecture correcte mais exécution inégale.** gemini-free combine les meilleures idées de ses concurrents (Timer1 matériel de qwen3.6 + StaticJsonDocument anti-fragmentation + web moderne) dans un code compact et fonctionnel. Cependant, le manque total de documentation hardware est rédhibitoire pour un projet DIY ESP8266 — sans schéma ni liste de composants, personne ne peut reproduire physiquement l'appareil. Les bugs `digitalWrite` ISR et résolution timer 50 µs sont corrigeables mais nécessitent des modifications structurelles. **Avec documentation complète + correction digitalWrite + réduction du timer step à 1 µs, ce projet pourrait atteindre un score de 7,0/10.**
