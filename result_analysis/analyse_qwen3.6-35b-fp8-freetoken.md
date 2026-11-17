# Analyse — qwen3.6-35b-fp8-freetoken

**Sources analysées** : [circuit.h](<../competitors/qwen3.6-35b-fp8-freetoken/circuit.h>) ; [deye_fan.ino](<../competitors/qwen3.6-35b-fp8-freetoken/deye_fan.ino>) ; [README.md](<../competitors/qwen3.6-35b-fp8-freetoken/README.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

Les deux fonctions ISR déclarées portent IRAM_ATTR ; l’intention est de mesurer une période puis programmer une sortie.

### Démarrage / Boot readiness

GPIO initialisés et AP configuré ; pas de vitesse de secours. La compilation bloquée empêche toute conclusion opérationnelle. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Interface Web

Page status/configuration et projet de DNS captif.

### Persistance / Stockage

Sauvegarde EEPROM des ratios et credentials ; une fonction auxiliaire évite de réécrire des octets identiques.

### Documentation hardware/code

README et circuit.h décrivent le câblage et une nomenclature, mais plusieurs connexions sont incorrectes.

### Qualité de code générale (positif)

- **Structure / organisation** : Prototypes et sections existent, mais les inclusions arrivent après des usages et le modèle des canaux est incohérent.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : Copies bornées à plusieurs endroits, absence de validation fiable des données EEPROM.
- **Duplication / DRY** : Une voie est artificiellement dupliquée à l’affichage ; fonctions EEPROM partiellement inutilisées.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Prototypes et sections existent, mais les inclusions arrivent après des usages et le modèle des canaux est incohérent.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Génération sortie / Timer1

- [critique] loadVal est calculé mais jamais chargé (368–381), aucune activation Timer1 n’est effectuée et le reload est zéro. currentOutputPin ne sélectionne qu’une sortie ; seul CH1 est configuré en génération (667–670), CH2 n’a pas de moteur indépendant.

### Fidélité au protocole tachymétrique / Fail-safe

- [critique] Une seule entrée GPIO13 remplace les deux canaux, les quatre tach étant mélangés selon README 106–117. rawRPM1 et rawRPM2 reçoivent la même mesure ; calculateRPMS coupe les sorties dès qu’aucune nouvelle période n’arrive dans une loop (652–689), sans timeout adapté.

### Documentation hardware/code

- [critique] README 114 et circuit.h 69 relient les cathodes des diodes OR au GND alors qu’elles constituent le rail positif : court-circuit si suivi littéralement. Le README 118–121 ajoute aussi un pull-up 10 kΩ de +12 V vers le nœud GPIO déjà tiré à GND par 10 kΩ : tension proche de 6 V, dangereuse pour l’ESP8266.

### Persistance / Stockage

- [majeur] EEPROM vierge vaut 0xFF, pas 0 : le test (224) ne reconnaît pas l’état vierge. Des float NaN deviennent les ratios. L’octet de passe maximal à 98 chevauche le premier ratio à l’offset 98.

### Qualité de code générale (négatif)

- [critique] EEPROM est utilisé sans include EEPROM.h ; timer1Write/timer1AttachInterrupt (341/380–381) ne sont pas les API timer1_write/timer1_attachInterrupt ; DNSServer.processNextDNSRequest (764) ne correspond pas à processNextRequest. Dépendances et appels incompatibles avec le cœur ciblé.
- [majeur] L’interruption est attachée avec LOW (730) au lieu d’un front FALLING/RISING ; pas de validation robuste des ratios ou erreur de sauvegarde. Le README prétend à tort qu’une coupure de 30 s efface l’EEPROM.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 0,6/10 — Une seule entrée, sorties non implémentées correctement

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 1,9/10 — Qualité insuffisante

Prototypes et sections existent, mais les inclusions arrivent après des usages et le modèle des canaux est incohérent. Copies bornées à plusieurs endroits, absence de validation fiable des données EEPROM.
