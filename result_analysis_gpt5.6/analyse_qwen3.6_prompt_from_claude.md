# Analyse — qwen3.6_prompt_from_claude

**Sources analysées** :
- `competitors/qwen3.6_prompt_from_claude/deye_fan.ino`
- `competitors/qwen3.6_prompt_from_claude/SCHEMATIC.md`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Intentions
- Deux captures, filtrage glissant/EMA, serveur async, OTA et LittleFS sont prévus.
- Le ratio `période/ratio` et les 2 PPR sont compris.
- Le schéma prévoit pull-ups 3,3 V et sorties collecteur ouvert.

### Qualité de code générale (positif)
- **Structure / organisation** : Le document essaie de séparer temps réel, stockage et réseau et fournit une UI complète.
- **Lisibilité / nommage** : les unités et rôles principaux sont identifiables dans les constantes et structures.
- **Commentaires / documentation inline** : les choix d'architecture sont explicités, avec une qualité proportionnelle au score du rapport.
- **Gestion d'erreurs / robustesse** : les protections réellement présentes sont détaillées dans les sections techniques ci-dessus.
- **Duplication / DRY** : les deux canaux sont factorisés lorsqu'une structure commune existe; les exceptions sont signalées dans les défauts.
- **Respect des conventions C++/Arduino** : l'évaluation tient compte des types fixes, de `volatile`, d'IRAM et des API de la plateforme.
- **Complexité / maintenabilité** : la séparation des responsabilités a été évaluée indépendamment de la correction fonctionnelle.
- **Sécurité basique** : les copies bornées, échappements, secrets et contrôles HTTP ont été vérifiés.
- **Testabilité** : la présence réelle de tests/builds a été distinguée des simples affirmations documentaires.

---

## ❌ Points techniques négatifs

### Compilation / API
- [critique] : La signature du handler POST Async, `request->getString`, certaines surcharges `send` et la portée de `restartTime` sont invalides. — impact : le firmware ne compile pas.
- [critique] : Le `Ticker scheduler` est local à `setup()`. — impact : son destructeur détache le callback à la sortie de setup, même après correction du build.

### Temps-réel / Sortie
- [critique] : Ticker logiciel à 20 kHz n'offre pas l'isolation Timer1/NMI revendiquée. — impact : jitter et surcharge sous WiFi.
- [critique] : Les sorties sont initialisées HIGH alors que HIGH active le NPN. — impact : lignes Deye maintenues basses au boot.
- [majeur] : Après stall, l'état GPIO n'est pas explicitement relâché quand la cible passe à zéro. — impact : sortie pouvant rester bloquée basse.

### Hardware
- [critique] : Le MT3608 est spécifié comme buck 12→5 V alors que c'est un convertisseur boost. — impact : il ne peut pas abaisser 12 V; alimentation inutilisable.

### Sécurité
- [majeur] : Pas d'authentification et validation incomplète des ratios/chaînes. — impact : contrôle réseau exposé.

### Qualité de code générale (négatif)
- [majeur] : Empilement de bibliothèques et de correctifs non compilés; commentaires de « FIX » non validés et portée C++ incorrecte. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 1,0/10 — Non compilable, scheduler détruit et alimentation mal choisie

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 3,5/10 — Ambitieux mais non intégré

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
