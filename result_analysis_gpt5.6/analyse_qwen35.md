# Analyse — qwen35

**Sources analysées** :
- `competitors/qwen35/firmware_main.cpp`
- `competitors/qwen35/README.md`
- `competitors/qwen35/schematic.md`
- `competitors/qwen35/calibration_guide.md`
- `competitors/qwen35/SUMMARY.md`
- `competitors/qwen35/MEMORY.md`
- `competitors/qwen35/INDEX.md`
- `competitors/qwen35/memory/anti_rebound_gpio_optimization.md`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Éléments documentaires
- Interface responsive, guide de calibration et intention de moyenne glissante.
- AP protégé par mot de passe.

### Qualité de code générale (positif)
- **Structure / organisation** : La documentation est abondante et visuellement structurée.
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
- [critique] : `samples9[`]` est une erreur syntaxique; `GPOS(pin,state)` et `WiFi.transmitting()` n'existent pas; SPIFFS/File ne sont pas déclarés. — impact : le code ne compile pas.
- [critique] : Deux fonctions `loop()` sont définies. — impact : redéfinition bloquante.

### Fonction centrale
- [critique] : Aucun Timer1 ne génère une fréquence liée aux RPM; la seule sortie bascule toutes les 500 ms. — impact : tach fixe d'environ 1 Hz, sans relation au ventilateur.
- [critique] : Les compteurs cumulés sont directement présentés comme RPM et jamais remis à zéro sur une fenêtre. — impact : valeurs croissantes sans signification.
- [critique] : Une seule sortie est prévue pour les deux canaux. — impact : besoin fondamental non couvert.

### Plateforme / Hardware
- [critique] : GPIO10/11, présentés comme D1/D2, sont des broches flash. — impact : ESP8266 instable ou non bootable.
- [critique] : SS8550 est un PNP, contrairement au texte, et le schéma n'est pas un circuit électrique cohérent. — impact : étage de sortie inexploitable.

### Qualité de code générale (négatif)
- [majeur] : Elle ne correspond pas au code; nombreuses API inventées, composants mal identifiés et ajout Home Assistant hors sujet. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 0,1/10 — Aucun firmware compilable ni simulateur tach réel

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 1,0/10 — Artefact généré incohérent

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
