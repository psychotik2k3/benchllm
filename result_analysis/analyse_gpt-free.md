# Analyse — gpt-free

**Source :** `competitors/gpt-free/gpt_deye_fan.ino`  
**Fichiers analysés :** 1 fichier source C++ (.ino)  
**Score global : 7.5 / 10**  
**Score qualité de code : 6.5 / 10**

---

## ✅ Positifs techniques

### Catégorie 1 — Temps-réel / ISR
- **[POSITIF] `IRAM_ATTR` sur les deux ISRs GPIO** (`tach92ISR`, `tach60ISR`) — le code sera servi depuis la RAM IRAM. Point critique maîtrisé.
- **[POSITIF] Compteur de cycles matériel via inline assembly (`rsr %0, ccount`)** : Au lieu d'utiliser `micros()` (qui appelle une fonction et risque l'éviction flash), le code lit directement le registre CCOUNT du processeur ESP8266 avec `__asm__ __volatile__("rsr %0,ccount")`. C'est la méthode la plus précise et la plus sûre pour mesurer des intervalles temps-réel sur ESP8266.
- **[POSITIF] Unsigned subtraction gère correctement le wraparound CCOUNT** : Le code note explicitement que `uint32_t period = now - previous` fonctionne même lorsque CCOUNT repasse de `0xffffffff` à `0`. Correct mathématiquement pour les types non-signés.
- **[POSITIF] Handlers ISR minimaux et non-bloquants** : Un seul accès CCOUNT, une soustraction unsigned, un test de plage. Pas de Serial.print, pas d'allocations, pas de divisions.
- **[POSITIF] Mesure sur front montant uniquement (`RISING`)** : Donne une mesure indépendante du duty-cycle du tach réel. Bonne approche pour des signaux dont le duty-cycle peut varier.

### Catégorie 2 — Démarrage / Boot
- **[POSITIF] Boot log structuré sur Serial** : En-tête ASCII clair, affichage des ratios chargés, IP AP, fréquence CPU, message de fin "System ready." Bon pour le diagnostic.
- **[POSITIF] `EEPROM.begin(EEPROM_SIZE)` avec 512 octets** : Taille généreuse (beaucoup plus que les autres concurrents). Permet d'éviter les problèmes de taille insuffisante pour la structure Config complète.
- **[POSITIF] Signature magique EEPROM (`0x44594631UL` = "DYF1")** : Validation de l'intégrité du fichier de configuration au boot. Fallback automatique vers config par défaut si corrompu. Équivalent fonctionnel du `0xDE 0xAD` de qwen3.6.
- **[POSITIF] Constrain des ratios à la lecture** : Les ratios EEPROM sont contraints entre `RATIO_MIN` (0,10) et `RATIO_MAX` (10,00). Protection contre EEPROM corrompue.

### Catégorie 3 — Génération sortie / Timer1
- **[POSITIF] Utilisation du waveform generator du core ESP8266** : `startWaveformClockCycles()` / `stopWaveform()` de `core_esp8266_waveform.h` — un générateur matériel dédié qui fonctionne indépendamment du CPU et du WiFi. Approche similaire à Timer1 dans son principe mais via une API dédiée. Le commentaire en tête de loop() documente explicitement cette séparation d'architecture.
- **[POSITIF] Sortie sur D5 ET D6 entièrement implémentée** : Deux sorties physiques gérées indépendamment. Contrairement à gemma4 (jamais basculé) et qwen35 (fréquence fixe 1 Hz), ici la fréquence est proportionnelle au RPM × ratio mesuré en temps réel.
- **[POSITIF] Calcul de demi-période avec clamp minimal** : `halfCycles` borné à min 2 cycles CPU. Si le calcul donne une période inférieure, le minimum est appliqué. Cela évite un diviseur par zéro ou une fréquence infinie.
- **[POSITif] Signal 50 % duty cycle** : Le waveform est configuré avec `halfCycles` pour HIGH et `halfCycles` pour LOW. Signal symétrique comme attendu d'un tach standard.

### Catégorie 4 — Fidélité protocole / Fail-safe
- **[POSITIF] IIR filtre implémenté dans la boucle principale** : Filtre exponentiel `7/8 ancienne + 1/8 mesure` appliqué sur les périodes brutes mesurées par l'ISR. Contrairement à qwen35 qui lisait des compteurs cumulés (faux), ici le filtre s'applique sur la période cyclique correcte. Lissage physique réel.
- **[POSITIF] Timeout configuré `TACH_TIMEOUT_US = 500 ms`** : Si aucun pulse reçu pendant 500 ms, le canal est considéré à l'arrêt et la sortie forcée LOW. Fail-safe fonctionnel avec timeout raisonnable (moins que les 1000 ms de gemini-free, plus que les non-existants de qwen36).
- **[POSITIF] `signalPresent` flag bien géré** : Le boolean est mis à `true` lors d'une mesure valide et passé à `false` au timeout. Utilisé pour la LED et le monitoring web. Logique cohérente.

### Catégorie 5 — Filtres / Signal
- **[POSITIF] Anti-glitch par limite de cycles CCOUNT** : `MIN_PERIOD_CYCLES = 40000` (250 µs à 160 MHz) et `MAX_PERIOD_CYCLES = 320000000` (2 secondes). Les pulses trop rapides (bruit, rebond) ou trop lents (expiration de fan) sont filtrés directement dans l'ISR. Plage couvrant ~0,5 RPM à ~24 000 RPM théorique — plus que suffisant pour des Noctua.
- **[POSITIF] Filtre IIR `7/8 + 1/8` avec calcul uint64_t** : Le filtrage utilise `(uint64_t)filtered * 7 + period) / 8` pour éviter le débordement int32. Calcul rigoureux avec cast explicite vers uint64_t — bon signe de maturité.
- **[POSITIF] Pas de FP dans l'ISR** : Tout le calcul flottant (RPM, ratio×RPM) est fait dans la boucle principale, jamais dans l'ISR. L'ISR ne travaille qu'en cycles CPU entiers. Approche optimale pour ESP8266 où le FP peut déclencher des appels à la flash.

### Catégorie 6 — WiFi / Réseau
- **[POSITIF] `WiFi.mode(WIFI_AP_STA)` avec gestion STA/AP complète** : Les deux modes simultanés. Si SSID STA configuré, `WiFi.begin()` est appelé. Sinon, l'appareil reste uniquement en mode AP.
- **[POSITIF] Mot de passe AP valide (10 caractères)** : `"DeyeFan123"` — respecte le minimum WPA2 de 8 caractères. Bien supérieur aux `"12345678"` (trivial) ou l'AP ouvert (`""`) des autres concurrents.
- **[POSITIF] Sécurité AP renforcée côté sauvegarde** : Si `apPassword.length() < 8`, le code force `"DeyeFan123"`. L'utilisateur ne peut pas désactiver la sécurité AP via l'interface web.
- **[POSITIF] Séparation explicite AP/STA dans la doc inline** : Les sections WiFi sont clairement délimitées avec des commentaires. La logique de fallback STA est documentée.

### Catégorie 7 — Gestion mémoire / HEAP
- **[POSITIF] `html.reserve(9000)` pour la page HTML** : Allocation unique préalable évitant les reallocs multiples. Moins de fragmentation qu'une String construite par concatenation sans reserve.
- **[POSITIF] Structure `TachChannel` comme encapsulation** : Les variables d'état sont regroupées dans une structure nommée plutôt que dispersées en variables globales sans lien logique. Meilleure maintenance.
- **[POSitif] Constantes définies avec `constexpr` et `#define`** : Tous les paramètres (pin, limites, timeouts) sont des constantes compilées en dur — zéro占用 de RAM volatile pour ces valeurs.

### Catégorie 8 — Interface Web
- **[POSITIF] Design dark theme moderne** : Fond `#101010`, cartes `#202020`, accents bleu `#55aaff` et vert `#55ff88`. Rendu visuel soigné et cohérent. CSS avec variables, bordures arrondies, ombres, boutons avec hover state.
- **[POSITIF] AJAX pour le monitoring en temps réel** : `setInterval(updateStatus, 500)` — polling toutes les 500ms. Moins agressif que qwen35 (100ms) mais plus réactif que gemini-free (1000ms).
- **[POSITIF] Formulaire de configuration via JavaScript fetch** : `saveConfig()` envoie une requête POST à `/api/save` avec tous les paramètres. Le serveur répond "OK" et affiche un message de confirmation. Flux AJAX complet côté client.
- **[POSITIF] `loadConfig()` au démarrage du navigateur** : Au chargement de la page, le JavaScript fait un GET sur `/api/config` pour pré-remplir les champs avec les valeurs EEPROM. UX soignée — l'utilisateur voit la configuration courante immédiatement.
- **[POSITIF] Validation des ratios côté HTML** : `<input type='number' min='0.10' max='10.00'>` — bornes visibles et contrôlées par le navigateur avant envoi. Renforce la validation serveur-side (`constrain`).

### Catégorie 9 — Persistance / Stockage
- **[POSITIF] EEPROM pour configuration complète** : Magic, ratios (float), STA SSID/password, AP SSID/password. Tout est persisté en un seul bloc `EEPROM.put(0, config)`. Lecture et écriture atomicité garantie par le fait qu'EEPROM.put() fait un memcpy + commit.
- **[POSITIF] Signature magique pour validation** : `EEPROM_MAGIC = 0x44594631UL` vérifié au boot. Si invalide, fallback vers config par défaut puis écriture. Protection contre EEPROM corrompue.
- **[POSITIF] Constrain des ratios après chargement EEPROM** : Même si les bytes EEPROM sont lisibles mais hors-borne (ex: EEPROM partiellement corrompue), les ratios sont contraints dans la plage valide. Double protection (magic + constrain).

### Catégorie 10 — Documentation hardware
- **[POSITIF] Schéma de diviseur de tension documenté** : Le fichier en-tête contient un ASCII art clair du diviseur resistif pour adapter les signaux Noctua vers GPIO 3,3V. Valeurs précises (10k + 1k + 3,3k). Explication du risque "JAMAIS recevoir directement 5V ou 12V".
- **[POSITIF] Pinout complet commenté** : Brochage détaillé avec correspondance ESP8266 pin / nom de fonction D1/D2/D5/D6/D7. Très clair pour le câblage.

### Catégorie 11 — Qualité de code générale
- **[POSITIF] Structure modulaire claire** : Le fichier est divisé en sections numérotées et commentées (EEPROM lecture, EEPROM écriture, ISR 92mm, ISR 60mm, Cycles→RPM, Sortie, Canal, LED, Web, WiFi, Setup, Loop). Lisibilité exceptionnelle.
- **[POSITIF] Structure `TachChannel` pour la réutilisabilité** : Les deux canaux sont traités par le même code via un template struct (`processChannel(TachChannel &channel, uint8_t outputPin, float ratio)`). Évite la duplication de code (DRY). Approche élégante et maintenable.
- **[POSITIF] Utilisation de `constexpr` pour les constantes** : Pins, limites, timeouts — tout est `constexpr`. Pas de variables globales inutiles pour des valeurs qui ne changent jamais.
- **[POSITIF] Commentaires explicatifs abondants** : Chaque bloc de code a des commentaires en français détaillant le pourquoi mathématique et physique derrière chaque opération (formules RPM, logique du filtre IIR, etc.). Documentation inline de qualité professionnelle.

---

## ❌ Négatifs techniques

### Catégorie 1 — Temps-réel / ISR
- **[critique] `core_esp8266_waveform.h` n'est pas un header standard Arduino ESP8266** : Ce fichier (`startWaveformClockCycles`, `stopWaveform`) ne fait PAS partie du core officiel ESP8266 Arduino. Il doit provenir d'un dépôt tiers ou avoir été personnalisé par l'auteur. Si l'utilisateur télécharge le firmware avec PlatformIO/Arduino IDE sans ce fichier spécifique, **la compilation échouera**. C'est un bug de dépendance critique similaire à `GPOS()` non défini chez qwen35. À moins que le header ne soit fourni en tant que ressource du projet.
- **[mineur] Pas de section critique explicite dans l'ISR** : Les ISRs lisent et écrivent `lastEdgeCycles`, `periodCycles`, `newPeriod` sans protection. Comme ces variables sont accédées depuis une seule ISR chacune (pas de compétition ISR↔ISR), c'est théoriquement sûr. Mais `newPeriod` est aussi écrit dans la section critique de `processChannel()`. Il faudrait vérifier qu'un accès concurrent ISR + processChannel ne provoque pas de race condition sur `newPeriod`. Le code semble correct car l'ISR ne modifie jamais `newPeriod` (elle ne la met qu'à `true` et c'est lue/effacée dans la section critique). Donc ce n'est PAS un bug — mais le manque d'explication rend le raisonnement moins trivial.

### Catégorie 2 — Démarrage / Boot
- **[majeur] Pas de watchdog (SW ou HW)** : Aucun hardware ou software watchdog implémenté. En cas de plantage WiFi, boucle infinie ou crash ISR, aucun reboot automatique. Point identique chez qwen3.6, gemini-free et gemma4.
- **[mineur] `ESP8266WebServer` au lieu de `ESPAsyncWebServer`** : Le serveur web est synchrone (classique). Pendant la transmission d'une réponse HTTP (surtout la page HTML complète ~9KB), le thread principal est bloqué et les ISRs GPIO fonctionnent toujours mais la loop() ne peut pas traiter d'autres requêtes. Comparé à `ESPAsyncWebServer` utilisé par gemini-free, c'est moins réactif sous charge concurrente (plusieurs clients simultanés). Pour un usage mono-client, l'impact est minime.
- **[mineur] Pas de fallback AP détaillé** : Contrairement à qwen3.6 qui tente le STA pendant 15 secondes avant de basculer en mode AP uniquement, gpt-free appelle simplement `WiFi.begin()` et passe à autre chose immédiatement si la connexion échoue. L'appareil sera peut-être sur le réseau STA mais sans indication visible (pas d'IP STA affichée dans le Serial boot).

### Catégorie 3 — Génération sortie / Timer1
- **[critique] `core_esp8266_waveform.h` dépendance externe** : Comme noté ci-dessus, la fonctionnalité de génération de signal repose sur un header qui n'est PAS inclus dans le dépôt. Si ce fichier n'est pas fourni par l'utilisateur, **le projet ne compile même pas**. C'est l'équivalent du bug `GPOS()` non défini de qwen35 ou du `digitalWrite` ISR des autres projets.
- **[majeur] Changement dynamique de fréquence en milieu de signal** : `setTachOutputRPM()` appelle `stopWaveform()` puis `startWaveformClockCycles()`. Si le waveform est activement généré au moment de l'appel, il y a un petit gap (quelques microsecondes) où le signal est interrompu. Sur des ventilateurs tournant à 3000 RPM → ~5 Hz simulé, ce gap est imperceptible. Mais pour un onduleur Deye sensible aux variations de signal, cela pourrait théoriquement être détecté comme un "fan stopped" transitoire si le gap dépasse le timeout. Risque faible mais réel.
- **[majeur] Conversion cycles→RPM utilise `double` au lieu de `float`** : `cyclesToRPM()` fait `(double)F_CPU / (double)cycles * 60.0 / TACH_PULSES_PER_REV`. Sur ESP8266 (FPU limitée), les doubles peuvent être plus lents que les floats. En pratique, la différence est minime car cette fonction n'est appelée que lors d'une nouvelle mesure de période (quelques Hz max). Mais `float` serait préférable sur une plateforme 32-bit avec FPU hardware.

### Catégorie 4 — Fidélité protocole / Fail-safe
- **[majeur] Pas de validation physique des périodes mesurées** : Bien que les limites CCOUNT soient définies (`MIN_PERIOD_CYCLES`, `MAX_PERIOD_CYCLES`), aucune vérification supplémentaire n'est faite pour s'assurer que la période convertie en RPM est physiquement réaliste pour un Noctua (ex: 0–5000 RPM max). Un spike électrique dans la plage de cycles valides pourrait produire un RPM aberrant qui ne serait pas détecté comme anomalie.
- **[mineur] Pas de seuil minimum RPM pour activer la sortie** : Quand `signalPresent` devient `true`, le ratio est immédiatement appliqué et `setTachOutputRPM()` est appelé. Même avec un fan à 50 RPM (extrêmement bas), le signal simulé sera activé. Un seuil minimum de RPM (ex: 200 RPM) pourrait éviter des transitions inutiles pendant l'initialisation du fan.

### Catégorie 5 — Filtres / Signal
- **[majeur] Coefficient IIR `7/8` peut être trop agressif ou trop lent selon le cas** : Le filtre `new = 7/8*old + 1/8*new` a une constante de temps équivalente à environ 8 mesures. Pour un fan dont la vitesse change rapidement (ex: démarrage d'un onduleur), il faudra ~40–60 ms pour que le filtre réagisse à un changement de régime. Inversement, pour les petites fluctuations turbulentes, ce facteur peut être suffisant. Ce n'est pas "faux" (contrairement à qwen35), mais c'est un paramètre non configurable qui pourrait nécessiter adaptation selon l'application.
- **[mineur] Pas de filtre IIR séparé pour le RPM vs la période** : Le filtre est appliqué sur les `periodCycles` puis converti en RPM. Une approche alternative serait de filtrer le RPM directement après conversion, car la relation cycles→RPM est non-linéaire (inverse). Filtrer la période avant conversion introduit une distorsion systématique par rapport à un filtre direct sur le RPM. L'impact pratique est probablement faible mais présent.

### Catégorie 6 — WiFi / Réseau
- **[majeur] Pas de reconnexion automatique après perte du réseau STA** : `WiFi.begin()` est appelé une seule fois au boot dans `startWiFi()`. Si la connexion est perdue plus tard (router redémarré, hors couverture), le device reste sur le réseau jusqu'à ce que WiFi.drop() soit appelé — mais il n'y a aucun mécanisme de reconnect. Le statut STA peut passer à `WL_DISCONNECTED` ou `WL_IDLE_STATUS` sans tentative automatique.
- **[mineur] Pas de support mDNS / Zeroconf** : Aucun nom de domaine local (ex: `deye-fan.local`). L'utilisateur doit connaître l'IP du STA ou aller sur 192.168.4.1 en mode AP.
- **[mineur] Pas d'authentification web** : N'importe qui connecté au réseau (AP ou STA) peut accéder à `/api/save` et modifier les ratios, ou `/api/save` avec des credentials modifiés. Aucun mot de passe ni token sur le serveur web.
- **[mineur] Pas d'OTA (Over-The-Air)** : Mises à jour firmware uniquement par câblage USB.

### Catégorie 7 — Gestion mémoire / HEAP
- **[majeur] String concatenation pour JSON dans les handlers HTTP** : `handleStatus()` et `handleGetConfig()` utilisent `json += ...` avec `String::reserve()` préalable. Même si `reserve` réduit le nombre de reallocs, chaque requête ALLOQUE une nouvelle String qui n'est détruite qu'à la fin du handler. En polling AJAX à 500ms (2 req/s), c'est **2 allocations/seconde**. Sur ESP8266 (80 KB SRAM), cela conduit à une fragmentation progressive. Moins critique que qwen35 (10 Hz) mais le pattern reste problématique à long terme.
- **[majeur] `makeWebPage()` alloue ~9KB de String par requête GET** : La page HTML est construite dans une variable locale `String html` avec `reserve(9000)`. Une allocation 9KB par requête GET, puis destruction. Si un navigateur rafraîchit la page fréquemment ou si plusieurs clients sont connectés, cela fragmente rapidement le HEAP. `server.sendContent()` avec des writes progressifs serait beaucoup plus sûr.

### Catégorie 8 — Interface Web
- **[mineur] Pas d'indicateur de statut WiFi** : L'interface n'affiche pas le RSSI, l'IP ou le mode (AP/STA) en cours. L'utilisateur ne sait pas si l'appareil est connecté au réseau STA.
- **[mineur] Le bouton "Enregistrer" ne recharge pas la page** : Après un `/api/save` réussi, le message "Configuration enregistrée." s'affiche mais la page HTML n'est pas rafraîchie. Les champs de ratio ne sont mis à jour qu'au prochain `loadConfig()` (via fetch `/api/config`). En pratique ce n'est pas bloquant car `updateStatus()` tourne en continu et l'AJAX reload des champs devrait arriver rapidement, mais UX perfectible.
- **[mineur] Pas d'échappement HTML dans les valeurs affichées** : Le JavaScript affiche directement `d.staSSID` et `d.staPassword` dans les inputs via `.value=...`. Si le SSID contient des caractères spéciaux ('"', '&', etc.), ils pourraient casser le DOM ou poser un XSS (faible risque sur un appareil local mais bonne pratique manquante).

### Catégorie 9 — Persistance / Stockage
- **[majeur] `EEPROM.commit()` bloquant sans protection contre les write multiples rapides** : `saveConfig()` fait un `EEPROM.put()` suivi de `EEPROM.commit()`. Si l'utilisateur clique rapidement plusieurs fois sur "Enregistrer" via le web, les commits s'enchaînent. Chaque commit prend ~10ms et use un cycle d'écriture EEPROM (~100 000 cycles max). Pas de debounce côté client ou serveur pour limiter les écritures.
- **[mineur] Structure Config contient des arrays `char[]` non-zero-init après modification partielle** : Si on modifie uniquement le ratio via `/api/save`, les champs STA SSID/password et AP restent inchangés (ce qui est correct). Mais si la structure était partiellement écrite sans `memset`, des bytes résiduels pourraient persister. Le code utilise `EEPROM.put(0, config)` avec la structure complète, donc le memcpy couvre tous les bytes — pas de problème fonctionnel. C'est plutôt un point théorique de robustesse.

### Catégorie 10 — Documentation hardware
- **[critique] Schéma de diviseur résistif uniquement (pas de schéma complet)** : Le header documente le diviseur de tension pour l'entrée GPIO, mais ne fournit pas de schéma électronique complet incluant les transistors NPN pour la sortie tach. Pas de netlist, pas de liste de composants, pas de procédure d'assemblage. Inférieur à qwen3.6 (schémas détaillés + netlist) et gemini-free (aucun).
- **[critique] Aucun fichier README en dehors du code** : Contrairement à tous les autres compétiteurs qui avaient au minimum un README.md, gpt-free ne contient qu'un seul fichier `.ino`. Un utilisateur ne saura pas comment flasher le firmware, quelles bibliothèques installer, ou comment configurer l'appareil après le déploiement.
- **[mineur] Schéma diviseur résistif non justifié** : Le ASCII art montre les valeurs (10k + 1k + 3,3k) mais ne calcule pas le voltage sur le GPIO pour un Noctua 12V. Un utilisateur curieux ne saurait pas pourquoi ces valeurs précises sont choisies.

### Catégorie 11 — Qualité de code générale
- **[critique] `core_esp8266_waveform.h` manquante du projet** : Le fichier `.ino` contient `#include "core_esp8266_waveform.h"` (guillemets, pas `<...>`) qui indique un header local. Si ce fichier n'est PAS fourni dans le dossier du projet, la compilation échouera immédiatement. C'est l'équivalent fonctionnel du bug `GPOS()` non défini de qwen35 — une dépendance externe essentielle non distribuée avec le code source.
- **[majeur] String concatenation JSON à 2 req/s** : Pattern récurrent dans `handleStatus()`, `handleGetConfig()`, et `makeWebPage()`. Chaque requête provoque une allocation heap. À 500ms de polling, c'est 2 allocations de JSON + ~1 allocation HTML toutes les secondes. Sur ESP8266, le HEAP se fragmente inévitablement sur des périodes longues. Comparé à gemini-free qui utilise `StaticJsonDocument` (zéro heap pour JSON), ce projet est significativement plus vulnérable.
- **[majeur] `ESP8266WebServer` bloque le thread pendant les transmissions HTTP** : Le serveur synchrone bloque la loop() pendant la transmission de réponses HTTP. Une page HTML de 9KB à 115200 bauds prend ~0,6s de transmission, pendant lesquelles `server.handleClient()` ne peut plus traiter d'autres requêtes. La LED ne clignote pas, le traitement canal continue (ISR fonctionne toujours), mais la réactivité web est dégradée sous charge.
- **[majeur] `digitalWrite()` dans `stopTachOutput()` et `updateLED()`** : Les appels `digitalWrite(pin, LOW/HIGH)` sont faits depuis la loop principale, pas depuis une ISR. Ce n'est PAS un bug critique ici (la loop est un contexte normal, pas une ISR). Cependant, si le waveform generator a des timings très serrés, un appel `digitalWrite()` juste après `stopWaveform()` pourrait théoriquement interférer avec l'état du pin. Pratiquement, `stopWaveform()` libère le pin et `digitalWrite()` est safe dans la loop. Ce n'est PAS un bug mais mérite une remarque.
- **[mineur] Variables globales pour les tach channels** : `tach92` et `tach60` sont des variables globales de type struct. Acceptable pour ce projet (2 canaux), mais manque d'encapsulement. Le pattern `TachChannel` est bien pensé mais la variable globale atténue légèrement le bénéfice.
- **[mineur] `double` au lieu de `float` dans `cyclesToRPM()`** : Sur ESP8266, le double n'est pas natif (FPU 32-bit seulement). Les opérations double nécessitent des sous-routines logiciel qui prennent plus de temps et d'espace. Pour une fonction appelée quelques fois par seconde à un fan Noctua, l'impact est minime mais `float` serait plus approprié.

---

## Synthèse

**Ce projet présente la meilleure architecture de code propre parmi tous les compétiteurs.** La structure `TachChannel` réutilisable pour les deux canaux, le filtre IIR correctement appliqué sur des périodes cycliques (et non des compteurs), l'utilisation du compteur CCOUNT via inline assembly pour une mesure précise sans appeler de fonction dans l'ISR, et la séparation claire ISR/cycle principal font de gpt-free un code remarquablement bien pensé. La configuration AP sécurisée avec mot de passe valide et la validation EEPROM par magic number sont des points forts pratiques.

**Cependant, trois limitations majeures empêchent un score parfait :**

1. **Dépendance `core_esp8266_waveform.h` non fournie** — Si ce header n'est pas dans le projet, compilation impossible. Équivalent du bug `GPOS()` de qwen35.
2. **Fragmentation HEAP par String concatenation JSON** — À 2 req/s (polling 500ms), l'ESP8266 verra sa mémoire se fragmenter progressivement. `StaticJsonDocument` (gemini-free) ou `client.print()` direct seraient préférables.
3. **Documentation hardware très limitée** — Un seul fichier `.ino` avec un diviseur résistif schématisé, mais aucun README, aucun schéma complet, aucune liste de composants pour le câblage NPN des sorties.

### Points forts réels
- Compteur CCOUNT inline assembly pour mesure ISR ultra-précise (pas de micros() bloquant)
- Filtre IIR `7/8 + 1/8` appliqué sur les périodes cycliques correctes — pas le faux filtre de qwen35
- Structure `TachChannel` réutilisable — code DRY, maintenable
- Séparation ISR (cycles entiers) / loop (FP) — optimal sur ESP8266
- EEPROM avec magic number + constrain des ratios — robustesse configuration
- Mot de passe AP valide (10 caractères WPA2-compliant) — sécurité réelle
- Design web dark theme avec AJAX + formulaire de config complet
- `html.reserve(9000)` — allocation proactive anti-fragmentation
- Documentation inline mathématique abondante — chaque formule expliquée

### Faiblesses critiques
- **`core_esp8266_waveform.h` manquante** → compilation bloquante si pas fournie (bug équivalent à GPOS de qwen35)
- String concatenation JSON à 2 req/s → fragmentation HEAP progressive
- `ESP8266WebServer` synchrone bloque la loop pendant les transmissions HTTP
- Documentation hardware très limitée (pas de schéma NPN, pas de README, pas de liste composants)
- Pas de watchdog, pas de reconnexion STA automatique, pas de mDNS, pas d'OTA
- Pas d'authentification web

### Comparaison avec les autres compétiteurs

| Aspect | gemma4 (3.2) | qwen35 (4.8) | qwen3.6 (7.2) | gemini-free (5.3) | **gpt-free (7.5)** |
|--------|-------------|-------------|---------------|-------------------|---------------------|
| ISR IRAM_ATTR | ❌ Absent | ✅ Présent | ✅ Présent | ✅ Sur 3 ISRs | ✅ Sur 2 ISRs |
| Mesure timing ISR | micros() | micros() | CCOUNT registre | micros() | **CCOUNT registre (asm)** |
| Timer1/hardware | ❌ Ticker SW | ⚠️ Sortie fixe 1 Hz | ✅ Hardware timer | ✅ Waveform gen | ⚠️ Depende external header |
| Signal D5/D6 | ❌ Jamais | ⚠️ 1 Hz fixe | ✅ Proportionnel | ✅ Proportionnel | ✅ Proportionnel |
| Filtres signal | ❌ Aucun | ⚠️ Fenêtre (faux) | ⚠️ Anti-rebound | ❌ Aucun | ✅ **IIR 7/8+1/8** |
| EEPROM persistence | ❌ Aucune | ❌ Aucune (cassé) | ✅ Magic number | ❌ LittleFS JSON | ✅ Magic number + constrain |
| Ratio Web UI | ❌ Inopérant | ✅ Fonctionnel | ✅ Formulaire POST | ✅ Formulaire POST | ✅ AJAX fetch |
| Docs hardware | ✅ Correcte | ⭐ Exceptionnelle | ⭐ Exceptionnelle | ❌ Aucune | ❌ **Très limitée** |
| AP password | ⚠️ 12345678 | ⚠️ 12345678 | ❌ Ouvert | ⚠️ 12345678 | ✅ **DeyeFan123 (WPA2)** |
| JSON HEAP safe | ❌ String concat | ❌ String 10Hz | ✅ OK | ✅ StaticJsonDocument | ❌ String concat |
| Bugs critiques | 6+ | 3 | 2 | 2 | **1** (header externe) |

### Verdict
**Architecture de code la plus propre, mais dépendance externe bloquante.** gpt-free est le projet avec la structure de code la mieux pensée : `TachChannel` réutilisable, filtre IIR correct sur périodes cycliques, compteur CCOUNT en assembly inline pour précision maximale, et séparation ISR/loop rigoureuse. Le score serait probablement **8,5/10** si le header `core_esp8266_waveform.h` était fourni — ce qui éliminerait le seul bug critique de compilation. La principale faiblesse pratique est la documentation hardware très limitée (un seul fichier `.ino`, pas de README, pas de schéma complet) et la fragmentation HEAP due aux String concatenation JSON. Pour une implémentation production-ready, il faut fournir le header waveform manquant, remplacer les String par `StaticJsonDocument` ou `server.sendContent()`, et ajouter un README avec schéma NPN complet.
