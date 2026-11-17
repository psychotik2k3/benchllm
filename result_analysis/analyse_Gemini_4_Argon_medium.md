# Analyse — Gemini_4_Argon_medium

**Sources analysées** : [deye_fan.ino](<../competitors/Gemini_4_Argon_medium/deye_fan.ino>) ; [SCHEMA_ET_ARCHITECTURE.md](<../competitors/Gemini_4_Argon_medium/SCHEMA_ET_ARCHITECTURE.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

Sorties pilotées par NMI, GPOS/GPOC et calendrier en cycles ; capture GPIO IRAM, timeout contrôlé dans la NMI (222–307).

### Démarrage / Boot readiness

Sorties basses, EEPROM puis init moteur avant WiFi (1155–1177). Capture initialisée par un premier front puis mesure au second ; délai d’acquisition de 10–20 ms à 3 000 RPM et 18,75–37,5 ms à 1 600 RPM, hors setup non mesuré. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Génération sortie / Timer1

DIV1 et prise en compte du mode CPU turbo ; anticipation de 2 µs, échéances accumulées et correction de grand retard.

### Fidélité au protocole tachymétrique / Fail-safe

PPR=2, ratios 0,1–10 ; demi-période minimale 500 µs, soit plafond 30 000 RPM ; perte nominale après 500 ms.

### Filtres / Signal

Rejet des fronts <2,5 ms et moyenne de quatre périodes, sans flottant dans la NMI.

### WiFi / Réseau

AP+STA, sommeil désactivé et reconnexion à 15 s, paramètres modifiables sans reboot.

### Gestion mémoire / HEAP

HTML PROGMEM, status construit dans un buffer de 768 octets, chaînes JSON partiellement échappées.

### Interface Web

SPA et polling à 500 ms, état des deux canaux et paramètres WiFi.

### Persistance / Stockage

Signature, version, CRC32 par offsetof ; ratios/chaînes contrôlés au chargement.

### Documentation hardware/code

Documentation détaillée des étages NPN et séparation des tach des deux ventilateurs d’une paire.

### Qualité de code générale (positif)

- **Structure / organisation** : Structures temps réel séparées, helpers de conversion et stockage dédiés.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : CRC et bornage corrects, mais réamorçage du signal incomplet et mutations WiFi non transactionnelles.
- **Duplication / DRY** : Traitements génériques des canaux, petits wrappers ISR.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Structures temps réel séparées, helpers de conversion et stockage dédiés.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Fidélité au protocole tachymétrique / Fail-safe

- [critique] La NMI stoppe la sortie au timeout mais ne remet pas hasSignal ni targetHalfCycles à zéro (243–249). Sans nouvel événement, l’âge en cycles déborde : après 53,687 s à 80 MHz ou 26,844 s à 160 MHz, il redevient inférieur à 500 ms et réactive une ancienne consigne pendant une fenêtre d’environ 500 ms.
- [majeur] Au premier front après un arrêt, processTachEdgeIRAM réinitialise le ring mais conserve measuredPeriodCycles/targetHalfCycles (328–334). L’ancienne vitesse peut être émise avant la seconde mesure valide.

### Documentation hardware/code

- [majeur] Alimentation maison à BC337 et résistance de 33 Ω (SCHEMA_ET_ARCHITECTURE.md 25–43). À 300 mA, la résistance chute de 9,9 V : il ne reste que 2,1 V depuis 12 V, insuffisant pour 4,4 V. Sans dimensionnement transitoire probant, les pics WiFi peuvent faire redémarrer le module.

### Temps-réel / ISR

- [majeur] La capture reste une IRQ GPIO masquable et peut être retardée pendant la flash. De plus, GPIO ISR et NMI modifient T1L (370–371 et 307), ce qui exige un protocole de programmation vérifié sous préemption.

### WiFi / Réseau

- [majeur] Routes de réglage/reset sans authentification et retour de sauvegarde ignoré dans les handlers.

### Qualité de code générale (négatif)

- [mineur] escapeJsonString ne traite pas les caractères de contrôle (938–947), donc un SSID avec retour ligne invalide le JSON ; les mutations WiFi sont partielles avant validation complète.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 4,9/10 — Moteur avancé, perte de signal et alimentation fragiles

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 6,8/10 — Maintenabilité à améliorer

Structures temps réel séparées, helpers de conversion et stockage dédiés. CRC et bornage corrects, mais réamorçage du signal incomplet et mutations WiFi non transactionnelles.
