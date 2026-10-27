# Analyse — qwen3.6

**Sources analysées** :
- `competitors/qwen3.6/deye_fan.ino`
- `competitors/qwen3.6/SCHÉMA_CIRCUIT.md`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Intentions
- Deux canaux, interface Web, EEPROM et collecteurs ouverts sont envisagés.
- La formule d'affichage RPM à 2 PPR est connue.

### Qualité de code générale (positif)
- **Structure / organisation** : Commentaires abondants et sections identifiables.
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
- [critique] : Le code emploie `hw_timer_t`, `timerBegin`, `timerRead`, `timerWrite` et autres API ESP32 sur ESP8266. — impact : le sketch ne compile pas pour la cible.
- [critique] : Une accolade surnuméraire suit `eepromLoadRatios`. — impact : erreur syntaxique supplémentaire.

### Mathématiques / Génération
- [critique] : La période est multipliée par le ratio puis utilisée comme demi-période. — impact : la fréquence est approximativement divisée par 2×ratio au lieu d'être multipliée.
- [critique] : GPIO15/D8 est utilisé comme entrée avec un circuit susceptible d'altérer le strap boot. — impact : boot impossible selon niveau tach.

### Démarrage / Hardware
- [majeur] : Le WiFi peut bloquer 15 s avant l'installation des ISR et du timer. — impact : aucun tach pendant l'association.
- [critique] : Le schéma alimente directement le Wemos depuis 12 V/VIN en s'appuyant sur un AMS1117. — impact : surchauffe ou destruction.

### Qualité de code générale (négatif)
- [majeur] : Le volume masque des incohérences fondamentales de plateforme, de formule et de matériel qu'une compilation aurait révélées. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 0,5/10 — Mauvaise plateforme, syntaxe et mathématiques inversées

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 2,5/10 — Long et explicatif, mais techniquement non viable

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
