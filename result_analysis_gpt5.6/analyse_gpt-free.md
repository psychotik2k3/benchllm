# Analyse — gpt-free

**Sources analysées** :
- `competitors/gpt-free/gpt_deye_fan.ino`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Temps-réel / Génération
- Deux captures CCOUNT courtes, IIR 1/8 et deux waveforms continues via le core ESP8266.
- Traitement factorisé par canal, premier front correctement ignoré et timeout à 500 ms.

### WiFi / Interface
- AP+STA, page sombre, statut AJAX et ratios appliqués immédiatement.

### Persistance
- Magie EEPROM, défauts explicites et bornage des ratios.

### Qualité de code générale (positif)
- **Structure / organisation** : Factorisation des canaux, fonctions de conversion lisibles et commentaire architectural utile.
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

### Hardware / Livraison
- [critique] : Le commentaire de câblage montre un diviseur vers la masse mais aucun pull-up alimentant le tach open-collector. — impact : entrée sans niveau haut défini, donc acquisition non fonctionnelle.
- [majeur] : Aucun fichier de schéma/BOM séparé n'est livré. — impact : installation électrique non vérifiable.

### Temps-réel / Portabilité
- [majeur] : Les seuils de cycles sont commentés pour 160 MHz alors que la fréquence CPU n'est pas imposée. — impact : bornes de période divisées par deux à 80 MHz.
- [majeur] : L'API waveform privée et sa signature ne sont pas validées. — impact : risque de compilation selon core.

### Persistance / Sécurité
- [majeur] : La configuration n'a ni version, ni taille, ni CRC. — impact : corruption flash non détectée.
- [critique] : Les mots de passe STA et AP sont renvoyés en clair par `/api/config` sans échappement ni authentification. — impact : fuite complète des secrets.

### Qualité de code générale (négatif)
- [majeur] : Plus de 1 500 lignes dans un fichier, très gros HTML dynamique, aucune suite de tests et contrat hardware incomplet. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 4,6/10 — Firmware raisonnable, montage d'entrée incomplet et secrets exposés

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 6,2/10 — Clair mais surdimensionné

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
