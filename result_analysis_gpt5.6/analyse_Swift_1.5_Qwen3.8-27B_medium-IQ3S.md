# Analyse — Swift_1.5_Qwen3.8-27B_medium-IQ3S

**Sources analysées** :
- `competitors/Swift_1.5_Qwen3.8-27B_medium-IQ3S/deye_fan.ino`
- `competitors/Swift_1.5_Qwen3.8-27B_medium-IQ3S/schema_electronique.txt`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Intentions
- Deux canaux, EMA, bornes de période, AP+STA et endpoints séparés.
- OR-ing et collecteur ouvert sont au moins identifiés comme besoins.

### Qualité de code générale (positif)
- **Structure / organisation** : Nommage relativement clair et séparation des deux canaux.
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
- [critique] : Le code utilise `ESP8266HTTPServer.h`, `Preferences`, `ESP8266TimerInterrupt` et `WiFi.connect`, API inexistantes ou incompatibles. — impact : pas de compilation sur la cible.
- [critique] : Les fonctions ISR ne sont pas placées en IRAM et les champs partagés sont insuffisamment `volatile`. — impact : comportement indéfini sous interruption.

### Génération / Fail-safe
- [critique] : La configuration Timer0 est invalide et non réarmée. — impact : pas de signal continu.
- [critique] : L'état pulse/période produit une fréquence erronée et la période stale maintient `ok` vrai. — impact : sortie trop lente et stall jamais détecté.

### Hardware
- [critique] : AP2112-3.3 est alimenté depuis 12 V alors que sa tension d'entrée maximale est très inférieure. — impact : destruction du régulateur.
- [critique] : Pull-downs et diodes côté sorties déforment la ligne tach inconnue. — impact : niveau Deye non compatible 3,3/5/12 V.

### Qualité de code générale (négatif)
- [majeur] : La contribution mélange plusieurs plateformes et bibliothèques sans vérification de compilation ni d'électronique. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 0,6/10 — Incompatible ESP8266 et matériel dangereux

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 2,5/10 — Structure apparente, fondations incorrectes

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
