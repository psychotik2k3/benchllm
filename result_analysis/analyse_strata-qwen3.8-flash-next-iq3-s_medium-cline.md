# Analyse — strata-qwen3.8-flash-next-iq3-s_medium-cline

**Sources analysées** : [deye_fan.ino](<../competitors/strata-qwen3.8-flash-next-iq3-s_medium-cline/deye_fan.ino>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

Entrée échantillonnée et sorties dans la même ISR ; données partagées volatile et état runtime séparé.

### Démarrage / Boot readiness

GPIO puis pause 60 ms, LittleFS et moteur avant WiFi (926–986). Intention de plancher 600 RPM avant mesure, mais code non compilable et pas de validation physique de ce mode. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Génération sortie / Timer1

DIV16 chargé à 125 ticks : pas de 25 µs, 40 kHz ; accumulateur de phase entier et limite RPM_MAX=20 000.

### Fidélité au protocole tachymétrique / Fail-safe

PPR entrée/sortie configurables, ratios, offsets, mode test et maintien optionnel après perte ; valeurs bornées dans les handlers.

### Filtres / Signal

Quatre échantillons concordants (100 µs) et moyenne des périodes sur fenêtre 400 ms.

### WiFi / Réseau

AP+STA, sommeil désactivé, mDNS et reprise station sans attendre la connexion.

### Gestion mémoire / HEAP

HTML PROGMEM, données temps réel fixes et String status réservé.

### Interface Web

Status détaillé, réglages de canaux et mode test séparés.

### Persistance / Stockage

LittleFS texte et valeurs par défaut déjà initialisées ; retour booléen de sauvegarde exploité.

### Documentation hardware/code

Commentaires embarqués détaillés ; aucun schéma annexe autonome fourni.

### Qualité de code générale (positif)

- **Structure / organisation** : AppConfig, commandes et état st distincts ; helpers de parsing et handlers regroupés.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : Bornage et retours FS présents ; calcul signé/non signé et sélection de canal erronés.
- **Duplication / DRY** : Moteur commun par canal ; logique de modes dense et publication de télémétrie redondante.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : AppConfig, commandes et état st distincts ; helpers de parsing et handlers regroupés.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Génération sortie / Timer1

- [critique] La phase ne progresse pas tant que pulseLeft est non nul (346–365). La période effective = durée d’accumulation + largeur d’impulsion, alors que outConfigure ne soustrait pas cette largeur. Pour 7 500 RPM, 2 PPR : inc=409, 161 ticks d’accumulation +53 ticks de pulse donnent ≈5 607 RPM, soit −25,2 %.

### Fidélité au protocole tachymétrique / Fail-safe

- [critique] Le test `if (!alive)` (344) regarde le masque global, pas le bit du canal. Si le canal 0 tourne, un canal 1 perdu peut continuer à émettre ; le bloc de perte (418–421) ne fait pas outKill, ce qui entretient la dernière fréquence.
- [majeur] Soustraction d’offset en uint32_t (398) : 1 000−3 000 sous-déborde puis est plafonné à 20 000 RPM (425), au lieu de zéro/plancher. Au démarrage age=0 rend aussi des entrées sans front temporairement « vivantes » et peut inventer 600 RPM.

### Temps-réel / ISR

- [majeur] Calculs de division uint64_t et génération à 40 kHz dans une IRQ normale. Pas de budget d’exécution/qualification IRAM des helpers produits par le compilateur ; les attributs IRAM sur des tableaux de données (289–290) exigent aussi vérification de section.

### WiFi / Réseau

- [majeur] Mutations via GET sans authentification (989–992), notamment factory reset. Le mode test est sauvegardé partiellement : testMode n’est pas écrit/rechargé dans config.txt.

### Qualité de code générale (négatif)

- [critique] TICK_US est utilisé dans handleStatus/setup (781/945–947) sans définition. LittleFS.begin(true) (953) ne correspond pas à FS::begin() du cœur 3.1.2. Deux erreurs certaines bloquent le build standard.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 1,7/10 — Défauts de compilation et fréquence NCO incorrecte

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 4,1/10 — Qualité insuffisante

AppConfig, commandes et état st distincts ; helpers de parsing et handlers regroupés. Bornage et retours FS présents ; calcul signé/non signé et sélection de canal erronés.
