# Analyse — muse_wonder_1.0_medium

**Sources analysées** :
- `competitors/muse_wonder_1.0_medium/deye_fan.ino`
- `competitors/muse_wonder_1.0_medium/schema_electronique.md`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Temps-réel / Génération
- Anneaux de timestamps ISR, traitement hors ISR et sortie NMI one-shot par échéances absolues.
- GPIO directs, buffers fixes pour le statut et deux canaux indépendants.

### Filtres / Interface
- Validation de période, moyenne, IIR, slew et interface qui évite d'écraser les champs en édition.
- AP+STA, WIFI_NONE_SLEEP et pages en PROGMEM.

### Hardware
- Schéma globalement bon: Schottky, buck, PTC/TVS, pull-ups 3,3 V et open-collectors.

### Qualité de code générale (positif)
- **Structure / organisation** : Structures par canal, statuts bornés et commentaires du moteur NMI sont de bon niveau.
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

### Persistance
- [critique] : `sizeof(cfg)-sizeof(cfg.crc)` vaut 214 alors que `crc` commence à l'offset 212; les deux octets CRC sont inclus à cause du padding final. — impact : un enregistrement valide ne repasse généralement pas la vérification au reboot.

### Temps-réel / Filtres
- [majeur] : L'anneau écrase silencieusement des entrées non consommées sans indicateur full/overflow. — impact : mesures perdues sous blocage prolongé de loop.
- [majeur] : Le slew de 2 % est appliqué à chaque tour de loop, pas à chaque nouvelle mesure. — impact : vitesse de convergence dépendante de la charge WiFi.
- [majeur] : Le timeout de 1,2 s est décidé dans loop alors que la sortie est en NMI. — impact : ancienne fréquence prolongée si loop est retardée.
- [mineur] : La médiane annoncée dans l'en-tête n'est pas implémentée. — impact : rejet d'outliers inférieur à la documentation.

### Hardware / Sécurité
- [majeur] : Le MP1584 est présenté comme maintenant 5 V près de 5 V d'entrée. — impact : brownout sans headroom.
- [majeur] : Aucune authentification/CSRF. — impact : configuration exposée.

### Qualité de code générale (négatif)
- [majeur] : Le calcul CRC aurait dû utiliser `offsetof`; absence de tests structure-layout, anneau et reprise. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 6,6/10 — Très bon temps réel, persistance cassée et dynamiques dépendantes de loop

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 7,7/10 — Soigné, avec plusieurs invariants manqués

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
