# Analyse — gemini 3.1 pro

**Sources analysées** :
- `competitors/gemini 3.1 pro/deye_fan.ino`
- `competitors/gemini 3.1 pro/schema_et_explications.md`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Architecture envisagée
- Deux canaux, Timer1 avant le WiFi, GPIO directs et page AJAX sont présents.
- Le calcul 2 PPR est compris et les sorties collecteur ouvert sont identifiées.

### Réseau
- AP+STA non bloquant et interface de réglage des ratios.

### Qualité de code générale (positif)
- **Structure / organisation** : Sections et commentaires rendent le flux général accessible.
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
- [critique] : `0xDEYE0001` n'est pas un littéral hexadécimal valide. — impact : le firmware ne compile pas.

### Mesure / Filtres
- [critique] : Les impulsions sont comptées sur une cadence supposée d'une seconde sans utiliser le temps réellement écoulé. — impact : tout retard de loop fausse directement les RPM.
- [majeur] : Aucun vrai filtre de période ni état d'amorçage/reprise. — impact : bruit et premier échantillon mal maîtrisés.

### Temps-réel
- [majeur] : Timer1 interrompt à 20 kHz en continu. — impact : charge CPU et quantification inutiles face à un ordonnanceur one-shot.

### Sécurité / Code
- [critique] : Des `strcpy` non bornés copient SSID et mots de passe. — impact : dépassement de tampon possible.
- [majeur] : Les mots de passe sont exposés par l'interface et aucune authentification n'existe. — impact : fuite et modification distante.

### Hardware
- [majeur] : Le document propose une 1N4148 pour l'OR-ing de puissance et un pseudo-diode NPN B-C sans qualification. — impact : échauffement, polarité ou isolation incorrects.

### Qualité de code générale (négatif)
- [majeur] : L'absence de compilation, de bornes mémoire et de mesure temporelle réelle domine la qualité. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 2,3/10 — Ne compile pas et mesure les RPM de façon trop grossière

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 3,8/10 — Intention lisible, robustesse faible

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
