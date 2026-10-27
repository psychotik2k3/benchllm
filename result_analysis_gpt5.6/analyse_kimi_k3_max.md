# Analyse — kimi_k3_max

**Sources analysées** :
- `competitors/kimi_k3_max/deye_fan.ino`
- `competitors/kimi_k3_max/schema_electronique.md`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Temps-réel / Génération
- Timer1 one-shot avec échéances absolues, deux sorties, GPIO directs et sections critiques courtes.
- Filtrage EMA, ratios bornés, EEPROM CRC16 et serveur AP+STA.

### Hardware
- OR-ing Schottky, buck, pull-ups 3,3 V, NPN open-collector et pull-downs sont correctement documentés.

### Interface / Code
- UI claire, structures de canal et fonctions communes; formules 2 PPR cohérentes.

### Qualité de code générale (positif)
- **Structure / organisation** : Tableaux par canal, calculs clairs, CRC et documentation matérielle de bonne qualité.
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
- [critique] : `captureEdge` accepte le tout premier front car il ne distingue pas l'absence de référence. — impact : sortie fabriquée depuis le temps écoulé depuis le boot.
- [critique] : Aucune borne maximale de période n'est appliquée après un stall. — impact : le premier front de reprise devient une période énorme, redémarre à la fréquence minimale puis pollue longtemps l'EMA.

### Temps-réel / Fail-safe
- [majeur] : Le stall est traité dans loop toutes les 100 ms et non dans le moteur Timer1. — impact : retard dépendant de la charge web.
- [majeur] : L'IRQ Timer1 standard n'établit pas l'isolation NMI annoncée. — impact : jitter sous WiFi/flash possible.

### Sécurité / Tests
- [majeur] : Interface sans authentification et aucun test livré. — impact : configuration exposée et transitions non vérifiées.

### Qualité de code générale (négatif)
- [majeur] : Machine d'état input trop implicite; le filtre ne se réamorce pas proprement à partir de deux fronts. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 6,1/10 — Architecture solide en régime, reprise après stall incorrecte

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 7,1/10 — Bien structuré mais fail-safe incomplet

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
