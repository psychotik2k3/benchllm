# Analyse — deepseek4 flash

**Source :** `competitors/deepseek4 flash/deye_fan.ino` + `README.md` + `SCHEMA_ELECTRONIQUE.md`  
**Fichiers analysés :** 1 fichier source C++ (.ino), 1 README, 1 schéma électronique  
**Score global : 8.8 / 10**  
**Score qualité de code : 8.2 / 10**

---

## ✅ Positifs techniques

### Catégorie 1 — Temps-réel / ISR
- **[POSITIF] `IRAM_ATTR` sur les deux ISRs GPIO** (`isr_tach0`, `isr_tach1`) et sur l'ISR Timer1 (`app_timer1_isr`). Toutes les interruptions sont servies depuis la RAM IRAM. Point critique maîtrisé.
- **[POSITIF] NMI (Non-Maskable Interrupt) pour le Timer1 FRC1** : Le code utilise `NmiTimSetFunc()` de la ROM ESP8266 pour brancher l'ISR de sortie sur le vecteur NMI du FRC1. Un NMI **ne peut pas être masqué**, même par les sections critiques WiFi (`ETS_INTR_LOCK()`). Cela garantit qu'une activité réseau intense ne peut jamais retarder une bascule de sortie. C'est la méthode la plus robuste possible sur ESP8266 — bien supérieure à un Ticker software (gemma4, qwen35_pFC), à un timer ISR classique (qwen3.6), et même à `timer1_attachInterrupt()` (deepseek, fallback).
- **[POSITIF] Accès directs aux registres matériel dans l'ISR Timer1** : L'ISR `app_timer1_isr()` n'appelle AUCUNE fonction de bibliothèque. Elle manipule uniquement les registres (`REG_GPIO_W1TS`, `REG_GPIO_W1TC`, `REG_FRC1_LOAD`, `REG_FRC1_COUNT`). Zéro accès flash, zéro appel flottant, zéro allocation. C'est l'ISR la plus minimale et la plus sûre de tous les compétiteurs analysés.
- **[POSITIF] Registres GPIO définis par adresses du datasheet** : `REG_GPIO_OUT`, `REG_GPIO_W1TS`, `REG_GPIO_W1TC` sont adressés via leurs offsets directs (`0x60000304UL`, `0x60000308UL`). Indépendant du noyau Arduino, portable entre versions. Approche bare-metal rigoureuse.
- **[POSITIF] Anti-rebound par timestamp micros() avec plage bornée** : Comparaison avec `TACH_MIN_US = 400 µs` et `TACH_MAX_US = 120 s`. Les fronts trop rapides (rebond) ou trop lents (expiration fan) sont rejetés. Plage couvrant ~0,5 RPM à ~150 000 RPM — bien au-delà de toute plage réaliste pour des Noctua.
- **[POSITIF] Mesure micros() dans ISR GPIO** : Utilise `micros()` qui sur ESP8266 est une macro inline lisant le registre SysTick — pas d'appel fonction, donc pas d'éviction flash. Correct et sécurisé.

### Catégorie 2 — Démarrage / Boot
- **[POSITIF] EEPROM.begin(512) avec taille généreuse** : 512 octets EEPROM alloués, bien plus que les autres concurrents (gpt-free aussi utilise 512). Suffisant pour la structure `CfgT` complète.
- **[POSITIF] Signature magique CRC8 sur toute la structure de config** : `crc8()` calcule un checksum sur tous les champs sauf le byte CRC lui-même. Validation double : magic bytes (`"DF10"`) + CRC8. Protection contre EEPROM corrompue supérieure au simple magic number de qwen3.6/gpt-free (qui n'ont pas de CRC).
- **[POSITIF] Borne des ratios à la lecture + bornage des chaînes** : `cfgLoad()` applique les limites `RATIO_MIN_X100`/`RATIO_MAX_X100` ET tronque les chaînes SSID/password au-delà de leur taille max. Double protection contre données corrompues.
- **[POSITIF] Fallback automatique aux valeurs par défaut** : Si EEPROM invalide, `cfgDefaults()` + `cfgSave()` réécrit une config saine. Pas de reset manuel nécessaire.

### Catégorie 3 — Génération sortie / Timer1
- **[POSITIF] Sortie sur D5 ET D6 entièrement implémentée et fonctionnelle** : Deux sorties physiques (GPIO14/D5, GPIO12/D6) gérées par un seul ISR FRC1 en NMI. Contrairement à gemma4 (jamais basculé), qwen35 (fréquence fixe 1 Hz), ici la fréquence est proportionnelle au RPM × ratio mesuré en temps réel.
- **[POSITIF] Event-calendar avec re-armement manuel du compteur** : L'ISR calcule le prochain événement via `nd = g_nextTicks[X]`, puis écrit `REG_WR(REG_FRC1_LOAD, delta)` et `REG_WR(REG_FRC1_COUNT, delta)`. Le compteur 23-bit repart pour `delta` ticks. Pas d'auto-reload — le firmware contrôle précisément chaque tick. Méthode robuste évitant l'accumulation de dérive temporelle.
- **[POSITIF] Bascule XOR avec application différentielle W1TS/W1TC** : L'ISR compare le nouveau état logique (`out`) avec l'état matériel réel (`g_outHW`), ne写 que les bits qui changent via `diff & out` (W1TS) et `diff & ~out` (W1TC). C'est la méthode la plus efficace pour basculer des pins GPIO — zéro risque de lecture-modification-écriture incorrecte.
- **[POSITIF] Résolution timer FRC1 de 0,2 µs** : Diviseur /16 sur APB 80 MHz → tick de 0,2 µs. Précision 5× supérieure à gemini-free (50 µs) et 200× supérieure au besoin (~400 Hz max → période 2500 µs = 12 500 ticks).
- **[POSITIF] Fan stopped state correctement géré** : `g_halfTicks[ch] = 0` quand un fan est à l'arrêt. L'ISR ignore les voies avec demi-période nulle, gardant les sorties à leur état statique (HIGH par défaut après reset GPIO). Comportement conforme au tach NMB d'origine.
- **[POSITIF] Borne de demi-période : min 2 ticks (0,4 µs) / max 0x400000 (838 860 ticks ≈ 168 ms)** : Clamp efficace empêchant des fréquences hors-borne physiologiquement impossibles pour un tach fan.

### Catégorie 4 — Fidélité protocole / Fail-safe
- **[POSITIF] Stall detection adaptatif** : Timeout = `max(STALL_MIN_US, s_smUs[ch] * STALL_MULT)` où `STALL_MULT = 4`. Si le fan tourne lentement (période longue), le timeout s'adapte (4× période au lieu de 2s fixe). Plus intelligent que les timeouts fixes de gpt-free (500ms) et qwen3.6_prompt_from_claude (2s).
- **[POSITIF] PULSES_PER_REV = 2 constant nommée** : Le nombre d'impulsions par tour est défini avec un `#define` clair, pas une constante magique en dur dans les formules. Portabilité et lisibilité améliorées.

### Catégorie 5 — Filtres / Signal
- **[POSITIF] Filtre exponentiel (EMA) sur période mesurée** : `s_smUs[ch] = (((s_smUs * ((1<<EMA_SHIFT)-1)) + perUs) >> EMA_SHIFT)` avec `EMA_SHIFT = 3` → alpha = 1/8. Lissage progressif des périodes tach brutes avant conversion en demi-période de sortie. Filtre IIR réel sur des **périodes cycliques** (pas des compteurs cumulés comme qwen35). Correct et rigoureux.
- **[POSITIF] Conversion ratio x100 → Q16 fixe-point** : `ratioToQ16()` convertit le ratio décimal en format Q16 fixe (2,50 → 163840) pour éviter tout calcul flottant dans les canaux de sortie. Approche temps-réel optimale sur ESP8266 où le FP peut déclencher des appels à la flash.

### Catégorie 6 — WiFi / réseau
- **[POSITIF] `WiFi.setHostname("deye-fan")`** : Nom d'hôte configuré pour la résolution DNS. Pratique pour identifier l'appareil sur un réseau.
- **[POSITIF] mDNS activé et fonctionnel** : `MDNS.begin("deye-fan")` + `MDNS.addService("http", "tcp", 80)` — découverte Zeroconf réelle avec publication de service HTTP. Seul compétiteur (avec glm5.3) ayant un mDNS réellement activé dans le code visible.
- **[POSITIF] Reconnexion STA non-bloquante et périodique** : Tentative de reconnexion toutes les 20 secondes (`tWiFiRetry`) seulement si `WiFi.status() != WL_CONNECTED`. L'AP reste actif en permanence. Le signal simulé ne gèle PAS pendant la tentative (contrairement à qwen3.6 qui bloque ~10s).
- **[POSITIF] Mode AP+STA simultanée** : Les deux modes activés conjointement. L'utilisateur peut configurer via l'AP ou se connecter au réseau existant via STA.

### Catégorie 7 — Gestion mémoire / HEAP
- **[POSITIF] Page HTML stockée en PROGMEM** : `static const char PAGE_HTML[] PROGMEM` — la page web complète (~4 KB) tient en flash, pas en SRAM. Économie significative de mémoire volatile. Contrairement à qwen35 et gpt-free qui avaient du HTML en SRAM par String concat.
- **[POSITIF] JSON construit sur la pile (`char g_stateBuf[640]`)** : Le handler `/api/state` utilise un buffer statique de 640 octets sur la pile + `snprintf_P()` avec des chaînes formatées stockées en flash (`PSTR`). **Zéro allocation heap par requête JSON**. Excellent choix anti-fragmentation. Supérieur à StaticJsonDocument (qui alloque quand même le document object) et bien meilleur que String concatenation de tous les autres projets.
- **[POSITIF] `snprintf_P` au lieu de concaténation String** : Le format JSON est construit via deux appels `snprintf_P()` avec des templates PSTR. Le compilateur optimise les chaînes en flash, la pile fait le travail. Pattern très efficace sur ESP8266.
- **[POSITIF] Pas de `#include <ArduinoJson.h>` ni autre bibliothèque externe** : Tout est du code pur Arduino ESP8266 core — pas de dépendances à externaliser. Compilation directe avec l'IDE Arduino.

### Catégorie 8 — Interface Web
- **[POSITIF] Sliders interactifs pour le ratio en temps réel** : `input[type=range]` avec `oninput="sendR(0,this.value)"` déclenche un fetch AJAX sur `/api/set?c=0&r=250`. Le changement est immédiat (sans rechargement de page ni besoin d'appuyer sur "Enregistrer"). UX fluide et intuitive.
- **[POSITIF] API JSON `/api/state` complète** : Retourne les RPM réels/simulés, périodes en µs, état live/dead pour chaque voie, statut WiFi (STA connecté/non, RSSI, IP, clients AP), uptime. Format machine-readable compatible avec Home Assistant, Node-RED, etc.
- **[POSITIF] Validation bornée côté client et serveur** : `clampVal(v, 40, 2000)` dans le JavaScript + `clampU32()` côté serveur sur `/api/set`. Les ratios sont toujours bornés entre 0,40 et 20,0.
- **[POSITIF] Escaping HTML dans le JavaScript** : `esc(s)` remplace `<`, `>`, `&`, `"` pour éviter les injections XSS dans le rendu web. Bonne pratique de sécurité.
- **[POSITIF] Auto-refresh toutes les 1,5 secondes** : `setInterval(refresh, 1500)` — polling AJAX sans rechargement complet. Pas trop agressif (contrairement à qwen35 @100ms) ni trop lent (contrairement à gpt-free @500ms qui est correct mais ici 1,5s est un bon compromis).

### Catégorie 9 — Persistance / Stockage
- **[POSITIF] EEPROM avec CRC8 intégralité structure** : Le CRC8 couvre tous les champs de `CfgT` sauf le byte CRC lui-même. Validation d'intégrité supérieure au simple magic number (qwen3.6/gpt-free) car il détecte toute corruption partielle, pas seulement une signature manquante.
- **[POSITIF] Structure typée avec tailles protégées** : `char staSsid[33]`, `char staPw[65]` — respectent les limites WiFi (max 32 caractères SSID, max 63 caractères password). Aucune déborder possible. Bornage appliqué à la lecture aussi.
- **[POSITIF] `cfgSave()` calcule le CRC avant écriture** : Le checksum est toujours à jour lors de l'écriture EEPROM. Pas de risque d'écrire une structure corrompue.

### Catégorie 10 — Documentation hardware
- **[POSITIF] Schéma électronique complet et structuré** : `SCHEMA_ELECTRONIQUE.md` contient un schéma bloc vue d'ensemble, un tableau de câblage détaillé (borne ↔ signal ↔ destination), 4 sous-schémas (alimentation, entrée tach, sortie simulée, LED), une BOM complète avec références, valeurs, quantités et usage, procédure de raccordement réversible à l'onduleur, guide de calibration des ratios.
- **[POSITIF] Calculs de dimensionnement fournis** : Courant dans le clamp BAT54S en pic (1,8 mA pour 12V source, 4,3 mA pour 24V), courant de base NPN (2,6 mA), courant de collecteur (~1,2 mA avec pull-up 10k). Chaque choix de composant est justifié par un calcul.
- **[POSITIF] Tableau de correspondance tensions source ↔ tension GPIO** : Démonstration que le clamp protège efficacement le GPIO pour 3,3 V, 5 V et 12 V sources — même en cas de surtension jusqu'à 24 V.
- **[POSITIF] Notes techniques dans le README** : Explication du choix NMI, jitter ≈ unité compteur (0,2 µs), deux voies indépendantes sur un seul timer. Documentation claire pour un développeur.

### Catégorie 11 — Qualité de code générale
- **[POSITIF] Code bare-metal volontaire** : Les accès registres (FRC1, GPIO W1TS/W1TC) sont faits en adresses directes issues du datasheet ESP8266. Indépendant de la version du noyau Arduino. Le commentaire le mentionne explicitement. C'est une force pour un firmware temps-réel critique.
- **[POSITIF] Macros REG_WR/REG_RD génériques** : `#define REG_WR(a, v) (*((volatile uint32_t *)(a)) = (uint32_t)(v))` — abstraction propre des accès mémoire-mapped. Réutilisable et clair.
- **[POSITIF] Fonctions statiques pour la scope locale** : `cfgDefaults()`, `cfgLoad()`, `cfgSave()`, `crc8()`, `clampU32()`, `ratioToQ16()`, `usToRpm()` sont toutes `static`. Portée limitée à la translation unit. Bonne pratique C/C++.
- **[POSITIF] Captures d'état atomiques dans `handleState()`** : Copie de `g_snap` et des ratios sous `noInterrupts()`/`interrupts()` avant sérialisation JSON. Évite un snapshot partiellement corrompu en cas d'interrupt entre deux lectures. Approche soignée.
- **[POSITIF] Parser ratio `parseRatio100()` robuste** : Gère les formats "2", "2.5", "2.50", séparateur `,` ou `.`, ignore la 3ème décimale. Bien plus polyvalent qu'un simple `arg.toFloat()`.
- **[POSITIF] Code de fallback NMI bien documenté** : `#if USE_NMI_TIMER` / `#else timer1_attachInterrupt()` avec `#warning` explicite sur la moins bonne immunité WiFi en mode ISR normale. Transparency technique appréciée.

---

## ❌ Négatifs techniques

### Catégorie 1 — Temps-réel / ISR
- **[mineur] `micros()` dans les ISRs GPIO** : Sur ESP8266, `micros()` est une macro inline qui lit le registre SysTick — pas un appel fonction. Donc ce n'est PAS un bug (contrairement aux autres microcontrôleurs où micros() peut appeler du code en flash). Cependant, lire ce registre 20 000 fois/seconde (max pour des Noctua à ~15 Hz × 2 pulses) crée un overhead constant mais négligeable.
- **[mineur] Pas de section critique explicite pour les lectures ISR↔loop multiples** : Dans `updateOutChannel()`, le code fait `noInterrupts(); lastUs = g_inLastUs[ch]; perUs = g_inPeriodUs[ch]; interrupts();`. Sur ESP8266 (32-bit), chaque lecture individuelle est atomique mais deux lectures séquentielles séparées par un interrupt ISR peuvent capturer un état incohérent partiel. Le risque est faible (ISR tach → max 15 Hz) et acceptable ici.

### Catégorie 2 — Démarrage / Boot
- **[critique] Pas de watchdog (SW ou HW)** : Aucun hardware ou software watchdog implémenté. En cas de plantage WiFi, boucle infinie, ou crash ISR NMI, aucun reboot automatique. Point identique chez qwen3.6_prompt_from_claude et glm5.3 — c'est un manque significatif pour un appareil industriel dédié à la surveillance d'un onduleur.
- **[majeur] Mot de passe AP faible par défaut** : `WiFi.softAP(g_cfg.apSsid, "deye-fan")` — le mot de passe par défaut est 8 caractères (respects WPA2 minimum) mais c'est un mot de passe trivial générique. N'importe qui à proximité peut se connecter et modifier les ratios ou le WiFi STA. Bien que supérieur à `""` (qwen3.6) ou `"12345678"` (gemma4, qwen35, gemini-free), ce n'est toujours pas une bonne pratique de sécurité.
- **[mineur] `WiFi.disconnect()` puis `WiFi.begin()` dans la reconnexion STA** : La procédure de reconnexion périodique déconnecte d'abord le WiFi (`WiFi.disconnect()`) puis reconnecte. Pendant cette transition (quelques centaines de ms), le réseau est coupé — l'AP aussi. Le serveur web devient inaccessible pendant quelques centaines de ms. Moins pire qu'un blocage 10s (qwen3.6) mais crée quand même des micro-coupures. Une reconnexion `WiFi.reconnect()` sans `disconnect()` serait plus transparente.

### Catégorie 3 — Génération sortie / Timer1
- **[majeur] Le fallback `USE_NMI_TIMER=0` utilise `core_esp8266_timer.h` non fourni par défaut** : Quand `USE_NMI_TIMER=0`, le code inclut `<core_esp8266_timer.h>` et utilise `timer1_attachInterrupt()`. Ce header fait partie du core ESP8266 Arduino mais son API publique est limitée. Selon la version du core, `timer1_attachInterrupt()` peut ne pas être disponible ou avoir une signature différente. C'est moins robuste que le NMI (qui utilise une fonction ROM `NmiTimSetFunc()` toujours présente). Note : par défaut `USE_NMI_TIMER=1`, donc ce n'est un bug qu'en mode fallback manuel.
- **[mineur] Période minimale de sortie non bornée explicitement en fréquence** : La borne haute sur la demi-période (`0x400000` ticks ≈ 168 ms) correspond à ~3 Hz max (période 333 ms → ~90 RPM simulés). C'est une limite basse de fréquence, pas haute. La limite haute de fréquence (période minimale) est `2 ticks = 0,4 µs` → 2,5 MHz théorique. En pratique, la période minimale de sortie est contrainte par `half >= 2` mais le rapport RPM→demi-périade n'a pas de clamp sur le ratio maximal (20×). Un ratio de 20 sur un fan à 3000 RPM → simRPM 60 000 → période de sortie ~1 µs = 5 ticks. Still within bounds (`>= 2`), donc ce n'est PAS un bug. C'est correct.

### Catégorie 4 — Fidélité protocole / Fail-safe
- **[majeur] Pas de mode cache-panne** : Contrairement à glm5.3 qui implémentait un "mode cache-panne" (simulation de fréquence fixe configurable en cas de perte de signal), deepseek4 flash met la sortie à l'état repos (HIGH) dès qu'un fan est détecté arrêté. L'onduleur Deye recevra un tach "mort" et pourrait déclencher une alarme ventilation insuffisante au lieu d'accepter le ratio simulé. C'est un choix de conception (honnêteté du signal) mais pas de protection contre les faux négatifs/temporaires.
- **[mineur] `g_snap.live[]` mis à jour seulement toutes les secondes** : Le flag `live` est calculé dans `updateEverySecond()` qui tourne chaque 1000 ms. La LED et le web UI ont donc une latence de ~1s pour détecter un arrêt de fan. Acceptable mais moins rapide que la détection dans `updateOutChannel()` (qui tourne à chaque itération loop()).

### Catégorie 5 — Filtres / Signal
- **[majeur] EMA avec alpha = 1/8 potentiellement lent à réagir** : `EMA_SHIFT = 3` → alpha = 0,125. La constante de temps équivalente est ~8 mesures. Pour un fan dont la vitesse change rapidement (démarrage onduleur), il faudra ~40–60 ms pour que le filtre atteigne 90% de la valeur finale. Ce n'est pas incorrect mais moins réactif que gpt-free (alpha = 1/8 sur période, mais appliqué plus souvent par front) ou qwen3.6_pFC (double EMA avec α=0,3 + α=0,5).
- **[mineur] Pas de moyenne glissante en complément** : Contrairement à qwen3.6_pFC qui utilise triple lissage (moyenne glissante 4 échantillons + double IIR), deepseek4 flash n'a qu'un seul niveau d'EMA (α=0,125). Suffisant pour les Noctua (turbulence faible), mais moins robuste face à des bruits électriques occasionnels.

### Catégorie 6 — WiFi / réseau
- **[majeur] Pas d'authentification web** : N'importe qui connecté au réseau (AP ou STA) peut accéder à `/api/set`, modifier les ratios, et `/save` pour changer la configuration WiFi. Aucune protection par mot de passe sur le serveur web. Identique à tous les autres compétiteurs — c'est un manque récurrent mais notable.
- **[mineur] Pas d'OTA (Over-The-Air)** : Mises à jour firmware uniquement par câblage USB. Absent ici alors que qwen3.6_pFC et glm5.3 l'ont implémenté. Pour un dispositif installé en hauteur (boîtier onduleur), OTA rendrait le maintien beaucoup plus pratique.
- **[mineur] Pas de limitation de taille de requête POST** : `handleSave()` lit directement les arguments du formulaire via `server.arg()`. Aucune limite de longueur n'est appliquée avant `strncpy`. Un client malveillant pourrait envoyer un SSID très long (bien que `strncpy` avec `-1` limite à la taille du buffer, il ne rajoute pas le null terminator si le texte est plus long — le code fait bien `g_cfg.staSsid[sizeof(g_cfg.staSsid)-1] = 0;` donc c'est couvert). En fait, ce point est corrigé. Ce n'est PAS un bug.
- **[mineur] `WiFi.softAPgetStationNum()` retourné dans l'API JSON** : L'interface affiche le nombre de clients connectés à l'AP, mais ne montre pas le RSSI STA dans le HTML (seulement dans le JSON). Un petit texte dans la page web indiquant la qualité du lien STA serait utile.

### Catégorie 7 — Gestion mémoire / HEAP
- **[positif mineur : aucun négatif majeur]** — Le code est remarquablement sobre en heap : Zéro allocation dynamique pour le JSON, HTML en PROGMEM, structure de config sur la pile. Le seul usage heap potentiel provient du web server TCP buffers internes à ESP8266WebServer (inévitable et contrôlé par le core).

### Catégorie 8 — Interface Web
- **[majeur] Formulaire POST expose les passwords en clair dans l'API state** : `handleState()` retourne `"pass":"%s"` et `"passAp":"%s"` dans le JSON, envoyant les mots de passe STA/AP en clair au navigateur. Le HTML affiche ensuite ces valeurs dans des champs `<input>` via JavaScript. Toute capture d'écran ou log réseau expose ces credentials. Contrairement à qwen3.6_pFC qui masque le password STA avec `"••••••••"`. Risque de sécurité réel.
- **[mineur] Pas de thème sombre** : Interface en clair (`background:#f7f7f2`, `color:#222`). Fonctionnel mais moins moderne que les dark themes de qwen3.6, gemini-free et gpt-free.
- **[mineur] Pas d'indicateur RSSI dans le HTML visible** : Le RSSI est présent dans le JSON `/api/state` mais pas affiché dans la page HTML (`<div class="card"><b>Wi-Fi & système</b></div>`). L'utilisateur ne voit pas la qualité de sa connexion STA.
- **[mineur] Sliders sans feedback visuel immédiat de la nouvelle valeur** : `oninput="sendR(0,this.value)"` envoie un AJAX mais n'affiche pas le ratio actuel sous le slider. Le label `#rv0` est mis à jour dans `refresh()` (1,5s plus tard). Il y a un délai perceptible entre le drag du slider et la confirmation visuelle.

### Catégorie 9 — Persistance / Stockage
- **[majeur] Ratios non persistés lors du réglage rapide (`/api/set`)** : Le endpoint `/api/set` met à jour `g_ratioX100[ch]` ET `g_cfg.ratioX100[ch]` en RAM mais ne fait PAS d'écriture EEPROM immédiate. Si l'appareil plante entre deux `cfgSave()` manuels (bouton "Enregistrer"), les réglages rapides sont perdus. Ce n'est pas un bug fonctionnel (l'intention est de persister seulement sur demande explicite), mais cela peut frustrer un utilisateur qui ajuste des ratios et subit un reboot inopiné.
- **[critique] `EEPROM.commit()` bloquant sans protection contre les écritures multiples** : `cfgSave()` appelle `EEPROM.commit()` (~10ms bloquant) après chaque POST `/save`. Si l'utilisateur clique rapidement plusieurs fois sur "Enregistrer", les commits s'enchaînent. Chaque commit use un cycle d'écriture EEPROM (~100 000 cycles max). Pas de cooldown ni debounce côté client ou serveur pour limiter les écritures. Identique à gpt-free.

### Catégorie 10 — Documentation hardware
- **[positif mineur : aucun négatif notable]** — La documentation est exceptionnelle, au niveau professionnel. Schéma bloc + sous-schémas détaillés + BOM complète + calculs de dimensionnement + procédure de calibration. Pas de critique pertinente ici.

### Catégorie 11 — Qualité de code générale
- **[majeur] Exposition des passwords en clair dans l'API JSON** : `handleState()` retourne `"pass":"%s"` et `"passAp":"%s"` avec les mots de passe STA/AP non masqués dans le body JSON. Le JavaScript affiche ensuite ces valeurs en clair dans des champs `<input>` via `.value`. Toute interception réseau ou capture d'écran expose ces credentials. Qwen3.6_pFC masque correctement avec `"••••••••"`. C'est un manque de sécurité notable.
- **[majeur] `handleSave()` applique les ratios sans validation serveur complète** : Le handler `/save` lit les ratios avec `parseRatio100()` et vérifie les bornes, mais ne valide pas la présence/validité des champs SSID/password (une chaîne vide est acceptée, ce qui est correct pour désactiver le STA). En fait, c'est acceptable — une chaîne vide = pas de tentative STA. Pas un bug, juste un point UX mineur.
- **[mineur] Variables globales sans encapsulement** : Toutes les variables d'état (`g_nowTicks`, `g_lastDelta`, etc.) sont au scope global. Acceptable pour un projet ESP8266 de cette taille (~350 lignes de state), mais manque d'encapsulement. Le pattern `typedef struct SnapT` aide部分iellement.
- **[critique] Code commenté en français mais avec anglais technique** : Le code alterne entre commentaires français ("démarrage", "pret") et anglais ("demarrage" avec accent manquante, "pret" au lieu de "prêt"). Incohérence linguistique mineure dans les chaînes Serial.print et commentaires. Pas un bug fonctionnel mais manque de rigueur stylistique.
- **[critique] `g_stateBuf[640]` — taille statique peut être dépassée** : Le buffer JSON est de 640 octets. Avec SSID/password longs, le JSON complet (ratios + RPM + WiFi info) peut dépasser 640 octets. Le code applique un check `if ((size_t)n >= sizeof(g_stateBuf)) n = sizeof(g_stateBuf) - 1;` puis tronque en coupant à mi-JSON (`...}` incomplet). Cela produirait un JSON malformed côté client, cassant le parsing JavaScript. Si SSID/password sont courts (<32 caractères), c'est largement suffisant, mais pas garanti pour des configurations avec de longs noms WiFi. Un `DynamicJsonDocument` ou un buffer plus grand (1024) serait plus sûr.

---

## Synthèse

**Ce projet est le plus abouti techniquement parmi tous les compétiteurs analysés.** L'utilisation du NMI (Non-Maskable Interrupt) pour le Timer1 FRC1 garantit qu'aucune activité WiFi ne peut retarder une bascule de sortie — c'est la méthode la plus robuste disponible sur ESP8266. L'ISR manipule uniquement des registres matériels bruts (W1TS/W1TC, event-calendar), zéro appel bibliothèque, zéro accès flash. Le calcul fixe-point Q16, le buffer JSON sur pile (`g_stateBuf[640]` + `snprintf_P`), et les adresses registre datasheet directes font de ce code un firmware temps-réel de qualité professionnelle. La documentation hardware est exceptionnelle (schémas, BOM, calculs, calibration).

**Cependant, deux faiblesses notables empêchent un score parfait :**

1. **Passwords exposés en clair dans l'API JSON** : `handleState()` retourne les mots de passe STA/AP sans masquage, contrairement à qwen3.6_pFC qui utilise `"••••••••"`. Risque de sécurité réel pour toute interception réseau ou capture d'écran.
2. **Pas de watchdog (SW ou HW)** : Aucun reboot automatique en cas de crash WiFi, loop infinie, ou bug non détecté. Manquant critique pour un appareil industriel dédié à la surveillance d'un onduleur.

### Points forts réels
- **NMI FRC1** pour génération sortie — immunité absolue aux sections critiques WiFi (meilleur que tous les autres)
- **Accès registres bare-metal** dans l'ISR (W1TS/W1TC, event-calendar) — zéro dépendance noyau Arduino
- **Buffer JSON sur pile + `snprintf_P`** — ZÉRO allocation heap par requête, anti-fragmentation parfaite
- **CRC8 intégralité structure EEPROM** — détection de corruption supérieure au simple magic number
- **EMA IIR réel sur périodes cycliques** — pas le faux filtre de qwen35
- **Q16 fixed-point** pour ratios — zéro flottant dans les canaux temps-réel
- **mDNS activé et fonctionnel** (`MDNS.begin` + `addService`) — Zeroconf réel
- **Reconnexion STA non-bloquante** (20s periodic) — signal simulé ne gèle PAS
- **HTML en PROGMEM** — économie ~4 KB SRAM
- **Documentation hardware exceptionnelle** (schémas, BOM, calculs, calibration)
- **Sliders interactifs AJAX** pour ratio temps-réel sans rechargement page
- **Validation bornée côté client ET serveur** sur les ratios

### Faiblesses critiques
- **Passwords exposés en clair dans API JSON** (`handleState` → `"pass":"%s"`) — risque sécurité réel
- **Pas de watchdog (SW ou HW)** — pas de reboot automatique en cas de crash
- `g_stateBuf[640]` potentiellement trop petit pour SSID/password longs → JSON tronqué malformed
- Ratio rapide (`/api/set`) non persisté immédiatement en EEPROM (uniquement au bouton "Enregistrer")
- **Pas d'OTA** — mises à jour uniquement par câblage USB
- **Pas d'authentification web** — configuration modifiable par n'importe qui sur le réseau
- EMA α=1/8 potentiellement lent à réagir (~8 mesures de constante de temps)
- Mot de passe AP faible par défaut (`"deye-fan"`)
- LED live/dead latence ~1s (mise à jour uniquement dans `updateEverySecond()`)

### Comparaison avec les autres compétiteurs

| Aspect | gemma4 (3.2) | qwen35 (4.8) | qwen3.6 (7.2) | gemini-free (5.3) | gpt-free (7.5) | qwen3.6_pFC (8.2) | glm5.3 (8.0) | **deepseek4 flash (8.8)** |
|--------|-------------|-------------|---------------|-------------------|----------------|--------------------|--------------|---------------------------|
| ISR IRAM_ATTR | ❌ Absent | ✅ Présent | ✅ Présent | ✅ Sur 3 ISRs | ✅ Sur 2 ISRs | ✅ Sur 2 ISrs | ⚠️ Noyau-attaché | ✅ Sur 3 ISRs |
| Sortie timer | ❌ Ticker SW | ⚠️ 1 Hz fixe | HW timer 1 µs | Waveform gen 50 µs | Dep. external hdr | Ticker @20kHz | FRC1 + CCOUNT | **NMI FRC1 + registres bruts** |
| Immunité WiFi ISR | N/A | ⚠️ Timer normal | HW timer isolé | Waveform matériel | External header | Software (Ticker) | FRC1 non-NMI | **NMI — immunité absolue** |
| Signal D5/D6 | ❌ Jamais | ⚠️ 1 Hz fixe | ✅ Proportionnel | ✅ Proportionnel | ✅ Proportionnel | ✅ Proportionnel | ✅ Proportionnel | ✅ **Proportionnel + NMI** |
| Filtres signal | ❌ Aucun | ⚠️ Fenêtre (faux) | ⚠️ Anti-rebound | ❌ Aucun | IIR 7/8+1/8 | Moyenne + double IIR | EMA α=0.25 | ✅ **EMA α=0.125** |
| Persistance | ❌ Aucune | ❌ SPIFFS cassé | ✅ EEPROM mag | ✅ LittleFS JSON | ✅ EEPROM magic | ✅ LittleFS JSON | ✅ EEPROM + CRC32 | ✅ **EEPROM + CRC8 intégral** |
| JSON HEAP | ❌ String concat | ❌ String 10Hz | ✅ OK | ✅ StaticJsonDocument | ❌ String concat | ⚠️ DynamicJson+char[] mixte | ❌ 0 libs (core) | ✅ **Pile statique + snprintf_P** |
| Docs hardware | ✅ Correcte | ⭐ Exceptionnelle | ⭐ Exceptionnelle | ❌ Aucune | ❌ Limitée | ⭐ Exceptionnelle | ⭐ Exceptionnelle | ⭐ **Exceptionnelle** |
| mDNS | ❌ | ❌ | ❌ | ❌ | ❌ | ⚠️ Inclus non actif | ✅ Actif | ✅ **Actif + addService** |
| OTA | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ Intégré | ❌ 0 libs | ❌ Absent |
| Password API | ✅ Masqué (pFC) | N/A | N/A | N/A | N/A | ✅ Masqué (pFC) | N/A | ❌ **En clair** |
| Bugs critiques | 6+ | 3 | 2 | 2 | 1 (header) | ~1 (GPOS SDK) | 0 fonctionnels | **~1 (watchdog + password)** |

### Verdict
**Architecture temps-réel la plus rigoureuse jamais vue parmi les compétiteurs.** Le NMI FRC1 garantit une immunité totale aux interférences WiFi — un avantage unique qui ne sera jamais égalé par Ticker software, waveform generator, ou timer ISR classique. Les accès registres bare-metal dans l'ISR (W1TS/W1TC direct), le formatage JSON sur pile avec `snprintf_P`, et le CRC8 intégral font de ce projet un firmware de qualité professionnelle, proche du production-ready. 

**Avec la correction de deux points — masquage des passwords dans l'API JSON (`"••••••••"` comme qwen3.6_pFC) et ajout d'un watchdog software (ex: feed périodique via `wdt_enable()` ou un timer de timeout en loop) — ce projet atteindrait un score de 9,2–9,5/10.**

Le buffer JSON `g_stateBuf[640]` pourrait aussi être augmenté à 1024 pour garantir qu'aucun SSID/password long ne produce un JSON tronqué. Ce sont des corrections triviales qui n'affectent pas l'excellente architecture sous-jacente.
