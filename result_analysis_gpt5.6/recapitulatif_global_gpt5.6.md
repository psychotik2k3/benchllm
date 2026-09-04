# Récapitulatif global GPT-5.6

## 1. Inventaire des analyses

Les 15 dossiers présents sous `competitors/` ont tous un rapport correspondant sous `result_analysis_gpt5.6/`. Avant ce run, **0 analyse existait** ; les **15 analyses sont nouvelles** et **aucun compétiteur ne manque**.

### Existantes avant run (0)

- Aucune.

### Nouvelles (15)

- `gpt-5.6 sol medium`
- `claude opus 5 medium`
- `glm5.3`
- `grok 4.6 medium`
- `composer 2.5 prompt from claude`
- `claude-sonnet5-free`
- `gpt-free`
- `gemini 3.1 pro`
- `deepseek4 flash`
- `qwen3.6_prompt_from_claude`
- `gemini-free`
- `composer 2.5`
- `qwen3.6`
- `gemma4`
- `qwen35`

### Sans analyse (0)

- Aucun compétiteur.

## 2. Notes révisées et justification

Les notes annoncées ont été retrouvées dans les 15 rapports individuels. La calibration transversale conserve toutes les notes sauf celles de `glm5.3`. Un astérisque `*` signale ci-dessous, et dans tous les tableaux et classements, une note ajustée dans ce récapitulatif seulement ; les rapports individuels ne sont pas modifiés.

### Ajustement retenu

- **`glm5.3` — note globale : 7,5 → 6,9*/10.** Son ordonnanceur Timer1 one-shot, son code structuré, ses buffers bornés, son EEPROM avec CRC32 et son hardware d'alimentation sont de bon niveau. Toutefois, son hypothèse centrale selon laquelle plusieurs tach open-collector câblés en parallèle « s'additionnent », puis peuvent être divisés par `parallelFans`, est physiquement fausse pour des ventilateurs non synchronisés : les impulsions se recouvrent et des fronts disparaissent. Le réglage affecte en plus la fréquence envoyée, contrairement à la documentation. Cette erreur touche directement la mesure, le fail-safe et la détection de panne d'un ventilateur. Elle justifie de placer la solution sous `grok 4.6 medium` (7,1), qui a un fail-safe imparfait mais lit correctement un seul tach par paire, et juste au-dessus de `composer 2.5 prompt from claude` (6,8), dont le scheduler à tick fixe dérive et dont les fronts invalides peuvent entretenir une ancienne consigne.
- **`glm5.3` — qualité de code : 8,0 → 7,6*/10.** La qualité structurelle reste supérieure à celle de `grok 4.6 medium` (7,3), grâce aux buffers fixes, à `send_P`, au CRC32, à l'échappement JSON et à l'ordonnanceur à échéances absolues. Elle ne peut toutefois rester presque au niveau de `gpt-5.6 sol medium` (8,2) alors que `parallelFans` encode un modèle métier incorrect, que le mode cache-panne présente un trou d'environ cinq secondes et que code et documentation se contredisent sur l'effet de ce paramètre.

### Notes conservées

Les autres notes restent cohérentes après comparaison directe :

- Le duo de tête est justifié : `gpt-5.6 sol medium` est le livrable global le plus équilibré (scheduler one-shot, 2 PPR, CRC32, schéma complet), tandis que `claude opus 5 medium` possède le moteur et la structure les plus sophistiqués mais dépend davantage d'API NMI internes et expose `apPass`.
- `grok 4.6 medium` mérite de rester à 7,1 malgré son architecture NMI crédible : tick permanent à 50 kHz, quantification de 20 µs, absence de filtre de période et mise à jour de fraîcheur avant validation limitent fortement le fail-safe.
- `composer 2.5 prompt from claude` reste à 6,8 : il couvre bien l'ensemble fonctionnel et matériel, mais `now + half`, `digitalWrite()` dans l'ISR, le filtre alimenté plusieurs fois par la même mesure et la fraîcheur mise à jour avant validation empêchent de rejoindre le groupe supérieur.
- `claude-sonnet5-free` conserve une qualité de code élevée mais une note globale basse : le firmware est lisible, cependant le schéma suppose à tort une pull-up interne 12 V du tach Noctua, ce qui rend l'entrée probablement inopérante.
- Les solutions à 4,2 et moins ont chacune au moins un défaut rédhibitoire : hardware essentiel absent, build probablement cassé, mauvais prescaler/unité Timer1, API ESP32 sur ESP8266, absence de génération tach ou formule RPM invalide. Leur ordre initial reflète correctement la quantité de code récupérable.

## 3. Tableau récapitulatif

| Version | Note globale | Note qualité code | Points positifs | Points négatifs |
|---|---:|---:|---|---|
| `gpt-5.6 sol medium` | 8,4/10 | 8,2/10 | • Timer1 one-shot partagé à prochaine échéance<br>• Écritures GPIO atomiques en ISR<br>• Formule 2 PPR et demi-période correctes<br>• IIR réinitialisé après perte<br>• AP+STA et secrets non réaffichés<br>• EEPROM versionnée avec CRC32<br>• OR-ing, buck-boost, TVS/PTC documentés | • Aucun tach synthétique de boot<br>• Timeout stall de 2 s<br>• Une période suffit à réactiver<br>• `micros()` en ISR dépend du core<br>• HTTP sans auth/CSRF/TLS<br>• Page HTML construite dans un gros `String`<br>• Saturation Timer1 non reflétée dans l'UI |
| `claude opus 5 medium` | 8,0/10 | 8,5/10 | • Moteur NMI et CCOUNT très rigoureux<br>• Seqlock entre NMI et boucle<br>• Échéances absolues et rattrapage borné<br>• PPR entrée/sortie configurables<br>• Validation numérique sur trois échantillons<br>• Page en PROGMEM et heap exposé<br>• EEPROM CRC32/version<br>• Hardware et tests instrumentés détaillés | • API NMI privées et build non figé<br>• Charge NMI potentiellement agressive<br>• Aucun signal tach avant mesure<br>• Un seul tach lu par paire<br>• Secret `apPass` exposé par `/api/status`<br>• HTTP non authentifié et chaînes non échappées<br>• Valeurs RC contradictoires code/documentation |
| `grok 4.6 medium` | 7,1/10 | 7,3/10 | • NMI Timer1 et GPIO directs<br>• Deux canaux indépendants<br>• Formules 2 PPR exactes<br>• Timeout court de 400 ms<br>• PROGMEM et EEPROM CRC32<br>• AP+STA avec reconnexion<br>• Un seul tach par paire, sans parallèle<br>• OR-ing et sorties open-collector corrects | • Tick NMI permanent à 50 kHz<br>• Quantification de 20 µs sans fractionnaire<br>• Fraîcheur mise à jour avant validation<br>• Aucun filtre numérique de période<br>• État multi-champs non transactionnel<br>• Pas de tach de boot et alimentation circulaire possible<br>• Auth/CSRF absents et hardware de protection incomplet |
| `glm5.3` | 6,9*/10 | 7,6*/10 | • Timer1 one-shot à échéances absolues<br>• Rattrapage ISR borné<br>• Buffers de timestamps fixes<br>• EMA et rejet de fronts rapides<br>• PROGMEM, `snprintf`, échappement JSON<br>• EEPROM CRC32/version<br>• AP+STA, portail captif, reconnexion<br>• Alimentation et collecteurs ouverts détaillés | • Modèle `parallelFans` physiquement faux<br>• Panne d'un ventilateur masquée<br>• Documentation contradictoire sur la fréquence envoyée<br>• Trou de cinq secondes du cache-panne<br>• EMA non réinitialisée après perte<br>• Aucun signal de boot immédiat<br>• Auth/CSRF absents et tach ancien gardé 2,5 s |
| `composer 2.5 prompt from claude` | 6,8/10 | 6,5/10 | • Timer1 matériel et deux échéances<br>• Formule 2 PPR correcte<br>• Moyenne et limitation de pente prévues<br>• Page PROGMEM et JSON statique<br>• AP+STA non bloquant<br>• LittleFS adapté à une config évolutive<br>• OR-ing, buck et sorties NPN documentés | • ISR fixe 20 kHz avec `digitalWrite()`<br>• `now + half` accumule les retards<br>• Quantification jusqu'à plusieurs pourcents<br>• Fronts invalides repoussent le timeout<br>• Filtre réutilisant la même mesure<br>• Boot retardé par LittleFS<br>• Pas d'auth/CSRF ni reconnexion STA<br>• Persistance non atomique |
| `claude-sonnet5-free` | 4,5/10 | 7,0/10 | • Code clair et factorisé par canal<br>• Timer1 matériel, sorties par registres<br>• 2 PPR correctement appliqués<br>• PROGMEM et buffers `char`<br>• EEPROM avec contrôle d'intégrité<br>• AP+STA non bloquant<br>• Sorties open-collector bien conçues | • Entrée fondée sur une fausse pull-up interne 12 V<br>• Premier front traité comme période depuis le boot<br>• Compteur `N+1` et dérive cumulative<br>• Résolution grossière de 100 µs<br>• Timeout stall de 3 s<br>• Aucun filtre de période<br>• HTTP non authentifié et JSON non échappé |
| `gpt-free` | 4,2/10 | 4,8/10 | • Capture CCOUNT en ISR courte<br>• Générateur waveform du core, hors `loop()`<br>• Formule 2 PPR correcte<br>• IIR entier 7/8–1/8<br>• AP+STA et interface complète<br>• Structures statiques<br>• Code très lisible et factorisé | • API waveform interne non versionnée<br>• Constantes dépendantes de 160 MHz<br>• Aucun tach anticipé au boot<br>• Hardware d'entrée/alimentation essentiel absent<br>• Secrets AP/STA renvoyés en clair<br>• EEPROM sans CRC/version<br>• Page de 9 ko construite dans le heap<br>• Réarmements sans garantie de phase |
| `gemini 3.1 pro` | 3,5/10 | 3,0/10 | • Timer1 matériel et GPIO directs<br>• Intention 2 PPR correcte<br>• Deux sorties indépendantes<br>• AP+STA et page PROGMEM<br>• OR-ing, buck et NPN décrits<br>• Code court et lisible | • `0xDEYE0001` empêche la compilation<br>• Comptage RPM suppose exactement 1 s<br>• Tick 20 kHz et dérive cumulative<br>• Ratios non bornés, zéro possible<br>• `strcpy` distant non borné<br>• Mot de passe STA exposé<br>• Aucun filtre ni vrai fail-safe<br>• Conseil NPN utilisé comme diode dangereux |
| `deepseek4 flash` | 3,4/10 | 3,9/10 | • Event-calendar one-shot ambitieux<br>• NMI et GPIO atomiques<br>• Formule 2 PPR correcte<br>• Timeout adaptatif<br>• EMA entière<br>• Buffer JSON fixe et page PROGMEM<br>• EEPROM versionnée<br>• OR-ing, buck et sorties NPN | • Copie de `volatile SnapT` probablement non compilable<br>• `noInterrupts()` ne protège pas d'une NMI<br>• Réveil idle→actif désynchronise `g_lastDelta`<br>• Premier front accepté comme période<br>• Parasites repoussent le timeout<br>• Secrets renvoyés en clair<br>• Registres FRC1 privés et courses matérielles<br>• Schéma d'entrée ambigu |
| `qwen3.6_prompt_from_claude` | 3,0/10 | 4,0/10 | • Bonne formule 2 PPR<br>• GPIO directs prévus<br>• Moyenne et lissage envisagés<br>• Page PROGMEM et buffers bornés<br>• AP+STA, OTA et interface riches<br>• Schéma fonctionnel assez complet | • Plusieurs erreurs de compilation certaines<br>• `Ticker` local détruit après `setup()`<br>• `Ticker` n'est pas Timer1 matériel<br>• Polarité NPN/fail-safe inversée<br>• Sortie non relâchée au stall<br>• MT3608 faux buck 12→5 V<br>• AP/OTA fixes et HTTP sans auth<br>• LittleFS sans CRC ni écriture atomique |
| `gemini-free` | 2,7/10 | 3,6/10 | • Timer1 matériel visé<br>• Formule 2 PPR correcte<br>• Deux compteurs indépendants<br>• AP+STA et serveur asynchrone<br>• PROGMEM et documents JSON statiques<br>• `strlcpy` pour les identifiants<br>• Interface simple | • Prescaler DIV16 mal calculé : fréquence ≈ ÷16<br>• `.cpp` sans `Arduino.h`<br>• Premier front accepté comme période<br>• Ratio invalide conserve l'ancienne sortie<br>• Anti-rebond plafonne vers 6000 RPM<br>• Aucun filtre ni hardware sûr<br>• LittleFS non vérifié/non atomique<br>• API sans auth et AP trivial |
| `composer 2.5` | 2,0/10 | 4,0/10 | • Intention Timer1 et deux canaux<br>• Formule 2 PPR correcte<br>• EEPROM avec CRC16<br>• AP+STA non bloquant<br>• Pull-up d'entrée 3,3 V correct<br>• Sorties collecteur ouvert prévues<br>• Code compact et lisible | • `0xDEYE` et symbole WiFi bloquent le build<br>• Timer1 reçoit 10 ticks, pas 10 µs : fréquence ×5<br>• Flottants dans ISR<br>• Fraîcheur mise à jour avant validation<br>• Timeout de 3 s<br>• Secret STA exposé, aucune auth<br>• Hardware conseille 12 V direct sur pin 5V<br>• Pas de pull-down de base ni tests |
| `qwen3.6` | 2,0/10 | 3,0/10 | • Intention deux canaux et timeout<br>• Formule d'affichage 2 PPR correcte<br>• Ratios bornés<br>• AP+STA visé<br>• Documentation et BOM présentes<br>• NPN de sortie pertinent en principe | • API Timer ESP32 sur cible ESP8266<br>• Erreur syntaxique certaine<br>• Ratio multiplié au lieu d'être divisé<br>• Demi-période confondue avec période<br>• Canal ne redémarre pas après stall<br>• Polarité NPN inversée<br>• GPIO15 de strap risqué<br>• AP ouvert et alimentation 12 V directe |
| `gemma4` | 0,5/10 | 1,0/10 | • Interface Web compacte<br>• AP protégé au minimum WPA<br>• NPN open-collector mentionné<br>• Petit code facile à reprendre<br>• Endpoint d'observation simple | • Ne compile pas (`PIN_TACH_6m`, tokens Markdown)<br>• GPIO10/11 réservés à la flash<br>• Aucune génération tach ni Timer1<br>• Une sortie pour deux canaux<br>• Formule RPM fausse et compteurs jamais remis à zéro<br>• Aucune persistance ni STA réelle<br>• Aucun OR-ing/buck/hardware complet<br>• ISR non IRAM et aucun fail-safe |
| `qwen35` | 0,5/10 | 1,0/10 | • Nombreuse documentation<br>• Interface responsive<br>• AP protégé<br>• Bornes de ratios côté serveur<br>• Avertissements haute tension | • Plusieurs erreurs de syntaxe/API<br>• GPIO10/11 de la flash<br>• Aucun Timer1 ni génération conforme<br>• Ticker exprimé en secondes, pas millisecondes<br>• RPM issu de compteurs cumulatifs<br>• Une sortie moyenne pour deux canaux<br>• Aucune persistance utile<br>• Schéma faux, PNP présenté comme NPN |

## 4. Classements séparés

Les égalités sont départagées par l'autre note, puis par ordre alphabétique si les deux notes sont identiques.

### Classement global

1. `gpt-5.6 sol medium` — **8,4/10**
2. `claude opus 5 medium` — **8,0/10**
3. `grok 4.6 medium` — **7,1/10**
4. `glm5.3` — **6,9*/10**
5. `composer 2.5 prompt from claude` — **6,8/10**
6. `claude-sonnet5-free` — **4,5/10**
7. `gpt-free` — **4,2/10**
8. `gemini 3.1 pro` — **3,5/10**
9. `deepseek4 flash` — **3,4/10**
10. `qwen3.6_prompt_from_claude` — **3,0/10**
11. `gemini-free` — **2,7/10**
12. `composer 2.5` — **2,0/10**
13. `qwen3.6` — **2,0/10**
14. `gemma4` — **0,5/10**
15. `qwen35` — **0,5/10**

### Classement qualité de code

1. `claude opus 5 medium` — **8,5/10**
2. `gpt-5.6 sol medium` — **8,2/10**
3. `glm5.3` — **7,6*/10**
4. `grok 4.6 medium` — **7,3/10**
5. `claude-sonnet5-free` — **7,0/10**
6. `composer 2.5 prompt from claude` — **6,5/10**
7. `gpt-free` — **4,8/10**
8. `qwen3.6_prompt_from_claude` — **4,0/10**
9. `composer 2.5` — **4,0/10**
10. `deepseek4 flash` — **3,9/10**
11. `gemini-free` — **3,6/10**
12. `gemini 3.1 pro` — **3,0/10**
13. `qwen3.6` — **3,0/10**
14. `gemma4` — **1,0/10**
15. `qwen35` — **1,0/10**

## 5. Recommandation de fusion

La meilleure base de fusion est `gpt-5.6 sol medium`, en important sélectivement l'instrumentation et certaines primitives de `claude opus 5 medium`, puis les buffers réseau de `glm5.3`. Il ne faut pas fusionner mécaniquement les moteurs Timer1 : choisir un seul ordonnanceur, formaliser ses invariants et le valider sur la version exacte du core ESP8266.

### Fonctionnalités IoT à emprunter

- **Timer1/scheduler — base `gpt-5.6 sol medium`.** Conserver `outputTimerIsr()`, `remainingTicks[]` et `requestedHalfTicks[]` : un Timer1 `TIM_SINGLE` est réarmé sur la prochaine échéance des deux canaux avec une résolution de 0,2 µs. Ajouter l'idée de phase absolue et de rattrapage borné de `claude opus 5 medium` (`nextEdgeCcy`, `ccyReached()`, garde à 16 itérations), mais seulement après validation du chemin NMI sur une toolchain figée. Écarter les ticks permanents de `grok 4.6 medium` et `composer 2.5 prompt from claude`, le mauvais prescaler de `gemini-free`, le mauvais `timer1_write()` de `composer 2.5`, et tout usage de `Ticker` pour fabriquer le tach.
- **Boot readiness — fonctionnalité à ajouter, absente de toutes les solutions.** Initialiser capture et Timer1 avant réseau comme dans `gpt-5.6 sol medium::setup()`/`claude opus 5 medium::engineBegin()`, mais ajouter un état explicite `BOOT_WAITING`, une fenêtre de grâce mesurée et, seulement si le comportement Deye l'exige, un tach provisoire borné et limité dans le temps. Ce mode doit être clairement signalé dans l'UI et s'arrêter dès que plusieurs périodes réelles cohérentes sont acquises. Si les rails ventilateur ne sont pas présents avant validation tach, prévoir une alimentation auxiliaire : le logiciel ne peut pas résoudre cette dépendance circulaire.
- **Capture 2 PPR et fail-safe — combiner les deux leaders.** Reprendre la formule explicite de `gpt-5.6 sol medium::updateChannel()` (`30 000 000 / période_us`) et la configurabilité `inPpr/outPpr` de `claude opus 5 medium`. Exiger au moins trois périodes cohérentes avant activation, ne mettre à jour la fraîcheur qu'après validation, réduire le timeout à un multiple borné de la période plutôt qu'à 2–3 s fixes, puis relâcher immédiatement le collecteur par GPIO LOW en cas de faute. Réinitialiser filtre et phase après stall. Lire chaque ventilateur si la panne de chacun doit être détectée ; ne jamais utiliser `glm5.3::parallelFans`.
- **Filtres — hybride validation + médiane + EMA.** Garder l'EMA de période réinitialisable de `gpt-5.6 sol medium` ou l'EMA entière de `claude opus 5 medium`, précédée d'une médiane courte sur trois périodes et d'un rejet relatif d'outlier. La validation sur trois échantillons de `claude opus 5 medium` est préférable à l'acceptation d'un seul intervalle. Éviter `composer 2.5 prompt from claude::smoothPeriod()` lorsqu'il réinsère la même mesure à chaque tour de boucle et les seuils qui mettent à jour le timestamp avant validation.
- **WiFi — reprendre le socle `glm5.3`, durci.** Conserver `WIFI_AP_STA`, `WiFi.persistent(false)`, reconnexion non bloquante, mDNS, portail captif et secrets non renvoyés. Ajouter une authentification applicative, un contrôle `Origin`/jeton CSRF, un mot de passe AP unique au premier boot, un débit limité sur les écritures et la possibilité de désactiver AP une fois STA validé. Ne jamais reproduire `claude opus 5 medium::handleStatus()` qui expose `apPass`, ni les endpoints GET mutatifs de plusieurs concurrents.
- **Persistance — EEPROM binaire versionnée pour ce petit état.** Partir du `PersistedConfig` de `gpt-5.6 sol medium` ou du `Config` de `claude opus 5 medium` : `magic`, version, taille implicite/explicite, CRC32, validation `isfinite()` et bornes. Ajouter un double-slot avec compteur de génération et ne committer que si la valeur change. LittleFS n'est utile que si le schéma devient réellement extensible ; dans ce cas, imposer fichier temporaire, `flush`, renommage atomique, version et CRC.
- **Observabilité IoT — emprunter `claude opus 5 medium`.** Exposer compteurs de fronts acceptés/rejetés, âge du dernier front valide, état de readiness, cause de faute, saturation de sortie, fréquence réellement générée, cadence/retard maximal ISR, uptime et heap. La télémétrie ne doit jamais inclure de secret.

### Bonnes pratiques qualité code à intégrer

- **Séparer les couches.** Extraire du sketch monolithique un moteur tach indépendant, un module capture/filtre, un stockage, un réseau et une interface Web. L'organisation logique de `claude opus 5 medium` est la meilleure référence ; conserver toutefois une abstraction stable devant les registres/API privées.
- **Formaliser les échanges ISR.** Utiliser snapshots/versionnement comme `claude opus 5 medium::engineReadInput()` ou une boîte aux lettres à publication atomique. Restaurer l'état précédent des interruptions au lieu d'appeler aveuglément `interrupts()`. Ne jamais supposer que `noInterrupts()` protège contre une NMI.
- **Garder l'ISR minimale et entièrement auditée IRAM/DRAM.** Aucune allocation, `String`, flottant, `digitalWrite()`, accès réseau ou persistance. Figer le core ESP8266 et vérifier la map de liens des appels transitifs tels que `micros()`/`timer1_write()`.
- **Réduire le heap dynamique.** Servir la page via `PROGMEM`/`send_P` comme `claude opus 5 medium`, `grok 4.6 medium` et `glm5.3`. Pour JSON, reprendre les buffers `char` bornés et l'échappement de `glm5.3`; éviter les pages de 7–9 ko assemblées en `String`.
- **Rendre la compilation reproductible.** Ajouter `platformio.ini` ou une commande `arduino-cli`, verrouiller core et bibliothèques, puis compiler en CI. Des erreurs triviales comme `0xDEYE`, les API ESP32 dans `qwen3.6` ou la copie `volatile` de `deepseek4 flash` auraient alors été éliminées avant revue.
- **Tester les invariants.** Tests hôte pour conversions PPR, demi-périodes, saturation, wraparound, CRC et filtres ; tests sur cible pour boot, stall, glitches et reconnexion ; mesures oscilloscope de fréquence/jitter sous trafic HTTP et écriture flash.
- **Sécuriser les frontières.** Validation serveur stricte, longueurs bornées, `isfinite()`, terminaison NUL, échappement HTML/JSON, réponses HTTP d'erreur vérifiées côté UI, authentification et CSRF. Aucun mot de passe ne doit être relu par une API.
- **Conserver les commentaires vérifiables.** Documenter unités, polarité du NPN et limites mesurées, sans promettre « zéro jitter » ni « indépendance WiFi » non démontrée.
- **Suivre le heap.** Exposer heap courant et minimum historique, compteur d'échec d'allocation et seuil d'alerte, en s'inspirant de `claude opus 5 medium` et `glm5.3`.
- **Qualifier le hardware.** Références exactes, pinouts, courants, Vceo, marge thermique et courbes PTC/TVS/buck doivent remplacer les listes de composants « interchangeables ».

## 6. Architecture de référence recommandée

### Chaîne fonctionnelle

1. **Alimentation et protection.** Chaque rail 12 V Deye entre par son propre fusible/PTC puis une Schottky qualifiée ; les deux sorties sont réunies sans backfeed. Ajouter TVS choisie selon la tension réellement mesurée, condensateur de réserve et découplages locaux. Alimenter le Wemos par un buck ou buck-boost 12→5 V référencé, dimensionné pour les pointes WiFi et testé avec les rails réels. Vérifier l'équipotentialité des masses ; sinon isoler les signaux.
2. **Entrées tach.** Lire séparément chaque ventilateur requis. Pour un Noctua open-collector, utiliser une pull-up locale 3,3 V, une résistance série, un petit RC validé au scope et éventuellement un buffer à hystérésis. Ne jamais supposer une pull-up 12 V interne et ne jamais mettre plusieurs tach en parallèle pour en déduire une somme de fréquences.
3. **Capture.** ISR GPIO IRAM courte : lire CCOUNT ou une horloge validée, calculer l'intervalle wrap-safe, rejeter les périodes hors plage, puis publier uniquement les fronts valides. Le premier front amorce l'état ; aucun RPM n'est publié avant plusieurs intervalles cohérents.
4. **Validation et filtre.** Dans la boucle applicative, prendre un snapshot atomique, appliquer médiane sur trois intervalles, rejet relatif, puis EMA entière ou flottante hors ISR. Réinitialiser le filtre après perte. Calculer les RPM avec PPR explicite et configurable.
5. **Génération.** Un unique Timer1 matériel en one-shot ordonnance les deux prochaines échéances. Chaque canal conserve phase, demi-période et état GPIO. Une nouvelle période est publiée par boîte aux lettres et adoptée sur une frontière de front. La programmation utilise les vraies unités du prescaler, conserve les reliquats et expose saturation/quantification réelles.
6. **Fail-safe et readiness.** États explicites `BOOT_WAITING`, `VALIDATING`, `RUNNING`, `STALE`, `FAULT`. Activation après plusieurs périodes ; arrêt sur âge calculé depuis le dernier front valide, pas depuis le dernier front brut. Sortie inactive = GPIO LOW, NPN bloqué, ligne Deye relâchée. Un éventuel signal de boot doit être borné, temporisé, observable et validé contre la fenêtre réelle de diagnostic Deye.
7. **Configuration et réseau.** Le moteur tach démarre avant WiFi. AP+STA est non bloquant ; l'AP de récupération utilise un secret unique. API authentifiée, CSRF protégée, sans secret en lecture. Les changements réseau répondent avant un redémarrage différé ; les changements de ratio sont atomiques et immédiatement observables.
8. **Persistance et interface.** Deux slots EEPROM versionnés avec CRC32 et génération, écrits seulement lors d'un changement. Page Web en PROGMEM, réponses JSON en buffers bornés, heap minimal suivi. L'UI montre mesure, sortie réellement générée, readiness, dernière période valide, cause du fail-safe, saturation, jitter mesuré et état réseau.
9. **Validation.** Build CI verrouillé, tests unitaires des mathématiques et tests matériels automatisés. Vérifier au scope fréquence, duty-cycle, premier/dernier pulse, changement de ratio, stall, parasites, wrap temporel, trafic WiFi, sauvegarde flash et brown-out.

### Éléments à éviter absolument

- `Ticker`, `delayMicroseconds()` ou polling pour générer les fronts tach.
- Confusion entre période tach et demi-période électrique, ou entre microsecondes et ticks Timer1.
- Hypothèse de prescaler codée sans conversion explicite et garde liée à `F_CPU`.
- Mise à jour du timestamp de fraîcheur avant validation d'une période.
- Premier front converti en période depuis le boot.
- Tach de plusieurs ventilateurs en parallèle traité comme une somme fiable.
- NPN de sortie sans pull-down base-émetteur, polarité fail-safe inversée ou GPIO direct vers une pull-up 5/12 V.
- 12 V appliqué à la broche 5 V du Wemos, faux buck comme le MT3608, ou réunion directe de deux rails sans OR-ing.
- API non authentifiée capable de modifier ratio/PPR/WiFi, secrets renvoyés par JSON/HTML et écritures par GET.
- Dépendances privées non verrouillées, absence de preuve de compilation et revendications de jitter non mesurées.
