# Analyse — claude-sonnet5-free

**Sources analysées** :
- `competitors/claude-sonnet5-free/deye_fan.ino`
- `competitors/claude-sonnet5-free/schema_electronique.md`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Temps-réel / Génération
- Timer1 périodique à 10 kHz, GPIO directs et deux compteurs de demi-période indépendants.
- Le calcul 2 PPR et la multiplication de fréquence sont cohérents en régime établi.

### WiFi / Persistance
- AP+STA, WIFI_NONE_SLEEP, serveur standard et checksum sur la configuration.
- HTML PROGMEM et statut JSON construit dans un tampon fixe.

### Documentation hardware/code
- Buck, OR-ing Schottky, sorties NPN avec pull-down de base et brochage sont décrits.
- Le code sépare capture, sortie, stockage et réseau.

### Qualité de code générale (positif)
- **Structure / organisation** : Noms clairs, structures par canal et calculs faciles à suivre.
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

### Démarrage / Reprise
- [critique] : Le premier front est accepté comme une période calculée depuis zéro, sans état « non amorcé ». — impact : émission immédiate d'un faux RPM au boot.
- [critique] : Après stall, le premier front très long est de nouveau accepté faute de borne maximale. — impact : redémarrage à une fréquence aberrante avant deux fronts réels.

### Hardware
- [critique] : L'étage d'entrée NPN documenté n'ajoute pas de pull-up au tach open-collector et suppose à tort un repos à 12 V fourni par le Noctua. — impact : entrée flottante ou muette sur un ventilateur conforme.
- [majeur] : Le document mélange tach Noctua open-collector et prétendue tension interne 12 V. — impact : risque de câblage incorrect et de diagnostic trompeur.

### Temps-réel / Filtres
- [majeur] : Timer1 reste une IRQ niveau 1 et non une NMI. — impact : jitter possible sous WiFi/flash malgré les formulations d'isolation.
- [mineur] : Le compteur bascule après half+1 ticks. — impact : petite erreur systématique de période, surtout aux hautes fréquences.

### Sécurité / Tests
- [majeur] : Aucune authentification HTTP et aucun test/build reproductible. — impact : configuration modifiable par le LAN et défauts non détectés.

### Qualité de code générale (négatif)
- [majeur] : La machine d'état d'entrée est absente; documentation et électronique reposent sur une propriété fausse du tach Noctua. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 4,8/10 — Firmware plausible, mais reprise et entrée matérielle insuffisantes

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 6,2/10 — Lisible, avec plusieurs hypothèses non vérifiées

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
