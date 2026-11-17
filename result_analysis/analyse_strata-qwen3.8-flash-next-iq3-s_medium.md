# Analyse — strata-qwen3.8-flash-next-iq3-s_medium

**Sources analysées** : [deye_fan/deye_fan.ino](<../competitors/strata-qwen3.8-flash-next-iq3-s_medium/deye_fan/deye_fan.ino>) ; [README.md](<../competitors/strata-qwen3.8-flash-next-iq3-s_medium/README.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

Volonté d’échantillonner entrées et sorties dans la même ISR IRAM, tableaux de deux canaux et données volatile.

### Démarrage / Boot readiness

Montage LittleFS, radio, serveur puis timer (817–849) ; aucun nominal de secours. En l’état, aucun train tach valide ne peut démarrer. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Filtres / Signal

Médiane de huit valeurs et EMA entière prévues ; paramètres de filtrage nommés.

### WiFi / Réseau

AP+STA, DNS captif et mDNS ; WiFi.persistent(false) et sommeil désactivé.

### Gestion mémoire / HEAP

HTML PROGMEM et buffer status de 640 octets.

### Interface Web

Contrôles des deux canaux, mode simulation/bypass et lecture status.

### Persistance / Stockage

LittleFS avec deux emplacements et sauvegarde différée.

### Documentation hardware/code

README très détaillé avec alimentation OU Schottky/buck, nomenclature et intention de chemin de secours.

### Qualité de code générale (positif)

- **Structure / organisation** : Structures de configuration et état par canal, mais moteur complexe et unités mélangées.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : Bornes de ratios prévues ; tests de trappe, armement et tailles CRC non vérifiés.
- **Duplication / DRY** : Boucles par canal et médiane commune, nombreux modes aux interactions non testées.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Structures de configuration et état par canal, mais moteur complexe et unités mélangées.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Génération sortie / Timer1

- [critique] tickISR appelle timer1_disable puis timer1_write sans réactiver T1C (374–375). Après la première interruption, le compteur reste désactivé. De plus, 800 ticks DIV16 =160 µs, pas TICK_US=50 : facteur 3,2 sur la base de temps.
- [critique] En mode SIM, outNext reste à zéro à l’initialisation ; outService ne produit un front que si outNext vaut 1 après décrément (341–359). Aucun code ne lance la première échéance en mode SIM : sortie figée.
- [majeur] La cible (487) multiplie e, déjà en ticks, par TICK_US puis la traite encore comme ticks. Le facteur 50 et l’addition de largeur de pulse faussent la fréquence, indépendamment du ratio demandé.

### Fidélité au protocole tachymétrique / Fail-safe

- [critique] Toutes les périodes <6 000 ticks sont classées « trappe » (428–433), soit les vitesses >100 RPM avec un tick de 50 µs. Les Noctua nominaux sont donc rejetés après huit périodes ; le flag trapped n’est pas réarmé.

### Persistance / Stockage

- [majeur] Config : crc à 220 et sizeof=224, donc sizeof−2=222 inclut le CRC (226/234/251). Les deux fichiers reçoivent la même valeur sans version temporelle, rename transactionnel ni retour d’écriture contrôlé. Modèle de layout reproduit.

### WiFi / Réseau

- [majeur] AP ouvert par défaut et routes de contrôle/sauvegarde sans authentification. inputLost est seulement indiqué mais n’interdit pas outService : maintien indéfini d’une ancienne fréquence si le moteur était réparé.

### Qualité de code générale (négatif)

- [critique] GPO_SET_SET/GPO_SET_CLEAR, GPIO_INPUT_REG(pin) et timer1_init (305/394/846) ne sont pas les primitives définies par le cœur ESP8266 standard ; aucune définition locale ne les fournit. Compilation bloquée.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 0,8/10 — Timer arrêté dans son ISR et sortie jamais amorcée

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 3,1/10 — Qualité insuffisante

Structures de configuration et état par canal, mais moteur complexe et unités mélangées. Bornes de ratios prévues ; tests de trappe, armement et tailles CRC non vérifiés.
