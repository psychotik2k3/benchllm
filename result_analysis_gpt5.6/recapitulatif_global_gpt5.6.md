# Récapitulatif global — réanalyse GPT-5.6 depuis zéro

Cette synthèse a été reconstruite directement à partir des fichiers actuels des 25 compétiteurs. Les anciens rapports n'ont pas servi de preuve. Toutes les variantes de modèle et de niveau de raisonnement restent des compétiteurs distincts.

## 1. Inventaire

- Dossiers compétiteurs : **25**
- Rapports individuels régénérés : **25**
- Compétiteurs sans rapport : **aucun**

## 2. Correction méthodologique majeure

La nouvelle lecture invalide plusieurs évaluations antérieures. Le cas le plus net est **Gemini_4_Argon_medium** : son timeout NMI ne supprime ni l'ancienne demi-période cible ni `hasSignal`; le premier front après stall rafraîchit l'âge et réactive donc l'ancienne fréquence avant une nouvelle période. La reprise n'est pas une reprise sûre sur deux fronts. Sa note globale est ramenée à **5,7/10**.

La même discipline a été appliquée à tous : compilation statique, unités Timer1, premier front, stall/reprise, validité IRAM transitive, électronique d'entrée/sortie, alimentation, persistance et sécurité Web.

## 3. Tableau récapitulatif

| Rang | Version | Note globale | Qualité code | Points forts | Défauts déterminants |
|---:|---|---:|---:|---|---|
| 1 | **claude opus 5 medium** | **8,8/10** | **9,0/10** | • Moteur Timer1 réellement routé en NMI, entièrement pensé pour IRAM/DRAM, sans flottant ni allocation.<br>• Échantillonnage d'entrée et sorties utilisent CCOUNT, des échéances absolues et un seqlock NMI→loop.<br>• Sorties relâchées avant leur passage en OUTPUT et moteur lancé avant le WiFi.<br>• Le premier front amorce la mesure; un second front cohérent est nécessaire avant émission. | • Aucun tach provisoire n'est émis avant l'acquisition validée.<br>• Le moteur dépend de macros, registres et conventions NMI internes au core ESP8266.<br>• Le placement IRAM transitif n'est pas prouvé par un map file livré.<br>• L'API de configuration n'est pas authentifiée et l'état JSON renvoie le mot de passe AP. |
| 2 | **Claude_Opus_5.5_xhigh** | **8,7/10** | **8,8/10** | • La chaîne tach est traitée dans une NMI FRC1 avec CCOUNT, helpers inline IRAM et accès GPIO direct.<br>• Les échéances absolues, la mise à jour en fin de cycle et les diagnostics de retard limitent jitter et dérive.<br>• Les collecteurs ouverts sont relâchés avant le réseau; l'AP/STA ne bloque pas l'acquisition.<br>• Le premier niveau d'entrée est échantillonné sans fabriquer de faux front. | • La sortie reste relâchée jusqu'à une mesure tach fiable.<br>• Les API NMI et détails Timer1 sont privés au core ESP8266.<br>• La NMI peut terminer une phase en cours après timeout avant relâchement complet.<br>• Le mot de passe administrateur est vide par défaut. |
| 3 | **GPT-5.6_Sol_xhigh** | **8,0/10** | **8,4/10** | • Capture CCOUNT sur deux ISR et génération via le moteur `startWaveform()` du core, isolée de loop.<br>• Premier front utilisé uniquement comme référence et calculs de ratio/2 PPR cohérents.<br>• Configuration magie/version/taille/CRC32 avec `offsetof`, parsing strict et échappement JSON.<br>• AP+STA, WIFI_NONE_SLEEP et capture démarrée avant le WiFi. | • La waveform ne démarre qu'après plusieurs mesures et une mise à jour de loop toutes les 200 ms.<br>• Le timeout d'entrée est de 2 s.<br>• `startWaveform()`/`startWaveformClockCycles()` est une API du core dont la compatibilité cible n'est pas démontrée par un build livré.<br>• Les tests ne couvrent ni ISR, ni moteur waveform, ni reprise après stall. |
| 4 | **gpt-5.6 sol medium** | **7,8/10** | **8,3/10** | • Timer1 one-shot partagé entre deux sorties, échéances en cycles absolus et accès GPIO direct.<br>• Boîte aux lettres 32 bits et publication seqlock NMI/loop correctement raisonnées.<br>• Premier front seulement comme référence, périodes bornées, glitches qui ne rafraîchissent pas `lastGood` et reprise sur deux fronts.<br>• Deux PPR, IIR entier et relâchement complet au stall à 350 ms. | • `timer1_attachInterrupt` utilise une interruption masquable plutôt qu'une NMI explicite.<br>• Les garanties transitives IRAM ne sont pas vérifiées par linker map.<br>• Aucune consigne provisoire avant deux fronts valides.<br>• Un seul ventilateur est surveillé par paire. |
| 5 | **muse_wonder_1.0_medium** | **6,6/10** | **7,7/10** | • Anneaux de timestamps ISR, traitement hors ISR et sortie NMI one-shot par échéances absolues.<br>• GPIO directs, buffers fixes pour le statut et deux canaux indépendants.<br>• Validation de période, moyenne, IIR, slew et interface qui évite d'écraser les champs en édition.<br>• AP+STA, WIFI_NONE_SLEEP et pages en PROGMEM. | • `sizeof(cfg)-sizeof(cfg.crc)` vaut 214 alors que `crc` commence à l'offset 212; les deux octets CRC sont inclus à cause du padding final.<br>• L'anneau écrase silencieusement des entrées non consommées sans indicateur full/overflow.<br>• Le slew de 2 % est appliqué à chaque tour de loop, pas à chaque nouvelle mesure.<br>• Le timeout de 1,2 s est décidé dans loop alors que la sortie est en NMI. |
| 6 | **kimi_k3_max** | **6,1/10** | **7,1/10** | • Timer1 one-shot avec échéances absolues, deux sorties, GPIO directs et sections critiques courtes.<br>• Filtrage EMA, ratios bornés, EEPROM CRC16 et serveur AP+STA.<br>• OR-ing Schottky, buck, pull-ups 3,3 V, NPN open-collector et pull-downs sont correctement documentés.<br>• UI claire, structures de canal et fonctions communes; formules 2 PPR cohérentes. | • `captureEdge` accepte le tout premier front car il ne distingue pas l'absence de référence.<br>• Aucune borne maximale de période n'est appliquée après un stall.<br>• Le stall est traité dans loop toutes les 100 ms et non dans le moteur Timer1.<br>• L'IRQ Timer1 standard n'établit pas l'isolation NMI annoncée. |
| 7 | **kimi k3** | **5,9/10** | **6,8/10** | • Timer1 one-shot commun à deux sorties, GPIO directs et resynchronisation d'échéance.<br>• Deux PPR et calcul de demi-période corrects.<br>• EMA d'entrée, timeout, CRC16 et AP+STA.<br>• Un seul tach par paire et schéma avec Schottky, buck et NPN. | • La première ISR calcule `dt = now - 0` et l'accepte si elle tombe dans la large plage.<br>• Après stall, la première période très longue n'a pas de borne maximale suffisamment stricte et peut amorcer le filtre.<br>• `timer1_attachInterrupt` n'est pas une preuve de NMI et l'ISR appelle `micros()`.<br>• Une nouvelle période est recalée sur `now` après retard. |
| 8 | **grok 4.6 medium** | **5,8/10** | **7,0/10** | • NMI périodique à 20 µs, accès GPIO direct et calcul de demi-période entier correct.<br>• Deux canaux indépendants et moteur démarré avant le WiFi.<br>• EEPROM avec CRC32, AP+STA, formulaire de ratios et validation de plage.<br>• Un seul tach par paire, OR-ing Schottky, buck et sorties NPN avec pull-down sont cohérents. | • Le timeout désactive seulement `nmiEnabled`; `havePeriod` et `periodCycles` restent valides.<br>• Le premier front de reprise actualise `lastEdgeUs` avant validation et loop réactive immédiatement la période ancienne.<br>• Un glitch actualise `lastCycle`, `lastEdgeUs` et le compteur avant contrôle de période.<br>• Appel de `micros()` depuis l'ISR et NMI fixe à 50 kHz. |
| 9 | **Gemini_4_Argon_medium** | **5,7/10** | **7,8/10** | • Timer1 NMI one-shot, CCOUNT, Q16, GPIO directs et watchdog NMI à 500 ms.<br>• Deux canaux indépendants, buffers fixes, HTML PROGMEM et statut JSON borné.<br>• Deux PPR, périodes plausibles et moyenne multi-échantillons.<br>• La sortie est relâchée par la NMI sans dépendre de loop. | • Au timeout, la NMI ne réinitialise ni `targetHalfCycles` ni `hasSignal`; elle ne met que `active` à zéro.<br>• Le premier front de reprise rafraîchit `lastEdgeCycle`, remet `hasSignal=1` et retourne sans invalider la consigne.<br>• Une multiplication 64 bits dans l'ISR GPIO peut appeler un helper libgcc hors IRAM.<br>• La détection CPU/APB lit le registre non documenté `0x3FF00014`. |
| 10 | **composer 2.5 prompt from claude** | **5,2/10** | **6,3/10** | • Deux entrées ISR, Timer1 périodique, ratios par canal et sortie initialisée avant le réseau.<br>• La direction du ratio, les 2 PPR et le calcul de demi-période sont corrects.<br>• Serveur asynchrone, LittleFS JSON, HTML PROGMEM et interface de statut compacte.<br>• AP+STA non bloquant et valeurs par défaut explicites. | • `micros()` et `digitalWrite()` sont appelés dans l'ISR Timer1 à 20 kHz.<br>• La génération recale chaque front sur `now + half`.<br>• L'ISR met `lastEdgeUs` à jour avant de rejeter une période invalide.<br>• Après timeout, le filtre et la période brute restent mémorisés; le premier front de reprise rafraîchit l'âge. |
| 11 | **deepseek4 flash** | **4,8/10** | **6,5/10** | • Event-calendar FRC1/NMI à deux voies, GPIO directs et ratio Q16 évitent les calculs flottants dans le moteur.<br>• Échéances relatives bornées et sorties relâchées au stall.<br>• Deux PPR, EMA et timeout adaptatif max(2 s, 4 périodes) sont cohérents en intention.<br>• AP+STA, mDNS, HTML PROGMEM et buffers statiques pour le statut. | • Le premier front valide une période depuis `g_inLastUs == 0` car la borne maximale vaut 120 s.<br>• Les fronts invalides mettent malgré tout à jour le timestamp et le compteur.<br>• La CRC est calculée sur `sizeof(CfgT)-1`, qui inclut un octet du champ CRC à cause de l'alignement.<br>• Le code programme directement FRC1/NMI et suppose des bits/adresses sans preuve de version. |
| 12 | **claude-sonnet5-free** | **4,8/10** | **6,2/10** | • Timer1 périodique à 10 kHz, GPIO directs et deux compteurs de demi-période indépendants.<br>• Le calcul 2 PPR et la multiplication de fréquence sont cohérents en régime établi.<br>• AP+STA, WIFI_NONE_SLEEP, serveur standard et checksum sur la configuration.<br>• HTML PROGMEM et statut JSON construit dans un tampon fixe. | • Le premier front est accepté comme une période calculée depuis zéro, sans état « non amorcé ».<br>• Après stall, le premier front très long est de nouveau accepté faute de borne maximale.<br>• L'étage d'entrée NPN documenté n'ajoute pas de pull-up au tach open-collector et suppose à tort un repos à 12 V fourni par le Noctua.<br>• Le document mélange tach Noctua open-collector et prétendue tension interne 12 V. |
| 13 | **gpt-free** | **4,6/10** | **6,2/10** | • Deux captures CCOUNT courtes, IIR 1/8 et deux waveforms continues via le core ESP8266.<br>• Traitement factorisé par canal, premier front correctement ignoré et timeout à 500 ms.<br>• AP+STA, page sombre, statut AJAX et ratios appliqués immédiatement.<br>• Magie EEPROM, défauts explicites et bornage des ratios. | • Le commentaire de câblage montre un diviseur vers la masse mais aucun pull-up alimentant le tach open-collector.<br>• Aucun fichier de schéma/BOM séparé n'est livré.<br>• Les seuils de cycles sont commentés pour 160 MHz alors que la fréquence CPU n'est pas imposée.<br>• L'API waveform privée et sa signature ne sont pas validées. |
| 14 | **Grok_4.7_medium** | **4,4/10** | **8,2/10** | • NMI one-shot à 32 µs pour échantillonner les entrées et planifier deux sorties par CCOUNT.<br>• GPIO directs, division 32 bits seulement, seqlock NMI→loop et resynchronisation sans rafale.<br>• Les glitches n'actualisent pas `lastGood`; le stall efface filtre, amorçage et sortie.<br>• Deux vrais fronts sont requis pour redémarrer après une perte. | • L'OR-ing relie le +12 V au collecteur d'un SS8050 et VRAW à sa base, émetteur ouvert.<br>• Le régulateur discret VBE/ballast dépend fortement de température et charge.<br>• La NMI se réveille au moins 31 250 fois/s même sans signal.<br>• Timer1 NMI et appel bas niveau dépendent du core sans build/map livré. |
| 15 | **glm5.3** | **2,8/10** | **6,5/10** | • Capture dans un anneau de 128 timestamps, mesure multi-fronts, EMA et ordonnanceur à échéances absolues.<br>• Deux sorties, boîte aux lettres loop→ISR et resynchronisation sans rafale.<br>• Portail captif, mDNS, scan asynchrone, AP+STA et interface complète.<br>• CRC32 via `offsetof`, validation et JSON échappé. | • `0xDEYEF001UL` contient la lettre Y.<br>• Les deux sorties tach open-collector de chaque paire sont mises en parallèle puis la fréquence est divisée par le nombre de ventilateurs.<br>• Le code et la documentation traitent cette division comme une correction d'affichage alors qu'elle influence la consigne.<br>• Le mode `timer1_attachInterrupt` n'établit pas la priorité NMI revendiquée. |
| 16 | **gemini-free** | **2,5/10** | **4,5/10** | • Deux canaux, Timer1, serveur asynchrone, LittleFS JSON et HTML PROGMEM.<br>• La formule conceptuelle 2 PPR et la demi-période sont identifiées.<br>• Page compacte avec polling et ratios configurables. | • Timer1 en DIV16 vaut 5 ticks/µs, mais le code recharge avec `80 * timerStepUs`.<br>• `digitalWrite()` est utilisé dans l'ISR.<br>• Le premier front est traité comme période depuis le boot.<br>• LittleFS et WiFi sont initialisés avant le moteur tach. |
| 17 | **gemini 3.1 pro** | **2,3/10** | **3,8/10** | • Deux canaux, Timer1 avant le WiFi, GPIO directs et page AJAX sont présents.<br>• Le calcul 2 PPR est compris et les sorties collecteur ouvert sont identifiées.<br>• AP+STA non bloquant et interface de réglage des ratios. | • `0xDEYE0001` n'est pas un littéral hexadécimal valide.<br>• Les impulsions sont comptées sur une cadence supposée d'une seconde sans utiliser le temps réellement écoulé.<br>• Aucun vrai filtre de période ni état d'amorçage/reprise.<br>• Timer1 interrompt à 20 kHz en continu. |
| 18 | **glm_5.2_high** | **2,0/10** | **4,0/10** | • Deux `startWaveform()` indépendants, timeout et moyenne de quatre périodes.<br>• LittleFS, AP+STA et traitement factorisé par canal.<br>• Schéma avec OR-ing Schottky, buck, pull-ups 3,3 V et collecteurs ouverts. | • Trois concaténations HTML contiennent une parenthèse fermante surnuméraire.<br>• `deltaCycles * 1000000UL` déborde en 32 bits pour pratiquement toute période tach réaliste.<br>• Le test `c1 > c0` rejette le wrap CCOUNT.<br>• Les deux tach d'une paire sont recommandés en parallèle. |
| 19 | **composer 2.5** | **1,2/10** | **4,0/10** | • Deux canaux, Timer1, sorties par registres et timeout d'entrée sont prévus.<br>• Le code utilise une structure par canal, une CRC16 et une interface de configuration.<br>• La formule 2 PPR et la direction du ratio sont correctes en intention. | • Le littéral `0xDEYE` contient la lettre Y et n'est pas un entier hexadécimal C++.<br>• Avec TIM_DIV16, `timer1_write(10)` programme environ 2 µs et non 10 µs.<br>• Le calcul flottant de ratio est exécuté dans l'ISR GPIO.<br>• Le premier front est mesuré depuis zéro. |
| 20 | **qwen3.6_prompt_from_claude** | **1,0/10** | **3,5/10** | • Deux captures, filtrage glissant/EMA, serveur async, OTA et LittleFS sont prévus.<br>• Le ratio `période/ratio` et les 2 PPR sont compris.<br>• Le schéma prévoit pull-ups 3,3 V et sorties collecteur ouvert. | • La signature du handler POST Async, `request->getString`, certaines surcharges `send` et la portée de `restartTime` sont invalides.<br>• Le `Ticker scheduler` est local à `setup()`.<br>• Ticker logiciel à 20 kHz n'offre pas l'isolation Timer1/NMI revendiquée.<br>• Les sorties sont initialisées HIGH alors que HIGH active le NPN. |
| 21 | **Swift_1.5_Qwen3.8-27B_medium-IQ3S** | **0,6/10** | **2,5/10** | • Deux canaux, EMA, bornes de période, AP+STA et endpoints séparés.<br>• OR-ing et collecteur ouvert sont au moins identifiés comme besoins. | • Le code utilise `ESP8266HTTPServer.h`, `Preferences`, `ESP8266TimerInterrupt` et `WiFi.connect`, API inexistantes ou incompatibles.<br>• Les fonctions ISR ne sont pas placées en IRAM et les champs partagés sont insuffisamment `volatile`.<br>• La configuration Timer0 est invalide et non réarmée.<br>• L'état pulse/période produit une fréquence erronée et la période stale maintient `ok` vrai. |
| 22 | **qwen3.6** | **0,5/10** | **2,5/10** | • Deux canaux, interface Web, EEPROM et collecteurs ouverts sont envisagés.<br>• La formule d'affichage RPM à 2 PPR est connue. | • Le code emploie `hw_timer_t`, `timerBegin`, `timerRead`, `timerWrite` et autres API ESP32 sur ESP8266.<br>• Une accolade surnuméraire suit `eepromLoadRatios`.<br>• La période est multipliée par le ratio puis utilisée comme demi-période.<br>• GPIO15/D8 est utilisé comme entrée avec un circuit susceptible d'altérer le strap boot. |
| 23 | **qwen3.6-35b-fp8-freetoken** | **0,3/10** | **2,0/10** | • AP+STA, portail captif, EEPROM et interface riche sont envisagés.<br>• Le principe d'un collecteur ouvert de sortie est mentionné. | • Une accolade/`EEPROM.end()` est hors fonction et les API `hw_timer_t`/timer sont celles d'ESP32.<br>• L'interruption est attachée en mode `LOW` plutôt que `FALLING`.<br>• Un seul tach d'entrée est partagé par les quatre ventilateurs et un seul timer/pin courant sert deux sorties.<br>• La période simulée est multipliée par le ratio et employée comme demi-période. |
| 24 | **gemma4** | **0,2/10** | **1,2/10** | • AP protégé et page de statut simples.<br>• Quelques handlers HTTP sont séparés. | • Le fichier contient un identifiant de broche erroné et se termine avec une clôture Markdown.<br>• Aucun générateur de waveform tach n'est implémenté.<br>• Un seul pin de sortie est prévu pour deux canaux.<br>• Les compteurs de fronts ne sont jamais remis à zéro ni rapportés à une durée. |
| 25 | **qwen35** | **0,1/10** | **1,0/10** | • Interface responsive, guide de calibration et intention de moyenne glissante.<br>• AP protégé par mot de passe. | • `samples9[`]` est une erreur syntaxique; `GPOS(pin,state)` et `WiFi.transmitting()` n'existent pas; SPIFFS/File ne sont pas déclarés.<br>• Deux fonctions `loop()` sont définies.<br>• Aucun Timer1 ne génère une fréquence liée aux RPM; la seule sortie bascule toutes les 500 ms.<br>• Les compteurs cumulés sont directement présentés comme RPM et jamais remis à zéro sur une fenêtre. |

## 4. Classement global

1. **claude opus 5 medium** — **8,8/10** — Meilleure proposition globale, à valider sur la toolchain cible
2. **Claude_Opus_5.5_xhigh** — **8,7/10** — Référence très complète, légèrement pénalisée par sécurité et validation
3. **GPT-5.6_Sol_xhigh** — **8,0/10** — Très bonne base officielle, vérification temps réel encore partielle
4. **gpt-5.6 sol medium** — **7,8/10** — Solution solide et prudente, sans NMI ni tests
5. **muse_wonder_1.0_medium** — **6,6/10** — Très bon temps réel, persistance cassée et dynamiques dépendantes de loop
6. **kimi_k3_max** — **6,1/10** — Architecture solide en régime, reprise après stall incorrecte
7. **kimi k3** — **5,9/10** — Base fonctionnelle, faux premier échantillon et timer non NMI
8. **grok 4.6 medium** — **5,8/10** — Bon moteur NMI, reprise stale et mesure à durcir
9. **Gemini_4_Argon_medium** — **5,7/10** — Moteur sophistiqué, mais reprise stale et alimentation critiques
10. **composer 2.5 prompt from claude** — **5,2/10** — Bonne ossature, fail-safe et reprise à corriger
11. **deepseek4 flash** — **4,8/10** — Architecture ambitieuse, persistence et bas niveau fragiles
12. **claude-sonnet5-free** — **4,8/10** — Firmware plausible, mais reprise et entrée matérielle insuffisantes
13. **gpt-free** — **4,6/10** — Firmware raisonnable, montage d'entrée incomplet et secrets exposés
14. **Grok_4.7_medium** — **4,4/10** — Excellent firmware, alimentation non fonctionnelle
15. **glm5.3** — **2,8/10** — Très ambitieux, bloqué par compilation et conception tach parallèle
16. **gemini-free** — **2,5/10** — Erreur de base de temps bloquante et aucun schéma
17. **gemini 3.1 pro** — **2,3/10** — Ne compile pas et mesure les RPM de façon trop grossière
18. **glm_5.2_high** — **2,0/10** — Non compilable et mesure de période cassée
19. **composer 2.5** — **1,2/10** — Non compilable et alimentation dangereuse
20. **qwen3.6_prompt_from_claude** — **1,0/10** — Non compilable, scheduler détruit et alimentation mal choisie
21. **Swift_1.5_Qwen3.8-27B_medium-IQ3S** — **0,6/10** — Incompatible ESP8266 et matériel dangereux
22. **qwen3.6** — **0,5/10** — Mauvaise plateforme, syntaxe et mathématiques inversées
23. **qwen3.6-35b-fp8-freetoken** — **0,3/10** — Code non compilable et architecture incapable de produire deux tach
24. **gemma4** — **0,2/10** — Ébauche non compilable sans simulateur tach
25. **qwen35** — **0,1/10** — Aucun firmware compilable ni simulateur tach réel

## 5. Classement qualité de code

1. **claude opus 5 medium** — **9,0/10** — Invariants temps réel remarquablement maîtrisés
2. **Claude_Opus_5.5_xhigh** — **8,8/10** — Très rigoureux mais volumineux
3. **GPT-5.6_Sol_xhigh** — **8,4/10** — Meilleure modularité et testabilité du lot
4. **gpt-5.6 sol medium** — **8,3/10** — Propre, défensive et maintenable
5. **Grok_4.7_medium** — **8,2/10** — Moteur temps réel parmi les meilleurs
6. **Gemini_4_Argon_medium** — **7,8/10** — Très structuré, avec un défaut d'état déterminant
7. **muse_wonder_1.0_medium** — **7,7/10** — Soigné, avec plusieurs invariants manqués
8. **kimi_k3_max** — **7,1/10** — Bien structuré mais fail-safe incomplet
9. **grok 4.6 medium** — **7,0/10** — Compact et lisible, machine d'état incomplète
10. **kimi k3** — **6,8/10** — Lisible mais trop confiant
11. **deepseek4 flash** — **6,5/10** — Bien commenté mais trop affirmatif
12. **glm5.3** — **6,5/10** — Architecture soignée mais non validée
13. **composer 2.5 prompt from claude** — **6,3/10** — Structuré mais insuffisamment déterministe
14. **claude-sonnet5-free** — **6,2/10** — Lisible, avec plusieurs hypothèses non vérifiées
15. **gpt-free** — **6,2/10** — Clair mais surdimensionné
16. **gemini-free** — **4,5/10** — Compact mais non vérifié
17. **glm_5.2_high** — **4,0/10** — Bien présenté mais non testé
18. **composer 2.5** — **4,0/10** — Présentation correcte, validation technique insuffisante
19. **gemini 3.1 pro** — **3,8/10** — Intention lisible, robustesse faible
20. **qwen3.6_prompt_from_claude** — **3,5/10** — Ambitieux mais non intégré
21. **Swift_1.5_Qwen3.8-27B_medium-IQ3S** — **2,5/10** — Structure apparente, fondations incorrectes
22. **qwen3.6** — **2,5/10** — Long et explicatif, mais techniquement non viable
23. **qwen3.6-35b-fp8-freetoken** — **2,0/10** — Très volumineux pour une logique non viable
24. **gemma4** — **1,2/10** — Code incomplet et incohérent
25. **qwen35** — **1,0/10** — Artefact généré incohérent

## 6. Comparaison des méthodes et architectures

### Meilleurs moteurs temps réel

- **claude opus 5 medium** : meilleure machine d'état d'entrée et meilleure cohérence NMI/CCOUNT. Les périodes invalides réamorcent le filtre, la reprise exige réellement deux fronts et les snapshots utilisent un seqlock.
- **Claude_Opus_5.5_xhigh** : chaîne NMI la plus complète en fonctionnalités, avec filtrage avancé, diagnostics de retard et fail-safe NMI rapide.
- **Grok_4.7_medium** : firmware temps réel excellent et reprise correcte, mais inutilisable tel quel parce que son OR-ing collecteur-base est polarisé dans le sens bloquant.
- **GPT-5.6_Sol_xhigh** : approche moins bas niveau avec le moteur waveform du core et meilleure modularité/testabilité, au prix d'une validation cible encore nécessaire.

### Erreurs récurrentes

- Confondre le premier front avec une période (`kimi k3`, `kimi_k3_max`, `composer 2.5`, `deepseek4 flash`).
- Couper seulement la sortie au stall sans invalider période/consigne (`Gemini_4_Argon_medium`, `grok 4.6 medium`).
- Appliquer filtre ou slew à chaque tour de loop au lieu de chaque nouvelle mesure (`muse_wonder_1.0_medium`, `composer 2.5 prompt from claude`).
- Paralléliser deux tach asynchrones et prétendre additionner/diviser leurs fréquences (`glm5.3`, `glm_5.2_high`, `qwen3.6-35b-fp8-freetoken`).
- Utiliser des API ESP32 sur ESP8266 (`qwen3.6`, `qwen3.6-35b-fp8-freetoken`, `Swift_1.5_Qwen3.8-27B_medium-IQ3S`).
- Dimensionner l'alimentation sans headroom ou avec une topologie qui ne conduit pas (`Gemini_4_Argon_medium`, `Grok_4.7_medium`, plusieurs Qwen).

## 7. Recommandation de fusion

### Base recommandée

**Prendre `claude opus 5 medium` comme base temps réel et `Claude_Opus_5.5_xhigh` comme référence fonctionnelle/hardware.** Le premier gère le mieux l'amorçage, les périodes invalides et la reprise; le second apporte le réseau, les diagnostics et le dossier électrique les plus complets.

### Éléments à emprunter

1. **Moteur et état d'entrée** — `claude opus 5 medium` : NMI one-shot, CCOUNT absolu, seqlock, `primed`, invalidation sur période incohérente et reprise sur deux fronts.
2. **Fail-safe indépendant de loop** — `Claude_Opus_5.5_xhigh` : timeout NMI court, en veillant à effacer période, consigne, filtres et état de validation.
3. **Mathématiques/tests** — `GPT-5.6_Sol_xhigh` : conserver `tach_math.h`, puis étendre les tests à boot, wrap, bruit, stall, reprise et calendrier de fronts.
4. **Hardware** — utiliser Schottky qualifiées, TVS/PTC, vrai buck-boost si la rail descend près de 5 V, pull-up tach Noctua à 3,3 V et BC337/2N2222 open-collector avec pull-down base-émetteur.
5. **Sécurité Web** — rendre l'authentification obligatoire, ajouter protection CSRF/origin, ne jamais renvoyer les secrets et limiter les écritures flash.
6. **Persistance** — magie + version + taille + CRC avec `offsetof(Config, crc)`, double slot ou écriture transactionnelle et tests save/reload.

### Corrections obligatoires avant déploiement

- Définir explicitement la politique de boot : zéro RPM, période provisoire bornée ou délai de grâce, puis la tester sur le vrai Deye.
- Surveiller idéalement les deux ventilateurs de chaque paire par une stratégie distincte; ne jamais mettre leurs tach en parallèle.
- Pinner le core ESP8266, compiler en CI, inspecter la map IRAM et mesurer jitter/timeout à l'oscilloscope sous WiFi et écriture flash.
- Tester les deux rails d'alimentation séparément, ensemble, à basse tension et pendant les pointes WiFi.

## 8. Conclusion

Le classement final est dominé par les deux Claude haut de gamme, avec un avantage léger à **claude opus 5 medium (8,8/10)** grâce à une reprise après stall techniquement plus correcte et une électronique viable. **Claude_Opus_5.5_xhigh (8,7/10)** reste la livraison la plus complète. **GPT-5.6_Sol_xhigh (8,0/10)** est la meilleure base de modularité et de tests. Aucun candidat ne doit être installé sans build cible et validation électrique/oscilloscope.
