# Analyse — grok 4.6 medium

**Sources analysées** : [deye_fan/deye_fan.ino](<../competitors/grok 4.6 medium/deye_fan/deye_fan.ino>) ; [SCHEMA_ELECTRONIQUE.md](<../competitors/grok 4.6 medium/SCHEMA_ELECTRONIQUE.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

Sortie NMI en IRAM, opérations entières et GPOS/GPOC ; données de consigne en volatile.

### Démarrage / Boot readiness

EEPROM puis NMI/capture avant WiFi. Sortie inactive sans mesure et premier front ignoré. Deux fronts puis loop et première demi-période nécessaires, hors setup non chronométré. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Génération sortie / Timer1

DIV16, reload=100 ticks : NMI à 50 kHz et pas de 20 µs. La formule periodToNmiHalf correspond bien au ratio exprimé ×100 à 80 MHz.

### Fidélité au protocole tachymétrique / Fail-safe

PPR=2 et ratio 0,5–8 ; délai logiciel de 400 ms ; saturation de demi-périodes.

### Filtres / Signal

Intervalle minimal d’entrée 200 µs à 80 MHz.

### WiFi / Réseau

AP+STA et reconnexion toutes les 15 s.

### Gestion mémoire / HEAP

HTML PROGMEM et String status réservé.

### Interface Web

Polling à 500 ms, formulaires et mesures séparés.

### Persistance / Stockage

CRC32 par offsetof et bornage des ratios chargés.

### Documentation hardware/code

Schéma OU Schottky + buck 5 V, entrées tirées à 3,3 V, NPN de sortie et un seul tach par paire.

### Qualité de code générale (positif)

- **Structure / organisation** : Structures par canal, conversions et stockage bien délimités.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : CRC et ratios bornés ; état de capture non invalidé à la perte et validation numérique incomplète.
- **Duplication / DRY** : Calcul générique par canal, mais NMI écrite deux fois à la main.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Structures par canal, conversions et stockage bien délimités.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Fidélité au protocole tachymétrique / Fail-safe

- [majeur] Au timeout, nmiEnabled est effacé mais havePeriod et lastEdgeUs restent valides (268–270). Après le wrap de micros (≈71 min 35 s), l’ancien timestamp redevient récent et l’ancienne fréquence peut être réémise pendant 400 ms sans ventilation.

### Filtres / Signal

- [majeur] tachCapture actualise lastCycle/lastEdgeUs avant le test anti-parasite (136–145). Des fronts tous <200 µs peuvent entretenir une ancienne période et empêcher l’arrêt ; aucun lissage ou borne maximale de période ne complète la capture.

### Temps-réel / ISR

- [majeur] NMI 50 kHz en permanence, mais timeout et nouvelles consignes dépendent de loop. La capture GPIO reste masquable ; le calendrier à ticks n’utilise pas un temps réel absolu pour compenser les retards de NMI.

### WiFi / Réseau

- [majeur] Aucune authentification ; les réglages reconfigurent immédiatement la radio avant la réponse HTTP (479–481), ce qui peut interrompre la requête. Le résultat commit est ignoré.

### Documentation hardware/code

- [mineur] Le schéma propose NF-A9 ≈2 000 RPM (212) alors que la référence NF-A9 FLX nominale est 1 600 RPM. Le ratio doit provenir d’une mesure, pas de cette estimation.

### Qualité de code générale (négatif)

- [mineur] parseRatioX100 (418–423) ne rejette pas explicitement NaN avant conversion vers uint16_t ; les SSID sont concaténés sans échappement JSON (454–455).

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 6,0/10 — NMI simple, surveillance de perte incomplète

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 6,7/10 — Maintenabilité à améliorer

Structures par canal, conversions et stockage bien délimités. CRC et ratios bornés ; état de capture non invalidé à la perte et validation numérique incomplète.
