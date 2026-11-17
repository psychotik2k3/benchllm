# Analyse — deepseek4 flash

**Sources analysées** : [deye_fan.ino](<../competitors/deepseek4 flash/deye_fan.ino>) ; [README.md](<../competitors/deepseek4 flash/README.md>) ; [SCHEMA_ELECTRONIQUE.md](<../competitors/deepseek4 flash/SCHEMA_ELECTRONIQUE.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

Intention NMI en IRAM ; calculs Q16 hors ISR et écritures GPIO W1TS/W1TC.

### Démarrage / Boot readiness

Initialisation annoncée avant WiFi ; GPIO bas et consignes nulles. Aucun délai de premier signal valide ne peut être garanti à cause des défauts Timer1. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Fidélité au protocole tachymétrique / Fail-safe

Formules à deux impulsions par tour, ratios entiers et délai de perte adaptatif max(2 s, 4 périodes filtrées).

### Filtres / Signal

EMA 1/8 et bornes de périodes, mais la limite haute de 120 s est beaucoup trop large pour un ventilateur nominal.

### WiFi / Réseau

AP+STA, mDNS et reconnexion toutes les 20 s.

### Gestion mémoire / HEAP

HTML PROGMEM, buffer status global de 640 octets et snprintf borné.

### Interface Web

Télémétrie et réglages séparés, polling annoncé à 1,5 s.

### Persistance / Stockage

Signature et stockage structuré, chaînes explicitement terminées au chargement.

### Documentation hardware/code

README et schéma détaillés, buck et sorties collecteur ouvert.

### Qualité de code générale (positif)

- **Structure / organisation** : Types Config/Snap et fonctions réseau distincts ; bas niveau développé indépendamment des définitions du cœur.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : Bornage de ratios et de buffers, mais erreurs de registres et copie volatile bloquantes.
- **Duplication / DRY** : Deux ISR d’entrée copiées ; sortie et calculs regroupés par canal.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Types Config/Snap et fonctions réseau distincts ; bas niveau développé indépendamment des définitions du cœur.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Génération sortie / Timer1

- [critique] FRC1_CTRL_DIV_16 est défini bit 8 et ENABLE bit 0 (105–108), alors que le cœur ESP8266 utilise TCPD=2 et TCTE=7. `initTimer` écrit donc une configuration qui n’active pas le compteur ; il écrit aussi le registre COUNT en lecture seule (509–519).
- [majeur] Le temps virtuel avance seulement de g_lastDelta (155) et ignore le temps réel de traitement/retard. La précision de 0,2 µs annoncée n’est donc pas démontrée, même après correction du registre.

### Persistance / Stockage

- [majeur] CfgT est aligné sur deux octets : crc à 206, sizeof=208. La somme sur sizeof−1=207 inclut le checksum lui-même (256/278) ; la relecture ne valide pas normalement les réglages sauvegardés. Calcul reproduit par modèle de layout.

### WiFi / Réseau

- [majeur] `/api/state` publie les deux mots de passe en clair (438–439), sans authentification et sans échappement JSON. Secrets lisibles par tout client joignable et JSON invalide avec guillemets.

### Fidélité au protocole tachymétrique / Fail-safe

- [majeur] Mettre g_halfTicks à zéro au timeout (572) ne relâche pas nécessairement le GPIO dans la NMI : son niveau peut rester haut, donc transistor conducteur. La perte est traitée dans loop et peut être retardée par HTTP.

### Qualité de code générale (négatif)

- [critique] `g_snap` est un volatile SnapT (141), mais `handleState` fait `s = g_snap` (416). L’opérateur de copie implicite d’une structure C++ ne prend pas volatile const SnapT& : conversion interdite dans un build C++ standard.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 1,8/10 — Accès registres erronés, aucune génération fiable

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 4,0/10 — Qualité insuffisante

Types Config/Snap et fonctions réseau distincts ; bas niveau développé indépendamment des définitions du cœur. Bornage de ratios et de buffers, mais erreurs de registres et copie volatile bloquantes.
