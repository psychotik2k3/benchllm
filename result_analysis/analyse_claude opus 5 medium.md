# Analyse — claude opus 5 medium

**Sources analysées** : [deye_fan/deye_fan.ino](<../competitors/claude opus 5 medium/deye_fan/deye_fan.ino>) ; [SCHEMA.md](<../competitors/claude opus 5 medium/SCHEMA.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

`fanEngineIsr` (373–425) échantillonne les deux entrées toutes les 40 µs en NMI ; les fonctions critiques sont en IRAM et les échanges utilisent des mots atomiques et un seqlock.

### Démarrage / Boot readiness

`setup` (1087–1119) charge la configuration avant `engineBegin`, puis démarre le réseau sans attente de connexion. Sorties relâchées, sans RPM de remplacement. Deux fronts valides sont nécessaires : à 1 600 RPM, environ 18,75–37,5 ms après activation de la capture, plus le calcul périodique de 20 ms et le setup non chronométré. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Génération sortie / Timer1

Échéances absolues en cycles, GPOS/GPOC et conversion explicite cycles/ticks à 80 ou 160 MHz. Rattrapage borné à 16 fronts par passage (340–367).

### Fidélité au protocole tachymétrique / Fail-safe

PPR entrée/sortie séparés, défaut 2 ; ratio entier au millième ; plafonnement configurable. `updateChannel` (683–739) applique RPM × ratio et arrête sur absence de signal.

### Filtres / Signal

Trois échantillons concordants (120 µs), périodes de 2 à 200 ms, EMA 1/8 et compteur de rejets.

### WiFi / Réseau

AP+STA sans sommeil, reconnexion à 30 s et mDNS. Routes HTTP de lecture et de configuration séparées.

### Gestion mémoire / HEAP

HTML en PROGMEM, petits blocs `snprintf` et réservation du String JSON ; aucune allocation dans le moteur.

### Interface Web

Polling séparé des formulaires ; télémétrie des fronts, rejets, écrêtage et activité NMI.

### Persistance / Stockage

Signature, version et CRC32 ; bornage des ratios/PPR au chargement (631–660).

### Documentation hardware/code

SCHEMA.md décrit OU Schottky + buck 5 V, entrées filtrées, sorties NPN et un seul tach par paire ; calculs et procédure de mesure fournis.

### Qualité de code générale (positif)

- **Structure / organisation** : Types InputChannel/OutputChannel/Config distincts et fonctions courtes pour le moteur, le stockage et le réseau.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : Bornage défensif des paramètres et CRC32, mais erreurs de sauvegarde non remontées.
- **Duplication / DRY** : Boucles à deux canaux et helpers GPIO partagés.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Types InputChannel/OutputChannel/Config distincts et fonctions courtes pour le moteur, le stockage et le réseau.
- **Sécurité basique** : Validation par copie temporaire pour les ratios et snprintf borné pour la télémétrie numérique.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Fidélité au protocole tachymétrique / Fail-safe

- [majeur] Le timeout de 1 200 ms est évalué dans `updateChannel`, appelé après HTTP dans `loop` (1122–1135). Une requête bloquante retarde l’arrêt d’autant ; le moteur NMI continue la dernière consigne sans contrôler lui-même la perte d’entrée.

### WiFi / Réseau

- [majeur] Aucune authentification des routes (1030–1039) et `/api/status` renvoie `apPass` (927). Un client réseau peut lire le secret AP, modifier les ratios ou redémarrer le module.

### Documentation hardware/code

- [mineur] SCHEMA.md (270–302) déduit la saturation jusqu’à 85–120 mA d’un hFE minimal. Avec Ib ≈ 1,16 mA, un gain forcé de 10 ne garantit qu’environ 11,6 mA ; le dimensionnement doit être vérifié avec le vrai pull-up.

### Persistance / Stockage

- [mineur] `configSave` ignore le résultat de `EEPROM.commit` (627), tout en annonçant un enregistrement réussi. Aucun fichier de secours ni limitation des sauvegardes répétées.

### Qualité de code générale (négatif)

- [majeur] Les SSID et mots de passe sont concaténés sans échappement JSON (926–931). Un SSID contenant un guillemet casse `/api/status` et son polling.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 7,4/10 — Moteur NMI convaincant, supervision à renforcer

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 7,7/10 — Structure solide

Types InputChannel/OutputChannel/Config distincts et fonctions courtes pour le moteur, le stockage et le réseau. Bornage défensif des paramètres et CRC32, mais erreurs de sauvegarde non remontées.
