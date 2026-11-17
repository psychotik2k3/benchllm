# Analyse — Grok_4.7_medium

**Sources analysées** : [deye_fan/deye_fan.ino](<../competitors/Grok_4.7_medium/deye_fan/deye_fan.ino>) ; [schema_electronique.md](<../competitors/Grok_4.7_medium/schema_electronique.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

NMI échantillonne les entrées, génère les sorties et gère le timeout ; helpers IRAM et calculs entiers.

### Démarrage / Boot readiness

CPU forcé à 80 MHz, GPIO relâchés et EEPROM puis startTach avant radio. Pas de valeur nominale avant deux fronts valides ; polling 32 µs. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Génération sortie / Timer1

DIV1 et cycles CPU à 80 MHz ; GPOS/GPOC, échéances accumulées et resynchronisation après retard.

### Fidélité au protocole tachymétrique / Fail-safe

PPR=2, ratios Q10 et arrêt qui efface le filtre et réamorce la capture (133–139).

### Filtres / Signal

Bornes de période et EMA entière 1/8 ; propriété du moteur confinée à la NMI.

### WiFi / Réseau

AP+STA et sommeil désactivé, connexion sans attente explicite.

### Gestion mémoire / HEAP

HTML PROGMEM, buffers status/config fixes de 320/192 octets.

### Interface Web

Polling 500 ms et chargement des réglages séparé ; états distincts selon canaux.

### Persistance / Stockage

Signature, version et CRC32 par offsetof ; chaînes terminées et ratios clampés.

### Documentation hardware/code

Schéma détaillé, un tach par paire, entrées/sorties NPN et calculs d’alimentation.

### Qualité de code générale (positif)

- **Structure / organisation** : Chan moteur, Config persistante et Snap de lecture séparent correctement les responsabilités.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : CRC et réamorçage propres ; les constantes intermédiaires 32 bits ne sont pas protégées.
- **Duplication / DRY** : Un moteur commun et helpers partagés ; peu de duplication entre canaux.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Chan moteur, Config persistante et Snap de lecture séparent correctement les responsabilités.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Fidélité au protocole tachymétrique / Fail-safe

- [critique] MAX_IN_CYC=300UL*80 000 000UL/1000 et LOST_CYC=350UL*80 000 000UL/1000 (66–67) débordent avant division. Ils valent respectivement 2 525 163 et 2 230 196 cycles, soit 31,565 ms et 27,877 ms, au lieu de 300/350 ms. Sous ≈1 076 RPM le timeout intervient avant le front suivant et empêche la mesure stable.

### Filtres / Signal

- [majeur] Échantillonnage sans validation de largeur : lastLevel puis lastEdge sont actualisés sur chaque transition (210–223). Un parasite capturé entre deux polls peut fausser la période ; seule sa durée entre fronts descendants est filtrée.

### Documentation hardware/code

- [majeur] Le régulateur discret proposé dépend de VBE et du partage thermique entre TO-92 (95–111). Le document reconnaît >80 °C sans flux d’air mais ne fournit pas de qualification au pire cas chaud ni de protection thermique. Risque de brownout, à tester sous charge WiFi et à température maximale.

### WiFi / Réseau

- [majeur] Configuration sans authentification. Les ratios ne sont recalculés qu’à la prochaine période acceptée ; après perte, la modification seule ne relance pas une sortie, comportement à documenter.

### Qualité de code générale (négatif)

- [mineur] appendJsonStr n’échappe pas les caractères de contrôle, parseRatio s’appuie sur toFloat et ne teste pas isfinite. commit est ignoré ; aucun test ne couvre les constantes en millisecondes.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 5,0/10 — Architecture cohérente, constantes de temps débordantes

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 6,6/10 — Maintenabilité à améliorer

Chan moteur, Config persistante et Snap de lecture séparent correctement les responsabilités. CRC et réamorçage propres ; les constantes intermédiaires 32 bits ne sont pas protégées.
