# Analyse — qwen35

**Sources analysées** : [calibration_guide.md](<../competitors/qwen35/calibration_guide.md>) ; [firmware_main.cpp](<../competitors/qwen35/firmware_main.cpp>) ; [INDEX.md](<../competitors/qwen35/INDEX.md>) ; [memory/anti_rebound_gpio_optimization.md](<../competitors/qwen35/memory/anti_rebound_gpio_optimization.md>) ; [MEMORY.md](<../competitors/qwen35/MEMORY.md>) ; [README.md](<../competitors/qwen35/README.md>) ; [schematic.md](<../competitors/qwen35/schematic.md>) ; [SUMMARY.md](<../competitors/qwen35/SUMMARY.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

Les deux compteurs d’entrée sont volatile et leurs handlers portent IRAM_ATTR ; seuil de 1 ms prévu.

### Démarrage / Boot readiness

Le texte contient un setup, mais sa portée est absorbée par un raw string non fermé au bon endroit. Aucun premier signal valide ne peut être garanti. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Filtres / Signal

Intention de moyenne glissante de quatre valeurs, sans conversion correcte des compteurs en fréquence.

### Interface Web

Page et nombreuses fonctions de configuration présentes dans le texte fourni, sans compilation opérationnelle.

### Documentation hardware/code

README, index, guide et schéma sont fournis ; ils contiennent aussi des ajouts Home Assistant sans rapport avec la génération tach.

### Qualité de code générale (positif)

- **Structure / organisation** : Sections et fonctions nommées mais mélange C++, HTML et YAML corrompu.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : Quelques bornes textuelles de ratios, aucune livraison compilable ni gestion correcte des erreurs.
- **Duplication / DRY** : Calculs copiés et fusionnés en une sortie ; contenu Home Assistant étranger au besoin.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Sections et fonctions nommées mais mélange C++, HTML et YAML corrompu.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Temps-réel / ISR

- [critique] GPIO10/11 utilisés comme entrées (33–34) sont des broches de la flash ESP-12S, pas D1/D2 ; leur usage pour le tach compromet l’exécution et n’est pas câblable comme indiqué.

### Fidélité au protocole tachymétrique / Fail-safe

- [critique] rpm_raw reçoit count cumulatif (119–132), jamais remis à zéro ni normalisé par le temps/2 PPR. La vitesse affichée augmente à rotation constante et reste positive après arrêt.

### Génération sortie / Timer1

- [critique] Un unique PIN_OUTPUT et une bascule après 500 ms dans updateSmoothedRPM (169–172), indépendante de simRPM : train d’environ 1 Hz au mieux si cet appel était périodique. Les Ticker textuels utilisent attach(250), en secondes, pas en millisecondes.

### Documentation hardware/code

- [majeur] Le schéma décrit SS8550 comme NPN (schematic.md vers 38–40), alors qu’il s’agit d’un PNP ; il ne fournit pas une topologie de sortie collecteur ouvert NPN valide. Contradictions entre documentation et broches effectives.

### Qualité de code générale (négatif)

- [critique] samples9[`] (142) contient un caractère parasite ; GPOS est appelé comme une fonction (170/184/194) alors que c’est un registre. WiFi.transmitting n’est pas une API du cœur ; plusieurs erreurs empêchent la compilation.
- [critique] Le raw string html_page ouvert à 227 n’est pas fermé avant les fonctions de setup/HTTP ; il ne trouve une fermeture correspondante qu’au bloc YAML vers 812. Des fonctions présentées comme du C++ font donc partie d’une chaîne ; le setup utile manque à l’édition de liens.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 0,4/10 — Source corrompue, génération hors sujet

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 1,2/10 — Qualité insuffisante

Sections et fonctions nommées mais mélange C++, HTML et YAML corrompu. Quelques bornes textuelles de ratios, aucune livraison compilable ni gestion correcte des erreurs.
