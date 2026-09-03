# Analyse — gemma4

**Source:** `competitors/gemma4/firmware_main.cpp` + `README.md`  
**Fichiers analysés:** 1 fichier source C++ (.cpp), 1 README  
**Score global : 3.2 / 10**  
**Score qualité de code : 3.5 / 10**

---

## ✅ Positifs techniques

### Catégorie 1 — Temps-réel / ISR
- **[POSITIF]** Utilise `attachInterrupt()` avec bord `FALLING` pour le comptage matériel des pulses — approche correcte pour mesurer des signaux tachymétriques.
- **[POSITIF]** Handlers ISR (`I_9cm`, `I_6m`) sont minimaux : un seul increment d'un volatile, pas de code bloquant ni d'appels `Serial.print`.

### Catégorie 2 — Démarrage / Boot
- **[POSITIF]** Ordre d'initialisation simple et compréhensible dans `setup()` : Serial → pinMode → attachInterrupt → Ticker → WiFi → Server.
- **[POSITIF]** Utilise `INPUT_PULLUP` pour les pins de réception, éliminant le besoin de résistances pull-up externes (bon choix pragmatique).

### Catégorie 3 — Génération sortie / Timer1
- **[POSITIF]** Utilisation de `Ticker` pour traiter la logique hors ISR — bonne approche qui évite de bloquer dans un timer.

### Catégorie 6 — WiFi / Réseau
- **[POSITIF]** Mode `WIFI_AP_STA` permet à la fois la configuration (via l'AP) et une éventuelle connexion au réseau existant (STA).
- **[POSITIF]** Structure web serveur simple mais fonctionnelle avec deux endpoints séparés (`/` pour le HTML, `/data` pour le JSON).

### Catégorie 8 — Interface Web
- **[POSITIF]** Design propre et moderne : layout centré en carte CSS, police Segoe UI, ombres et bordures arrondies.
- **[POSITIF]** Actualisation automatique via `setInterval` + fetch AJAX toutes les secondes — fluide, pas de rechargement complet de page.

### Catégorie 10 — Documentation hardware
- **[POSITIF]** README.md bien structuré avec vue d'ensemble claire du projet, liste des composants et description de la logique du circuit.
- **[POSITIF]** Tableau de connexion précis pour les sections Input, Output et Visual Feedback.
- **[POSITIF]** Explication du concept "Universal Solution" (transistor NPN isolant le microcontrôleur) bien documentée dans le README.

---

## ❌ Négatifs techniques

### Catégorie 1 — Temps-réel / ISR
- **[critique] `IRAM_ATTR`/`ICACHE_RAM_ATTR` absent** : Sur ESP8266, les ISRs doivent impérativement être en RAM. Sans cet attribut, le code ISR peut être paginé en flash et provoquer un crash (Guru Meditation) pendant l'activité WiFi. C'est une erreur fondamentale sur cette plateforme.
- **[majeur] Pas de section critique pour les accès aux compteurs** : `processData()` et le callback LED accèdent à `count9`/`count6` sans `noInterrupts()`. Sur ESP8266 (architecture 32-bit), un accès 4-byte peut être atomique, mais c'est non garanti et dépend du compilateur.
- **[mineur] Callback LED mal positionné dans Ticker** : Le callback de la LED lit les compteurs bruts au lieu d'utiliser les RPM calculés — pas incohérent (pour un blinking à 2Hz), mais sémantiquement incorrect (le nom `ledPulseTicker` suggère un lien avec les pulses tachymétriques réels).

### Catégorie 2 — Démarrage / Boot
- **[critique] Mot de passe WiFi faible par défaut** : `"12345678"` est un mot de passe trivial qui ne protège aucunement l'AP. N'importe quel voisin peut se connecter et accéder à l'interface web sans authentification.
- **[majeur] Pas de watchdog (SW ou HW)** : En cas de plantage WiFi ou mémoire, aucune récupération automatique. Système non résilient.
- **[mineur] Aucun statut de boot sur Serial** : Impossible de diagnostiquer le démarrage en absence d'un accès réseau.

### Catégorie 3 — Génération sortie / Timer1
- **[critique] La génération de signal simulé est complètement absente du code !** Le pin `PIN_OUTPUT` (D5) est configuré en OUTPUT mais jamais basculé pour produire la traine d'impulsions décrite dans le README. C'est la fonction principale du projet qui n'est pas implémentée.
- **[majeur] Incohérence critique entre documentation et code** : Le README affirme qu'un timer génère un train d'impulsions, mais le code ne fait absolument rien sur D5. L'utilisateur branchera l'appareil et le signal simulé ne sortira jamais.

### Catégorie 4 — Fidélité protocole / Fail-safe
- **[critique] Les champs "Ratio" de l'interface web sont totalement déconnectés** : Les `<input>` pour `v9` et `v6` affichent des valeurs mais n'ont aucun bouton submit ni événement JavaScript pour envoyer les modifications au serveur. Le ratio ne peut JAMAIS être modifié via le Web UI, rendant ces champs purement décoratifs.
- **[majeur] Pas de debounce sur les interrupteurs** : Les signaux tachymétriques de ventilateurs mécaniques peuvent avoir du rebond (bounce) causant des faux compteurs, surtout à bas régime.
- **[majeur] Pas de détection de panne de signal (fail-safe)** : Si un fan s'arrête, le système ne le détecte pas et continue de produire un simRPM basé sur des données périmées.
- **[mineur] Pas de validation de plage pour les ratios** : Même si le mécanisme était fonctionnel, aucune restriction n'est appliquée aux valeurs entrées (ratio négatif ? ratio > 10 ?).

### Catégorie 5 — Filtres / Signal
- **[critique] Aucun filtrage du signal** : Pas de debounce, pas de filtre IIR, pas de moyenne mobile exponentielle, pas de détection de stall. Chaque noise ou spike est compté comme un pulse valide.
- **[majeur] Ratio calculé statiquement** : `ratio9 = 2.0` et `ratio6 = 2.5` sont des constantes compilées en dur — aucune adaptation possible en fonction du type de fan réel.

### Catégorie 6 — WiFi / Réseau
- **[majeur] Aucune authentification sur le web server** : Toute personne connectée à l'AP peut accéder aux données RPM et tenter d'interagir avec l'appareil.
- **[mineur] Pas de support mDNS / Zeroconf** : Difficulté à identifier l'appareil sur un réseau local (pas de nom de domaine comme `deye-fan-config.local`).
- **[mineur] Pas de OTA (Over-The-Air)** : Mises à jour firmware uniquement par câblage USB.

### Catégorie 7 — Gestion mémoire / HEAP
- **[majeur] Construction JSON par String concatenation** : `"{"r9\":" + String(rpm9) ..."` provoque des allocations heap dynamiques répétées à chaque appel de `handleData()`, ce qui fragmente la mémoire sur ESP8266 (connue pour sa fragmentation rapide).
- **[mineur] Chaîne HTML brute en SRAM** : La variable `html_page` est une string statique ~4KB+ en SRAM. Sur l'ESP8266 (80 KB SRAM disponibles), c'est acceptable mais non optimal — `PROGMEM` serait préférable.

### Catégorie 8 — Interface Web
- **[majeur] Formulaire d'écriture cassé** : Voir [critique] catégorie 4 — les inputs ratio n'ont aucun mécanisme POST/PUT associé. L'utilisateur voit des champs configurables mais ne peut rien modifier.
- **[mineur] Pas de thème sombre (dark mode)** : Interface uniquement en clair, incompatible avec les standards modernes d'accessibilité visuelle.

### Catégorie 9 — Persistance / Stockage
- **[critique] Aucune persistance** : Pas d'EEPROM, pas de LittleFS, pas de NVS. Tous les ratios sont perdus au redémarrage. Configuration uniquement en RAM volatile.
- **[majeur] Impossible de sauvegarder une configuration personnalisée** : Sans stockage non-volatile, chaque reset du dispositif réinitialise l'appareil aux valeurs par défaut arbitraires (2.0 et 2.5).

### Catégorie 10 — Documentation hardware
- **[mineur] Schéma électrique manquant** : Le README décrit les connexions mais ne fournit pas de schéma circuit avec valeurs précises des résistances du côté transistor.
- **[mineur] Calculs de timing absents** : Pas de formule reliant le RPM simulé à la fréquence d'impulsions sur D5 (ex: pour 2000 RPM avec ratio X, la fréquence de sortie est Y Hz).

### Catégorie 11 — Qualité de code générale
- **[critique] Fonctionnalité principale manquante** : Le projet prétend simuler un signal RPM mais ne produit aucune impulsion. C'est le bug le plus critique possible pour ce type de projet.
- **[majeur] Incohérence de suffixes ISR** : `I_9cm` vs `I_6m` (pas `I_6cm`) — erreur de nommage qui pourrait induire en erreur lors d'un debug.
- **[majeur] Concaténation String en boucle serveur** : Chaque requête `/data` alloue dynamiquement via `String`, ce qui sur ESP8266 conduit à une fragmentation mémoire inévitable. Utiliser `client.print()` directement ou un buffer statique est impératif.
- **[mineur] Balise code fermante erronée** : Le fichier `.cpp` contient une balise de fin de bloc markdown (```) en fin de fichier, preuve d'un copier-coller depuis un notebook/documentation — sale et inapproprié pour un fichier source.
- **[mineur] Pas de `#include` pour `<Arduino.h>`** : Bien que `ESP8266WiFi.h` l'inclue implicitement, ce n'est pas standard ni portable.
- **[mineur] Variables globales sans scope** : Toutes les variables sont globales (`rpm9`, `ratio9`, etc.) — aucun encapsulement. Pour un projet aussi petit c'est acceptable mais manque de rigueur.

---

## Synthèse

**Ce projet est fondamentalement non-fonctionnel.** Bien que l'infrastructure soit correctement pensée (ISR, web server, interface), la fonctionnalité centrale — la génération du signal simulé sur D5 — n'est implémentée nulle part dans le code. Le fichier source configure un pin OUTPUT mais ne le fait jamais basculer. Le README décrit une fonctionnalité qui n'existe pas dans l'implémentation.

### Points forts réels
- Bonne documentation hardware (README clair et structuré)
- Web UI propre et moderne avec AJAX non-destructif
- ISR bien structurées (hors problème ESP8266)
- Mode AP+STA pour flexibilité de déploiement

### Faiblesses critiques
- Signal simulé non généré du tout (bug fatal)
- Ratios du Web UI déconnectés (inputs inopérants)
- Pas de persistance de configuration
- Pas de filtrage ni fail-safe sur les signaux
- Fragmentation HEAP par concaténation String
- Sécurité WiFi absente

### Verdict
Ce code est un **squelette non-fonctionnel** — la structure web et ISR est correcte mais la logique métier essentielle (simulation du signal RPM) manque entièrement. Il nécessite une refonte importante de la fonction `loop()` pour ajouter la génération d'impulsions sur D5, ainsi qu'une connexion réelle des inputs ratio au backend.
