# Récapitulatif Global des Évaluations — Simulateur Tachymétrique Deye (ESP8266)

**Évaluateur** : Gemini 3.8 Flash (Analyse comparative multicritères)  
**Date** : Vendredi 4 septembre 2026  
**Dossier d'analyse** : `result_analysis_gemini_3.8_flash/`  
**Échantillon évalué** : 15 compétiteurs (modèles LLM et architectures générées)

---

## 1. Inventaire des analyses

| Rang | Compétiteur | Note Globale | Note Qualité Code | Statut Technique & Résumé |
|:---:|---|:---:|:---:|---|
| **1** | **gpt-5.6 sol medium** | **8.5 / 10** | 8.5 / 10 | ✅ **Production Ready** — Ordonnanceur one-shot FRC1 (0,2 µs), schéma matériel ultra-complet (PTC, TVS, buck-boost, clamp NPN), CRC32. |
| **2** | **claude opus 5 medium** | **8.3 / 10** | 8.7 / 10 | ✅ **Production Ready** — Moteur NMI Timer1 le plus poussé, seqlock sans verrou, zero-float, horloge CCOUNT, documentation irréprochable. |
| **3** | **grok 4.6 medium** | **7.2 / 10** | 7.4 / 10 | ⚠️ **Viable** — Bonne NMI Timer1 et schéma propre, mais quantification 20 µs, pas de lissage de période, rafraîchissement d'horodatage sur parasite. |
| **4** | **glm5.3** | **6.9 / 10** | 7.6 / 10 | ⚠️ **Viable avec corrections** — FRC1 one-shot, CRC32, zéro allocation web, mais postulat erroné sur la mise en parallèle des tachymètres. |
| **5** | **composer 2.5 prompt from claude** | **6.8 / 10** | 7.2 / 10 | ⚠️ **Viable avec corrections** — Excellent schéma et web asynchrone, mais `digitalWrite()` dans l'ISR à 20 kHz et dérive temporelle `now + half`. |
| **6** | **claude-sonnet5-free** | **5.8 / 10** | 7.5 / 10 | ❌ **Non viable matériellement** — Code très propre avec Timer1 10 kHz, mais schéma affirmant à tort une pull-up interne 12V sur Noctua (étage d'entrée inopérant). |
| **7** | **deepseek4 flash** | **5.5 / 10** | 6.2 / 10 | ⚠️ **Partiellement viable** — NMI Timer1 et virgule fixe Q16 remarquables, mais latence de calcul RPM à 1 000 ms et code monolithique. |
| **8** | **qwen3.6_prompt_from_claude** | **5.2 / 10** | 6.5 / 10 | ❌ **Crash au démarrage** — Excellentes intentions (AsyncWebServer, OTA, LittleFS), mais `Ticker scheduler` instancié localement dans `setup()` : timer détruit au boot ! |
| **9** | **gemini 3.1 pro** | **4.8 / 10** | 5.5 / 10 | ❌ **Non viable** — Mesure RPM rafraîchie seulement à 1 Hz (latence 1 s inacceptable pour Deye), pas de CRC, diodes 1N4148 sous-dimensionnées. |
| **10** | **gpt-free** | **4.5 / 10** | 6.0 / 10 | ❌ **Non viable matériellement** — Utilisation habile du waveform generator ESP8266, mais conseil absurde de pont diviseur sans pull-up sur le tach Noctua (signal à 0V). |
| **11** | **gemini-free** | **4.0 / 10** | 5.0 / 10 | ❌ **Non viable** — Aucun schéma, `delay(50)` dans loop(), et bug mathématique fatal : horloge rechargée 16 fois trop lentement (1,25 kHz au lieu de 20 kHz). |
| **12** | **composer 2.5** | **3.8 / 10** | 4.5 / 10 | ❌ **Dangereux** — Divisions flottantes en ISR, registres non atomiques, allocations String massives, et schéma préconisant d'injecter du 12V dans le Wemos. |
| **13** | **qwen3.6** | **2.0 / 10** | 2.5 / 10 | ❌ **Ne compile pas** — API timer ESP32 sur ESP8266, formule de période inversée (multiplication au lieu de division), 12V direct sur VIN Wemos. |
| **14** | **qwen35** | **1.2 / 10** | 1.8 / 10 | ❌ **Ne compile pas & halluciné** — Erreurs de syntaxe, GPIO flash SPI, transistor PNP qualifié de NPN, Ticker réglé à 4 minutes, YAML d'un onduleur Growatt. |
| **15** | **gemma4** | **1.0 / 10** | 1.5 / 10 | ❌ **Projet factice** — Erreur de syntaxe, broches Flash SPI, sortie simulée jamais pilotée, RPM s'accumulant à l'infini, un seul canal. |

---

## 2. Tableau récapitulatif global comparatif

| Compétiteur | Note Globale | Note Qualité | Mode Timer1 / ISR | Schéma Électrique | Alim & Buck | Filtrage Entrée | Intégrité EEPROM/Flash | Interface Web |
|---|:---:|:---:|---|---|---|---|---|---|
| **gpt-5.6 sol medium** | **8.5** | **8.5** | One-shot FRC1 (0,2 µs), registres W1TS/W1TC | Exceptionnel (PTC, TVS, clamp NPN) | Schottky SS34 + Buck-boost UVLO | EMA (\(\alpha=0,25\)) + Bornage strict | CRC32 + Magic 4B | Thème sombre, polling 1s, API REST |
| **claude opus 5 medium** | **8.3** | **8.7** | NMI Vector (0,2 µs), Seqlock, GPOS/GPOC | Exhaustif et rigoureux (calculs complets) | Schottky SS34 + Buck MP1584EN 5V | Déparasiteur 25 kHz (120 µs) + EMA | CRC32 + Magic 4B | Thème sombre, polling 600ms, badges |
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
| **qwen35** | **1.2** | **1.8** | Incompilable (stray backtick, GPOS fonction) | Erroné (PNP SS8550 pris pour NPN) | Incohérent | Ticker 250s, broches Flash SPI | Aucun | Hallucination Growatt SPF YAML |
| **gemma4** | **1.0** | **1.5** | Sortie jamais pilotée, compte à l'infini | Inexistant | Inexistant | Broches Flash SPI (crash boot) | Aucun | Formulaire factice |

---

## 3. Notes révisées et justifications d'harmonisation

Le barème d'évaluation a été calibré avec une exigence stricte portant sur la **viabilité réelle** en environnement industriel (onduleur solaire haute tension Deye SUN-8K) et la **sécurité matérielle**.

1. **gpt-5.6 sol medium (8.5 / 10)** : Reçoit la note la plus haute du benchmark. C'est le seul livrable qui combine un ordonnanceur one-shot à haute précision (0,2 µs), une immunité matérielle complète (fusibles PTC réarmables, diodes transil TVS, clamp adaptateur NPN en entrée) et une architecture logicielle modulaire en C++ moderne sans faille bloquante.
2. **claude opus 5 medium (8.3 / 10)** : L'implémentation logicielle la plus remarquable (vecteur NMI direct, registre de cycles CPU `ccount`, protocole seqlock sans verrou, zero-float en ISR). Légèrement pénalisée par rapport à GPT-5.6 Sol Medium en raison de la complexité extrême du code NMI bas niveau (difficile à maintenir pour un non-expert) et d'une fuite mineure du mot de passe AP dans le JSON de statut.
3. **grok 4.6 medium (7.2 / 10)** : Excellente tenue générale avec sa NMI Timer1 et sa documentation claire interdisant le couplage parallèle des tachymètres. Toutefois, la cadence fixe à 50 kHz introduit une gigue de 20 µs, le signal manque de lissage et le rafraîchissement d'horodatage sur front parasité peut empêcher le timeout de sécurité.
4. **glm5.3 (6.9 / 10)** : Très haute note de qualité de code (7.6/10) grâce à une gestion mémoire exemplaire (zéro allocation dynamique, snprintf partout) et un timer one-shot FRC1 calé sur `ESP.getCycleCount()`. Sa note globale est dégradée par une erreur d'ingénierie physique : supposer que deux signaux collecteur ouvert en parallèle s'additionnent mathématiquement, ce qui fausse le ratio appliqué à la sortie.
5. **composer 2.5 prompt from claude (6.8 / 10)** : Présente un excellent schéma électronique et un serveur asynchrone élégant. Sa note est pénalisée par deux défauts d'ISR : l'usage de `digitalWrite()` à 20 kHz (consommant 15 % du temps CPU) et l'incrémentation `now + half` qui cumule les retards temporels.
6. **claude-sonnet5-free (5.8 / 10)** : Excellente note de qualité de code (7.5/10) pour sa structure claire et son Timer1 à 10 kHz, mais sanctionné sur la note globale par une erreur fatale dans son schéma électronique : l'affirmation que le ventilateur Noctua possède une pull-up interne 12V rend son montage d'entrée incapable de détecter la moindre rotation.
7. **deepseek4 flash (5.5 / 10)** : Salué pour son moteur NMI en virgule fixe Q16 et son schéma avec diodes de clamp BAT54S. En revanche, le calcul de vitesse basé sur un comptage d'impulsions rafraîchi une seule fois par seconde introduit une latence de 1 000 ms entre la consigne physique et l'affichage web, et son code monolithique est difficile à appréhender.
8. **qwen3.6_prompt_from_claude (5.2 / 10)** : Très bon travail documentaire et structurel (LittleFS, AsyncWebServer, OTA), mais neutralisé par une erreur de programmation fatale : l'ordonnanceur `Ticker scheduler` est déclaré en variable locale dans `setup()`, ce qui le détruit dès la fin de l'initialisation.
9. **gemini 3.1 pro (4.8 / 10)** : Bien que doté d'un Timer1 fonctionnel à registres atomiques, la mise à jour de la consigne à une cadence d'une seule seconde est inadaptée aux accélérations thermiques de l'onduleur Deye, et la préconisation de diodes 1N4148 (150 mA max) pour alimenter un ESP8266 sous pics WiFi constitue une faute électronique.
10. **gpt-free (4.5 / 10)** : Code lisible exploitant la génération de forme d'onde native, mais absence de schéma et recommandation d'un pont diviseur passif sans pull-up sur le tach Noctua (la broche reste perpétuellement à 0V).
11. **gemini-free (4.0 / 10)** : Code inachevé sans documentation, comportant un `delay(50)` dans `loop()` et une minuterie Timer1 divisée par 16 par inadvertance (fréquence réelle de 1,25 kHz au lieu de 20 kHz).
12. **composer 2.5 (3.8 / 10)** : Multiplie les non-sens : calculs flottants en interruption, registres GPIO non atomiques en lecture-modification-écriture, et schéma proposant d'alimenter directement la broche 5V du Wemos en 12V.
13. **qwen3.6 (2.0 / 10)** : Code incompilable (API timer ESP32 injectée dans un projet ESP8266, syntaxe invalide) et formule mathématique inversée.
14. **qwen35 (1.2 / 10)** : Incompilable, utilise les broches de la Flash SPI, confond un transistor PNP avec un NPN, et hallucine un script Home Assistant pour un onduleur Growatt.
15. **gemma4 (1.0 / 10)** : Projet fantôme ne compilant pas, utilisant les broches interdites du bus Flash SPI et ne pilotant jamais aucune sortie physique.

---

## 4. Classements séparés

### Classement 1 : Note Globale (Viabilité projet, sécurité matérielle, exactitude temps-réel)

1. **gpt-5.6 sol medium** — **8.5 / 10**
2. **claude opus 5 medium** — **8.3 / 10**
3. **grok 4.6 medium** — **7.2 / 10**
4. **glm5.3** — **6.9 / 10**
5. **composer 2.5 prompt from claude** — **6.8 / 10**
6. **claude-sonnet5-free** — **5.8 / 10**
7. **deepseek4 flash** — **5.5 / 10**
8. **qwen3.6_prompt_from_claude** — **5.2 / 10**
9. **gemini 3.1 pro** — **4.8 / 10**
10. **gpt-free** — **4.5 / 10**
11. **gemini-free** — **4.0 / 10**
12. **composer 2.5** — **3.8 / 10**
13. **qwen3.6** — **2.0 / 10**
14. **qwen35** — **1.2 / 10**
15. **gemma4** — **1.0 / 10**

---

### Classement 2 : Qualité de Code (Architecture C++, maintenabilité, robustesse logicielle)

1. **claude opus 5 medium** — **8.7 / 10** (Maîtrise absolue du bas niveau Xtensa, seqlock, zero-float)
2. **gpt-5.6 sol medium** — **8.5 / 10** (C++ moderne, namespace, constexpr, robustesse sans faille)
3. **glm5.3** — **7.6 / 10** (Zéro allocation dynamique, snprintf, structure très propre)
4. **claude-sonnet5-free** — **7.5 / 10** (Code extrêmement lisible, didactique et bien typé)
5. **grok 4.6 medium** — **7.4 / 10** (Code propre, bonne encapsulation, CRC32 irréprochable)
6. **composer 2.5 prompt from claude** — **7.2 / 10** (Excellente intégration LittleFS et ArduinoJson)
7. **qwen3.6_prompt_from_claude** — **6.5 / 10** (Belle structure C++, pénalisée par le scope local du Ticker)
8. **deepseek4 flash** — **6.2 / 10** (Excellente technique en virgule fixe Q16, mais fichier monolithique)
9. **gpt-free** — **6.0 / 10** (Code bien présenté mais dépendant d'un en-tête non public)
10. **gemini 3.1 pro** — **5.5 / 10** (Code trop simpliste, allocations String non maîtrisées)
11. **gemini-free** — **5.0 / 10** (Inachevé, présence de delay() bloquant)
12. **composer 2.5** — **4.5 / 10** (Float en ISR, écriture de registre non atomique)
13. **qwen3.6** — **2.5 / 10** (Mélange de plateformes incompatible, syntaxe invalide)
14. **qwen35** — **1.8 / 10** (Erreurs C++ basiques, registres pris pour des fonctions)
15. **gemma4** — **1.5 / 10** (Code factice non compilable)

---

## 5. Recommandation pour la fusion (Architecture cible idéale)

L'analyse comparative approfondie des 15 livrables met en lumière les composants d'élite à retenir pour concevoir le **simulateur tachymétrique ultime** pour l'onduleur Deye SUN-8K :

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        ARCHITECTURE DE SYNTHÈSE IDÉALE                                 │
├────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                        │
│  [ÉTAGE ALIMENTATION & PROTECTION] (Emprunté à GPT-5.6 Sol Medium & GLM5.3)           │
│  ├── 2x Connecteurs 12V ventilateurs onduleur Deye                                    │
│  ├── Fusibles réarmables PTC (750 mA) + OU-diode Schottky SS34 (3A / 40V)             │
│  ├── Diode Transil TVS de protection contre les surtensions transitoires (SMBJ18A)     │
│  ├── Condensateur réservoir de 1 000 µF / 25V (absorption de coupure transitoire)       │
│  └── Convertisseur Buck DC/DC externe MP1584EN réglé à 5,0V vers la broche 5V du Wemos │
│                                                                                        │
│  [ÉTAGE D'ENTRÉE TACHYMÉTRIQUE] (Emprunté à GPT-5.6 Sol Medium & DeepSeek4 Flash)      │
│  ├── Raccordement d'UN SEUL ventilateur physique par paire (interdiction du parallèle) │
│  ├── Résistance de pull-up 4,7 kΩ vers 3,3V (ou adaptateur NPN inverseur tolérant 12V) │
│  ├── Diodes de clamp Schottky BAT54S vers rail 3,3V + filtre RC HF (470 Ω / 1 nF)      │
│  └── Capture par interruption GPIO matérielle (attachInterrupt sur front adapté)      │
│                                                                                        │
│  [COEUR TEMPS-RÉEL & GÉNERATION] (Emprunté à Claude Opus 5 Medium)                     │
│  ├── Dérivation du Timer1 sur le vecteur NMI (niveau 3) avec NmiTimSetFunc()          │
│  ├── Ordonnanceur one-shot à échéance la plus proche (résolution 0,2 µs)               │
│  ├── Horodatage de cycle CPU via le registre assembleur ccount                        │
│  ├── Commutation atomique des broches par registres matériels directs GPOS et GPOC     │
│  ├── Calculs de régulation en nombres entiers ou virgule fixe Q16 (zéro float en ISR)  │
│  ├── Protocole Seqlock pour l'échange de données atomique sans verrou vers loop()      │
│  └── Filtrage passe-bas exponentiel (EMA shift 3) avec plancher et plafond de sécurité │
│                                                                                        │
│  [ÉTAGE DE SORTIE COLLECTEUR OUVERT] (Emprunté à GPT-5.6 Sol Medium & Grok 4.6)        │
│  ├── 2x Transistors NPN rapides (BC337 ou 2N2222) en émetteur commun                   │
│  ├── Résistance de base 1,5 kΩ à 2,2 kΩ pilotée par les sorties ESP8266               │
│  ├── Résistance de pull-down 47 kΩ entre base et émetteur (maintien bloqué au boot)   │
│  └── Collecteurs reliés directement aux entrées tachymétriques de l'onduleur Deye     │
│                                                                                        │
│  [SERVEUR WEB & GESTION SYSTÈME] (Emprunté à Composer 2.5 Prompt Claude & GLM5.3)      │
│  ├── ESPAsyncWebServer + ArduinoJson sur système de fichiers LittleFS                 │
│  ├── Interface dynamique moderne avec polling AJAX asynchrone (/api/status)            │
│  ├── Zéro allocation dynamique par l'emploi de tampons statiques pré-réservés         │
│  ├── Persistance de la configuration protégée par mot magique 4B et CRC32 complet      │
│  └── Reconnexion automatique WiFi non bloquante et point d'accès de secours permanent  │
│                                                                                        │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

En intégrant la robustesse électrique de **GPT-5.6 Sol Medium**, la précision nanoseconde de l'ISR NMI de **Claude Opus 5 Medium** et la légèreté asynchrone du serveur web sous LittleFS, cette synthèse élimine tous les écueils identifiés dans le banc d'essai et garantit une compatibilité parfaite, silencieuse et durable avec l'onduleur Deye.
