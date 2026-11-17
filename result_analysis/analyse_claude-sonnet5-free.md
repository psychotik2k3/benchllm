# Analyse — claude-sonnet5-free

**Sources analysées** : [deye_fan.ino](<../competitors/claude-sonnet5-free/deye_fan.ino>) ; [schema_electronique.md](<../competitors/claude-sonnet5-free/schema_electronique.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

Capture GPIO et Timer1 annotés ICACHE_RAM_ATTR ; accès GPOS/GPOC, instantané protégé par noInterrupts (188–257).

### Démarrage / Boot readiness

Timer1 activé après EEPROM et avant WiFi ; aucune attente STA. Sortie inactive jusqu’au traitement, toutes les 150 ms. La première période utilise toutefois un horodatage initial nul. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Génération sortie / Timer1

Timer DIV16 chargé à 500 : cadence de 100 µs, deux compteurs indépendants.

### Fidélité au protocole tachymétrique / Fail-safe

Formule RPM = 30 000 000 / période et consigne divisée par le ratio ; arrêt logiciel après 3 s sans front.

### Filtres / Signal

Rejet des intervalles inférieurs à 2 ms ; instantané atomique des timestamps.

### WiFi / Réseau

AP+STA, WIFI_NONE_SLEEP et CPU réglé à 160 MHz ; connexion STA non bloquante.

### Gestion mémoire / HEAP

HTML PROGMEM, réponses status/config dans des buffers de 220/256 octets.

### Interface Web

Actualisation des seules mesures toutes les secondes, formulaires chargés séparément.

### Persistance / Stockage

Configuration structurée, signature et checksum pondérée ; défauts en cas d’invalidité.

### Documentation hardware/code

Schéma des deux voies, sorties NPN et alimentation OU Schottky + buck.

### Qualité de code générale (positif)

- **Structure / organisation** : Fonctions capture, sortie, stockage et HTTP clairement séparées.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : Validation des ratios web, mais pas de contrôle de finitude/chaînes après chargement.
- **Duplication / DRY** : Tableaux communs pour les deux voies, seuls les wrappers ISR sont dupliqués.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Fonctions capture, sortie, stockage et HTTP clairement séparées.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Documentation hardware/code

- [critique] Le circuit d’entrée (schema_electronique.md 108–124) suppose un pull-up 12 V interne au Noctua mais n’en fournit aucun sur son tach : la résistance 4,7 kΩ est au collecteur du transistor tampon. Avec une sortie Noctua collecteur ouvert sans pull-up, la base ne reçoit aucun niveau haut garanti et la capture peut rester à zéro.

### Génération sortie / Timer1

- [majeur] Le compteur est rechargé à N puis décrémenté N fois avant le prochain toggle (220–230) : demi-période réelle (N+1)×100 µs. À N=10, plafond affiché 15 000 RPM mais réel 13 636 RPM (−9,09 %).

### Temps-réel / ISR

- [majeur] `timer1_attachInterrupt` fournit une interruption masquable, pas une NMI ; les ticks perdus pendant les sections critiques ne sont pas compensés. Le timeout de 3 s dépend également de loop.

### Fidélité au protocole tachymétrique / Fail-safe

- [mineur] Le premier front est interprété comme une période depuis le démarrage (190–195). La première valeur est fausse et le délai d’arrêt atteint 3 s + 150 ms hors blocage.

### Qualité de code générale (négatif)

- [majeur] JSON des SSID sans échappement (432–433), routes sans authentification et `EEPROM.commit` ignoré. Un guillemet dans le SSID invalide la réponse ; une erreur de stockage reste invisible.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 4,1/10 — Firmware lisible, entrée matérielle incomplète

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 6,0/10 — Maintenabilité à améliorer

Fonctions capture, sortie, stockage et HTTP clairement séparées. Validation des ratios web, mais pas de contrôle de finitude/chaînes après chargement.
