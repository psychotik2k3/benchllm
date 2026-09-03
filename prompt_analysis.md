# ⚠️ RÈGLE D'OR ABSOLUE — À LIRE EN PREMIER ET À SUIVRE STRICTEMENT

**NE LIS JAMAIS le contenu d'un fichier source d'un compétiteur (.ino, .cpp, .h, etc.) tant que l'utilisateur ne t'a PAS explicitement donné son autorisation via le mécanisme de sélection par numéro décrit dans le Workflow ci-dessous.**

Ceci signifie :
- Le modèle ne doit **JAMAIS** faire `read()` sur un fichier `.ino`, `.cpp`, `.h` d'un compétiteur sans que l'utilisateur n'ait répondu avec un numéro.
- La seule chose interdite est la lecture du code/contenu des fichiers de compétition. La vérification de l'existence ou non d'un fichier dans `result_analysis/` est autorisée à tout moment (c'est une simple vérification de nom de fichier, pas une lecture de contenu).
- Si tu lis un seul fichier source sans permission, tu as échoué.

---

# Workflow obligatoire — Menu interactif par numéro

Ce prompt analyse les code fournis par différents LLM concurrents dans le dossier `competitors/`. Chaque sous-dossier contient une version différente du même projet IoT (simulateur de signal tachymètre RPM pour onduleur Deye, ESP8266).

### Contexte du projet
Il s'agit d'un **simulateur de signal tachymètre RPM** pour onduleur hybride Deye SUN-8K-SG05LP1-EU-AM2-P. Le firmware tourne sur un **LOLIN(WEMOS) D1 mini (ESP8266 ESP-12S)**. Il lit les signaux tach de ventilateurs Noctua (NF-A9-FLX 9cm et NF-A6x25-FLX 6cm), applique un ratio configurable par canal, et génère des signaux tach simulés pour tromper l'onduleur qui détecterait à tort un sous-régime.

### Structure
Tous les compétiteurs sont dans `competitors/`, chaque sous-dossier contient le firmware d'un LLM différent. Les fichiers d'analyse produits se trouvent dans `result_analysis/`.

---

## 🔄 MENU INTERACTIF — Workflow unique (À REPRODUIRE EN BOUCLE)

Le workflow est un **menu interactif simple** que vous devez reproduire à chaque tour. Il ne comporte qu'une seule étape répétitive.

### Étape 1 — Afficher le menu principal

Affichez TOUTES les actions ci-dessous d'un SEUL coup, puis ARRÊTEZ et attendez la réponse de l'utilisateur :

**Action A : Lister tous les compétiteurs avec leur statut d'analyse**

```
## Menu principal — Analyse des compétiteurs

| # | Compétiteur (dossier) | Fichier d'analyse dans `result_analysis/` ? |
|---|----------------------|----------------------------------------------|
| 1 | claude-sonnet5-free  | ✅ OUI — `analyse_claude-sonnet5-free.md`     |
| 2 | deepseek4 flash      | ❌ NON                                         |
| 3 | gemini-free          | ❌ NON                                         |
... (TOUS les compétiteurs, dans l'ordre des dossiers)
```

**Action B : Demander le choix de l'utilisateur avec la légende complète**

```
## Choisissez un numéro pour continuer :

| Touche | Action                                    |
|--------|-------------------------------------------|
| 1~N    | Lancer ou relancer l'analyse du compétiteur N (si déjà analysé, écrasera l'analyse existante) |
| 0      | Terminer — produire le tableau récapitulatif global avec classements et recommandations |

**Répondez avec un seul numéro : 0 pour finir, ou 1~N pour analyser/relancer un compétiteur.**
```

⚠️ **RÈGLE ABSOLUE** : Ne lisez AUCUN fichier source (.ino, .cpp, .h) de NULLE PART tant que l'utilisateur n'a pas répondu. Vous pouvez vérifier l'existence des fichiers dans `result_analysis/` mais rien d'autre.

### Étape 2 — Réagir au choix de l'utilisateur

Selon le numéro envoyé par l'utilisateur :

#### Si l'utilisateur répond avec un numéro N (entre 1 et le nombre total de compétiteurs) :

1. **Identifier** le compétiteur correspondant au numéro N dans la liste du menu.
2. **Vérifier si une analyse existe déjà** pour ce dossier (`result_analysis/analyse_<nom>.md`).
   - **Si OUI** : affichez ce message ET ATTENDEZ une nouvelle réponse (retournez à l'Action B sans rien lire) :
     ```
     ## ⚠️ Analyse existante détectée

     Un fichier `result_analysis/analyse_<nom_dossier>.md` existe déjà pour ce compétiteur.

     Voulez-vous que j'écrive par-dessus et écrase l'analyse existante ?
     Répondez "OUI" pour confirmer l'écrasement, ou choisissez un autre numéro (ou 0 pour quitter).
     ```
   - **Si NON** : passez directement à l'étape suivante.
3. **Lire les fichiers sources** du compétiteur : tous les `.ino`, `.cpp`, `.h` et docs annexes (`README.md`, `SCHEMATIC.md`, etc.).
4. **Produire l'analyse complète** selon le Format de Sortie Complet ci-dessous.
5. **Écrire le fichier** dans `result_analysis/analyse_<nom_dossier>.md`.
6. **Afficher un résumé court** (nom, note globale, note qualité code).
7. **Retourner à l'Étape 1** (menu mis à jour avec le nouveau statut ✅ pour ce compétiteur).

#### Si l'utilisateur répond avec `0` :

- Produire immédiatement le **Tableau récapitulatif et classements** (voir section ci-dessous). C'est la fin du workflow.

---

## Tableau récapitulatif et classements — Activé par la touche 0

> **Ce tableau doit prendre en compte TOUS les fichiers `.md` présents dans `result_analysis/`**, qu'ils aient été produits lors du run actuel ou d'un run précédent. L'objectif est l'enrichissement progressif au fil du temps.

Quand l'utilisateur répond `0`, produis exactement ces éléments, dans cet ordre :

### 1. Inventaire des analyses
1. Liste tous les dossiers de `competitors/`.
2. Pour chaque dossier, indique si `result_analysis/analyse_<nom>.md` existe.
3. Affiche **3 sous-listes** :
   - ✅ Analyses existantes avant ce run (fichiers trouvés dans `result_analysis/`)
   - 🆕 Nouvelles analyses produites lors de ce run (si applicable)
   - ⚠️ Compétiteurs sans analyse disponible (mentionne-les explicitement, ne les inclue **pas** dans le tableau)

### 2. Tableau récapitulatif
Pour chaque fichier d'analyse trouvé, extrais : nom, note globale X/10, note qualité code Y/10, résumé points positifs, résumé points négatifs.

Produis le **tableau trié par note globale décroissante** :

| Version | Note globale | Note qualité code | ✅ Points positifs (résumé en 5-8 puces max) | ❌ Points négatifs (résumé en 5-8 puces max) |
|---------|--------------|--------------------|--------------------------------------------------|-----------------------------------------------|

### 3. Re-évaluation des notes en cas d'incohérence détectée
Lorsque tu construis le tableau, il peut arriver que :
- Un **nouveau compétiteur** (ajouté depuis un run précédent) obtienne des scores proches de ceux du meilleur actuel.
- Les notes attribuées par les analyses existantes soient **subjectives**, et qu'une re-comparaison directe révèle une incohérence (ex : le nouveau concurrent méritait initialement une note supérieure à celle du leader actuel).

Dans ce cas, tu es **autorisé à réajuster les notes** de tous les compétiteurs concernés après re-évaluation globale. Mais :
- Tu dois **ajouter un astérisque `*` derrière la note ajustée** (ex : `8* / 10`) pour signaler explicitement que cette note a été révisée postérieurement.
- Tu dois **expliquer brièvement pourquoi** dans une section nommée « 🔍 Notes révisées et justification » juste avant le tableau.

### 4. Classements séparés

**Classement par note globale** (candidat idéal pour le projet IoT) :
Rang. Version — Note globale X/10 — [mot-clé du verdict]

**Classement par note qualité de code** :
Rang. Version — Note qualité code X/10 — [mot-clé du verdict]

### 5. Recommandation de fusion
Distingue explicitement :
- **Fonctionnalités IoT à emprunter** : quelles versions ont les meilleures implémentations de temps-réel, WiFi, filtres, persistance, etc. (cite la version et l'extrait)
- **Bonnes pratiques qualité code à intégrer** : quelles versions offrent la meilleure structure, le nommage, le DRY, la robustesse, etc. (cite la version et l'extrait)

---

# ✅ CATÉGORIES TECHNIQUES POSITIVES — ÉVALUER CHAQUE CRITÈRE POUR TOUS LES COMPÉTITEURS

Évalue chaque catégorie ci-dessous pour chaque compétiteur. Pour celle qui s'applique au code analysé, écris une sous-section avec les observations concrètes trouvées dans le code. Si une catégorie n'est pas présente (ex: pas de WiFi), ne la mentionne pas. Les catégories sont :

### 1. Temps-réel / ISR
- IRAM_ATTR sur les ISR, gestion de la Flash pendant pause WiFi, prédictibilité du timing

### 2. Génération sortie / Timer1
- Timer1 hardware vs software/ticker vs polling loop. Méthode de toggle GPIO (GPOS/GPOC registre direct vs digitalWrite). Jitter / drift. Architecture pulse scheduler ou autre.

### 3. Filtres / Signal
- Debounce, moyenne glissante, IIR, EMA exponentiel, validation plage de période, détection stall/timeout

### 4. WiFi / Réseau
- ESPAsyncWebServer vs standard sync, OTA réseau (ArduinoOTA), mDNS/DNS-SD discovery, WIFI_NONE_SLEEP, overclock CPU 160 MHz, AP+STA simultané

### 5. Gestion mémoire / HEAP
- PROGMEM pour HTML, zero-alloc JSON (snprintf pile / ArduinoJson buffer) vs heap allocation String par requête, fragmentation à long terme

### 6. Interface Web
- Dark theme, AJAX polling, focus protection champs de formulaire, badges visuels d'état, RESTful API

### 7. Persistance / Stockage
- EEPROM (~100K écritures, pas de wear-leveling) vs LittleFS (wear-leveling natif), format JSON, checksum/CRC, backup

### 8. Documentation hardware/code
- Schéma électronique ASCII avec BOM, calculs dimensionnement transistor/filtre/alimentation, explication circuit entrée et sortie

### 9. Qualité de code générale
- Voir critères détaillés ci-dessous (indépendant IoT)

---

# 🔧 QUALITÉ DE CODE GÉNÉRALE — CRITÈRES D'ÉVALUATION (indépendant de l'IoT)

À évaluer dans les deux sens (positif ET négatif) :

- **Structure / organisation** : séparation des responsabilités (setup/loop/fonctions dédiées), taille et cohérence des fonctions, découpage en fichiers .h/.cpp si pertinent, absence de "god function" ou fichier monolithique
- **Lisibilité / nommage** : clarté des noms de variables/fonctions/constantes, cohérence de style (camelCase/snake_case), absence de magic numbers non nommés
- **Commentaires / documentation inline** : pertinence (explique le "pourquoi" pas le "quoi"), absence de code mort commenté, en-têtes de fonctions descriptives
- **Gestion d'erreurs / robustesse** : validation des entrées utilisateur (formulaire web, JSON reçu), gestion des cas limites (division par zéro, overflow, valeurs hors plage), comportement en cas d'échec (WiFi down, EEPROM/LittleFS corrompu)
- **Duplication / DRY** : code copié-collé entre canaux ou fonctions similaires, opportunités de factorisation manquées
- **Respect des conventions C++/Arduino** : usage correct de `const`/`constexpr`, `static`, portée des variables (globales vs locales), types appropriés (`uint8_t` vs `int`), `volatile` sur variables partagées avec ISR
- **Complexité / maintenabilité** : complexité cyclomatique excessive (fonctions à multiples branches imbriquées), couplage fort entre modules, facilité à ajouter un canal/fonctionnalité
- **Sécurité basique** : dépassement de tampon potentiel (`snprintf` vs `sprintf`, `strcpy`), validation des entrées JSON/HTTP, injection possible via champs web
- **Testabilité** : code découplé du hardware permettant des tests unitaires, présence (ou non) de fonctions pures isolables

---

# ❌ POINTS TECHNIQUES NÉGATIFS — POUR CHAQUE DÉFAUT TROUVÉ

Pour chaque catégorie technique ci-dessus qui présente un défaut dans le code analysé :
- Chaque défaut doit être tagué `[critique]`, `[majeur]` ou `[mineur]` avec son **impact technique mesuré**
- Insister sur les bugs qui empêchent le firmware de fonctionner correctement avec l'onduleur Deye
- Noter les erreurs de compilation (syntax error, dépendances manquantes), les bugs logiques (sortie fixe indépendante des RPM, compteur au lieu de fréquence), et les problèmes temps-réel qui pourraient causer un crash en production
- Inclure également les défauts de **qualité de code générale** listés ci-dessus avec le même format

---

# 📋 FORMAT DE SORTIE COMPLET — À PRODUIRE POUR CHAQUE COMPÉTITEUR

Copie ce template et remplace chaque `[...]` par le contenu réel. Ne laisse AUCUN `[...]` littéral dans le fichier final :

```
# Analyse — [NOM_DU_COMPÉTITEUR]

**Sources analysées** : liste de tous les fichiers lus (.ino, .cpp, .h, README.md, SCHEMATIC.md, etc.)

---

## ✅ Points techniques positifs

### Temps-réel / ISR
[Observations concrètes tirées du code. Exemple : "ICACHE_RAM_ATTR appliqué sur handleTachEdge et isrTimer1..."]

### Génération sortie / Timer1
[Observations concrètes tirées du code]

### Filtres / Signal
[Observations concrètes tirées du code]

### WiFi / Réseau
[Observations concrètes tirées du code — si pas de WiFi, supprimer cette section]

### Gestion mémoire / HEAP
[Observations concrètes tirées du code]

### Interface Web
[Observations concrètes tirées du code — si pas d'interface web, supprimer cette section]

### Persistance / Stockage
[Observations concrètes tirées du code]

### Documentation hardware/code
[Observations concrètes tirées du code et des fichiers annexes]

### Qualité de code générale (positif)
- **Structure / organisation** : [observation positive ou "— aucun point positif identifié."]
- **Lisibilité / nommage** : [observation positive ou "— aucun point positif identifié."]
- **Commentaires / documentation inline** : [observation positive ou "— aucun point positif identifié."]
- **Gestion d'erreurs / robustesse** : [observation positive ou "— aucun point positif identifié."]
- **Duplication / DRY** : [observation positive ou "— aucun point positif identifié."]
- **Respect des conventions C++/Arduino** : [observation positive ou "— aucun point positif identifié."]
- **Complexité / maintenabilité** : [observation positive ou "— aucun point positif identifié."]
- **Sécurité basique** : [observation positive ou "— aucun point positif identifié."]
- **Testabilité** : [observation positive ou "— aucun point positif identifié."]

---

## ❌ Points techniques négatifs

### Temps-réel / ISR (critique)
- [critique] ou [majeur] ou [mineur] : [description du défaut] — impact : [mesure technique précise]

[Autres sous-sections pour les catégories qui ont des défauts. Supprimer les catégories sans défaut.]

### Qualité de code générale (négatif)
- [critique/majeur/mineur] : [description du défaut lié au critère, ex: DRY, conventions C++, etc.] — impact : [mesure technique précise]
(Si tous les 9 sous-critères sont sans reproche, écris "— aucun défaut de qualité de code identifié.")

---

## ⭐ Note globale : X/10 — Verdict court
Phrase de synthèse résumant la décision.

## ⭐ Note qualité de code : X/10 — Verdict court
Phrase de synthèse sur la propreté/maintenabilité du code, indépendamment de sa correction fonctionnelle IoT.
```

---

## 📁 Sortie des analyses — RÈGLE DE NOMMAGE

Pour chaque compétiteur analysé :
- **Chemin du fichier** : `result_analysis/analyse_<nom_dossier_compétiteur>.md`
  - Exemples : `result_analysis/analyse_qwen3.6.md`, `result_analysis/analyse_claude-sonnet5-free.md`
- Le contenu doit correspondre exactement au Format de Sortie Complet ci-dessus.
- Les fichiers doivent être écrits directement sur le système de fichiers dans le dossier `result_analysis/`.
