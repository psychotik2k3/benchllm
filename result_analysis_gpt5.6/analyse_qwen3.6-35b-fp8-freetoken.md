# Analyse — qwen3.6-35b-fp8-freetoken

**Sources analysées** :
- `competitors/qwen3.6-35b-fp8-freetoken/deye_fan.ino`
- `competitors/qwen3.6-35b-fp8-freetoken/README.md`
- `competitors/qwen3.6-35b-fp8-freetoken/circuit.h`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Intentions
- AP+STA, portail captif, EEPROM et interface riche sont envisagés.
- Le principe d'un collecteur ouvert de sortie est mentionné.

### Qualité de code générale (positif)
- **Structure / organisation** : Quelques contrôles EEPROM/HTTP et explications de 2 PPR existent.
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

### Compilation / Plateforme
- [critique] : Une accolade/`EEPROM.end()` est hors fonction et les API `hw_timer_t`/timer sont celles d'ESP32. — impact : le source ne compile pas sur ESP8266.
- [critique] : L'interruption est attachée en mode `LOW` plutôt que `FALLING`. — impact : tempête d'interruptions tant que la ligne est basse.

### Architecture / Mathématiques
- [critique] : Un seul tach d'entrée est partagé par les quatre ventilateurs et un seul timer/pin courant sert deux sorties. — impact : deux fréquences indépendantes impossibles.
- [critique] : La période simulée est multipliée par le ratio et employée comme demi-période. — impact : fréquence inversée et divisée par deux.
- [critique] : Le timer calculé n'est jamais configuré avec un alarm/reload valide. — impact : aucune waveform exploitable.

### Hardware
- [critique] : Quatre tach asynchrones sont mis en parallèle et tirés vers 12 V avant un diviseur. — impact : fronts fusionnés et niveaux peu fiables.
- [critique] : Le buck 3,3 V est relié à la broche 5V du Wemos. — impact : LDO sous-alimenté et brownout.

### Qualité de code générale (négatif)
- [majeur] : Duplication, API de mauvaise plateforme, un seul chemin de signal pour deux canaux et documentation contradictoire. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 0,3/10 — Code non compilable et architecture incapable de produire deux tach

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 2,0/10 — Très volumineux pour une logique non viable

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
