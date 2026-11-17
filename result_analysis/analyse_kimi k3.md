# Analyse — kimi k3

**Sources analysées** : [deye_fan.ino](<../competitors/kimi k3/deye_fan.ino>) ; [schema_electronique.md](<../competitors/kimi k3/schema_electronique.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

ISR IRAM, capture micros et EMA entière 1/4 ; GPOS/GPOC en sortie.

### Démarrage / Boot readiness

Capture avant EEPROM, Timer1 avant WiFi ; sorties basses sans nominal artificiel. La première mesure utilise toutefois la durée depuis zéro. Le timer inactif peut dormir une seconde avant de voir la première consigne. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Génération sortie / Timer1

Scheduler DIV16 au prochain front, facteur 5 ticks/µs et échéances accumulées ; récupération des retards.

### Fidélité au protocole tachymétrique / Fail-safe

2 PPR, ratio 0,5–10 et arrêt après 1,5 s dans updateChannels, contrôlé toutes les 100 ms.

### Filtres / Signal

Anti-parasites 300 µs et EMA 1/4.

### WiFi / Réseau

AP+STA, auto-reconnexion et mDNS.

### Gestion mémoire / HEAP

HTML PROGMEM, status dans un buffer de 320 octets.

### Interface Web

Actualisation des mesures toutes les secondes sans recharger le formulaire.

### Persistance / Stockage

CRC16, chaînes terminées, ratios vérifiés au chargement ; pour ce layout précis, sizeof(Config)−2 exclut bien le CRC.

### Documentation hardware/code

Buck 5 V, diodes Schottky, sorties NPN et un tach par paire explicités.

### Qualité de code générale (positif)

- **Structure / organisation** : Channel et Config compacts, fonctions courtes de capture/stockage/HTTP.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : CRC et copies de chaînes bornées ; amorçage et validation de finitude incomplets.
- **Duplication / DRY** : Capture et traitement communs aux deux canaux.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Channel et Config compacts, fonctions courtes de capture/stockage/HTTP.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Démarrage / Boot readiness

- [majeur] onPulse calcule dt=micros−0 au premier front (157–162) et l’injecte dans l’EMA. Après un redémarrage ou une longue pause, cette durée n’est pas une période de rotation ; la consigne est fausse pendant plusieurs fronts.

### Génération sortie / Timer1

- [majeur] L’ISR inactive recharge jusqu’à 1 000 000 µs (174/198) et la boucle ne force pas de réveil lorsqu’une consigne apparaît (312). Le premier signal peut donc être retardé d’environ 1 s, puis son échéance nextToggle initialisée à zéro impose un rattrapage.

### Fidélité au protocole tachymétrique / Fail-safe

- [majeur] Le timeout dépend de loop après HTTP et les données de période/timestamp ne sont pas prises ensemble sous section critique (299–305). Un callback lent prolonge l’émission de l’ancienne vitesse au-delà de 1,5 s.

### WiFi / Réseau

- [majeur] Routes de configuration accessibles sans authentification ; pas de remontée d’erreur de sauvegarde et pas de validation complète des credentials WiFi.

### Qualité de code générale (négatif)

- [mineur] Validation EEPROM par comparaisons < et > seulement (134–135) : NaN peut passer. Pas de borne haute de période en capture ni contrôle du résultat commit.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 5,7/10 — Simple et exploitable après correction de l’amorçage

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 6,1/10 — Maintenabilité à améliorer

Channel et Config compacts, fonctions courtes de capture/stockage/HTTP. CRC et copies de chaînes bornées ; amorçage et validation de finitude incomplets.
