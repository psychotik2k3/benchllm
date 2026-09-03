# Analyse — claude-sonnet5-free

**Source :** `competitors/claude-sonnet5-free/deye_fan.ino` + `schema_electronique.md`  
**Fichiers analysés :** 1 fichier source C++ (.ino), 1 fichier documentation technique (.md)  
**Score global : 8,5 / 10**  
**Score qualité de code : 8,2 / 10**

---

## ✅ Positifs techniques

### Catégorie 1 — Temps-réel / ISR

- **[POSITIF] `ICACHE_RAM_ATTR` sur les 3 ISRs** (`onTimer1ISR`, `isrTachA`, `isrTachB`) — toutes les interruptions sont servies depuis la RAM IRAM. Point critique maîtrisé pour l'ensemble des handlers.
- **[POSITIF] Timer1 matériel en mode périodique (TIM_LOOP)** : Configuration `timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP)` avec `TIMER1_TICKS_PER_ISR = 500` ticks → interruption toutes les **100 µs** exactement. Le timer tourne indépendamment du CPU et du WiFi, assurant une génération de signal stable sans jitter réseau. Approche rigoureuse et explicitement documentée dans un diagramme d'architecture détaillé en entête du fichier source.
- **[POSITIF] Écriture directe registre GPIO (GPOS/GPOC) dans l'ISR Timer1** : Contrairement à qwen3.6 (`digitalWrite()` dans ISR), deepseek4 flash et gemini-free, ce projet utilise `GPOS = (1UL << pin)` et `GPOC = (1UL << pin)` — écriture directe au registre, quelques cycles CPU, aucun appel fonction. **Bug critique de qwen3.6/gemini-free corrigé.**
- **[POSITIF] Mechanisme countdown + reset propre** : Chaque sortie utilise un compteur `chCountdown100us[ch]` qui est réinitialisé à `chHalfPeriod100us[ch]` après chaque bascule (au lieu d'une soustraction incrémentale). Méthode « nextTick absolu » équivalente à celle de qwen3.6, évitant l'accumulation de dérive temporelle.
- **[POSITIF] Handlers ISR entrée minimaux et non-bloquants** : `handleTachEdge()` effectue une seule lecture `micros()`, un calcul de différence unsigned (gère correctement le wrap-around), un test de plage, et assignment de 3 variables volatiles. Pas de Serial.print, pas d'allocations, pas de divisions.
- **[POSITIF] Calcul RPM déporté dans loop()** : La boucle principale calcule les RPM et met à jour les cibles de sortie toutes les ~150 ms. L'ISR Timer1 se contente uniquement des bascules GPIO — **zéro calcul flottant dans l'ISR**, optimal pour ESP8266 où le FP peut déclencher des appels en flash.
- **[POSITIF] Désactivation modem sleep WiFi** : `WiFi.setSleepMode(WIFI_NONE_SLEEP)` évite les rafales RF périodiques qui retarderaient de quelques centaines de µs les interruptions logicielles. Mentionnée explicitement dans la section d'architecture comme stratégie anti-gigue.
- **[POSITIF] Overclock CPU à 160 MHz** : `system_update_cpu_freq(SYS_CPU_160MHZ)` double la capacité de traitement pour maximiser la marge entre deux ticks Timer1. Gain significatif pour le serveur web et les calculs loop().
- **[POSITIF] Commentaire d'architecture explicite sur l'isolation temps-réel** : La section entête documente honnêtement les limites (le modem WiFi peut masquer ISR pendant quelques µs) mais montre que l'auteur a conscience des compromis ESP8266 mono-cœur. Cette transparence architecturale est rare et précieuse.

### Catégorie 2 — Démarrage / Boot

- **[POSITIF] EEPROM.begin() avec taille exacte du struct Config** : `EEPROM.begin(sizeof(Config))` alloue exactement la taille nécessaire (pas surallocation comme les concurrents qui hardcodent 512 octets). Plus élégant et maintenable.
- **[POSITIF] Signature magique EEPROM (`0xDEE5FA20UL`)** : Validation d'intégrité au boot. Fallback automatique vers config par défaut si invalide. Équivalent fonctionnel du `0xDE 0xAD` de qwen3.6 et du CRC8 de deepseek4 flash.
- **[POSITIF] Checksum calculé sur l'intégralité de la structure** : `calcChecksum()` parcourt tous les bytes sauf le champ checksum lui-même avec pondération par indice (`i + 1`). La pondération rend le checksum plus robuste qu'un simple CRC8 ou somme additive — un byte déplacé entre deux positions produira un checksum différent. Approche originale et rigoureuse.
- **[POSITIF] `EEPROM.end()` dans les deux fonctions** : Libération explicite de l'handle EEPROM après lecture/écriture. Bonne pratique (bien que certains pourraient argumenter qu'elle n'est pas nécessaire sur ESP8266).
- **[POSITIF] Fallback AP toujours actif** : L'AP est démarré en permanence (`WiFi.softAP(config.ap_ssid, config.ap_pass)`), assurant un canal de configuration alternatif même si le STA échoue.

### Catégorie 3 — Génération sortie / Timer1

- **[POSITIF] Sortie sur D5 ET D6 entièrement implémentée et fonctionnelle** : Deux sorties physiques (GPIO14/D5, GPIO12/D6) gérées par un seul ISR Timer1 à 10kHz. Contrairement à gemma4 (jamais basculé), qwen35 (fréquence fixe 1 Hz), ici la fréquence est proportionnelle au RPM × ratio mesuré en temps réel.
- **[POSITIF] Approche countdown absolu** : `chCountdown100us[ch]` est réinitialisé à `chHalfPeriod100us[ch]` après chaque bascule, pas incrémenté. Méthode robuste évitant l'accumulation de dérive temporelle (exactement comme qwen3.6).
- **[POSITIF] Fan stopped state correctement géré** : Quand un fan est détecté à l'arrêt (`haveSignal == false`), `setChannelOutput(ch, false, 0)` appelle `GPOC = (1UL << pin)` pour mettre le pin LOW permanent → transistor NPN de sortie bloqué → ligne tirée au niveau haut par le pull-up interne du Deye. Comportement correct et cohérent avec un tach NMB d'origine (ligne flottante/pull-up quand ventilateur mort).
- **[POSITIF] Résolution 100 µs bien dimensionnée** : Pour des fréquences Noctua typiques (3–50 Hz), une résolution de 100 µs (= demi-tick) offre une précision d'au maximum ±2,5% par demi-période. Plus que suffisant pour un onduleur Deye qui a des plages de tolérance larges.
- **[POSITIF] Borne haute de fréquence applicable** : `MIN_OUT_HALF_PERIOD_100US = 10` → période complète minimale 20 × 100µs = 2ms → 500 Hz max simulés → ~15 000 RPM simulés (pour PULSES_PER_REV=2). Plafond généreux couvrant tout le spectre Noctua + marge de ratio×10.
- **[POSITIF] Calcul de période sortie direct sans repasser par RPM** : `periodOutUs = periodInUs / ratio` — applique le multiplicateur directement sur la période cyclique mesurée, pas via une conversion intermédiaire RPM (qui introduirait des erreurs d'arrondi en chaîne). Approche plus précise que les projets qui font `RPM × ratio → nouvelle période`.

### Catégorie 4 — Fidélité protocole / Fail-safe

- **[POSITIF] Stall detection avec timeout configurable** : `TACH_TIMEOUT_MS = 3000 ms` (3 secondes). Si aucun pulse reçu pendant 3 secondes, le canal est mis à l'arrêt. Timeout raisonnable pour des Noctua (qui ne descendent jamais en dessous de ~800 RPM en fonctionnement normal → période < 125 ms × 2 = 250 ms entre pulses). Plus conservateur que gemini-free (1s) mais cohérent avec un onduleur industriel.
- **[POSITIF] Anti-rebound par timestamp micros()** : `TACH_MIN_PERIOD_US = 2000 µs` (= 2 ms) dans l'ISR. Ignore les fronts trop rapprochés (< ~15 000 RPM théorique). Rejet immédiat dans l'ISR, pas de post-traitement. Correct pour éliminer les rebonds mécaniques/électriques.
- **[POSITIF] Gestion propre des cas « absence de signal »** : Quand `haveSignal == false`, le code remet tout à zéro (`rpmIn[ch]=0`, `rpmOut[ch]=0`, `setChannelOutput(ch, false)`). L'onduleur reçoit un niveau haut statique = état ventilateur mort. Logique cohérente et prévisible.

### Catégorie 6 — WiFi / Réseau

- **[POSITIF] Mode AP+STA simultanée** : `WiFi.mode(WIFI_AP_STA)` avec tentative STA non-bloquante au boot. Si SSID configuré, connexion automatique à la reconnexion hardware.
- **[POSITIF] Mot de passe AP valide (13 caractères)** : `"changeme123"` respecte le minimum WPA2 de 8 caractères et est plus sécurisant que `\"12345678\"` ou `\"deye-fan\"`. Pas parfait mais correct.
- **[POSITIF] Mot de passe AP modifiable via web UI** : L'interface permet de changer le mot de passe AP (avec validation min 8 caractères). Pratique pour la sécurité opérationnelle.
- **[POSITIF] `system_update_cpu_freq(160)` + `WIFI_NONE_SLEEP` documentés** : Les choix de performance WiFi sont explicitement justifiés dans le commentaire d'architecture. Transparency technique appréciée.

### Catégorie 7 — Gestion mémoire / HEAP

- **[POSITIF] Buffer JSON sur pile (`char json[220]`, `char json[256]`)** : Les handlers `/status` et `/config` utilisent des buffers statiques sur la pile + `snprintf()`. **Zéro allocation heap par requête**. Anti-fragmentation parfaite — supérieur à String concatenation de gemma4/qwen3.6_prompt_from_claude.
- **[POSITIF] HTML stocké en PROGMEM** : `const char PAGE_MAIN[] PROGMEM = R"HTMLPAGE(...)"` — la page web tient en flash, pas en SRAM. Économie significative de mémoire volatile (~5 KB). Contrairement à gpt-free qui construisait une grande String heap en SRAM.
- **[POSITIF] `server.send_P()` pour HTML + `server.send()` pour JSON** : API web ESP8266WebServer utilisée correctement — `send_P` pour les ressources PROGMEM, `send` pour les responses dynamiques. Pattern propre et standard.
- **[POSITIF] Pas de bibliothèque externe** : Compilation pure Arduino ESP8266 core (ESP8266WiFi.h + ESP8266WebServer.h + EEPROM.h). Zéro dépendance à externaliser. Plus portable que gemini-free (AsyncWebServer+ArduinoJson) et qwen3.6_pFC (ArduinoOTA+mMDNS).

### Catégorie 8 — Interface Web

- **[POSITIF] Design dark theme moderne** : Fond `#111`, texte `#eee`, bordures `#333`/`#444`, badges colorés (`#264` vert pour OK, `#742` orange pour attente). Rendu visuel soigné et cohérent. CSS minimaliste mais efficace.
- **[POSITIF] AJAX JSON pour le monitoring en temps réel** : `setInterval(refreshStatus, 1000)` — polling toutes les secondes vers `/status`. Pas de rechargement complet de page. Flux de données fluide.
- **[POSITIF] Formulaire de configuration via JavaScript fetch + URLSearchParams** : `handleSave()` lit les paramètres POST, valide et applique les ratios/bornes. Reboot différé non-bloquant (flag `needRestart` traité dans `loop()` après 2 secondes). UX moderne et fluide.
- **[POSITIF] Chargement config au démarrage page** : `loadConfig()` fait un GET sur `/config` pour pré-remplir les champs HTML. L'utilisateur voit la configuration courante immédiatement.
- **[POSITIF] Validation des ratios côté serveur** : Les ratios sont bornés entre 0,1 et 10,0 dans le handler `/save`. Protection contre valeurs aberrantes envoyées par requête brute.
- **[POSITIF] Bouton « Enregistrer » avec feedback textuel** : Le message de confirmation s'affiche après le POST. UX soignée.

### Catégorie 9 — Persistance / Stockage

- **[POSITIF] EEPROM pour configuration complète** : Ratios + WiFi STA/AP persistés dans un struct typé unique (`Config`). Lecture et écriture atomique via `EEPROM.put()`/`EEPROM.get()`. Signature magique + checksum sur intégralité structure.
- **[POSITIF] Checksum pondéré par indice** : Le poids multiplicatif `(i + 1)` rend le checksum plus robuste qu'une simple somme — un byte déplacé changera le résultat. Approche原创 et rigoureuse pour la détection de corruption EEPROM.

### Catégorie 10 — Documentation hardware

- **[POSITIF] Schéma électronique complet et structuré** : `schema_electronique.md` contient une vue d'ensemble avec ASCII art, une BOM détaillée avec 15+ composants (réf, valeur, rôle, disponibilité), un bloc alimentation (OU schottky + buck), un étage d'entrée NPN (BC547 avec pull-up RC et filtre C), un étage de sortie open-collector, un schéma LED, et un brochage complet. Niveau professionnel comparable à qwen3.6_pFC et deepseek4 flash.
- **[POSITIF] Explication détaillée du fonctionnement par bloc** : Chaque sous-circuit (alimentation, entrée, sortie, LED) est documenté avec le fonctionnement étape par étape (« Tach au repos → base de Q1 polarisée → Q1 saturé → collecteur proche de 0V → GPIO lit un niveau BAS »). Pédagogique et utile pour un DIY.
- **[POSITIF] Calculs de dimensionnement implicites** : Les valeurs des composants (Rb=10k, Rc=4.7k, Rprot=220Ω, C_filt=1nF, Rbe=10k) sont cohérentes pour un signal Noctua 12V → GPIO 3,3V. La fonctionnalité « collecteur ouvert compatible pull-up inconnu (3,3/5/12V) » est explicitement expliquée.
- **[POSITIF] Points d'attention / mise en service** : Section pratique avec choix du ratio, connecteurs Y pour alimenter 2 Noctua par connecteur d'origine, et avertissement de sécurité fonctionnelle (l'onduleur ne détectera plus les pannes réelles).

### Catégorie 11 — Qualité de code générale

- **[POSITIF] Architecture explicite et documentée en entête** : Le fichier commence par un diagramme ASCII détaillé expliquant l'isolation temps-réel (Timer1 matériel vs loop web vs interruptions GPIO), les limites ESP8266 mono-cœur, et la justification des choix architecturaux. Rare de trouver une telle documentation inline dans un projet ESP8266.
- **[POSITIF] Sections commentées numérotées** : Délimitations par `// ==================== SECTION ===`. Lisibilité exceptionnelle. Structure claire séparant EEPROM, ISR, Timer1, Web, Setup/Loop.
- **[POSITIF] Constantes bien nommées et groupées** : Pin definitions groupées au début, paramètres tach groupés, paramètre EEPROM groupé. Pas de constantes magiques dispersées dans le code.
- **[POSITIF] Variables `static const` pour les noms de canal** : `static const char *CHANNEL_NAME[NUM_CHANNELS]` — portée locale à la translation unit, pas pollué l'espace global. Bonne pratique C++.
- **[POSITIF] Reboot différé non-bloquant (2 secondes)** : Le handler `/save` ne fait pas `ESP.restart()` immédiatement. Il pose un flag `needRestart` + timestamp dans 2 secondes, traité proprement dans `loop()`. Garantit que la réponse HTTP arrive avant le reboot. Approche mature — identique à qwen3.6_pFC (qui utilisait 200ms).
- **[POSITIF] Calcul de RPM direct sur période cyclique** : `rpmIn = 60000000.0f / period` — formule correcte et optimisée (pas de FP dans l'ISR, le calcul est fait uniquement dans loop() via un seul `60000000.0f / (float)period`). Pas d'étape intermédiaire par RPM × ratio qui cumulait les erreurs d'arrondi.

---

## ❌ Négatifs techniques

### Catégorie 1 — Temps-réel / ISR

- **[majeur] Macro `GPOS`/`GPOC` non définie dans le fichier source** : Le code utilise `GPOS = (1UL << pin)` et `GPOC = (1UL << pin)` mais ces symboles ne sont ni inclus ni définis explicitement. Sur ESP8266 Arduino core, `GPOS` est un alias de registre du SDK bare-metal (`#define GPOS (*(&GPIO_OUT_SET0) + 0)`). Le header `user_interface.h` est inclus via `extern "C"`, mais `GPOS`/`GPOC` ne font PAS partie des headers standard Arduino. **Cela dépend de la version du core ESP8266** — certaines versions les définissent, d'autres non. Risque de compilation modéré mais réel. À comparer avec qwen3.6_pFC qui utilise exactement le même pattern (mêmes symboles SDK bare-metal). Le risque est similaire à celui signalé chez qwen3.6_pFC (~1 bug critique « GPOS non défini selon core »).
- **[mineur] `micros()` dans ISR entrée** : Sur ESP8266, `micros()` est une macro inline lisant le registre SysTick — pas d'appel fonction, donc pas d'éviction flash. Correct mais moins précis que CCOUNT (gpt-free). L'overhead est négligeable car les fronts Noctua sont rares (< 100 Hz max → < 200 appels ISR/seconde × 2 canaux).
- **[mineur] Pas de section critique pour les lectures ISR↔loop multiples** : `updateChannel()` lit `lastPeriodMicros` et `lastPulseMillis` sous `noInterrupts()`/`interrupts()`, ce qui est correct. Mais ces variables sont des `volatile uint32_t` — lecture 4-byte atomique sur ESP8266 (32-bit). Le risque de race entre deux lectures séquentielles séparées par un interrupt est théoriquement faible car les ISR entrée tournent à < 100 Hz max. Acceptable ici.

### Catégorie 2 — Démarrage / Boot

- **[critique] Pas de watchdog (SW ou HW)** : Aucun hardware ou software watchdog implémenté. En cas de plantage WiFi, loop infinie, ou bug non détecté, aucun reboot automatique. Manquant critique pour un appareil industriel dédié à un onduleur. Identique à qwen3.6_pFC et glm5.3 — c'est le point faible récurrent de tous les projets haut de gamme.
- **[majeur] Pas de reconnexion WiFi automatique après perte du réseau STA** : `WiFi.begin()` est appelé une seule fois au boot dans `setup()`. Si la connexion est perdue plus tard (router redémarré, hors couverture), le device reste sur le réseau jusqu'à ce qu'un reboot physique soit effectué. Contrairement à deepseek4 flash qui tente une reconnexion périodique toutes les 20 secondes dans `loop()`. Ce projet n'a **aucun mécanisme de reconnexion**. La LED indiquera « attente » si le canal tach n'est pas détecté, mais l'utilisateur ne sait même pas que le WiFi est coupé.
- **[mineur] Mot de passe AP par défaut `changeme123` reste hardcoded** : Même s'il est plus sécurisé que `12345678`, c'est un mot de passe par défaut facile à deviner et connu publiquement (documenté dans le code). Un utilisateur ne changeant jamais son AP password, n'importe qui dans la zone de couverture peut se connecter au réseau « DeyeFanCtrl » et accéder à l'interface web.

### Catégorie 3 — Génération sortie / Timer1

- **[majeur] Résolution du timer de 100 µs (vs 1 µs chez qwen3.6)** : `TIMER1_TICKS_PER_ISR = 500` avec prescaler DIV16 (5 MHz) → tick de 100 µs. À haute RPM (ex: Noctua à 2500 RPM × ratio 4,0 → simRPM ≈ 10 000), la demi-période simulée est d'environ 50 µs = **moins d'un seul tick**. La borne `MIN_OUT_HALF_PERIOD_100US = 10` (= 1 ms) limite la fréquence de sortie à 500 Hz max. Pour un fan Noctua à 200 RPM × ratio 4 → simRPM ≈ 800 → période simulée 750 µs → demi-période 375 µs = **3,75 ticks**. Le rounding à 3 ou 4 ticks introduit une erreur de ±13%. À haute vitesse, l'erreur relative augmente. **Comparé à qwen3.6 qui utilisait un timer avec résolution ~0,2 µs (DIV16 direct), la précision est nettement inférieure.**
- **[mineur] Période minimale de sortie bornée arbitrairement** : `MIN_OUT_HALF_PERIOD_100US = 10` (= 1 ms → 500 Hz max) peut être trop restrictif pour des combinaisons ratio×RPM élevées. Un fan à 2000 RPM × ratio 8 → simRPM ≈ 16 000, période simulée 37,5 µs, demi-période 18,75 µs = **~0,19 tick** — bien en-deçà du minimum de 10 ticks. Le pin toggle à 500 Hz maximum au lieu des ~16 000 RPM simulés théoriques. L'onduleur pourrait détecter un décalage de fréquence pour les ratios élevés.

### Catégorie 4 — Fidélité protocole / Fail-safe

- **[majeur] Pas de détection d'erreur de ratio cohérent** : Les ratios sont bornés entre 0,1 et 10,0 mais aucune vérification n'est faite pour détecter si un ratio×RPM produit une fréquence hors-borne physiologique. Par exemple, ratio 10 sur un fan à 50 RPM → simRPM = 500 (cohérent), mais ratio 10 sur un fan à 3000 RPM → simRPM ≈ 30 000 (physiologiquement impossible pour un ventilateur). L'onduleur Deye peut tolérer cela, mais le firmware ne signale pas cette incohérence.
- **[mineur] Anti-rebound de 2 ms trop large pour basses fréquences** : `TACH_MIN_PERIOD_US = 2000 µs` signifie que tout pulse arrivant dans les 2 ms suivant le précédent est ignoré. Pour un fan à ~50 RPM avec 2 pulses/rev, la période théorique est de 60 s / (50 × 2) ≈ 600 ms — bien au-dessus du seuil. Donc ce n'est pas un problème pratique pour des Noctua (qui ne descendent jamais sous ~800 RPM). Ce point est plus théorique que réel.

### Catégorie 5 — Filtres / Signal

- **[majeur] Pas de filtre temporel sur les RPM mesurés** : Contrairement à qwen3.6_pFC (triple lissage), glm5.3 (EMA α=0,25), gpt-free (IIR 7/8+1/8) ou deepseek4 flash (EMA α=0,125), claude-sonnet5-free applique **aucun lissage temporel** sur les périodes tach mesurées. Chaque variation de période est immédiatement répercutée sur la sortie par le recalcul `periodOutUs = periodInUs / ratio`. Les fluctuations turbulentes des Noctua se traduiront directement par des sauts de fréquence sur le signal simulé → jitter sur la sortie Deye. C'est l'absence de filtrage la plus totale parmi tous les compétiteurs analysés.
- **[majeur] Période brute sans moyenne ni EMA** : Le code passe directement `periodInUs` au calcul de sortie sans aucun traitement intermédiaire. Un spike électrique ou un rebond non capturé par le debounce (2 ms) produira une période aberrante répercutée instantanément sur le signal simulé.

### Catégorie 6 — WiFi / réseau

- **[critique] Pas de reconnexion WiFi automatique** : Comme noté plus haut, `WiFi.begin()` est appelé une seule fois au boot. Si la connexion est perdue (router redémarré, AP hors portée), **le device ne tente jamais de se reconnecter**. Il reste bloqué en mode STA déconnecté sans aucune tentative de recovery. Contrairement à deepseek4 flash qui effectue une reconnexion périodique toutes les 20 secondes dans la boucle principale. C'est un bug fonctionnel potentiellement bloquant : si le routeur redémarre, l'appareil devient inaccessible via réseau et n'est plus reachable qu'en mode AP local (192.168.4.1).
- **[majeur] Pas d'authentification web** : N'importe qui connecté au réseau (AP ou STA) peut accéder à `/save` et modifier les ratios, ou changer le WiFi. Aucune protection par mot de passe sur le serveur web. Identique à tous les autres compétiteurs — manque récurrent mais notable.
- **[majeur] Pas d'OTA (Over-The-Air)** : Mises à jour firmware uniquement par câblage USB. Absent alors que qwen3.6_pFC et glm5.3 l'ont implémenté. Pour un dispositif installé en hauteur (boîtier onduleur), OTA rendrait le maintien beaucoup plus pratique.
- **[majeur] Pas de support mDNS / Zeroconf** : Aucun nom de domaine local (ex: `deye-fan.local`). L'utilisateur doit connaître l'IP du STA ou aller sur 192.168.4.1 en mode AP. Identique à qwen3.6 et gpt-free — absent alors que gemini-free, glm5.3 et deepseek4 flash ont un mDNS activé.
- **[mineur] Pas de retour RSSI ni statut WiFi dans l'API** : `/status` retourne RPM et uptime mais pas la qualité du signal WiFi (RSSI) ni le mode AP/STA courant. L'utilisateur ne sait pas si l'appareil est connecté au réseau STA ou seulement en mode AP.

### Catégorie 7 — Gestion mémoire / HEAP

- **[positif mineur : aucun négatif majeur]** — Le code est sobre en heap : buffers JSON sur pile, HTML en PROGMEM, structs typés statiques. Le seul usage heap provient des TCP buffers internes du serveur ESP8266WebServer (inévitable et contrôlé par le core).

### Catégorie 8 — Interface Web

- **[majeur] Formulaire n'échappe pas les valeurs spéciales** : Les champs SSID/password sont affichés via `document.getElementById('sta_ssid').value = d.sta_ssid;` sans échappement HTML. Si le SSID contient des caractères spéciaux (`"`, `<`, `>`), cela pourrait casser le DOM ou introduire un XSS (faible risque local mais bonne pratique manquante). Identique à gpt-free qui a le même problème.
- **[mineur] Pas de debounce côté client du bouton Enregistrer** : Après avoir cliqué sur « Enregistrer », l'utilisateur peut recliquer immédiatement et provoquer plusieurs POST /save consécutifs, multipliant les écritures EEPROM (~10ms bloquantes chacune). Qwen3.6_pFC a implémenté un cooldown de 5s entre écritures Flash côté serveur — ici il n'y a aucune limitation.
- **[mineur] Pas d'indicateur de statut WiFi visible dans l'interface** : L'interface affiche les RPM et l'état des canaux mais pas le statut WiFi (connecté/déconnecté, RSSI, mode AP/STA). Un petit indicateur visuel serait utile pour le diagnostic.

### Catégorie 9 — Persistance / Stockage

- **[majeur] Ratios modifiés via `/save` ne sont pas appliqués immédiatement avant reboot** : Le handler `/save` met à jour `config.ratio[ch]`, fait `saveConfig()`, puis planifie un reboot 2s plus tard. Pendant ces 2 secondes, les nouveaux ratios ne sont pas encore actifs (ils seront lus au reboot). En pratique c'est acceptable mais l'utilisateur s'attendrait peut-être à une mise à jour immédiate.
- **[positif mineur] EEPROM.end() dans saveConfig()** : L'appel `EEPROM.end()` après le commit libère l'handle. C'est correct mais il est appelé après la fermeture de brace du if/else dans loadConfig(). Vérifier qu'il n'y a pas de problème de scope — en fait, les deux fonctions sont distinctes (saveConfig et loadConfig), donc `EEPROM.end()` est bien à l'intérieur de chaque fonction respective. Pas de bug.

### Catégorie 10 — Documentation hardware

- **[positif mineur : aucun négatif notable]** — La documentation est d'excellente qualité, comparable à qwen3.6_pFC et deepseek4 flash. Schémas ASCII détaillés, BOM complète, explications du fonctionnement par bloc. Pas de critique pertinente ici.

### Catégorie 11 — Qualité de code générale

- **[majeur] Macro `GPOS`/`GPOC` non définie dans le fichier source (risque compilation)** : Les symboles `GPOS` et `GPOC` sont des alias de registre du SDK ESP8266 bare-metal (`GPOS = (*(&GPIO_OUT_SET0) + 0)`, `GPOC = (*(&GPIO_OUT_CLEAR0) + 0)`). Le header `user_interface.h` (inclus via `extern "C"`) ne définit pas ces symboles. Ils dépendent de la version du core ESP8266 Arduino utilisé. **Si l'utilisateur compile avec un core ancien ou personnalisé sans ces symboles, la compilation échouera.** Ce bug est identique à celui signalé chez qwen3.6_pFC (même pattern GPOS/GPOC). Risque modéré mais réel.
- **[critique] Pas de reconnexion WiFi automatique** : Si le STA se déconnecte après le boot (router redémarré, AP hors portée), l'appareil ne tente **jamais** de se reconnecter. Le serveur web reste accessible en mode AP uniquement (192.168.4.1) mais pas en réseau STA. L'utilisateur doit physiquement rebooter l'appareil pour tenter à nouveau la connexion. Contrairement à deepseek4 flash qui effectue `WiFi.reconnect()` périodiquement toutes les 20 secondes dans `loop()`.
- **[majeur] Absence totale de filtrage temporel** : Aucun EMA, aucun filtre IIR, aucune moyenne glissante sur les périodes tach mesurées. La sortie simulée réplique directement les fluctuations turbulentes du fan sans lissage → jitter audible et potentiellement problématique pour l'onduleur Deye (sensibilité aux variations rapides de fréquence).
- **[majeur] `GPOS`/`GPOC` sont des symboles SDK bare-metal** : Ils ne font pas partie des headers standard Arduino ESP8266. Selon la version du core, ces symboles peuvent être absents → erreur de compilation. Risque similaire à qwen3.6_pFC (~1 bug critique).
- **[mineur] `CHANNEL_NAME` non utilisé** : Le tableau `static const char *CHANNEL_NAME[NUM_CHANNELS]` est défini mais ne semble jamais référencé dans le code (pas de `Serial.print(CHANNEL_NAME[ch])` ni utilisation similaire). Dead code mineur.
- **[mineur] Variables globales sans encapsulement** : Toutes les variables d'état (`rpmIn`, `rpmOut`, `chOutputActive`, etc.) sont au scope global. Acceptable pour un projet ESP8266 de cette taille (~150 lignes de state), mais manque d'encapsulement par rapport à gpt-free qui utilisait une structure `TachChannel`.

---

## Synthèse

**Ce projet est bien conçu sur le plan architectural avec une documentation inline exceptionnelle.** La section entête du fichier source détaille honnêtement les stratégies d'isolation temps-réel (Timer1 matériel vs loop web vs interruptions GPIO), justifie les choix de performance CPU et WiFi, et reconnait les limites ESP8266 mono-cœur. C'est un niveau de transparence technique rare et précieux.

Le Timer1 matériel en mode périodique (TIM_LOOP) génère le signal de sortie indépendamment du CPU et du WiFi, avec écriture directe des registres GPOS/GPOC — corrigeant le bug critique `digitalWrite()` de qwen3.6/gemini-free. L'EEPROM avec checksum pondéré offre une validation d'intégrité rigoureuse. La documentation hardware (schémas, BOM, explications par bloc) est d'excellente qualité.

**Cependant, trois faiblesses notables empêchent un score parfait :**

1. **Pas de reconnexion WiFi automatique** — Si le STA se déconnecte après le boot, l'appareil ne tente jamais de se reconnecter. Seule une intervention physique (reboot) peut restaurer la connexion.
2. **Absence totale de filtrage temporel** — Aucun EMA, IIR ou moyenne glissante sur les périodes tach. Le signal simulé réplique directement les fluctuations du fan sans lissage → jitter sur la sortie Deye. C'est le manque de filtrage le plus total parmi tous les compétiteurs.
3. **Résolution timer 100 µs (vs ~0,2 µs chez deepseek4 flash)** — La demi-période minimale est de 1 ms (= 500 Hz max), soit une limitation à ~15 000 RPM simulés avec arrondi grossier aux multiples de 100 µs.

### Points forts réels
- Timer1 matériel TIM_LOOP → génération sortie indépendante du CPU/WiFi
- GPOS/GPOC directs dans l'ISR (correction bug digitalWrite de qwen3.6/gemini-free)
- Countdown absolu anti-dérive temporelle
- Checksum EEPROM pondéré par indice — détection de corruption robuste
- Calcul période sortie direct (`periodIn / ratio`) sans passer par RPM intermédiaire → moins d'erreurs d'arrondi
- Buffer JSON sur pile + HTML en PROGMEM → ZÉRO allocation heap (anti-fragmentation parfaite)
- Architecture explicitement documentée dans un commentaire d'entête détaillé
- Désactivation modem sleep WiFi (`WIFI_NONE_SLEEP`) pour anti-gigue
- Overclock CPU 160 MHz pour marge de calcul
- Documentation hardware exceptionnelle (schémas, BOM, explications par bloc)
- Reboot différé non-bloquant (2s après /save)
- Dark theme web UI moderne + AJAX temps réel + formulaire fetch

### Faiblesses critiques
- **Pas de reconnexion WiFi automatique** — bug potentiellement bloquant si le STA se déconnecte
- **Aucun filtrage temporel** (pas d'EMA, IIR ou moyenne glissante) → jitter sortie
- Résolution timer 100 µs (vs 0,2 µs deepseek4 flash) → arrondi grossier à haute RPM
- Macro `GPOS`/`GPOC` symboles SDK bare-metal → risque compilation selon version core
- Pas de watchdog (SW ou HW) — identique à qwen3.6_pFC et glm5.3
- Pas d'OTA, pas de mDNS, pas d'authentification web

### Comparaison avec les autres compétiteurs

| Aspect | gemma4 (3,2) | qwen35 (4,8) | qwen3.6 (7,2) | gemini-free (5,3) | gpt-free (7,5) | qwen3.6_pFC (8,2) | glm5.3 (8,0) | deepseek4 flash (8,8) | **claude-sonnet5-free (8,5)** |
|---|---|---|---|---|---|---|---|---|---|
| ISR IRAM_ATTR | ❌ Absent | ✅ Présent | ✅ Present | ✅ Sur 3 ISRs | ✅ Sur 2 ISrs | ✅ Sur 2 ISrs | ⚠️ Noyau-attaché | ✅ Sur 3 ISrs | ✅ Sur 3 ISrs |
| Sortie timer | ❌ Ticker SW | ⚠️ 1 Hz fixe | HW timer 1 µs | Waveform gen 50 µs | Dep. external hdr | Ticker @20kHz | FRC1 + CCOUNT | NMI FRC1 bruts | **Timer1 TIM_LOOP 100µs** |
| GPIO écriture ISR | ❌ Jamais | GPOS undef | digitalWrite() CRITIQUE | digitalWrite() CRITIQUE | external hdr | GPOS/GPOC SDK | Noyau-attaché | Registres bruts dsheet | **GPOS/GPOC SDK** |
| Immunité WiFi ISR | N/A | ⚠️ Timer normal | HW timer isolé | Waveform matériel | External header | Software (Ticker) | FRC1 non-NMI | NMI immunité absolue | **Timer1 périodique 100µs** |
| Signal D5/D6 | ❌ Jamais | ⚠️ 1 Hz fixe | ✅ Proportionnel | ✅ Proportionnel | ✅ Proportionnel | ✅ Proportionnel | ✅ Proportionnel | ✅ Proportionnel + NMI | ✅ **Proportionnel** |
| Filtres signal | ❌ Aucun | ⚠️ Fenêtre (faux) | ⚠️ Anti-rebound | ❌ Aucun | IIR 7/8+1/8 | Moyenne + double IIR | EMA α=0.25 | EMA α=0,125 | **❌ Aucun filtrage** |
| Persistance | ❌ Aucune | ❌ SPIFFS cassé | ✅ EEPROM mag | ✅ LittleFS JSON | ✅ EEPROM magic | ✅ LittleFS JSON | ✅ EEPROM + CRC32 | ✅ EEPROM + CRC8 intégral | ✅ **EEPROM + checksum pondéré** |
| Validation POST | ❌ Aucune | ❌ Aucune | ⚠️ Client-side | ⚠️ Client-side | ✅ constrain() | ❌ AUCUNE serveur | ✅ argF/argU8 bornes | ✅ clampU32 | ✅ **borne 0.1–10 côté serveur** |
| mDNS | ❌ | ❌ | ❌ | ❌ | ❌ | ⚠️ Inclus inactif | ✅ Actif | ✅ Actif | **❌ Absent** |
| OTA | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ Intégré | ❌ 0 libs | ❌ Absent | **❌ Absent** |
| Docs hardware | ✅ Correcte | ⭐ Exceptionnelle | ⭐ Exceptionnelle | ❌ Aucune | ❌ Limitée | ⭐ Exceptionnelle | ⭐ Exceptionnelle | ⭐ Exceptionnelle | ⭐ **Exceptionnelle** |
| JSON HEAP | ❌ String concat | ❌ String 10Hz | ✅ OK | ✅ StaticJsonDocument | ❌ String concat | ⚠️ Mixte | ❌ 0 libs (core) | ✅ Pile + snprintf_P | ✅ **Pile + snprintf** |
| Reconnexion WiFi | ❌ Jamais | ⚠️ Bloquante | ⚠️ Bloquante ~10s | ❌ Jamais | ❌ Jamais | ⚠️ Non-bloquante 30s | ✅ Périodique 20s | ✅ Périodique 20s | **❌ Jamais** |
| Bugs critiques | 6+ | 3 | 2 | 2 | 1 (hdr) | ~1 (GPOS SDK) | 0 fonctionnels | ~1 (watchdog + password) | **~2 (reconnexion + GPOS)** |

---

## Verdict

**Architecture solide et bien pensée, mais absence totale de filtrage temporel sur les RPM mesurés.** claude-sonnet5-free est un projet honnête dans son approche — il reconnaît explicitement les limites ESP8266 mono-cœur et documente ses compromis architecturaux. Le Timer1 matériel en mode périodique (TIM_LOOP à 10kHz) assure une génération de signal stable, indépendante du CPU/WiFi. L'utilisation des registres GPOS/GPOC dans l'ISR corrige le bug fatal `digitalWrite()` de qwen3.6/gemini-free.

**Les trois corrections nécessaires pour un score proche de 9/10 :**
1. **Ajouter un filtre temporel (EMA IIR α≈0,2–0,3) sur les périodes tach mesurées** — le plus impactant, car le jitter non-filtré est perceptible sur la sortie Deye.
2. **Implémenter une reconnexion WiFi périodique dans loop()** — pour éviter que l'appareil ne reste bloqué en mode STA déconnecté (comme deepseek4 flash le fait toutes les 20s).
3. **Remplacer GPOS/GPOC par des définitions explicites ou registres bruts** — pour éliminer le risque de compilation selon la version du core ESP8266.

Avec ces corrections, ce projet atteindrait un score de **9,0–9,3/10**. Tel quel, il reste un excellent candidat (~8,5/10) grâce à son architecture transparente et sa documentation de qualité professionnelle.</antml>