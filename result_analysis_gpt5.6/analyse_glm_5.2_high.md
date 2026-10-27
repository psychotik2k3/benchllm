# Analyse — glm_5.2_high

**Sources analysées** :
- `competitors/glm_5.2_high/deye_fan.ino`
- `competitors/glm_5.2_high/schema_electronique.md`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Architecture envisagée
- Deux `startWaveform()` indépendants, timeout et moyenne de quatre périodes.
- LittleFS, AP+STA et traitement factorisé par canal.

### Hardware
- Schéma avec OR-ing Schottky, buck, pull-ups 3,3 V et collecteurs ouverts.

### Qualité de code générale (positif)
- **Structure / organisation** : Fonctions par canal, noms clairs et documentation détaillée.
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

### Compilation
- [critique] : Trois concaténations HTML contiennent une parenthèse fermante surnuméraire. — impact : le sketch ne compile pas.

### Mesure
- [critique] : `deltaCycles * 1000000UL` déborde en 32 bits pour pratiquement toute période tach réaliste. — impact : périodes arbitraires, généralement rejetées; aucune sortie fiable.
- [majeur] : Le test `c1 > c0` rejette le wrap CCOUNT. — impact : perte périodique d'une mesure environ toutes les 53,7 s à 80 MHz.
- [majeur] : Les deux tach d'une paire sont recommandés en parallèle. — impact : fronts asynchrones fusionnés, RPM non représentatif.

### Fail-safe / Sécurité
- [majeur] : Les timestamps sont rafraîchis avant validation complète et le filtre ancien n'est pas explicitement invalidé. — impact : parasites susceptibles de prolonger le signal.
- [majeur] : Mots de passe réaffichés et aucune authentification. — impact : fuite et modification distante.

### Hardware
- [majeur] : Le document affirme qu'un buck LM2596 régule 5 V depuis une entrée à 5 V. — impact : absence de headroom et brownout à basse tension.

### Qualité de code générale (négatif)
- [majeur] : Aucun build ni test arithmétique élémentaire; erreurs syntaxiques et overflow auraient été immédiatement détectés. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 2,0/10 — Non compilable et mesure de période cassée

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 4,0/10 — Bien présenté mais non testé

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
