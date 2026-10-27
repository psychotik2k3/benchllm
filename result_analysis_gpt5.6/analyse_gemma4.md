# Analyse — gemma4

**Sources analysées** :
- `competitors/gemma4/firmware_main.cpp`
- `competitors/gemma4/README.md`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Éléments présents
- AP protégé et page de statut simples.
- Quelques handlers HTTP sont séparés.

### Qualité de code générale (positif)
- **Structure / organisation** : La petite taille facilite l'identification de ce qui existe.
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
- [critique] : Le fichier contient un identifiant de broche erroné et se termine avec une clôture Markdown. — impact : le source ne compile pas.

### Fonction centrale
- [critique] : Aucun générateur de waveform tach n'est implémenté. — impact : l'onduleur ne reçoit aucun signal simulé.
- [critique] : Un seul pin de sortie est prévu pour deux canaux. — impact : deux fréquences indépendantes impossibles.
- [critique] : Les compteurs de fronts ne sont jamais remis à zéro ni rapportés à une durée. — impact : les valeurs affichées ne sont pas des RPM.

### Plateforme / Hardware
- [critique] : GPIO10/11 sont utilisés comme D1/D2 alors qu'ils servent à la flash sur ESP8266. — impact : plantage ou impossibilité de boot.
- [majeur] : Aucune persistance ni schéma électrique exploitable. — impact : besoin fonctionnel incomplet.

### Qualité de code générale (négatif)
- [majeur] : Il ne s'agit pas d'une implémentation du cahier des charges; commentaires et README revendiquent des fonctions absentes. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 0,2/10 — Ébauche non compilable sans simulateur tach

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 1,2/10 — Code incomplet et incohérent

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
