# Analyse — composer 2.5

**Sources analysées** :
- `competitors/composer 2.5/deye_fan/deye_fan.ino`
- `competitors/composer 2.5/deye_fan/schema_electronique.md`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Architecture envisagée
- Deux canaux, Timer1, sorties par registres et timeout d'entrée sont prévus.
- Le code utilise une structure par canal, une CRC16 et une interface de configuration.

### Protocole tach
- La formule 2 PPR et la direction du ratio sont correctes en intention.

### Qualité de code générale (positif)
- **Structure / organisation** : Le découpage des responsabilités et le modèle de canal rendent l'intention compréhensible.
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
- [critique] : Le littéral `0xDEYE` contient la lettre Y et n'est pas un entier hexadécimal C++. — impact : le sketch ne compile pas.

### Génération sortie
- [critique] : Avec TIM_DIV16, `timer1_write(10)` programme environ 2 µs et non 10 µs. — impact : ISR proche de 500 kHz, charge CPU extrême et fréquence fausse.
- [majeur] : Le calcul flottant de ratio est exécuté dans l'ISR GPIO. — impact : helper/libgcc ou accès flash possible en contexte interruption.

### Démarrage / Mesure
- [majeur] : Le premier front est mesuré depuis zéro. — impact : fausse période et sortie prématurée.

### Hardware
- [critique] : Le schéma relie le bus après Schottky, encore proche de 12 V, directement à la broche 5V du Wemos. — impact : destruction probable de la carte.
- [majeur] : Le texte affirme qu'un AMS1117 embarqué accepte ce branchement. — impact : justification électrique incorrecte.

### Qualité de code générale (négatif)
- [majeur] : Aucune compilation ni revue des unités timer/alimentation; les commentaires rassurants contredisent le circuit réel. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 1,2/10 — Non compilable et alimentation dangereuse

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 4,0/10 — Présentation correcte, validation technique insuffisante

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
