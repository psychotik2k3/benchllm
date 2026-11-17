# Analyse — glm_5.2_high

**Sources analysées** : [deye_fan.ino](<../competitors/glm_5.2_high/deye_fan.ino>) ; [schema_electronique.md](<../competitors/glm_5.2_high/schema_electronique.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

Capture des cycles dans des ISR IRAM et instantané sous section critique.

### Démarrage / Boot readiness

GPIO au repos ; LittleFS, WiFi et serveur précèdent l’attachement des ISR (551–568). Pas de vitesse de secours et pas d’attente STA explicite ; en l’état, les mesures sont rejetées par le calcul de période. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Génération sortie / Timer1

Réutilisation du moteur waveform du cœur ; état appliqué distinct de la consigne.

### Fidélité au protocole tachymétrique / Fail-safe

Formule PPR=2 et timeout logiciel 1,5 s ; sortie relâchée à l’arrêt.

### Filtres / Signal

Moyenne de quatre périodes non nulles et seuil de mise à jour pour limiter les reprogrammations.

### WiFi / Réseau

AP+STA et paramètres configurables depuis HTTP.

### Interface Web

Polling des cellules du tableau sans réécrire les champs du formulaire.

### Persistance / Stockage

LittleFS, taille de structure et signature vérifiées ; ratios assainis.

### Documentation hardware/code

BOM, buck et sorties NPN détaillés.

### Qualité de code générale (positif)

- **Structure / organisation** : CanalMap, Config et CanalState séparés ; fonctions regroupées par responsabilité.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : Taille/magic du fichier vérifiés, mais erreurs d’unités et chaînes chargées non terminées explicitement.
- **Duplication / DRY** : Boucles et table de brochage communes aux canaux.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : CanalMap, Config et CanalState séparés ; fonctions regroupées par responsabilité.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Fidélité au protocole tachymétrique / Fail-safe

- [critique] `deltaCycles * 1000000UL` (270) déborde en 32 bits avant division par 80 000 000. Le quotient est toujours ≤53 µs, alors que le filtre exige ≥1 000 µs (271) : aucune période ne peut être acceptée. Par exemple 1 500 000 cycles (1 600 RPM) deviennent 13 µs.

### Documentation hardware/code

- [majeur] Le schéma recommande les tach de deux ventilateurs en parallèle (114–119) : ils se masquent. Le buck annoncé 7–35 V est ensuite présenté comme fonctionnant depuis 5 V (72–77), sans marge pour fournir 5 V après OR-ing.

### Filtres / Signal

- [mineur] Le seuil (307) vaut 20 % de la fréquence appliquée +0,05 Hz, et non 0,2 Hz : un changement de 100 à 115 Hz n’est pas appliqué. La comparaison c1>c0 rejette également la période traversant le wrap du compteur.

### Qualité de code générale (négatif)

- [critique] GPIO12/GPIO13/GPIO14/GPIO4/GPIO5 sont utilisés comme constantes (76–81) sans définition fournie. Le cœur standard attend des numéros GPIO ou des alias Dn ; ces identifiants ne sont pas ceux des broches D1 mini.
- [majeur] escapeHtml traite les guillemets doubles mais pas l’apostrophe, alors que les attributs utilisent des apostrophes (344–354/395–406). Mots de passe affichés, routes sans authentification et stockage sans CRC/backup.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 1,8/10 — Conversion de cycles inutilisable

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 4,4/10 — Qualité insuffisante

CanalMap, Config et CanalState séparés ; fonctions regroupées par responsabilité. Taille/magic du fichier vérifiés, mais erreurs d’unités et chaînes chargées non terminées explicitement.
