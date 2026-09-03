Tu es un expert en code et en firmware embarqué IoT sur ESP8266, ainsi qu'un reviewer de code C++ senior exigeant sur la qualité logicielle. Analyse tous les compétiteurs présents dans le dossier `competitors/` (chaque sous-dossier contient une version différente du même projet).

## Contexte du projet
Il s'agit d'un **simulateur de signal tachymètre RPM** pour onduleur hybride Deye SUN-8K-SG05LP1-EU-AM2-P. Le firmware tourne sur un **LOLIN(WEMOS) D1 mini (ESP8266 ESP-12S)**. Il lit les signaux tach de ventilateurs Noctua (NF-A9-FLX 9cm et NF-A6x25-FLX 6cm), applique un ratio configurable par canal, et génère des signaux tach simulés pour tromper l'onduleur qui détecterait à tort un sous-régime.

## Structure
Tous les compétiteurs sont dans `competitors/`, chaque sous-dossier contient le firmware d'un LLM différent.

## 🔄 Workflow à suivre — OBLIGATOIRE

### Étape 1 — Lister tous les compétiteurs
Dresse la liste complète de tous les dossiers présents dans `competitors/`. Indique-les sous forme de tableau numéroté (1, 2, 3, …) avec le nom de chaque dossier. **C'est ce premier message qui doit être envoyé à l'utilisateur.**

### Étape 2 — Pour CHAQUE compétiteur, dans l'ordre :

1. **Vérifier si une analyse existe déjà** : regarde s'il existe un fichier `result_analysis/analyse_<nom_dossier>.md` pour ce compétiteur.
2. **Si le fichier d'analyse EXISTE déjà** :
   - Indique-le clairement à l'utilisateur
   - Passe directement au compétiteur suivant (Étape 2, point 1)
3. **Si le fichier n'existe PAS** :
   - Lis **d'abord** tous les fichiers sources du compétiteur (`.ino`, `.cpp`, `.h`, etc.) ainsi que les docs annexes s'ils existent (`README.md`, `SCHEMATIC.md`, etc.)
   - Produit l'analyse détaillée selon le format ci-dessous
   - **ÉCRIS immédiatement** le fichier d'analyse dans `result_analysis/analyse_<nom_dossier>.md`
   - Affiche un résumé de la note et du verdict
   - **PausE et demande explicitement à l'utilisateur** : *« Analyse de [nom_du_compétiteur] terminée (note X/10). Souhaitez-vous que je procède à l'analyse du compétiteur suivant ? »*
   - **N'ouvre AUCUN fichier source du compétiteur suivant** tant que l'utilisateur n'a pas confirmé.

### Étape 3 — Tableau récapitulatif (après traitement de tous les compétiteurs)

> **Ce tableau doit prendre en compte TOUS les fichiers d'analyse existants dans `result_analysis/`**, qu'ils aient été produits lors du run actuel ou lors d'un run précédent. L'objectif est d'enrichir progressivement les résultats au fil du temps, notamment lorsque de nouveaux compétiteurs sont ajoutés.

Pour construire le tableau récapitulatif :
1. Parcours **tous** les fichiers `.md` présents dans `result_analysis/`
2. Pour chaque fichier trouvé, extrait la note et les points positifs/négatifs depuis l'analyse correspondante (même si elle n'a pas été générée lors du run en cours)
3. **Liste explicitement** :
   - Les analyses déjà existantes avant ce run
   - Les nouvelles analyses produites lors de ce run
   - Les compétiteurs sans analyse disponible (indique-les clairement, ne les inclus pas dans le tableau) si certains compétiteurs manquent d'analyse
4. Produis le **tableau récapitulatif** trié par note décroissante avec :

| Version | Note globale | Note qualité code | ✅ Points positifs (résumé en 5-8 puces max) | ❌ Points négatifs (résumé en 5-8 puces max) |
|---------|--------------|--------------------|--------------------------------------------------|-----------------------------------------------|

Puis **deux classements séparés** :
1. Classement par **note globale** (candidat idéal pour le projet IoT)
2. Classement par **note qualité de code** uniquement (utile si le meilleur firmware IoT n'est pas le mieux écrit)

Et enfin une **recommandation** sur la fusion des meilleures pratiques entre les versions les mieux notées, en distinguant explicitement :
- Quelles fonctionnalités **techniques IoT** emprunter à chaque version (temps-réel, WiFi, filtres, etc.)
- Quelles bonnes pratiques **qualité de code/architecture** intégrer (structure, nommage, DRY, robustesse, etc.)

---

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
- **Qualité de code générale** : bonnes pratiques C++ observées (voir critères détaillés ci-dessous)


---

### 🔧 Qualité de code générale — Critères d'évaluation (indépendant de l'IoT)
À évaluer pour chaque version, dans les deux sens (points positifs ET négatifs) :
- **Structure / organisation** : séparation des responsabilités (setup/loop/fonctions dédiées), taille et cohérence des fonctions, découpage en fichiers .h/.cpp si pertinent, absence de "god function" ou fichier monolithique
- **Lisibilité / nommage** : clarté des noms de variables/fonctions/constantes, cohérence de style (camelCase/snake_case), absence de magic numbers non nommés
- **Commentaires / documentation inline** : pertinence (explique le "pourquoi" pas le "quoi"), absence de code mort commenté, en-têtes de fonctions descriptives
- **Gestion d'erreurs / robustesse** : validation des entrées utilisateur (formulaire web, JSON reçu), gestion des cas limites (division par zéro, overflow, valeurs hors plage), comportement en cas d'échec (WiFi down, EEPROM/LittleFS corrompu)
- **Duplication / DRY** : code copié-collé entre canaux ou fonctions similaires, opportunités de factorisation manquées
- **Respect des conventions C++/Arduino** : usage correct de `const`/`constexpr`, `static`, portée des variables (globales vs locales), types appropriés (`uint8_t` vs `int`, etc.), `volatile` sur variables partagées avec ISR
- **Complexité / maintenabilité** : complexité cyclomatique excessive (fonctions à multiples branches imbriquées), couplage fort entre modules, facilité à ajouter un canal/fonctionnalité
- **Sécurité basique** : dépassement de tampon potentiel (`snprintf` vs `sprintf`, `strcpy`), validation des entrées JSON/HTTP, injection possible via champs web
- **Testabilité** : code découplé du hardware permettant des tests unitaires, présence (ou non) de fonctions pures isolables

---

### ❌ Points techniques négatifs (classés par criticité)
- Chaque défaut doit être tagué `[critique]`, `[majeur]` ou `[mineur]` avec son impact technique mesuré
- Insister sur les bugs qui empêchent le firmware de fonctionner correctement avec l'onduleur Deye
- Noter les erreurs de compilation (syntax error, dépendances manquantes), les bugs logiques (sortie fixe indépendante des RPM, compteur au lieu de fréquence), et les problèmes temps-réel qui pourraient causer un crash en production
- Inclure également les défauts de **qualité de code générale** listés ci-dessus (ex : `[majeur] duplication massive du code de gestion des 4 canaux, aucune factorisation — impact : maintenance x4, risque d'incohérence lors d'un futur changement`)

## Format de sortie attendu pour chaque version
X. [NOM DU LLM] — Description courte du fichier (.ino / .cpp + docs annexes)
✅ Points techniques positifs
Temps-réel / ISR
 ...
 ...
Génération sortie / Timer1
 ...
[... autres catégories techniques ...]
Qualité de code générale
 ...
 ...

❌ Points techniques négatifs
Temps-réel / ISR (critique)
 [critique] ... impact : ...
 [majeur] ... impact : ...
[... autres catégories techniques ...]
🔧 Qualité de code générale
 [majeur] ... impact : ...
 [mineur] ... impact : ...

⭐ Note globale : X/10 — Verdict court
Phrase de synthèse.
⭐ Note qualité de code (sous-note) : X/10 — Verdict court sur la propreté/maintenabilité du code, indépendamment de sa correction fonctionnelle IoT

