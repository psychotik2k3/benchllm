# Analyse — claude opus 5 medium

**Sources analysées** :
- `competitors/claude opus 5 medium/deye_fan/deye_fan.ino`
- `competitors/claude opus 5 medium/SCHEMA.md`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Temps-réel / ISR
- Moteur Timer1 réellement routé en NMI, entièrement pensé pour IRAM/DRAM, sans flottant ni allocation.
- Échantillonnage d'entrée et sorties utilisent CCOUNT, des échéances absolues et un seqlock NMI→loop.

### Démarrage / Boot readiness
- Sorties relâchées avant leur passage en OUTPUT et moteur lancé avant le WiFi.
- Le premier front amorce la mesure; un second front cohérent est nécessaire avant émission.

### Génération sortie / Timer1
- Ordonnanceur one-shot sur la plus proche échéance, sans dérive cumulative.
- Les changements de période sont adoptés aux frontières de front et les retards pathologiques sont bornés.

### Fidélité / Filtres / Fail-safe
- Deux PPR, bornes de période réalistes, anti-rebond sur trois échantillons et EMA entière.
- Un intervalle invalide remet le filtre à zéro; la reprise après stall exige de nouveau deux fronts.

### WiFi / Persistance / Interface
- AP+STA, WIFI_NONE_SLEEP, mDNS, reconnexion non bloquante, interface AJAX et diagnostics NMI.
- Configuration versionnée, CRC32, validation défensive et application des ratios sans casser la phase.

### Documentation hardware/code
- OR-ing Schottky, buck, entrées 3,3 V, sorties BC337/2N2222 avec pull-down et procédures de mesure sont correctement dimensionnés.
- La documentation distingue explicitement le tach d'un seul ventilateur par paire.

### Qualité de code générale (positif)
- **Structure / organisation** : Découpage logique clair, types fixes, ownership inter-contextes explicite, arithmétique wrap-safe et validation robuste.
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

### Démarrage / Boot readiness
- [majeur] : Aucun tach provisoire n'est émis avant l'acquisition validée. — impact : le Deye voit 0 RPM pendant le boot, la montée en vitesse et deux fronts tach.

### Temps-réel / Portabilité
- [majeur] : Le moteur dépend de macros, registres et conventions NMI internes au core ESP8266. — impact : une mise à jour de core peut casser compilation ou acquittement Timer1.
- [mineur] : Le placement IRAM transitif n'est pas prouvé par un map file livré. — impact : la garantie pendant les écritures flash reste à confirmer.

### WiFi / Sécurité
- [majeur] : L'API de configuration n'est pas authentifiée et l'état JSON renvoie le mot de passe AP. — impact : un client AP/LAN peut lire les identifiants ou modifier les ratios.

### Fidélité / Vérification
- [majeur] : Un seul rotor est surveillé dans chaque paire de deux ventilateurs. — impact : la panne du second Noctua est invisible.
- [mineur] : Aucun test automatisé, build reproductible ou mesure oscilloscope n'est livré. — impact : les garanties NMI et jitter restent statiques.

### Qualité de code générale (négatif)
- [majeur] : Le sketch reste monolithique et fortement couplé aux API privées du core; l'absence de tests réduit la maintenabilité. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 8,8/10 — Meilleure proposition globale, à valider sur la toolchain cible

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 9,0/10 — Invariants temps réel remarquablement maîtrisés

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
