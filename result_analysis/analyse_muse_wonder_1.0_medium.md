# Analyse — muse_wonder_1.0_medium

**Sources analysées** : [deye_fan.ino](<../competitors/muse_wonder_1.0_medium/deye_fan.ino>) ; [schema_electronique.md](<../competitors/muse_wonder_1.0_medium/schema_electronique.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

ISR de capture très courtes en IRAM, buffers de timestamps et sortie NMI entière.

### Démarrage / Boot readiness

GPIO bas et capture avant EEPROM, NMI prévue avant réseau. Aucun signal valide au démarrage à froid avec le code fourni : l’autorisation d’interruption Timer1 n’est pas armée. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Génération sortie / Timer1

Deux compteurs de reste et GPOS/GPOC ; tick idle de 5 ms, demi-périodes bornées.

### Fidélité au protocole tachymétrique / Fail-safe

PPR=2 et ratio 0,5–5 ; perte demandée après 1,2 s et retour de sortie relâché dans le moteur.

### Filtres / Signal

Périodes bornées 100 µs–150 ms, EMA 1/4, rejet de variation isolée et limitation de variation de 2 % par appel.

### WiFi / Réseau

AP+STA, mDNS et reconnexion périodique sans attente.

### Gestion mémoire / HEAP

HTML PROGMEM, status snprintf de 512 octets et buffers d’entrée fixes.

### Interface Web

Polling à 500 ms, chargement des réglages séparé.

### Persistance / Stockage

Configuration structurée avec version et CRC16 dans l’EEPROM, mais calcul de taille erroné.

### Documentation hardware/code

Schéma détaillé avec OU Schottky, protections, buck et un tach par paire.

### Qualité de code générale (positif)

- **Structure / organisation** : Structures entrée/config et moteur de sortie séparés, nombreuses fonctions spécialisées.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : Bornes et filtres présents, mais armement matériel, CRC et dépassement de ring non vérifiés.
- **Duplication / DRY** : Boucles par canal, deux wrappers de capture essentiellement identiques.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Structures entrée/config et moteur de sortie séparés, nombreuses fonctions spécialisées.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Génération sortie / Timer1

- [critique] setup fait timer1_enable puis écrit seulement T1L (580–585). L’activation du bit d’interruption de front TEIE1, réalisée par timer1_write dans le cœur, manque. Au démarrage à froid, le compteur peut tourner sans déclencher la NMI : sorties figées.
- [majeur] nmiProgrammed décompte la durée programmée au lieu du temps réel (317–320), donc les durées ISR s’ajoutent ; CPU_MHZ est fixé à 80 pour la capture, sans détection de la fréquence réelle.

### Persistance / Stockage

- [majeur] Config possède crc à l’offset 212 et sizeof=216 sur ABI 32 bits ; sizeof−2=214 inclut les deux octets du CRC (191/205). La vérification est normalement fausse après reboot ; préférer offsetof(Config, crc). Modèle de layout reproduit.

### Temps-réel / ISR

- [majeur] Les buffers de 16 timestamps (235–243) n’ont pas de détection de débordement : après exactement 16 événements sans traitement, head rejoint tail et tous paraissent consommés. À 3 000 RPM, cela arrive après seulement 160 ms de blocage.

### Fidélité au protocole tachymétrique / Fail-safe

- [majeur] Timeout dans loop et simulatedRpm calcule RPM×ratio (392–394) au lieu de la période réellement appliquée après slew/écrêtage. L’interface peut annoncer une vitesse que le signal ne produit pas.

### WiFi / Réseau

- [majeur] Routes de sauvegarde et reboot non authentifiées ; commit non contrôlé, absence de protection contre les sauvegardes répétées.

### Qualité de code générale (négatif)

- Les défauts transversaux de validation, de concurrence et de gestion d’erreur sont décrits dans leurs catégories techniques ci-dessus. Testabilité : Aucun test fourni ; les calculs sont mêlés au firmware.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 2,7/10 — Moteur NMI non armé au démarrage

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 5,5/10 — Maintenabilité à améliorer

Structures entrée/config et moteur de sortie séparés, nombreuses fonctions spécialisées. Bornes et filtres présents, mais armement matériel, CRC et dépassement de ring non vérifiés.
