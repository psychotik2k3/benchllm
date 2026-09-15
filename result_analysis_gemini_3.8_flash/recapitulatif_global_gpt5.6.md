# Récapitulatif Global des Évaluations — Simulateur Tachymétrique Deye (ESP8266)

**Évaluateur** : Gemini 3.8 Flash (Analyse comparative multicritères)  
**Date** : Mardi 15 septembre 2026  
**Dossier d'analyse** : `result_analysis_gemini_3.8_flash/`  
**Échantillon évalué** : 18 compétiteurs (modèles LLM, variantes de thinking et architectures générées)

---

## 1. Inventaire des analyses

| Rang | Compétiteur | Note Globale | Note Qualité Code | Statut Technique & Résumé |
|:---:|---|:---:|:---:|---|
| **1** | **gpt-5.6 sol medium** | **8.5 / 10** | 8.5 / 10 | ✅ **Production Ready** — Ordonnanceur one-shot FRC1 (0,2 µs), schéma matériel ultra-complet (PTC, TVS, buck-boost, clamp NPN), CRC32. |
| **2** | **kimi_k3_max** | **8.4 / 10** | 8.6 / 10 | ✅ **Production Ready** — Ordonnanceur FRC1 one-shot calé sur les cycles CPU `ESP.getCycleCount()` (12,5 ns), schéma irréprochable (buck MP1584, OR-ing 1N5819, pull-up 3,3V, R4 B-E 10k), CRC16-CCITT, mise à jour à chaud sans reboot. |
| **3** | **claude opus 5 medium** | **8.3 / 10** | 8.7 / 10 | ✅ **Production Ready** — Moteur NMI Timer1 le plus poussé, seqlock sans verrou, zero-float, horloge CCOUNT, documentation irréprochable. |
| **4** | **kimi k3** *(thinking high)* | **7.6 / 10** | 8.0 / 10 | ✅ **Production Ready avec réserves** — FRC1 one-shot en `micros()`, bon schéma (buck MP1584), CRC16 ; pénalisé par le sommeil 1 s en idle et une fausse première période au boot (corrigés dans la version Max). |
| **5** | **grok 4.6 medium** | **7.2 / 10** | 7.4 / 10 | ⚠️ **Viable** — Bonne NMI Timer1 et schéma propre, mais quantification 20 µs, pas de lissage de période, rafraîchissement d'horodatage sur parasite. |
| **6** | **glm5.3** | **6.9 / 10** | 7.6 / 10 | ⚠️ **Viable avec corrections** — FRC1 one-shot, CRC32, zéro allocation web, mais postulat erroné sur la mise en parallèle des tachymètres. |
| **7** | **composer 2.5 prompt from claude** | **6.8 / 10** | 7.2 / 10 | ⚠️ **Viable avec corrections** — Excellent schéma et web asynchrone, mais `digitalWrite()` dans l'ISR à 20 kHz et dérive temporelle `now + half`. |
| **8** | **claude-sonnet5-free** | **5.8 / 10** | 7.5 / 10 | ❌ **Non viable matériellement** — Code très propre avec Timer1 10 kHz, mais schéma affirmant à tort une pull-up interne 12V sur Noctua (étage d'entrée inopérant). |
| **9** | **deepseek4 flash** | **5.5 / 10** | 6.2 / 10 | ⚠️ **Partiellement viable** — NMI Timer1 et virgule fixe Q16 remarquables, mais latence de calcul RPM à 1 000 ms et code monolithique. |
| **10** | **qwen3.6_prompt_from_claude** | **5.2 / 10** | 6.5 / 10 | ❌ **Crash au démarrage** — Excellentes intentions (AsyncWebServer, OTA, LittleFS), mais `Ticker scheduler` instancié localement dans `setup()` : timer détruit au boot ! |
| **11** | **gemini 3.1 pro** | **4.8 / 10** | 5.5 / 10 | ❌ **Non viable** — Mesure RPM rafraîchie seulement à 1 Hz (latence 1 s inacceptable pour Deye), pas de CRC, diodes 1N4148 sous-dimensionnées. |
| **12** | **gpt-free** | **4.5 / 10** | 6.0 / 10 | ❌ **Non viable matériellement** — Utilisation habile du waveform generator ESP8266, mais conseil absurde de pont diviseur sans pull-up sur le tach Noctua (signal à 0V). |
| **13** | **gemini-free** | **4.0 / 10** | 5.0 / 10 | ❌ **Non viable** — Aucun schéma, `delay(50)` dans loop(), et bug mathématique fatal : horloge rechargée 16 fois trop lentement (1,25 kHz au lieu de 20 kHz). |
| **14** | **composer 2.5** | **3.8 / 10** | 4.5 / 10 | ❌ **Dangereux** — Divisions flottantes en ISR, registres non atomiques, allocations String massives, et schéma préconisant d'injecter du 12V dans le Wemos. |
| **15** | **qwen3.6** | **2.0 / 10** | 2.5 / 10 | ❌ **Ne compile pas** — API timer ESP32 sur ESP8266, formule de période inversée (multiplication au lieu de division), 12V direct sur VIN Wemos. |
| **16** | **qwen3.6-35b-fp8-freetoken** | **1.5 / 10** | 2.5 / 10 | ❌ **Non fonctionnel & ne compile pas** — API timer ESP8266 invalide (`timer1Write`), attachement en mode `LOW` provoquant un crash WDT immédiat, `loadVal` non transmis au timer, mesure RPM détruite toutes les 10 ms, canal 2 non généré, diviseur d'entrée plafonnant à 1,25 V. |
| **17** | **qwen35** | **1.2 / 10** | 1.8 / 10 | ❌ **Ne compile pas & halluciné** — Erreurs de syntaxe, GPIO flash SPI, transistor PNP qualifié de NPN, Ticker réglé à 4 minutes, YAML d'un onduleur Growatt. |
| **18** | **gemma4** | **1.0 / 10** | 1.5 / 10 | ❌ **Projet factice** — Erreur de syntaxe, broches Flash SPI, sortie simulée jamais pilotée, RPM s'accumulant à l'infini, un seul canal. |

---

## 2. Tableau récapitulatif global comparatif

| Compétiteur | Note Globale | Note Qualité | Mode Timer1 / ISR | Schéma Électrique | Alim & Buck | Filtrage Entrée | Intégrité EEPROM/Flash | Interface Web |
|---|:---:|:---:|---|---|---|---|---|---|
| **gpt-5.6 sol medium** | **8.5** | **8.5** | One-shot FRC1 (0,2 µs), registres W1TS/W1TC | Exceptionnel (PTC, TVS, clamp NPN) | Schottky SS34 + Buck-boost UVLO | EMA (\(\alpha=0,25\)) + Bornage strict | CRC32 + Magic 4B | Thème sombre, polling 1s, API REST |
| **kimi_k3_max** | **8.4** | **8.6** | One-shot FRC1 (0,2 µs, CCOUNT), GPOS/GPOC | Exemplaire (pull-up 10k/3,3V, R4 B-E 10k) | Schottky 1N5819 + Buck MP1584 5V | Anti-rebond 400 µs + EMA (\(\alpha=0,25\)) | CRC16-CCITT + Magic 4B | Thème sombre, polling 1s, pas de reboot requis |
| **claude opus 5 medium** | **8.3** | **8.7** | NMI Vector (0,2 µs), Seqlock, GPOS/GPOC | Exhaustif et rigoureux (calculs complets) | Schottky SS34 + Buck MP1584EN 5V | Déparasiteur 25 kHz (120 µs) + EMA | CRC32 + Magic 4B | Thème sombre, polling 600ms, badges |
| **kimi k3** *(thinking high)* | **7.6** | **8.0** | One-shot FRC1 (micros, 5 ticks/µs), GPOS/GPOC | Très bon (pull-up 10k/3,3V, R1 1k) | Schottky 1N5819 + Buck MP1584 5V | Anti-rebond 300 µs + EMA entier | CRC16-CCITT + Magic 4B | Thème sombre, polling 1s, reboot sur config |
| **grok 4.6 medium** | **7.2** | **7.4** | NMI Vector périodique (20 µs tick), GPOS/GPOC | Très bon (alerte anti-parallèle) | Schottky SS14 + Buck MP1584/Mini360 | Anti-rebond 200 µs seul (pas d'EMA) | CRC32 + Magic 4B | Thème sombre, polling 500ms |
| **glm5.3** | **6.9** | **7.6** | One-shot FRC1, cycles CCOUNT, GPOS/GPOC | Très soigné (TVS SMBJ14A, BC337) | Schottky SS14 + Buck MP1584EN | Fenêtrage + EMA (\(\alpha=0,25\)) | CRC32 + Magic 4B | Zéro allocation (snprintf), mDNS |
| **composer 2.5 prompt claude** | **6.8** | **7.2** | Périodique 50 µs, digitalWrite() sous ISR | Très bon (dimensionnement BC547) | Schottky SS14 + Buck MT3608 | Moyenne 4 éch. + Slew rate limit | LittleFS config.json (ArduinoJson) | ESPAsyncWebServer + dynamic fetch |
| **claude-sonnet5-free** | **5.8** | **7.5** | Périodique 100 µs (10 kHz), GPOS/GPOC | Erreur critique : suppose pull-up 12V Noctua | Schottky SS14 + Buck 12V→5V | Rejet < 2 ms (pas de lissage) | Checksum simple pondéré | Thème sombre, badges dynamiques |
| **deepseek4 flash** | **5.5** | **6.2** | NMI FRC1 one-shot, Virgule fixe Q16 | Très bon (diodes clamp BAT54S) | Schottky SS34 + Buck MP1584/LM2596 | Anti-rebond 400 µs + EMA shift 3 | CRC8 + Magic 4B | Curseurs AJAX en direct |
| **qwen3.6 prompt claude** | **5.2** | **6.5** | Ticker 20 kHz (Détruit à la fin de setup !) | Très bon (calculs RC entrée) | Schottky BAT54 + Buck MT3608 | Moyenne 4 éch. (Borne trop basse) | LittleFS config.json | AsyncWebServer + OTA |
| **gemini 3.1 pro** | **4.8** | **5.5** | Périodique 50 µs, GPOS/GPOC | Dangereux (diodes 1N4148 en alim) | Buck présent mais diodes fragiles | Aucun filtre anti-rebond | Magic seul (aucun CRC) | Remplacements String sur le tas |
| **gpt-free** | **4.5** | **6.0** | Core waveform generator (tone/waveform) | Inopérant (pont diviseur sans pull-up) | Non documenté (aucun schéma) | IIR shift 3 + Rejet < 2,5 ms | Magic seul (aucun CRC) | Web standard, polling 500ms |
| **gemini-free** | **4.0** | **5.0** | Timer1 erroné (800 µs au lieu de 50 µs) | Inexistant (aucun schéma) | Non documenté | Anti-rebond 2 ms seul | LittleFS (sans check return) | AsyncWebServer + delay(50) dans loop |
| **composer 2.5** | **3.8** | **4.5** | Périodique 10 µs, float math en ISR, RW GPIO | Dangereux (12V direct sur Wemos) | Aucun buck (destruction LDO) | Bornage simple (500 µs - 600 ms) | CRC16 | String churn massif (`<meta refresh>`) |
| **qwen3.6** | **2.0** | **2.5** | Incompilable (API Timer ESP32 sur ESP8266) | Destructeur (12V direct sur VIN) | Aucun buck (destruction LDO) | Période simulée inversée (× ratio) | Aucun CRC | Formulaire basique |
| **qwen3.6-35b-fp8-freetoken** | **1.5** | **2.5** | Faux FRC2, `LOW` interrupt (WDT crash), timer1Write non standard | Erreur critique : diviseur 1,25 V max, 4 fans // | Buck 3,3V vers VIN (brownout LDO) | Rejet borné seul, reset RPM si > 10 ms sans pulse | Aucun CRC/magic | HTML String concat, reload 5s |
| **qwen35** | **1.2** | **1.8** | Incompilable (stray backtick, GPOS fonction) | Erroné (PNP SS8550 pris pour NPN) | Incohérent | Ticker 250s, broches Flash SPI | Aucun | Hallucination Growatt SPF YAML |
| **gemma4** | **1.0** | **1.5** | Sortie jamais pilotée, compte à l'infini | Inexistant | Inexistant | Broches Flash SPI (crash boot) | Aucun | Formulaire factice |

---

## 3. Notes révisées et justifications d'harmonisation

Le barème d'évaluation a été calibré avec une exigence stricte portant sur la **viabilité réelle** en environnement industriel (onduleur solaire haute tension Deye SUN-8K) et la **sécurité matérielle**.

1. **gpt-5.6 sol medium (8.5 / 10)** : Reçoit la note la plus haute du benchmark. C'est le livrable le plus complet sur le plan des sécurités matérielles (fusibles PTC réarmables, diodes transil TVS, clamp adaptateur NPN en entrée, buck-boost avec UVLO) associé à un ordonnanceur one-shot à haute précision (0,2 µs) et une architecture logicielle modulaire en C++ moderne sans faille.
2. **kimi_k3_max (8.4 / 10)** : Directement sur la 2ᵉ marche du podium. Le firmware atteint un niveau d'excellence remarquable : ordonnanceur FRC1 one-shot cadencé au cycle CPU près (`ESP.getCycleCount()`, 12,5 ns), accumulation d'échéances sans dérive cumulative, calculs entiers sous ISR, commutation par `GPOS`/`GPOC`, lissage EMA (\(\alpha=0,25\)), intégrité EEPROM validée par CRC16-CCITT, et mise à jour dynamique des ratios sans redémarrage de l'ESP8266. Le schéma électronique est réaliste, sûr (MP1584 réglé à 5,0 V sur la broche 5V, pull-up 10 kΩ vers 3,3V sur le collecteur ouvert Noctua, pull-down 10 kΩ base-émetteur) et proscrit formellement le couplage parallèle des signaux.
3. **claude opus 5 medium (8.3 / 10)** : L'implémentation logicielle la plus remarquable (vecteur NMI direct de niveau 3, registre de cycles CPU `ccount`, protocole seqlock sans verrou, zero-float en ISR). Légèrement pénalisée par rapport à GPT-5.6 Sol Medium et Kimi K3 Max en raison de la complexité extrême du code NMI bas niveau (difficile à maintenir pour un non-expert) et d'une fuite mineure du mot de passe AP dans le JSON de statut.
4. **kimi k3 *(thinking high)* (7.6 / 10)** : Très solide compétiteur précurseur de la version Max. Il mettait déjà en place un FRC1 one-shot propre à registre atomique, une gestion EEPROM avec CRC16 et un schéma électronique réaliste avec MP1584. Cependant, il présentait encore quelques défauts de jeunesse gommés dans la version Max : sommeil Timer1 de 1 s en phase inactive pouvant retarder la reprise, calcul `now - 0` au premier front d'entrée, absence de pull-down base-émetteur sur les transistors au boot et écrasement de saisie dans le formulaire web.
5. **grok 4.6 medium (7.2 / 10)** : Excellente tenue générale avec sa NMI Timer1 et sa documentation claire interdisant le couplage parallèle des tachymètres. Toutefois, la cadence fixe à 50 kHz introduit une gigue de 20 µs, le signal manque de lissage et le rafraîchissement d'horodatage sur front parasité peut empêcher le timeout de sécurité.
6. **glm5.3 (6.9 / 10)** : Très haute note de qualité de code (7.6/10) grâce à une gestion mémoire exemplaire (zéro allocation dynamique, snprintf partout) et un timer one-shot FRC1 calé sur `ESP.getCycleCount()`. Sa note globale est dégradée par une erreur d'ingénierie physique : supposer que deux signaux collecteur ouvert en parallèle s'additionnent mathématiquement, ce qui fausse le ratio appliqué à la sortie.
7. **composer 2.5 prompt from claude (6.8 / 10)** : Présente un excellent schéma électronique et un serveur asynchrone élégant. Sa note est pénalisée par deux défauts d'ISR : l'usage de `digitalWrite()` à 20 kHz (consommant 15 % du temps CPU) et l'incrémentation `now + half` qui cumule les retards temporels.
8. **claude-sonnet5-free (5.8 / 10)** : Excellente note de qualité de code (7.5/10) pour sa structure claire et son Timer1 à 10 kHz, mais sanctionné sur la note globale par une erreur fatale dans son schéma électronique : l'affirmation que le ventilateur Noctua possède une pull-up interne 12V rend son montage d'entrée incapable de détecter la moindre rotation.
9. **deepseek4 flash (5.5 / 10)** : Salué pour son moteur NMI en virgule fixe Q16 et son schéma avec diodes de clamp BAT54S. En revanche, le calcul de vitesse basé sur un comptage d'impulsions rafraîchi une seule fois par seconde introduit une latence de 1 000 ms entre la consigne physique et l'affichage web, et son code monolithique est difficile à appréhender.
10. **qwen3.6_prompt_from_claude (5.2 / 10)** : Très bon travail documentaire et structurel (LittleFS, AsyncWebServer, OTA), mais neutralisé par une erreur de programmation fatale : l'ordonnanceur `Ticker scheduler` est déclaré en variable locale dans `setup()`, ce qui le détruit dès la fin de l'initialisation.
11. **gemini 3.1 pro (4.8 / 10)** : Bien que doté d'un Timer1 fonctionnel à registres atomiques, la mise à jour de la consigne à une cadence d'une seule seconde est inadaptée aux accélérations thermiques de l'onduleur Deye, et la préconisation de diodes 1N4148 (150 mA max) pour alimenter un ESP8266 sous pics WiFi constitue une faute électronique.
12. **gpt-free (4.5 / 10)** : Code lisible exploitant la génération de forme d'onde native, mais absence de schéma et recommandation d'un pont diviseur passif sans pull-up sur le tach Noctua (la broche reste perpétuellement à 0V).
13. **gemini-free (4.0 / 10)** : Code inachevé sans documentation, comportant un `delay(50)` dans `loop()` et une minuterie Timer1 divisée par 16 par inadvertance (fréquence réelle de 1,25 kHz au lieu de 20 kHz).
14. **composer 2.5 (3.8 / 10)** : Multiplie les non-sens : calculs flottants en interruption, registres GPIO non atomiques en lecture-modification-écriture, et schéma proposant d'alimenter directement la broche 5V du Wemos en 12V.
15. **qwen3.6 (2.0 / 10)** : Code incompilable (API timer ESP32 injectée dans un projet ESP8266, syntaxe invalide) et formule mathématique inversée.
16. **qwen3.6-35b-fp8-freetoken (1.5 / 10)** : Projet lourdement défaillant qui accumule les fautes éliminatoires : fonctions timer non déclarées dans le core ESP8266 (`timer1Write`, `timer1AttachInterrupt`), `loadVal` calculé mais jamais écrit dans le timer (écrit `0` en dur), attachement d'interruption en mode `LOW` provoquant la saturation CPU et un WDT reset dès la première impulsion, arrêt du timer dès qu'aucune impulsion n'est reçue pendant 10 ms (signal tach haché en permanence), canal 2 jamais généré, pont diviseur d'entrée plafonnant à 1,25 V (seuil HIGH inaccessible), regroupement de 4 ventilateurs asynchrones sur une broche unique et buck 3,3V branché sur l'entrée VIN du régulateur du Wemos.
17. **qwen35 (1.2 / 10)** : Incompilable, utilise les broches de la Flash SPI, confond un transistor PNP avec un NPN, et hallucine un script Home Assistant pour un onduleur Growatt.
18. **gemma4 (1.0 / 10)** : Projet fantôme ne compilant pas, utilisant les broches interdites du bus Flash SPI et ne pilotant jamais aucune sortie physique.

---

## 4. Classements séparés

### Classement 1 : Note Globale (Viabilité projet, sécurité matérielle, exactitude temps-réel)

1. **gpt-5.6 sol medium** — **8.5 / 10**
2. **kimi_k3_max** — **8.4 / 10**
3. **claude opus 5 medium** — **8.3 / 10**
4. **kimi k3** *(thinking high)* — **7.6 / 10**
5. **grok 4.6 medium** — **7.2 / 10**
6. **glm5.3** — **6.9 / 10**
7. **composer 2.5 prompt from claude** — **6.8 / 10**
8. **claude-sonnet5-free** — **5.8 / 10**
9. **deepseek4 flash** — **5.5 / 10**
10. **qwen3.6_prompt_from_claude** — **5.2 / 10**
11. **gemini 3.1 pro** — **4.8 / 10**
12. **gpt-free** — **4.5 / 10**
13. **gemini-free** — **4.0 / 10**
14. **composer 2.5** — **3.8 / 10**
15. **qwen3.6** — **2.0 / 10**
16. **qwen3.6-35b-fp8-freetoken** — **1.5 / 10**
17. **qwen35** — **1.2 / 10**
18. **gemma4** — **1.0 / 10**

---

### Classement 2 : Qualité de Code (Architecture C++, maintenabilité, robustesse logicielle)

1. **claude opus 5 medium** — **8.7 / 10** (Maîtrise absolue du bas niveau Xtensa, seqlock, zero-float)
2. **kimi_k3_max** — **8.6 / 10** (Code pur, sans gigue cumulative, CCOUNT 12,5 ns, zéro float en ISR, CRC16)
3. **gpt-5.6 sol medium** — **8.5 / 10** (C++ moderne, namespace, constexpr, robustesse sans faille)
4. **kimi k3** *(thinking high)* — **8.0 / 10** (Structure très claire, FRC1 one-shot compact, factorisé et soigné)
5. **glm5.3** — **7.6 / 10** (Zéro allocation dynamique, snprintf, structure très propre)
6. **claude-sonnet5-free** — **7.5 / 10** (Code extrêmement lisible, didactique et bien typé)
7. **grok 4.6 medium** — **7.4 / 10** (Code propre, bonne encapsulation, CRC32 irréprochable)
8. **composer 2.5 prompt from claude** — **7.2 / 10** (Excellente intégration LittleFS et ArduinoJson)
9. **qwen3.6_prompt_from_claude** — **6.5 / 10** (Belle structure C++, pénalisée par le scope local du Ticker)
10. **deepseek4 flash** — **6.2 / 10** (Excellente technique en virgule fixe Q16, mais fichier monolithique)
11. **gpt-free** — **6.0 / 10** (Code bien présenté mais dépendant d'un en-tête non public)
12. **gemini 3.1 pro** — **5.5 / 10** (Code trop simpliste, allocations String non maîtrisées)
13. **gemini-free** — **5.0 / 10** (Inachevé, présence de delay() bloquant)
14. **composer 2.5** — **4.5 / 10** (Float en ISR, écriture de registre non atomique)
15. **qwen3.6** — **2.5 / 10** (Mélange de plateformes incompatible, syntaxe invalide)
16. **qwen3.6-35b-fp8-freetoken** — **2.5 / 10** (Bonne mise en forme mais fonctions inexistantes et erreurs bas niveau critiques)
17. **qwen35** — **1.8 / 10** (Erreurs C++ basiques, registres pris pour des fonctions)
18. **gemma4** — **1.5 / 10** (Code factice non compilable)

---

## 5. Focus : Impact du Thinking Level (Kimi K3 Thinking High vs Kimi K3 Max)

La comparaison directe entre les deux variantes du même moteur met en évidence des gains d'ingénierie majeurs apportés par la version Max :

| Critère technique | **Kimi K3 (Thinking High)** | **Kimi K3 Max** | Impact sur le système réel |
|---|---|---|---|
| **Résolution d'horloge** | `micros()` (1 µs) | `ESP.getCycleCount()` (12,5 ns) | Précision temporelle accrue d'un facteur 80, gigue de phase négligeable. |
| **Comportement en reprise (idle)** | Sommeil d'1 s fixe ; réveil passif | Réarmement immédiat dès qu'une voie s'active | Élimination de la latence d'attente d'1 s lors du démarrage des ventilateurs. |
| **Première mesure d'entrée** | `dt = now - 0` accepté sans plafond | Validation et garde-fou | Pas de distorsion de l'EMA au premier front. |
| **Mise à jour des ratios** | Redémarrage de l'ESP8266 requis | À chaud, immédiate et sans redémarrage | Pas d'interruption du signal tachymétrique vers l'onduleur. |
| **Sécurité matérielle au boot** | Pas de pull-down B-E | Résistance R4 (10 kΩ) base-émetteur | Transistor rigoureusement bloqué pendant les phases Hi-Z du bootloader. |
| **Ergonomie du formulaire Web** | Écrasement périodique par polling AJAX | Protection `document.activeElement` | Saisie fluide sans réinitialisation involontaire du champ en cours d'édition. |
| **Note Globale / Qualité** | **7.6 / 8.0** | **8.4 / 8.6** | **+0.8 point global**, passant de la 4ᵉ à la 2ᵉ place du classement général. |

---

## 6. Recommandation pour la fusion (Architecture cible idéale)

L'analyse comparative approfondie des 18 livrables met en lumière les composants d'élite à retenir pour concevoir le **simulateur tachymétrique ultime** pour l'onduleur Deye SUN-8K :

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        ARCHITECTURE DE SYNTHÈSE IDÉALE                                 │
├────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                        │
│  [ÉTAGE ALIMENTATION & PROTECTION] (Emprunté à GPT-5.6 Sol Medium, Kimi & GLM5.3)      │
│  ├── 2x Connecteurs 12V ventilateurs onduleur Deye                                    │
│  ├── Fusibles réarmables PTC (750 mA) + OU-diode Schottky SS34 (3A / 40V)             │
│  ├── Diode Transil TVS de protection contre les surtensions transitoires (SMBJ18A)     │
│  ├── Condensateur réservoir de 1 000 µF / 25V (absorption de coupure transitoire)       │
│  └── Convertisseur Buck DC/DC externe MP1584EN réglé à 5,0V vers la broche 5V du Wemos │
│                                                                                        │
│  [ÉTAGE D'ENTRÉE TACHYMÉTRIQUE] (Emprunté à Kimi K3 Max & GPT-5.6 Sol Medium)          │
│  ├── Raccordement d'UN SEUL ventilateur physique par paire (interdiction du parallèle) │
│  ├── Résistance de pull-up 10 kΩ vers 3,3V Wemos (compatible collecteur ouvert Noctua) │
│  ├── Résistance série de 1 kΩ + diodes de clamp BAT54S vers rail 3,3V (protection ESD)│
│  └── Capture par interruption GPIO matérielle (attachInterrupt FALLING) horodatée CCOUNT│
│                                                                                        │
│  [COEUR TEMPS-RÉEL & GÉNERATION] (Emprunté à Kimi K3 Max & Claude Opus 5 Medium)       │
│  ├── Ordonnanceur Timer1 (FRC1) one-shot sur échéances absolues en cycles CPU (12,5 ns)│
│  ├── Absence totale de dérive temporelle cumulative (accumulation nextEdgeCycle += hp) │
│  ├── Commutation atomique instantanée des broches par registres matériels GPOS et GPOC │
│  ├── Calculs d'ISR 100% en entiers / cycles (zéro flottant en interruption)           │
│  ├── Filtrage passe-bas exponentiel (EMA alpha = 0,25) et timeout de sécurité 1,5 s    │
│  └── Section critique ultra-courte (< 1 µs) pour la mise à jour des consignes         │
│                                                                                        │
│  [ÉTAGE DE SORTIE COLLECTEUR OUVERT] (Emprunté à Kimi K3 Max & GPT-5.6 Sol Medium)    │
│  ├── 2x Transistors NPN rapides (2N2222 ou BC337) en collecteur ouvert                 │
│  ├── Résistance de base 2,2 kΩ pilotée par les GPIOs ESP8266                           │
│  ├── Résistance de pull-down 10 kΩ entre base et émetteur (maintien bloqué au boot)   │
│  └── Collecteurs reliés directement aux entrées tachymétriques de l'onduleur Deye     │
│                                                                                        │
│  [SERVEUR WEB & GESTION SYSTÈME] (Emprunté à Kimi K3 Max, GLM5.3 & Composer 2.5)       │
│  ├── Interface web claire, thème sombre, rafraîchissement asynchrone toutes les 1 s   │
│  ├── Zéro allocation dynamique via tampons statiques pré-réservés (snprintf sans %f)   │
│  ├── Application immédiate des ratios en temps réel SANS nécessiter de redémarrage     │
│  ├── Persistance de la configuration en EEPROM protégée par mot magique et CRC16/CRC32 │
│  └── Reconnexion automatique WiFi non bloquante et point d'accès de secours permanent  │
│                                                                                        │
└────────────────────────────────────────────────────────────────────────────────────────┘
```
