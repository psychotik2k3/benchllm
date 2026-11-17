# Analyse — qwen3.6_prompt_from_claude

**Sources analysées** : [deye_fan.ino](<../competitors/qwen3.6_prompt_from_claude/deye_fan.ino>) ; [SCHEMATIC.md](<../competitors/qwen3.6_prompt_from_claude/SCHEMATIC.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

ISR capture IRAM, buffers de périodes et volonté de séparation réseau/génération.

### Démarrage / Boot readiness

LittleFS avant GPIO ; sortie initiale haute et aucun signal avant mesure. Aucun fonctionnement démontrable car erreurs de compilation et scheduler local. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Filtres / Signal

Moyenne de quatre périodes, bornes de 3 à 100 Hz et IIR ; seuil anti-parasites 500 µs.

### WiFi / Réseau

ESPAsyncWebServer, AP/STA, mDNS et ArduinoOTA protégée par un mot de passe configuré dans le code.

### Gestion mémoire / HEAP

HTML PROGMEM ; réponses JSON en buffers, mais nombreuses incompatibilités de types.

### Interface Web

API de réglage et lectures séparées.

### Persistance / Stockage

LittleFS JSON et demande de sauvegarde différée.

### Documentation hardware/code

BOM, entrées 3,3 V et sorties NPN fournies dans SCHEMATIC.md.

### Qualité de code générale (positif)

- **Structure / organisation** : Sections nombreuses, mais contrats de fonctions et durée de vie des objets incohérents.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : Erreurs de JSON reconnues mais paramètres chargés sans défauts complets/validation ; nombreux types incompatibles.
- **Duplication / DRY** : Boucle commune au traitement des voies, ISR dupliquées et variables de contrôle redondantes.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Sections nombreuses, mais contrats de fonctions et durée de vie des objets incohérents.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Génération sortie / Timer1

- [critique] Ticker scheduler est local à setup (941–942) ; sa destruction détache le callback à la sortie de setup. Ticker est un timer logiciel SDK, pas Timer1/NMI. Aucun moteur de génération durable n’est établi.

### Documentation hardware/code

- [critique] SCHEMATIC.md 54/123–128 propose MT3608 comme abaisseur 12 V→5 V. La fiche AEROSEMI décrit un élévateur : ce composant ne peut pas réaliser cette alimentation, la sortie peut exposer le Wemos au rail d’entrée.

### Fidélité au protocole tachymétrique / Fail-safe

- [majeur] À la perte, target est mis à zéro sans forcer la broche à l’état relâché (432–433). updateTargetPeriods recalcule la cible depuis un historique jamais vidé avant checkStall ; le scheduler peut voir des consignes transitoires périmées.

### Persistance / Stockage

- [majeur] Reboot différé après 200 ms (1066) mais sauvegarde interdite avant cooldown de 5 s (1051). Une modification faite dans les cinq premières secondes peut provoquer le reboot avant sa sauvegarde.

### Qualité de code générale (négatif)

- [critique] Conversions de pointeurs volatile vers non-volatile dans updateTargetPeriods/checkStall (377–381/428–429), buffer const donné à une fonction non-const (386), static_cast de char* vers uint8_t* (658/767), handler HTTP_POST avec deux arguments (672/785) et restartTime utilisé hors de sa portée (1073) : plusieurs erreurs C++ certaines.
- [critique] LittleFS.begin(true) (453) ne correspond pas à FS::begin() du cœur ESP8266 3.1.2 ; request->getString n’est pas le mécanisme de réception de corps de l’API AsyncWebServer habituelle. Dépendances non verrouillées.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 1,0/10 — Architecture annoncée non exécutable

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 2,9/10 — Qualité insuffisante

Sections nombreuses, mais contrats de fonctions et durée de vie des objets incohérents. Erreurs de JSON reconnues mais paramètres chargés sans défauts complets/validation ; nombreux types incompatibles.
