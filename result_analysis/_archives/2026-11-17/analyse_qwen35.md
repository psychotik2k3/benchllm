# Analyse — qwen35

**Source :** `competitors/qwen35/firmware_main.cpp` + `README.md` + `calibration_guide.md` + `SUMMARY.md` + `INDEX.md`  
**Fichiers analysés :** 1 fichier source C++ (.cpp), 4 fichiers documentation (.md)  
**Score global : 4.8 / 10**  
**Score qualité de code : 3.2 / 10**

---

## ✅ Positifs techniques

### Catégorie 1 — Temps-réel / ISR
- **[POSITIF] `IRAM_ATTR` présent sur les deux ISRs** (`I_9cm` et `I_6m`) — contrairement à gemma4, le code ISR sera correctement servi depuis la RAM IRAM, évitant les Guru Meditation crash pendant l'activité WiFi. Point critique résolu.
- **[POSITIF] Anti-rebound fonctionnel** : Comparaison de timestamp micros() avec seuil `DEBOUNCE_MIN_US = 1000µs` (1 ms). Les fronts trop rapprochés sont ignorés, éliminant les faux comptages par rebond de contact ou bruit électrique. Plus précis que gemma4 qui n'avait aucun mécanisme.
- **[POSITIF] Handlers ISR minimaux et non-bloquants** : Incrément conditionnel d'un volatile + assignment de `rpm9_raw`, pas de Serial.print ni allocations.
- **[POSITIF] Variables timestamp edge volatiles** (`lastEdge9Us`, `lastEdge6Us`) accessibles depuis ISR et main loop, bien marquées `volatile`.

### Catégorie 2 — Démarrage / Boot
- **[POSITIF] Boot log détaillé sur Serial** : En-tête ASCII art, messages de confirmation à chaque étape (CPU, interrupts, WiFi), IP AP affichée. Facilité de diagnostic en cas de problème.
- **[POSITIF] `delay(200)` après Serial.begin** : Délai stabilisant la liaison série, bonne pratique pour éviter les caractères perdus au tout début.
- **[POSITIF] Fallback AP élégant** : Tentative de connexion STA si SSID renseigné, fallback automatique en mode AP si échec.

### Catégorie 3 — Génération sortie / Timer1
- **[POSITIF] Sortie sur D5 implémentée dans la boucle de lissage** (`updateSmoothedRPM()`) : Le pin est toggling via GPOS/GPOC à chaque tick du Ticker (250 ms). Contrairement à gemma4, le code tente bien d'implémenter la génération de signal.
- **[POSITIF] Approche hardware-optimized pour la sortie** : Utilisation présumée de registres GPOS/GPOC pour toggle en 1 cycle CPU (si la macro est correctement définie).

### Catégorie 5 — Filtres / Signal
- **[POSITIF] Lissage par fenêtre glissante** : 4 échantillons sur une période de 1 seconde, moyenne arithmétique. Plus sophistiqué qu'un simple comptage brut.
- **[POSITIF] Multi-niveaux de filtrage** : Anti-rebound ISR (1 ms) + lissage temporel (fenêtre 4 échantillons). Double protection contre le bruit.

### Catégorie 6 — WiFi / Réseau
- **[POSITIF] Détection d'occupation WiFi** (`WiFi.transmitting()` / mécanisme `wifiBusy`) : Tentative de synchronisation des opérations critiques avec les cycles de transmission WiFi pour réduire le jitter. Approche originale, même si discutée.
- **[POSITIF] Mode AP+STA** : Flexibilité maximale — l'appareil peut fonctionner en standalone (AP) ou se connecter à un réseau existant (STA).

### Catégorie 8 — Interface Web
- **[POSITIF] Formulaire de ratio fonctionnel** : Les inputs `#v9` et `#v6` ont des écouteurs `change` qui envoient des requêtes `fetch('/config?channel=9&ratio=...')`. Contrairement à gemma4 où ces champs étaient décoratifs, ici ils sont connectés au backend.
- **[POSITIF] Validation client-side des ratios** : Les bornes (min 0, max 4.67) sont vérifiées avant envoi, évitant requêtes inutiles.
- **[POSITIF] Rafraîchissement haute fréquence (100ms)** + bouton refresh manuel : Interface réactive et interactive.
- **[POSITIF] Design CSS moderne avec variables** : Thème cohérent via `:root`, transitions smooth, cartes avec bordures colorées, feedback visuel (classe `.active`).

### Catégorie 9 — Persistance / Stockage
- **[POSITIF] Tentative d'exportation vers SPIFFS** (`exportHAAutomation()` utilise `SPIFFS.open()`) : Indique une intention de persistance. Néanmoins, SPIFFS n'est ni inclus ni initialisé (bug fonctionnel).

### Catégorie 10 — Documentation hardware
- **[POSITIF] Documentation exceptionnelle** : README structuré avec sections claires, tableaux de calibration détaillés (valeurs réelles Noctua NF-A9/NF-A6), scénarios d'usage multiples.
- **[POSITIF] Guide de calibration dédié** (`calibration_guide.md`) : Processus étape par étape, tableaux de correspondance RPM×ratio, méthodologie progressive. Niveau professionnel.
- **[POSITIF] Schéma de connexion documenté** dans le README et fichier `schematic.md` séparé.
- **[POSITIF] Section dépannage complète** : Problèmes courants avec causes probables et solutions.

### Catégorie 11 — Qualité de code générale
- **[POSITIF] Sections commentées clairement** : Délimitations par `// ===...===`, noms de constantes explicites (`PIN_TACH_9CM`, `RATIO_9CM_DEFAULT`). Bonne lisibilité.
- **[POSITIF] Documentation inline abondante** en français et anglais, expliquant le pourquoi de chaque décision technique.
- **[POSITIF] Architecture structurée** : ISR → Ticker callbacks → server handlers → setup/loop. Séparation claire des responsabilités.

---

## ❌ Négatifs techniques

### Catégorie 1 — Temps-réel / ISR
- **[critique] Algorithme de lissage incorrectement appliqué à des compteurs bruts** : `samples9[]` stocke `rpm9_raw` (qui vaut `count9`, un compteur cumulé croissant). Diviser un compteur cumulatif par 4 ne produit pas une moyenne significativement plus smooth — c'est essentiellement le même nombre. L'intention de lissage sur RPM est bonne mais l'implémentation applique la moyenne sur des compteurs, pas sur des fréquences.
- **[majeur] Aucune section critique pour les accès aux compteurs** : `updateSmoothedRPM()` lit `count9`, `count6`, `rpm9_raw`, `rpm6_raw` depuis un callback Ticker (non-ISR) mais ces variables sont modifiées par ISR. Sur ESP8266, une lecture 32-bit peut être atomique, mais ce n'est pas garanti. Il faudrait `noInterrupts()` / `interrupts()` autour des accès multiples.
- **[critique] Macro `GPOS()` non définie** : Le code utilise `GPOS(PIN_OUTPUT, !outState)` et `GPOS(PIN_LED, ledState)` mais la macro `GPOS` n'est ni déclarée ni définie nulle part dans le fichier source. Le compilateur Arduino ESP8266 ne connaît pas `GPOS`. Causant une erreur de compilation fatale. L'intention était probablement d'utiliser les registresdirects (`GPIO_OUT_W1TC` / `GPIO_OUT_W1TS`), mais cela nécessite un calcul manuel du bit à partir du pin number.

### Catégorie 2 — Démarrage / Boot
- **[critique] Pas de watchdog (SW ou HW)** : Aucun feed de hardware watchdog ni implémentation de software watchdog. En cas de plantage WiFi ou boucle infinie, aucun reboot automatique.
- **[majeur] Mot de passe AP faible par défaut** : `12345678` est un mot de passe trivial. Même si c'est un appareil local dédié, aucune protection ne peut être considérée comme une bonne pratique.
- **[critique] Code Home Assistant YAML inline inutilisable** : Le firmware contient un entier bloc YAML d'automatisation Home Assistant (`ha_automation_yaml`) avec `exportHAAutomation()` qui appelle `SPIFFS.open()`. SPIFFS n'est ni inclus ni initialisé (pas de `#include <SPIFFS.h>` ni `SPIFFS.begin()`). Cette section est du dead code qui ne compilera même pas (fonction appelée dans setup() mais non définie) et ajoute ~150 lignes de bruit.

### Catégorie 3 — Génération sortie / Timer1
- **[critique] Signal simulé à fréquence fixe, pas basé sur le RPM calculé** : `if (millis() - lastPulseMs > 500)` toggle la sortie à exactement 1 Hz (période de 1 seconde = 60 RPM), indépendamment du RPM mesuré. C'est un signal arbitraire qui n'a aucun rapport avec les RPM réels des ventilateurs. La formule mathématique (`simRPM = (rpm9_smoothed * ratio9 + rpm6_smoothed * ratio6) / 2.0f`) est calculée mais **jamais utilisée pour générer le train d'impulsions**.
- **[majeur] Pas de conversion RPM → fréquence d'impulsion** : Pour qu'un onduleur Deye lise correctement un signal simulé, il faut convertir `simRPM` en une durée réelle entre impulsions (ex: `duration_ms = 60000.0 / simRPM`). Le firmware ne fait cette conversion nulle part.
- **[majeur] Double toggle de sortie** : `updateSmoothedRPM()` contient un bloc de toggle (`if (millis() - lastPulseMs > 500) { GPOS... }`), ET il existe une fonction `toggleOutput()` redéfinissant exactement la même logique. Le toggle est appelé depuis le Ticker `calculationTicker`, mais s'il y avait un second ticker utilisant `toggleOutput()`, cela créerait un double-toggle désynchronisé.
- **[mineur] Période fixe 500 ms (1 Hz) non paramétrable** : Même si la logique RPM→fréquence était implémentée, le seuil `500` est en dur et ne reflète pas les spécifications Deye.

### Catégorie 4 — Fidélité protocole / Fail-safe
- **[critique] Pas de détection de fan stall (ventilateur arrêté)** : Si un ventilateur s'arrête alors que le compteur avait atteint une valeur non nulle, `dataValid9` et `dataValid6` ne sont jamais mis à jour à `true`. Les indicateurs restent donc toujours à `false`, ce qui signifie que la LED ne clignote JAMAIS (car `if (dataValid9 || dataValid6)` est toujours faux). Bug de logique qui rend l'indicateur LED inopérant.
- **[majeur] Pas de limite haute sur les compteurs** : `count9` et `count6` sont des `uint32_t`. En fonctionnement continu, ils finiront par overflow (à ~1 milliard d'impulsions ≈ plusieurs mois). Aucun mécanisme de reset n'est prévu.
- **[majeur] Pas de validation de la cohérence du signal** : Les compteurs cumulés `count9`/`count6` n'ont pas de limite maximale théorique. Un spike électrique pourrait faire compter un nombre disproportionné d'impulsions sans que le système ne détecte l'anomalie (pas de vérification `count > MAX_EXPECTED`).

### Catégorie 5 — Filtres / Signal
- **[majeur] Lissage sur compteurs cumulés au lieu de fréquences** : Comme noté plus haut, lisser des valeurs qui ne font qu'augmenter (`rpm9_raw = count9`) donne un résultat mathématiquement absurde. La moyenne arithmétique d'un compteur croissant n'a aucune signification physique.
- **[mineur] PAS de filtre IIR ou EMA** : Le filtre est une moyenne arithmétique simple (finie, pas recursive). Un filtre IIR (`alpha * nouveau + (1-alpha) * ancien`) serait plus léger en mémoire et plus lisse pour des transitions progressives.

### Catégorie 6 — WiFi / Réseau
- **[critique] `WiFi.transmitting()` n'est pas une méthode de la bibliothèque ESP8266WiFi** : Cette méthode n'existe pas dans le SDK ESP8266 Arduino. Le code va compiler (car c'est un appel direct sur l'objet, potentiellement ignoré par le linker) ou produire une erreur de compilation selon la version du SDK. Même s'il compile, il ne détectera PAS réellement les transmissions WiFi.
- **[majeur] Mécanisme `wifiBusy` inefficace et basé sur une API inexistante** : Puisque `WiFi.transmitting()` n'existe pas, tout le mécanisme de protection contre les interférences WiFi est inopérant. Le bloc dans `loop()` qui vérifie également `WiFi.transmitting()` souffre du même problème.
- **[mineur] Pas de support mDNS / Zeroconf** : Aucun nom de domaine local (ex: `deye-fan.local`). Difficile d'accéder à l'interface sans connaître l'IP attribuée.
- **[mineur] Pas de OTA (Over-The-Air)** : Mises à jour firmware uniquement par câblage USB.

### Catégorie 7 — Gestion mémoire / HEAP
- **[majeur] Construction JSON par String concatenation** : `handleData()` utilise `"{\"r9\":" + String(rpm9_smoothed) + ...` qui alloue dynamiquement à chaque requête `/data`. Avec un polling toutes les 100ms, c'est **10 allocations/seconde** de fragmentations HEAP. Sur ESP8266 (80 KB SRAM), cela conduit à une fragmentation rapide.
- **[mineur] Grande string HTML en SRAM** : La page web complète est stockée comme `const char*` en SRAM (~5 KB). L'utilisation de `PROGMEM` réduirait la pression mémoire et laisserait plus d'espace pour le WiFi buffer.

### Catégorie 8 — Interface Web
- **[majeur] Formule de calcul client-side hardcoded avec les ratios par défaut** : Dans le JavaScript, `updateDisplay()` calcule `rs9` et `rs6` en utilisant `lastR9 * 2.0` et `lastR6 * 2.5` (valeurs hardcodées) au lieu des ratios envoyés depuis le serveur. Si l'utilisateur change un ratio via `/config`, le calcul côté client affichera une valeur incorrecte tant qu'une nouvelle donnée `/data` ne rafraîchit pas.
- **[mineur] Pas de thème sombre** : Interface exclusivement en clair, bien que moderne.

### Catégorie 9 — Persistance / Stockage
- **[critique] Ratios non persistés** : `ratio9` et `ratio6` sont modifiés via `/config` mais uniquement en RAM volatile. Un reboot ou perte d'alimentation réinitialise les ratios aux valeurs par défaut (`2.0` et `2.5`). Aucune EEPROM, LittleFS, NVS ou autre mécanisme de stockage n'est activé.
- **[majeur] SPIFFS mentionné mais jamais initialisé** : Le code contient `exportHAAutomation()` qui utilise `SPIFFS`, mais il n'y a ni `#include <SPIFFS.h>`, ni `SPIFFS.begin()`, ni appel dans `setup()`. Dead code avec risque de compilation.

### Catégorie 10 — Documentation hardware
- **[positif mineur : aucun négatif notable]** — La documentation est l'un des points forts de ce projet, au même titre que les positifs listés ci-dessus.

### Catégorie 11 — Qualité de code générale
- **[critique] Macro `GPOS` non définie → compilation échouera** : Toutes les utilisations de `GPOS()` causeront une erreur de compilation. C'est le bug bloquant principal du projet.
- **[critique] Dead code Home Assistant (~150 lignes)** : Le bloc YAML + `exportHAAutomation()` + `displayHAInfo()` sont inclus dans le firmware principal mais ne servent à rien dans le contexte d'un contrôleur de tachymètre. L'automatisation HA est un sujet orthogonal et son inclusion indique une confusion des priorités du modèle.
- **[critique] `simRPM` calculé mais non utilisé pour la sortie** : La formule produit une valeur que personne ne lit (sauf envoi en JSON). Le pin D5 toggle à 1 Hz fixe, pas en fonction de `simRPM`. C'est un bug fonctionnel majeur : le cœur du projet est cassé.
- **[majeur] Incohérence de suffixes ISR** : `I_9cm` mais `I_6m` (pas `I_6cm`). Erreur de nommage récurrente.
- **[majeur] Fonction `toggleOutput()` non utilisée** : Redéfinit exactement la même logique que le bloc inline dans `updateSmoothedRPM()`. Code mort.
- **[majeur] String concaténation en JSON à 10 Hz** : Chaque appel à `handleData()` provoque une allocation heap. Avec polling toutes les 100ms, le HEAP se fragmente en quelques minutes d'exécution continue.
- **[mineur] Variables globales sans scope** : Toutes les variables sont au scope global (acceptable pour un projet ESP8266 de cette taille mais manque de rigueur).
- **[critique] Boucle principale (`loop()`) ne fait presque rien** : `server.handleClient()` + une vérification redondante de `WiFi.transmitting()` (qui n'existe pas) + `yield()`. Toute la logique métier est dans les callbacks Ticker, ce qui est correct architecturalement mais la loop elle-même ne contient aucune gestion d'erreurs, aucun heartbeat, aucun mécanisme de récupération.

---

## Synthèse

**Ce projet est ambitieux mais souffre de bugs bloquants critiques.** Le code structure correctement l'architecture (ISR + Ticker + Web Server), intègre des mécanismes intéressants (anti-rebound, fenêtre glissante, wifi busy detection) et possède une documentation professionnelle. Cependant, **trois bugs majeurs rendent le firmware non-fonctionnel tel quel** :

1. **`GPOS()` n'existe pas** → compilation impossible
2. **Signal simulé à fréquence fixe 1 Hz** → ne reflète aucun RPM mesuré (le cœur métier est cassé)
3. **`WiFi.transmitting()` n'existe pas** → toute la protection WiFi est inopérante

### Points forts réels
- Documentation hardware exceptionnelle (README, calibration guide, dépannage)
- `IRAM_ATTR` sur les ISR (contrairement à gemma4)
- Anti-rebound fonctionnel (1 ms)
- Lissage par fenêtre glissante (intention bonne, implémentation imparfaite)
- Web UI avec configuration ratio **fonctionnelle** (connectée au backend, contrairement à gemma4)
- Validation client-side des ratios
- Boot log détaillé pour diagnostic

### Faiblesses critiques
- `GPOS()` non défini → compilation échoue
- Signal de sortie à 1 Hz fixe, indépendant du RPM réel (bug fatal)
- Formule simRPM calculée mais jamais utilisée pour la génération physique du signal
- `WiFi.transmitting()` n'existe pas → protection WiFi inexistante
- Lissage appliqué à des compteurs cumulés (logique mathématiquement fausse)
- Dead code Home Assistant (~150 lignes inutiles)
- Ratios non persistés (perdus au reboot)
- JSON par String concatenation → fragmentation HEAP à 10 requêtes/s
- Pas de watchdog

### Verdict
**Code structurellement sophistiqué mais fonctionnellement cassé sur les points critiques.** La bonne architecture (ISR, Ticker, Web Server) est gâchée par :
- Des bugs de compilation (`GPOS` indéfini)
- Un bug fatal de logique métier (sortie à 1 Hz fixe au lieu d'un signal proportionnel au RPM)
- Un mécanisme WiFi basé sur une API inexistante

Avec correction de ces trois points, ce projet serait compétitif (score potentiel ~7/10). Tel quel, il nécessite des corrections substantielles pour être fonctionnel.
