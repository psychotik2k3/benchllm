# Analyse — glm5.3

**Sources analysées** : [deye_fan/deye_fan.ino](<../competitors/glm5.3/deye_fan/deye_fan.ino>) ; [deye_fan/README.md](<../competitors/glm5.3/deye_fan/README.md>) ; [deye_fan/schema_electronique.md](<../competitors/glm5.3/deye_fan/schema_electronique.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

ISR IRAM, buffer de 128 horodatages, génération à échéances absolues et GPOS/GPOC.

### Démarrage / Boot readiness

Intention : EEPROM, sorties relâchées, capture/Timer1 puis WiFi sans attente. Quatre fronts et un tick de contrôle 100 ms sont nécessaires ; aucune sortie réelle en l’état non compilable. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Génération sortie / Timer1

Timer1 DIV1, rattrapage limité à trois fronts et commandes pendantes par canal.

### Fidélité au protocole tachymétrique / Fail-safe

PPR=2 et mode cache-panne optionnel explicitement désactivé par défaut ; perte 2,5 s, secours nominal seulement après 7,5 s depuis la dernière mesure valide.

### Filtres / Signal

Rejet des fronts <400 µs, fenêtre d’historique 2 s, EMA de fréquence 1/4.

### WiFi / Réseau

AP+STA sans sommeil, DNS captif, mDNS, scan asynchrone et reprise STA.

### Gestion mémoire / HEAP

HTML PROGMEM et buffers snprintf pour status/config.

### Interface Web

Status et formulaires séparés, scan et contrôle des canaux.

### Persistance / Stockage

Signature/version/taille et CRC32 avec résultat EEPROM.commit contrôlé par certains handlers.

### Documentation hardware/code

Schéma complet avec OU Schottky + buck et procédures de calibration.

### Qualité de code générale (positif)

- **Structure / organisation** : InChannel/OutChannel/Settings et fonctions de mesure/contrôle séparés.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : CRC et erreurs EEPROM vérifiés à plusieurs endroits, mais mises à jour en place non transactionnelles.
- **Duplication / DRY** : Boucles à deux voies et helpers de validation ; séparation acceptable malgré un gros fichier.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : InChannel/OutChannel/Settings et fonctions de mesure/contrôle séparés.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Documentation hardware/code

- [critique] README 17 et schema_electronique.md 47–48 imposent les deux tach d’une paire en parallèle puis une division par deux. Deux collecteurs ouverts réalisent une combinaison de niveaux, pas une addition de fronts : à fréquence/phase identiques, on ne reçoit qu’une fréquence et le calcul divise la vitesse réelle par deux ; à d’autres phases, des impulsions se masquent.

### Temps-réel / ISR

- [majeur] Timer1 ordinaire et copie de 128 timestamps sous noInterrupts (213–217), soit jusqu’à 512 octets copiés sans IRQ. L’isolation totale et la latence maximale annoncées dans le README ne sont pas établies.

### Fidélité au protocole tachymétrique / Fail-safe

- [majeur] Le mode cache-panne maintient volontairement des RPM après perte réelle ; la détection et la bascule sont traitées dans loop. La mesure nécessite quatre fronts : amorçage de 30–40 ms à 3 000 RPM puis jusqu’à 100 ms de contrôle, hors setup.

### WiFi / Réseau

- [majeur] Routes config, reboot et factory reset sans authentification. Les paramètres chargés avec CRC ne sont pas systématiquement assainis (chaînes, ratios) avant usage.

### Qualité de code générale (négatif)

- [critique] SETTINGS_MAGIC=0xDEYEF001UL (114) contient Y. settingsLoad/Save ne peuvent pas compiler avec cette constante.
- [majeur] handleConfigPost et handleWifiPost modifient S avant la validation globale (532–585). Une requête finalement refusée peut avoir déjà changé certains champs en RAM ; aucune transaction par copie.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 2,2/10 — Non compilable, mélange des tach incorrect

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 5,2/10 — Maintenabilité à améliorer

InChannel/OutChannel/Settings et fonctions de mesure/contrôle séparés. CRC et erreurs EEPROM vérifiés à plusieurs endroits, mais mises à jour en place non transactionnelles.
