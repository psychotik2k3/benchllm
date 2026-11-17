# Analyse — composer 2.5

**Sources analysées** : [deye_fan/deye_fan.ino](<../competitors/composer 2.5/deye_fan/deye_fan.ino>) ; [deye_fan/schema_electronique.md](<../competitors/composer 2.5/deye_fan/schema_electronique.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

ISR marquées IRAM_ATTR, tableaux TachChannel et bornes de période explicites.

### Démarrage / Boot readiness

Intention : sorties basses, capture puis EEPROM, Timer1 avant WiFi sans attente STA. En l’état, aucun premier signal exploitable puisque la compilation est bloquée. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Génération sortie / Timer1

Architecture à compteurs par canal et fonctions GPIO dédiées.

### Fidélité au protocole tachymétrique / Fail-safe

PPR=2 et division de période par ratio 1–6 ; arrêt demandé après 3 s dans loop.

### WiFi / Réseau

AP+STA avec connexion non bloquante, WiFi.persistent(false).

### Persistance / Stockage

Structure Config et CRC16, paramètres conservés dans EEPROM.

### Documentation hardware/code

Schémas et BOM fournis ; sorties NPN collecteur ouvert correctement identifiées.

### Qualité de code générale (positif)

- **Structure / organisation** : TachChannel et Config identifiables, fonctions dédiées mais page construite par concaténations.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : Ratios web bornés ; compilation, unités et alimentation non vérifiées.
- **Duplication / DRY** : Traitement générique de deux canaux, wrappers ISR minimaux.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : TachChannel et Config identifiables, fonctions dédiées mais page construite par concaténations.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Documentation hardware/code

- [critique] schema_electronique.md (62–68) propose le rail ≈12 V directement sur la broche 5V et invoque un AMS1117. Le D1 mini visé exige une alimentation 5 V régulée ; ce câblage peut détruire la carte. Ajouter un abaisseur est obligatoire, pas optionnel au-delà de 12 V.

### Génération sortie / Timer1

- [critique] `timer1_write(TIMER_TICK_US)` (411) charge 10 ticks DIV16, donc 2 µs au lieu de 10 µs. Après correction des erreurs de compilation, la fréquence serait multipliée par cinq et la charge ISR atteindrait 500 kHz.

### Temps-réel / ISR

- [majeur] `handleTachEdge` (167–175) divise en flottant dans l’ISR ; le ratio partagé n’est pas volatile. Les délais ISR et la visibilité des mises à jour demandent correction et vérification du binaire.

### Interface Web

- [mineur] Rechargement complet toutes les 2 s (291), donc perte du focus et des valeurs en cours de saisie.

### Qualité de code générale (négatif)

- [critique] `EEPROM_MAGIC 0xDEYE` (51) contient Y, interdit dans un entier hexadécimal. `loadConfig` l’utilise : le firmware ne peut pas être compilé tel quel.
- [majeur] SSID et mot de passe sont insérés sans échappement dans des attributs HTML (332–337), sans authentification. Un apostrophe peut sortir de l’attribut et injecter du HTML.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 1,4/10 — Non compilable et alimentation à corriger

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 3,8/10 — Qualité insuffisante

TachChannel et Config identifiables, fonctions dédiées mais page construite par concaténations. Ratios web bornés ; compilation, unités et alimentation non vérifiées.
