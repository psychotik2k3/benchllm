# Analyse — gemini-free

**Sources analysées** :
- `competitors/gemini-free/gemini-code-1787264142602.cpp`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Architecture envisagée
- Deux canaux, Timer1, serveur asynchrone, LittleFS JSON et HTML PROGMEM.
- La formule conceptuelle 2 PPR et la demi-période sont identifiées.

### Interface
- Page compacte avec polling et ratios configurables.

### Qualité de code générale (positif)
- **Structure / organisation** : Code relativement court et responsabilités identifiables.
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

### Génération sortie
- [critique] : Timer1 en DIV16 vaut 5 ticks/µs, mais le code recharge avec `80 * timerStepUs`. — impact : intervalle environ 16 fois trop long et RPM simulés environ 16 fois trop faibles.
- [majeur] : `digitalWrite()` est utilisé dans l'ISR. — impact : latence et sécurité IRAM non garanties.

### Démarrage / Mesure
- [critique] : Le premier front est traité comme période depuis le boot. — impact : fausse sortie initiale.
- [majeur] : LittleFS et WiFi sont initialisés avant le moteur tach. — impact : longue fenêtre sans sortie, voire formatage bloquant.

### Filtres / Hardware
- [majeur] : Aucune moyenne/validation robuste et aucune documentation électrique n'est fournie. — impact : bruit non maîtrisé et montage impossible à auditer.

### Sécurité
- [majeur] : Routes ouvertes et validation faible. — impact : configuration distante non protégée.

### Qualité de code générale (négatif)
- [majeur] : Aucune vérification des unités Timer1, de l'amorçage ou du hardware; dépendances externes non figées. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 2,5/10 — Erreur de base de temps bloquante et aucun schéma

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 4,5/10 — Compact mais non vérifié

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
