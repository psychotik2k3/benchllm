# Analyse — grok 4.6 medium

**Sources analysées** :
- `competitors/grok 4.6 medium/deye_fan/deye_fan.ino`
- `competitors/grok 4.6 medium/SCHEMA_ELECTRONIQUE.md`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Temps-réel / Génération
- NMI périodique à 20 µs, accès GPIO direct et calcul de demi-période entier correct.
- Deux canaux indépendants et moteur démarré avant le WiFi.

### Persistance / Réseau
- EEPROM avec CRC32, AP+STA, formulaire de ratios et validation de plage.

### Hardware
- Un seul tach par paire, OR-ing Schottky, buck et sorties NPN avec pull-down sont cohérents.

### Qualité de code générale (positif)
- **Structure / organisation** : Code court, structures par canal, formules bien expliquées et CRC via `offsetof`.
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

### Fail-safe / Reprise
- [critique] : Le timeout désactive seulement `nmiEnabled`; `havePeriod` et `periodCycles` restent valides. — impact : ancienne consigne conservée.
- [critique] : Le premier front de reprise actualise `lastEdgeUs` avant validation et loop réactive immédiatement la période ancienne. — impact : fausse rotation annoncée avant un intervalle réel.

### Mesure / ISR
- [majeur] : Un glitch actualise `lastCycle`, `lastEdgeUs` et le compteur avant contrôle de période. — impact : bruit pouvant prolonger la présence et casser la période suivante.
- [majeur] : Appel de `micros()` depuis l'ISR et NMI fixe à 50 kHz. — impact : charge permanente et sécurité IRAM à confirmer.

### Sécurité / Documentation
- [majeur] : JSON SSID non échappé et interface sans authentification. — impact : JSON cassable et configuration distante.
- [mineur] : Aucun test ni build reproductible. — impact : défaut de reprise non détecté.

### Qualité de code générale (négatif)
- [majeur] : Pas d'état `unprimed/stalled/repriming`; plusieurs variables sont rafraîchies avant validation. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 5,8/10 — Bon moteur NMI, reprise stale et mesure à durcir

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 7,0/10 — Compact et lisible, machine d'état incomplète

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
