# Analyse — GPT-5.6_Sol_xhigh

**Sources analysées** :
- `competitors/GPT-5.6_Sol_xhigh/DESIGN.md`
- `competitors/GPT-5.6_Sol_xhigh/IMPLEMENTATION_PLAN.md`
- `competitors/GPT-5.6_Sol_xhigh/SCHEMA_ET_MONTAGE.md`
- `competitors/GPT-5.6_Sol_xhigh/deye_fan.ino`
- `competitors/GPT-5.6_Sol_xhigh/tach_math.h`
- `competitors/GPT-5.6_Sol_xhigh/test_tach_math.cpp`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Temps-réel / Génération
- Capture CCOUNT sur deux ISR et génération via le moteur `startWaveform()` du core, isolée de loop.
- Premier front utilisé uniquement comme référence et calculs de ratio/2 PPR cohérents.

### Persistance / Robustesse
- Configuration magie/version/taille/CRC32 avec `offsetof`, parsing strict et échappement JSON.
- AP+STA, WIFI_NONE_SLEEP et capture démarrée avant le WiFi.

### Tests / Documentation
- `tach_math.h` extrait les calculs purs et `test_tach_math.cpp` couvre conversions et bornes.
- Design, plan d'implémentation et montage avec Schottky, TVS, PTC et option buck-boost sont détaillés.

### Interface
- Statut temps réel, configuration persistante et secrets non réaffichés.

### Qualité de code générale (positif)
- **Structure / organisation** : Séparation en module mathématique, documents de conception, validation stricte et tests natifs constituent la meilleure base d'ingénierie.
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

### Démarrage / Réactivité
- [majeur] : La waveform ne démarre qu'après plusieurs mesures et une mise à jour de loop toutes les 200 ms. — impact : fenêtre de 0 RPM supérieure à celle des meilleurs NMI.
- [majeur] : Le timeout d'entrée est de 2 s. — impact : un ventilateur calé peut rester annoncé en rotation pendant deux secondes.

### Temps-réel / Portabilité
- [majeur] : `startWaveform()`/`startWaveformClockCycles()` est une API du core dont la compatibilité cible n'est pas démontrée par un build livré. — impact : risque de signature/version.
- [mineur] : Les tests ne couvrent ni ISR, ni moteur waveform, ni reprise après stall. — impact : la partie critique reste non vérifiée.

### Sécurité / Fonctionnel
- [majeur] : Aucune authentification HTTP. — impact : configuration modifiable par tout client.
- [majeur] : Un seul tach est lu par paire. — impact : la seconde panne ventilateur est masquée.

### Qualité de code générale (négatif)
- [majeur] : Couverture de test limitée aux formules; dépendance au core waveform et grands intervalles de loop/timeout. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 8,0/10 — Très bonne base officielle, vérification temps réel encore partielle

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 8,4/10 — Meilleure modularité et testabilité du lot

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
