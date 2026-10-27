# Analyse — composer 2.5 prompt from claude

**Sources analysées** :
- `competitors/composer 2.5 prompt from claude/deye_fan.ino`
- `competitors/composer 2.5 prompt from claude/schema_electronique.md`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Architecture / Génération
- Deux entrées ISR, Timer1 périodique, ratios par canal et sortie initialisée avant le réseau.
- La direction du ratio, les 2 PPR et le calcul de demi-période sont corrects.

### WiFi / Stockage / UI
- Serveur asynchrone, LittleFS JSON, HTML PROGMEM et interface de statut compacte.
- AP+STA non bloquant et valeurs par défaut explicites.

### Documentation hardware/code
- Schéma cohérent avec Schottky, buck, pull-ups 3,3 V et collecteurs ouverts BC547.

### Qualité de code générale (positif)
- **Structure / organisation** : Sections cohérentes, structures simples et schéma exploitable.
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

### Temps-réel / ISR
- [majeur] : `micros()` et `digitalWrite()` sont appelés dans l'ISR Timer1 à 20 kHz. — impact : latence et dépendances non IRAM possibles, jitter accru.
- [majeur] : La génération recale chaque front sur `now + half`. — impact : la latence ISR devient une dérive de fréquence cumulative.

### Fail-safe / Reprise
- [critique] : L'ISR met `lastEdgeUs` à jour avant de rejeter une période invalide. — impact : des parasites peuvent maintenir une ancienne période en vie.
- [critique] : Après timeout, le filtre et la période brute restent mémorisés; le premier front de reprise rafraîchit l'âge. — impact : l'ancienne fréquence redémarre avant deux fronts valides.

### Filtres / Robustesse
- [majeur] : La même valeur brute est réinsérée et le slew est appliqué à chaque tour de loop. — impact : dynamique dépendante de la charge réseau plutôt que des nouvelles mesures.
- [majeur] : Plusieurs `strncpy` peuvent laisser les chaînes sans NUL. — impact : lecture hors limites et corruption de JSON/configuration.

### Sécurité
- [majeur] : Routes de configuration sans authentification ni CSRF. — impact : modification distante des ratios et du WiFi.

### Qualité de code générale (négatif)
- [majeur] : Le code confond cadence de loop et cadence de mesure; la machine d'état stall/reprise est incomplète. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 5,2/10 — Bonne ossature, fail-safe et reprise à corriger

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 6,3/10 — Structuré mais insuffisamment déterministe

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
