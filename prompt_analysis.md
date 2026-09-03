Tu es un expert en code et en firmware embarqué IoT sur ESP8266. Analyse tous les compétiteurs présents dans le dossier `competitors/` (chaque sous-dossier contient une version différente du même projet).

## Contexte du projet
Il s'agit d'un **simulateur de signal tachymètre RPM** pour onduleur hybride Deye SUN-8K-SG05LP1-EU-AM2-P. Le firmware tourne sur un **LOLIN(WEMOS) D1 mini (ESP8266 ESP-12S)**. Il lit les signaux tach de ventilateurs Noctua (NF-A9-FLX 9cm et NF-A6x25-FLX 6cm), applique un ratio configurable par canal, et génère des signaux tach simulés pour tromper l'onduleur qui détecterait à tort un sous-régime.

## Structure
Tous les compétiteurs sont dans `competitors/`, chaque sous-dossier contient le firmware d'un LLM différent. Parcoure tous les sous-dossiers et analyse **chacun d'eux**. Si tu trouves des fichiers `.md` annexes (README, SCHEMATIC, etc.) dans un dossier concurrent, prends-les aussi en compte.

## Ce que je veux pour CHAQUE version

1. **Lecture complète** de tous les fichiers sources (.ino / .cpp / .h)
2. Si un schéma électronique ou documentation hardware existe (SCHEMATIC.md, schema_*, README.md), le lire également
3. Pour chaque version, produire une analyse détaillée incluant :

### ✅ Points techniques positifs (catégorisés)
- **Temps-réel / ISR** : IRAM_ATTR sur les ISR, gestion de la Flash pendant pause WiFi, prédictibilité du timing
- **Génération sortie / Timer1** : Timer1 hardware vs software/ticker vs polling loop. Méthode de toggle GPIO (GPOS/GPOC registre direct vs digitalWrite). Jitter / drift. Architecture pulse scheduler ou autre.
- **Filtres / Signal** : Debounce, moyenne glissante, IIR, EMA exponentiel, validation plage de période, détection stall/timeout
- **WiFi / Réseau** : ESPAsyncWebServer vs standard sync, OTA réseau (ArduinoOTA), mDNS/DNS-SD discovery, WIFI_NONE_SLEEP, overclock CPU 160 MHz, AP+STA simultané
- **Gestion mémoire / HEAP** : PROGMEM pour HTML, zero-alloc JSON (snprintf pile / ArduinoJson buffer) vs heap allocation String par requête, fragmentation à long terme
- **Interface Web** : Dark theme, AJAX polling, focus protection champs de formulaire, badges visuels d'état, RESTful API
- **Persistance / Stockage** : EEPROM (~100K écritures, pas de wear-leveling) vs LittleFS (wear-leveling natif), format JSON, checksum/CRC, backup
- **Documentation hardware/code** : Schéma électronique ASCII avec BOM, calculs dimensionnement transistor/filtre/alimentation, explication circuit entrée et sortie


### ❌ Points techniques négatifs (classés par criticité)
- Chaque défaut doit être tagué `[critique]`, `[majeur]` ou `[mineur]` avec son impact technique mesuré
- Insister sur les bugs qui empêchent le firmware de fonctionner correctement avec l'onduleur Deye
- Noter les erreurs de compilation (syntax error, dépendances manquantes), les bugs logiques (sortie fixe indépendante des RPM, compteur au lieu de fréquence), et les problèmes temps-réel qui pourraient causer un crash en production

## Format de sortie attendu pour chaque version
X. [NOM DU LLM] — Description courte du fichier (.ino / .cpp + docs annexes)
✅ Points techniques positifs
Temps-réel / ISR
 ...
 ...
Génération sortie / Timer1
 ...
[... autres catégories ...]

❌ Points techniques négatifs
Temps-réel / ISR (critique)
 [critique] ... impact : ...
 [majeur] ... impact : ...
[... autres catégories ...]

⭐ Note globale : X/10 — Verdict court
Phrase de synthèse.



## 📁 Sortie des analyses

Pour **chaque compétiteur**, écris un fichier d'analyse dédié dans le dossier `result_analysis/`.
- Nom de fichier : `analyse_<nom_dossier_compétiteur>.md` (ex: `analyse_qwen3.6.md`, `analyse_claude-sonnet5-free.md`, etc.)
- Le contenu du fichier doit être exactement le format de sortie décrit ci-dessous.
- Les fichiers doivent être écrits directement sur le système de fichiers dans `result_analysis/`.

## À la fin, fournir un tableau récapitulatif trié par note décroissante avec :

| Version | Note | ✅ Points positifs (résumé en 5-8 puces max) | ❌ Points négatifs (résumé en 5-8 puces max) |
|---------|------|--------------------------------------------------|-----------------------------------------------|

Puis un **classement final** de tous les compétiteurs par rang.

Et enfin une **recommandation** sur la fusion des meilleures pratiques entre les versions les mieux notées (quelle version utiliser comme base, quelles fonctionnalités emprunter aux autres).
