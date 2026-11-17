# Analyse — gpt-5.6 sol medium

**Sources analysées** : [deye_fan.ino](<../competitors/gpt-5.6 sol medium/deye_fan.ino>) ; [SCHEMA_ET_INSTALLATION.md](<../competitors/gpt-5.6 sol medium/SCHEMA_ET_INSTALLATION.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

Capture IRAM et sortie par registres ; calculs et allocations hors ISR ; instantanés atomiques.

### Démarrage / Boot readiness

EEPROM puis setupSignalEngine avant réseau ; sorties basses et deux fronts exigés. À 1 600 RPM, acquisition 18,75–37,5 ms, puis jusqu’à 10 ms de timer idle, hors setup. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Génération sortie / Timer1

Timer1 DIV16, conversion explicite 5 ticks/µs et ordonnanceur commun à deux sorties ; saturation des demi-périodes.

### Fidélité au protocole tachymétrique / Fail-safe

PPR=2, ratios 0,1–20 et timeout 2 s ; filtre réamorcé à la perte de signal.

### Filtres / Signal

Périodes 500 µs–1,5 s ; EMA 1/4 appliquée seulement si le compteur de mesures change (514–523).

### WiFi / Réseau

AP+STA et connexion non bloquante ; WiFi.persistent(false).

### Gestion mémoire / HEAP

Aucune allocation ISR ; réservations des String HTML/JSON.

### Interface Web

Dark theme, polling et formulaires séparés ; mots de passe non réaffichés.

### Persistance / Stockage

Version, taille, CRC32 et contrôle de finitude des ratios ; configuration HTTP validée dans une copie temporaire.

### Documentation hardware/code

BOM, niveaux électriques, buck/buck-boost selon plage d’entrée, un tach par paire et limites métrologiques explicites.

### Qualité de code générale (positif)

- **Structure / organisation** : Namespace interne, types persistants/runtime distincts et fonctions d’instantané bien délimitées.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : Validation finie, CRC, copies bornées et mise à jour transactionnelle des paramètres HTTP.
- **Duplication / DRY** : Moteur et conversion génériques par canal, constantes partagées.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Namespace interne, types persistants/runtime distincts et fonctions d’instantané bien délimitées.
- **Sécurité basique** : Validation finie des ratios et échappement HTML couvrant aussi les apostrophes des attributs.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Génération sortie / Timer1

- [majeur] outputTimerIsr décompte `armedTimerTicks` au lieu du temps réel écoulé (205–243), puis recharge le timer après son traitement. Les temps ISR et latences s’ajoutent à chaque intervalle : dérive négative de fréquence. Aucun plafond de jitter mesuré ne peut être déduit.

### Temps-réel / ISR

- [majeur] Timer1 et GPIO restent masquables pendant les opérations critiques/flash. Le timeout (500–510) n’est contrôlé que dans loop ; HTTP peut prolonger l’émission d’une ancienne consigne au-delà de 2 s.

### Fidélité au protocole tachymétrique / Fail-safe

- [majeur] À la perte, inputLastValidEdgeUs/inputPeriodUs ne sont pas invalidés (504–511). Au wrap de micros après ≈71 min 35 s, l’ancien timestamp redevient récent ; l’EMA est alors à zéro et aucun nouveau compteur n’arrive pour la réamorcer : division par zéro et conversion non finie possibles (514–533).

### Persistance / Stockage

- [mineur] configIsValid ne vérifie pas un NUL dans toutes les chaînes ; les copies HTTP sont sûres, mais une configuration CRC-valide forgée peut être non terminée. Le résultat commit est ignoré.

### WiFi / Réseau

- [majeur] Pas d’authentification sur la configuration ; toute machine joignable peut modifier la fréquence ou programmer un reboot via changement WiFi.

### Qualité de code générale (négatif)

- Les défauts transversaux de validation, de concurrence et de gestion d’erreur sont décrits dans leurs catégories techniques ci-dessus. Testabilité : Aucun test fourni ; les calculs sont mêlés au firmware.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 6,8/10 — Base claire, ordonnanceur et arrêt à consolider

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 7,5/10 — Structure solide

Namespace interne, types persistants/runtime distincts et fonctions d’instantané bien délimitées. Validation finie, CRC, copies bornées et mise à jour transactionnelle des paramètres HTTP.
