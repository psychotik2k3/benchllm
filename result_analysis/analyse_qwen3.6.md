# Analyse — qwen3.6

**Source :** `competitors/qwen3.6/deye_fan.ino` + `SCHÉMA_CIRCUIT.md`  
**Fichiers analysés :** 1 fichier source C++ (.ino), 1 fichier documentation technique (.md)  
**Score global : 7.2 / 10**  
**Score qualité de code : 6.8 / 10**

---

## ✅ Positifs techniques

### Catégorie 1 — Temps-réel / ISR
- **[POSITIF] `IRAM_ATTR` sur les deux ISRs GPIO** (`fan1TachISR`, `fan2TachISR`) — le code d'interruption sera servi depuis la RAM IRAM, évitant les Guru Meditation crash pendant l'activité WiFi. Point critique maîtrisé.
- **[POSITIF] Architecture triple-barrière documentée** : Documentation architecturale explicite en entête du fichier (schéma ASCII) décrivant les 3 couches d'isolation (ISR GPIO entrée → Timer1 ISR sortie → loop() web). Approche de conception consciente et bien pensée.
- **[POSITIF] Anti-rebond par timestamp micros()** : Comparaison de timestamp avec période minimale (`MIN_TACH_PERIOD_US = 80 µs`). Les fronts trop rapprochés sont rejetés dans l'ISR elle-même, pas en post-traitement. Précision microseconde réelle.
- **[POSITIF] Calcul du ratio simulé directement dans l'ISR** : Le `simPeriodU32` est calculé au moment de la détection du front descendant (`period * fan1RatioInt10 / 10`). Pas d'étape intermédiaire perdue — le rapport multiplicateur est appliqué en temps réel. Approche plus directe et précise que toute la stratégie de lissage par fenêtre glissante de qwen35.
- **[POSITIF] Version int10 des ratios pour ISRs** : Les ratios sont stockés sous forme `int16_t × 10` (2,5 → 25) pour éviter les opérations virgule flottante dans l'ISR. Solution élégante et portable sur ESP8266 où le FP peut toucher la flash.
- **[POSITIF] Timer1 matériel dédié** : Un `hw_timer_t` (numéro=1) gère exclusivement la génération des signaux tach. Fonctionne indépendamment du CPU et du WiFi, garantissant une périodicité stable sans jitter réseau.

### Catégorie 2 — Démarrage / Boot
- **[POSITIF] Boot log structuré** : En-tête ASCII clair, messages de confirmation par composant ([EEPROM], [Hardware], [Timer1], [Web]). IP affichée selon le mode (AP/STA). Bon pour le diagnostic.
- **[POSITIF] Fallback AP élégant** : Tentative STA avec credentials EEPROM, timeout 15 secondes, fallback automatique AP si échec. UX de déploiement flexible.
- **[POSITIF] `WiFi.persistent(false)`** : Évite la corruption du SPIFFS/NVS en empêchant la bibliothèque WiFi d'écrire ses propres données de connexion en concurrence avec les writes EEPROM manuels.

### Catégorie 3 — Génération sortie / Timer1
- **[POSITIF] Sortie sur D5 ET D6 entièrement implémentée** : Deux sorties physiques (GPIO12/D6 pour fan 9cm, GPIO14/D5 pour fan 6cm) gérées par un seul ISR Timer1. Contrairement à gemma4 (jamais basculé) et qwen35 (fréquence fixe 1 Hz), ici la fréquence de toggle est proportionnelle au RPM mesuré × ratio. C'est la vraie fonctionnalité du projet qui fonctionne.
- **[POSITIF] Approche nextTick absolu** : Chaque sortie conserve son `nextTick` absolu. L'ISR compare `nextTick <= timerRead()` et programme le prochain tick en conséquence. Méthode robuste évitant l'accumulation de dérive temporelle (pas d'addition incrémentale).
- **[POSITIF] Gestion du « fan stopped » dans l'ISR** : Quand un ventilateur s'arrête (`fanActive == false`), le pin est forcé HIGH permanent et `nextTick = UINT32_MAX`. L'onduleur reçoit un niveau HAUT statique (= état « fan dead »), exactement comme un tach d'onduleur PC standard attend.
- **[POSITIF] Calcul de demi-période pour alternance** : Le ISR bascule à chaque demi-période (`simPeriodUs / US_PER_TICK`), créant un signal carré 50% duty cycle. Logique correcte pour un signal tach symétrique.
- **[POSITIF] Résolution Timer1 de 1 µs** : Prescaler 80 sur CPU 80 MHz → tick de 1 µs. Précision sub-microseconde, largement suffisante pour les fréquences tach nécessaires (< 30 kHz max).

### Catégorie 4 — Fidélité protocole / Fail-safe
- **[POSITIF] État stopped state correctement interprété** : GPIO HIGH → NPN OFF → collecteur flottant tiré par le pull-up interne de l'onduleur → niveau HAUT. C'est l'état standard attendu par un onduleur Deye pour « ventilateur arrêté ». Logique de cablage et d'interprétation correcte.
- **[POSITIF] Validation de période dans l'ISR** : `period >= MIN_TACH_PERIOD_US && period <= MAX_TACH_PERIOD_US` filtre les artefacts électriques (trop court = bruit, trop long = expiration ou glitch). Limite haute à 100 000 µs (~60 RPM en RPM réels), couvre la plage raisonnable.
- **[POSITIF] Fan stall detection avec timeout configurable** : `TACH_STALE_MS = 2000` ms. Si aucun front reçu dans les 2 secondes, le fan est marqué « à l'arrêt » et la sortie forcée HIGH. Fail-safe présent et fonctionnel.

### Catégorie 5 — Filtres / Signal
- **[POSITIF] Anti-rebound ISR (1 ms effectif via MIN_TACH_PERIOD_US)** : La constante `80 µs` est la limite théorique de période pour ~18 750 Hz max, bien en-dessous des fréquences normales d'un tach Noctua. Rejet immédiat dans l'ISR, pas de post-traitement nécessaire.
- **[POSITIF] Permet une marge de sécurité sur les fronts** : Un fan à 3000 RPM avec 2 pulses/rev = 60 Hz → période ~16 666 µs entre pulses. Bien au-dessus du seuil minimum de 80 µs, donc aucun front valide ne sera rejeté par erreur.

### Catégorie 6 — WiFi / Réseau
- **[POSITIF] Mode AP+STA simultanée** : Flexibilité maximale — l'appareil peut fonctionner en standalone (AP = 192.168.4.1) ou se connecter à un réseau existant (STA). Les deux peuvent coexister.
- **[POSITIF] WiFi credentials stockés en EEPROM** : SSID + password persistés, lus au boot pour tentative de connexion automatique. Pas besoin de repasser par l'AP après chaque reboot.

### Catégorie 7 — Gestion mémoire / HEAP
- **[POSITIF] `server.send()` avec content direct** : Contrairement à qwen35 qui faisait `String(json) += ...` par concatenation (10 allocations/sec), ici le JSON est construit dans une variable locale `String json` et passé directement à `server.send()`. Moins de fragmentation car le buffer est alloué une fois par requête puis détruit.
- **[POSITIF] Constantes définies avec `#define`** : Pas de variables globales inutiles pour les constantes statiques (pin definitions, timeouts, prescaler). Réduction de la pression mémoire SRAM.

### Catégorie 8 — Interface Web
- **[POSITIF] Design dark theme moderne** : CSS complet avec variables de couleur (`#1a1a2e` background, `#00ff88` accents verts), polices, bordures arrondies, cartes avec style `.card`. Meilleur rendu visuel que gemma4 (clair) et qwen35 (clair).
- **[POSITIF] Formulaire ratio fonctionnel** : `<form method="GET" action="/save">` avec `input[type=number]` step 0.1, min 0.5, max 10.0. POST à `/save` → `handleSave()` lit les args, met à jour ratios, write EEPROM + restart. Flux complet et fonctionnel.
- **[POSITIF] Formulaire WiFi séparé** : `/wifisave` gère les credentials réseau avec reconnexion automatique post-save. UX soignée.
- **[POSITIF] Auto-refresh HTML (2 secondes)** : `<meta http-equiv="refresh" content="2">` rafraîchit toute la page automatiquement. Pas besoin de JavaScript pour le monitoring temps réel.
- **[POSITIF] API JSON séparée** : `/api/status` expose les données en format machine-readable (pour domoticz, Home Assistant, etc.) avec CORS `Access-Control-Allow-Origin: *`.
- **[POSITIF] Affichage uptime + RSSI** : Informations système contextuelles visibles immédiatement dans l'interface.

### Catégorie 9 — Persistance / Stockage
- **[POSITIF] EEPROM pour ratios ET WiFi credentials** : Deux adresses distinctes (0=magic, 2-5=ratio1 float, 6-9=ratio2 float, 10-43=SSID, 42-107=password). Signature magique `0xDE 0xAD` pour validation d'intégrité. Persistance complète de la configuration utilisateur.
- **[POSITIF] Validation des ratios à lecture** : Si les valeurs EEPROM sont hors limites (f < 0,5 ou f > 10,0) ou si le magic number ne correspond pas, fallback sur 2,5 par défaut. Protection contre EEPROM corrompue.

### Catégorie 10 — Documentation hardware
- **[POSITIF] Documentation hardware exceptionnelle** : `SCHÉMA_CIRCUIT.md` contient des ASCII art détaillés pour chaque bloc (alimentation, entrée tach, sortie tach, LED), liste complète du matériel avec prix estimés, procédure d'assemblage étape par étape, et netlist complète en annexe. Niveau professionnel — surpassant largement tout ce qu'on a vu chez les autres compétiteurs.
- **[POSITIF] Choix de composants justifiés** : Chaque composant (Schottky BAT54S, 2N2222/BC547 pour NPN, 4,7kΩ pull-up) est accompagné d'une explication du « pourquoi ». Calculs de courant fournis (`(12V - 3,7V) / 4,7k = ~1,8mA`).
- **[POSITIF] Netlist complète (Annexe A)** : Schéma électrique noté net par net, directement utilisable pour un routage PCB. Rare de trouver cette qualité de documentation dans des projets DIY ESP8266.

### Catégorie 11 — Qualité de code générale
- **[POSITIF] Sections commentées clairement** : Délimitations par `// ===== SECTION N — DESCRIPTION ===` avec numérotation explicite. Structure modulaire et lisible.
- **[POSITIF] Bon usage des fonctions statiques** : Les fonctions internes (`generateHtml`, `handleSave`, `eepromLoadRatios`, etc.) sont marquées `static` pour limiter la portée à la translation unit. Bonne pratique C/C++.
- **[POSITIF] Typage explicite et conversions sûres** : `(uint64_t)period * fan1RatioInt10 / 10` cast explicite vers uint64_t pour éviter le débordement lors du calcul de période simulée. Manifeste de rigueur.

---

## ❌ Négatifs techniques

### Catégorie 1 — Temps-réel / ISR
- **[critique] `digitalWrite()` dans l'ISR Timer1** : `tachOutputAllISR()` appelle `digitalWrite(FAN1_SIM_OUT, HIGH)` et `LOW`. Sur ESP8266, `digitalWrite()` n'est **pas** marquée `IRAM_ATTR` — elle effectue des lookup de cache pour mapper le numéro de pin vers le registre GPIO. Si la page flash contenant `digitalWrite` est évictée pendant que l'ISR Timer1 tourne (le Timer1 fonctionne indépendamment du CPU, même quand le CPU est inactif), cela provoque un **Guru Meditation Fault** dans l'ISR. C'est un bug critique. La solution consiste à utiliser les registres GPIO directs (`GPIO_OUT_W1TS` / `GPIO_OUT_W1TC`) avec le bit calculé manuellement à partir du pin number.
- **[mineur] Timer1 ISR n'est pas IRAM_ATTR** : `tachOutputAllISR()` est déclarée `static void IRAM_ATTR` mais l'attribut `IRAM_ATTR` ne peut s'appliquer qu'à une fonction locale au fichier (donc `static`). Cependant, sur ESP8266, `IRAM_ATTR` étend la macro `ICACHE_RAM_ATTR` qui utilise le linker section `.iram1`. Vérifier que cette syntaxe compile bien — si non, l'ISR Timer1 pourrait être servi depuis la flash et crasher.
- **[mineur] Pas de section critique pour les variables partagées ISR↔loop** : Dans `loop()`, le bloc de stale detection lit `fan1Active` (volatile) puis `lastFan1IRQ_us` (volatile) sans `noInterrupts()`/`interrupts()`. Sur ESP8266 (32-bit ARM), une lecture 4-byte est théoriquement atomique, mais ce n'est pas garanti par l'Arduino core. Si un `fan1TachISR()` tourne entre les deux lectures, on peut obtenir un état cohérent partiellement corrompu (ex: fan toujours marqué active mais old timestamp).

### Catégorie 2 — Démarrage / Boot
- **[critique] Dangling `EEPROM.end()` après la brace de fermeture de `eepromLoadRatios()`** : À la ligne suivant la `}` fermante de `eepromLoadRatios()`, il y a un `} EEPROM.end();` supplémentaire. Ce `EEPROM.end()` est hors de toute fonction — c'est du code orphelin qui causera une **erreur de compilation**. Même si le compilateur l'accepte (selon la version), il s'exécute au scope global, ce qui est non-défini et potentiellement dangereux.
- **[majeur] Mot de passe AP vide par défaut** : `WiFi.softAP(DEFAULT_AP_SSID, "")` crée un AP ouvert sans mot de passe. N'importe quel voisin peut se connecter et modifier les ratios ou le WiFi STA. C'est un risque de sécurité réel même sur un réseau local.
- **[mineur] Pas de watchdog (SW ou HW)** : Aucun hardware ou software watchdog implémenté. En cas de plantage du WiFi ou boucle infinie, aucun reboot automatique.

### Catégorie 3 — Génération sortie / Timer1
- **[majeur] `timerWrite(tachTimer, next)` utilise des valeurs absolues avec un timer qui tick continuellement** : Le code programme le prochain interrupt à `next = min(fan1NextTick, fan2NextTick)` via `timerWrite(tachTimer, next)`. Mais `timerWrite` sur ESP8266 définit la *valeur courante* du compteur Timer, pas la prochaine valeur d'interrupt. Si le timer tick déjà au-delà de `next`, l'ISR se déclenchera **immédiatement** (au prochain cycle). Cela semble correct si `next > timerRead()` au moment du write, mais il faut garantir que cette condition est toujours vraie. En pratique, comme l'ISR lit `timerRead()` et compare avec `nextTick`, le risque de race condition existe si le timer tick entre la lecture et l'écriture.
- **[mineur] Période minimale simulée non contrainte** : Bien que `simPeriodUsFanX` soit borné dans l'ISR (min 80 µs, max 200 000 µs), une valeur calculée avec un ratio élevé sur un fan très rapide pourrait théoriquement dépasser MAX_TACH_PERIOD_US * 2. La borne supérieure est appliquée mais ne force pas de clamp — si la période simulée est hors-borne, `simPeriodUsFan1` conserve sa valeur précédente (qui peut être ancienne et inexacte).

### Catégorie 4 — Fidélité protocole / Fail-safe
- **[majeur] Fan stall detection avec un gap de ~500 ms** : Le stale check dans `loop()` ne tourne que toutes les 500 ms (`if (nowMs - lastCheck1 > 500)`). Si un fan s'arrête entre deux checks, il faudra attendre jusqu'à 1 seconde (voire plus si le timer ISR est dans une phase critique) pour que la sortie soit forcée HIGH. Ce n'est pas critiques pour un onduleur (qui tolère des variations), mais c'est moins rapide qu'une détection basée sur le timestamp micros() directement dans l'ISR d'entrée.
- **[mineur] Pas de validation des ratios après `/wifisave`** : Le endpoint `/save` valide les ratios (0,5–10,0), mais si un utilisateur entre une valeur invalide via une requête brute (hors web UI), elle sera ignorée silencieusement sans retour d'erreur. La page ne montre pas de message « invalid value ».

### Catégorie 5 — Filtres / Signal
- **[mineur] Pas de filtre IIR ou EMA** : Le code repose uniquement sur l'anti-rebound ISR + un mécanisme de timeout pour le stall detection. Pas de lissage temporel (moyenne glissante, IIR, etc.) pour lisser les fluctuations de période d'un fan réel (les Noctua ont une turbulence naturelle qui crée des variations de ±1-2% sur la période tach). Les périodes transmises au Timer1 peuvent fluctuer d'un tick à l'autre, créant du jitter sur le signal simulé. Moins smooth que le filtre par fenêtre glissante de qwen35 (même si celui-ci était mal implémenté).

### Catégorie 6 — WiFi / Réseau
- **[majeur] Pas de support mDNS / Zeroconf** : Aucun nom de domaine local (ex: `deye-tachsim.local`). L'utilisateur doit connaître l'IP attribuée (soit celle du STA, soit 192.168.4.1 en mode AP). Inconvénient UX sur un réseau avec plusieurs appareils.
- **[mineur] Pas de OTA (Over-The-Air)** : Mises à jour firmware uniquement par câblage USB. Avec un ESP8266, l'OTA est trivial à ajouter et rendrait le déploiement beaucoup plus pratique.
- **[majeur] Reconnexion WiFi bloquante dans `handleWifiSave()`** : Après avoir sauvegardé un nouveau SSID/password, le code appelle `WiFi.begin()` avec une boucle `while (WiFi.status() != WL_CONNECTED && attempts < 20)` qui bloque tout (y compris le serveur web et les ISR GPIO si le WiFi consomme du CPU). Pendant ces ~10 secondes de connexion, les ISR Timer1 continuent mais l'anti-rebound ISR ne peut pas mettre à jour `simPeriodUsFanX`. Le signal simulé reste figé sur la dernière valeur connue. Risque que l'onduleur détecte une période « gelée » et interprète comme un fan stopped (le timeout de 2s est dépassé pendant la reconnexion).
- **[mineur] Pas d'authentification web** : N'importe qui connecté au réseau (AP ou STA) peut accéder à `/save` et modifier les ratios, ou `/wifisave` pour changer le WiFi. Aucune protection par mot de passe sur le serveur web.

### Catégorie 7 — Gestion mémoire / HEAP
- **[mineur] String concatenation dans `generateHtml()`** : La page HTML est construite avec `html += "<...>"` répété des centaines de fois. Chaque `+=` sur un `String` alloue potentiellement un nouveau buffer heap si le resize dépasse la capacité actuelle. Une seule allocation par requête `/`, donc l'impact est acceptable (une fois toutes les 2s avec meta refresh). Mais pour une optimisation future, utiliser `server.sendContent()` avec des writes directs serait plus sûr.
- **[mineur] Construction JSON dans `handleApiStatus()`** : Même pattern que `generateHtml()` — une variable locale `String json` construite par `+=`. Une allocation par requête API. Acceptable tant que le polling n'est pas excessif.

### Catégorie 8 — Interface Web
- **[mineur] Meta refresh au lieu d'AJAX** : La page HTML se recharge entièrement toutes les 2 secondes (`<meta http-equiv="refresh" content=\"2\">`). Cela provoque une re-dessine complète du DOM, perte de focus possible sur les inputs, et flash visuel à chaque reload. Comparé au polling AJAX (100ms) de qwen35 qui ne rafraîchit que les données, c'est moins fluide mais nettement plus simple côté client (pas besoin de JavaScript).
- **[mineur] Les ratios ne sont pas persistés en temps réel** : `handleSave()` écrit EEPROM + fait `ESP.restart()`. L'utilisateur doit redémarrer l'appareil pour appliquer le changement. C'est sûr (évite de modifier des variables pendant que le Timer1 ISR lit), mais peu ergonomique. Un utilisateur pourrait s'attendre à un changement immédiat.
- **[mineur] Formulaire avec method="GET"** : Les formulaires utilisent GET au lieu de POST. Sur un appareil dédié local, c'est acceptable (pas de données sensibles en transit réel — les passwords sont hachés ou courts), mais cela signifie que les ratios et credentials apparaissent dans l'URL et donc potentiellement dans les logs navigateur/proxy.

### Catégorie 9 — Persistance / Stockage
- **[positif mineur] EEPROM write timing** : `EEPROM.commit()` est bloquant (~10ms sur ESP8266). `handleSave()` appelle `eepromWriteRatios()` qui fait `commit()`. Si l'utilisateur clique rapidement plusieurs fois, les commits s'enchaînent. Pas de problème fonctionnel immédiat mais à surveiller si l'EEPROM atteint sa limite d'écriture (~100 000 cycles).
- **[critique] Dangling EEPROM.end() (voir Catégorie 2)** — Ce même bug affecte la persistance car il peut corrompre l'état EEPROM global.

### Catégorie 10 — Documentation hardware
- **[positif mineur : aucun négatif notable]** — La documentation est d'excellente qualité, probablement la meilleure parmi tous les compétiteurs analysés. Pas de critique pertinente ici.

### Catégorie 11 — Qualité de code générale
- **[critique] Dangling `EEPROM.end()` — erreur de syntaxe/blockage compilation** : Après la brace fermante de `eepromLoadRatios()`, une ligne isolée `} EEPROM.end();` apparaît hors de portée de toute fonction. Le compilateur Arduino va rejeter ce code (erreur de parsing) ou, dans le pire des cas, l'exécuter au scope global avec un comportement non défini. C'est un bug bloquant.
- **[critique] `digitalWrite()` dans ISR Timer1** : Comme expliqué ci-dessus, utiliser `digitalWrite()` dans une ISR qui n'est pas en IRAM (ou dont le code cible peut être paginé en flash) provoque des crashes aléatoires sur ESP8266. Remplacer par accès directs aux registres GPIO (`GPIO_OUT_W1TS` / `GPIO_OUT_W1TC`) est nécessaire.
- **[majeur] Bloc de reconnexion WiFi dans `handleWifiSave()` bloque toute l'exécution** : La boucle `while (WiFi.status() != WL_CONNECTED && attempts < 20)` pendant ~10 secondes bloque le thread principal, empêchant `server.handleClient()` et potentiellement les ISR GPIO. Le signal simulé reste figé sur la dernière valeur mesurée avant la reconnexion. Si cette période excède 2s (TACH_STALE_MS), l'onduleur détectera un fan stopped pendant la reconnexion.
- **[majeur] `static bool ledState` dans une scope de loop() — variable statique locale** : Les variables `static` dans `loop()` persistent entre appels, ce qui est correct. Mais il y a deux blocs `if (apModeOnly)` et `else` avec chacun leur propre `static bool ledState`. Ces deux variables sont indépendantes l'une de l'autre — un effet secondaire acceptable ici mais potentiellement confus pour un futur mainteneur.
- **[mineur] Variables globales sans scope** : Toutes les variables d'état (fan1PeriodUs, fan2Ratio, etc.) sont au scope global. Acceptable pour un projet ESP8266 de cette taille (~100 lignes de state), mais manque d'encapsulement si le projet devait être étendu.

---

## Synthèse

**Ce projet est l'architecture la plus aboutie parmi tous les compétiteurs analysés.** La séparation des responsabilités (ISR GPIO entrée → Timer1 ISR sortie → loop web) est correcte et bien documentée. L'utilisation du Timer1 matériel pour la génération de signal, la persistance EEPROM avec validation par magic number, et la documentation hardware exceptionnelle placent qwen3.6 significativement au-dessus des autres.

Cependant, **deux bugs critiques empêchent le code de fonctionner correctement en production** :

1. **Dangling `EEPROM.end()`** — erreur de syntaxe hors de toute fonction → compilation impossible ou comportement non défini.
2. **`digitalWrite()` dans l'ISR Timer1** — sur ESP8266, `digitalWrite` n'est pas IRAM_ATTR ; une eviction flash provoque un crash ISR. Les registres GPIO directs doivent être utilisés à la place.

### Points forts réels
- Architecture triple-barrière (ISR entrée / Timer1 sortie / loop web) — isolation matérielle réelle
- Timer1 matériel dédié pour génération tach — fréquence stable, sans jitter WiFi
- EEPROM avec validation par signature magique (0xDE 0xAD) — robustesse contre corruption
- Calcul ratio dans l'ISR via int10 fixe-point — pas de FP dans ISR sur ESP8266
- Fan stopped state correctement géré (GPIO HIGH permanent = NPN OFF)
- Documentation hardware exceptionnelle (schémas, netlist, procédure d'assemblage)
- Design web dark theme moderne
- API JSON séparée `/api/status` avec CORS

### Faiblesses critiques
- `digitalWrite()` dans ISR Timer1 → crash ESP8266 (remplacer par registres GPIO directs)
- Dangling `EEPROM.end()` hors scope → compilation bloquante
- Reconnexion WiFi bloquante (~10s pendant lesquelles le signal simulé est gelé)
- AP ouvert sans mot de passe + aucune auth web
- Pas de filtre IIR/EMA sur les périodes tach (jitter possible)
- Pas de watchdog, pas de mDNS, pas d'OTA

### Comparaison avec les autres compétiteurs

| Aspect | gemma4 (3.2) | qwen35 (4.8) | **qwen3.6 (7.2)** |
|--------|-------------|-------------|-------------------|
| ISR IRAM_ATTR | ❌ Absent | ✅ Présent | ✅ Présent |
| Timer1 matériel | ❌ Ticker (software) | ⚠️ Sortie fixe 1 Hz | ✅ Hardware timer dédié |
| Signal simulé | ❌ Jamais implémenté | ⚠️ Fréquence fixe 1 Hz | ✅ Proportionnel au RPM × ratio |
| EEPROM persistence | ❌ Aucune | ❌ Aucune | ✅ Ratios + WiFi avec validation |
| Ratio Web UI | ❌ Inopérant | ✅ Fonctionnel | ✅ Formulaire fonctionnel |
| Filtres signal | ❌ Aucun | ⚠️ Fenêtre glissante (faux) | ⚠️ Anti-rebound uniquement |
| Documentation hardware | ✅ Correcte | ⭐ Exceptionnelle | ⭐ Exceptionnelle (+ schémas détaillés) |
| Sortie D5/D6 | ❌ Jamais basculée | ⚠️ 1 Hz fixe | ✅ Toggle dynamique |
| Bugs critiques | 6+ | 3 | 2 |

### Verdict
**Architecture de premier niveau, bugs d'implémentation corrigeables.** qwen3.6 est le projet le plus complet et le mieux architecturé. Les deux bugs critiques (`digitalWrite` ISR + dangling `EEPROM.end()`) sont des erreurs simples à corriger — remplacer par registres GPIO directs et supprimer la ligne orpheline. Avec ces corrections, ce projet atteindrait facilement un **score de 8,0/10** et serait le plus compétitif pour une implémentation production-ready. La documentation hardware est d'un niveau professionnel qui pourrait servir de référence à toute l'équipe.
