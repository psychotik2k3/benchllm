# Analyse — kimi_k3_max

**Sources analysées** : [deye_fan.ino](<../competitors/kimi_k3_max/deye_fan.ino>) ; [schema_electronique.md](<../competitors/kimi_k3_max/schema_electronique.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

Capture et Timer1 en IRAM, GPOS/GPOC et sections critiques lors des changements de sortie.

### Démarrage / Boot readiness

GPIO relâchés avant EEPROM ; réseau et HTTP initialisés avant capture (440–465). Timer démarré à la première consigne, pas de RPM nominal de secours ; loop traite toutes les 100 ms. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Génération sortie / Timer1

Calendrier en cycles à échéances absolues, saturation de demi-période et rattrapage des échéances dépassées.

### Fidélité au protocole tachymétrique / Fail-safe

PPR=2, ratios 0,5–8, durée de perte 1,5 s et RPM affichés dérivés de la demi-période écrêtée.

### Filtres / Signal

Rejet des intervalles <400 µs et EMA 1/4 dans le traitement.

### WiFi / Réseau

AP+STA, WIFI_NONE_SLEEP et reprise de la station toutes les 15 s.

### Gestion mémoire / HEAP

HTML PROGMEM et buffer snprintf status de 360 octets.

### Interface Web

Polling 1 s avec protection explicite document.activeElement pour les ratios (360).

### Persistance / Stockage

CRC16 par offsetof, chaînes terminées et clampRatio résistant à NaN.

### Documentation hardware/code

BOM, buck 5 V, entrées à 3,3 V, NPN de sortie avec rappel base-émetteur et un tach par paire.

### Qualité de code générale (positif)

- **Structure / organisation** : FanChannel et ChannelState distincts ; fonctions de capture et programmation séparées.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : CRC, chaînes terminées et NaN borné ; timestamps et calendrier insuffisamment invalidés.
- **Duplication / DRY** : Mêmes fonctions pour les deux voies, wrappers IRQ minimaux.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : FanChannel et ChannelState distincts ; fonctions de capture et programmation séparées.
- **Sécurité basique** : Copies de credentials explicitement terminées et clampRatio qui traite NaN.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Fidélité au protocole tachymétrique / Fail-safe

- [majeur] captureEdge accepte le premier dt depuis zéro (196–202), sans maximum. Après une longue pause, la période inclut le temps d’arrêt et l’EMA peut générer une vitesse très basse avant convergence.
- [majeur] Au timeout, periodCycles et lastEdgeCycle ne sont pas invalidés (280–285). Après un tour du compteur de cycles, l’ancienne mesure redevient récente pendant 1,5 s : reprise possible sans nouveau front, tous les 53,687 s à 80 MHz (26,844 s à 160 MHz).

### Génération sortie / Timer1

- [majeur] scheduleNext utilise une différence non signée (218–224). Si une échéance est dépassée au moment d’une reprogrammation depuis loop, la différence devient énorme et le délai peut être plafonné à 1,678 s au lieu d’un réveil immédiat. Le facteur fixe 16 est également faux à CPU 160 MHz : il faut 32 cycles CPU par tick DIV16.

### Temps-réel / ISR

- [majeur] Timer1 normal et timeout dans loop ; le filtre est appliqué toutes les 100 ms même si aucune nouvelle mesure n’est arrivée. La charge HTTP influe sur le suivi et le délai de perte.

### WiFi / Réseau

- [majeur] Pas d’authentification et handleWifi n’appelle pas WiFi.disconnect lorsque staEnable est décoché (409–412) : la station peut rester connectée malgré la désactivation demandée.

### Qualité de code générale (négatif)

- [mineur] Résultat de commit ignoré ; la précision d’un dixième dans la réponse status est inférieure à la précision de configuration float. Tests des débordements absents.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 5,8/10 — Version structurée, calendrier et vieillissement à corriger

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 6,5/10 — Maintenabilité à améliorer

FanChannel et ChannelState distincts ; fonctions de capture et programmation séparées. CRC, chaînes terminées et NaN borné ; timestamps et calendrier insuffisamment invalidés.
