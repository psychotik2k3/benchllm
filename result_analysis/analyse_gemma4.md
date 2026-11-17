# Analyse — gemma4

**Sources analysées** : [firmware_main.cpp](<../competitors/gemma4/firmware_main.cpp>) ; [README.md](<../competitors/gemma4/README.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Démarrage / Boot readiness

Le texte prévoit AP+STA et des Ticker, mais aucune sortie tach valide ne peut être produite en l’état. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Interface Web

Page de visualisation avec fetch /data ; le polling ne remplace que les mesures.

### Documentation hardware/code

README mentionne un tampon NPN, mais son implémentation et les promesses réseau ne concordent pas avec le code.

### Qualité de code générale (positif)

- **Structure / organisation** : Quelques petites fonctions nommées ; la chaîne HTML corrompue rend leur portée incertaine.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : Aucune validation ou stratégie d’erreur utilisable.
- **Duplication / DRY** : Canaux copiés séparément et fusionnés en une seule valeur.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Quelques petites fonctions nommées ; la chaîne HTML corrompue rend leur portée incertaine.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Temps-réel / ISR

- [critique] Les entrées sont GPIO10/11 (23–24), utilisées par la flash sur le module ESP-12S ; elles ne correspondent pas aux broches D1/D2 annoncées. Les ISR n’ont pas IRAM_ATTR.

### Fidélité au protocole tachymétrique / Fail-safe

- [critique] processData multiplie des compteurs cumulés jamais remis à zéro (45–48), sans durée ni 2 PPR. Une rotation constante produit une valeur croissante ; après arrêt elle ne revient pas à zéro.

### Génération sortie / Timer1

- [critique] PIN_OUTPUT est seulement configuré en sortie ; aucun générateur ni toggle n’est appelé. Un unique résultat moyen remplace deux canaux : zéro train tach envoyé à l’onduleur.

### Interface Web

- [majeur] Les champs ratio ne sont reliés à aucun handler de modification ; aucun paramètre WiFi STA ou stockage persistant implémenté malgré le README.

### Qualité de code générale (négatif)

- [critique] Le raw string commence par R"=====( (53) et se termine avec six signes = au lieu de cinq (92) ; PIN_TACH_6m (113) ne correspond pas à PIN_TACH_6cm (24), et le fichier finit par un résidu Markdown (135). Erreurs de syntaxe/identifiant bloquantes.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 0,5/10 — Livraison inutilisable pour le projet

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 1,3/10 — Qualité insuffisante

Quelques petites fonctions nommées ; la chaîne HTML corrompue rend leur portée incertaine. Aucune validation ou stratégie d’erreur utilisable.
