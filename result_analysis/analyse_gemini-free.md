# Analyse — gemini-free

**Sources analysées** : [gemini-code-1787264142602.cpp](<../competitors/gemini-free/gemini-code-1787264142602.cpp>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

ISR annotées IRAM et horodatage par micros ; callbacks réseau asynchrones.

### Démarrage / Boot readiness

GPIO bas, LittleFS et WiFi avant la capture et Timer1 (193–222). Aucune attente explicite STA ; premier signal après une mesure et jusqu’à 50 ms de loop. Pas de valeur nominale de secours. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Fidélité au protocole tachymétrique / Fail-safe

Mesure RPM=30 000 000/période et demi-période demandée=15 000 000/RPM ; timeout logiciel de 1 s.

### Filtres / Signal

Rejet des intervalles ≤5 ms, sans lissage.

### WiFi / Réseau

ESPAsyncTCP/ESPAsyncWebServer, AP+STA permanent.

### Gestion mémoire / HEAP

HTML déclaré PROGMEM et JSON statique sérialisé vers String.

### Interface Web

Routes data/config séparées des réglages.

### Persistance / Stockage

LittleFS JSON, tailles de copies bornées par strlcpy.

### Qualité de code générale (positif)

- **Structure / organisation** : Fonctions stockage, capture et handlers séparées, mais globals dispersées.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : Copies de chaînes bornées ; ratios et montage FS non sécurisés.
- **Duplication / DRY** : Traitement intégralement dupliqué pour les deux voies.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Fonctions stockage, capture et handlers séparées, mais globals dispersées.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Génération sortie / Timer1

- [critique] onTimerISR annonce un pas de 50 µs mais recharge 80×50=4 000 ticks en DIV16 (91/221), soit 800 µs. La fréquence réelle est environ 16 fois trop faible : une consigne de 7 500 RPM donne ≈468,75 RPM hors surcoût ISR.

### Temps-réel / ISR

- [majeur] Interruption Timer1 masquable et digitalWrite à chaque bascule ; aucune compensation des pauses flash. Le timeout est géré dans loop avec delay(50).

### Fidélité au protocole tachymétrique / Fail-safe

- [mineur] Premier front mesuré depuis micros=0 ; les paires timestamp/période sont lues et parfois écrasées dans loop sans instantané atomique cohérent.

### Persistance / Stockage

- [majeur] Montage LittleFS non vérifié, sauvegarde directe sans backup ni résultat propagé. Un filesystem indisponible rend la configuration non persistante sans erreur web.

### WiFi / Réseau

- [majeur] Routes sans authentification ; delay(1000) et reboot dans le callback asynchrone WiFi (261–263). Le contexte réseau peut être bloqué et la réponse tronquée.

### Qualité de code générale (négatif)

- [majeur] Les ratios HTTP sont acceptés sans bornes ni isfinite (251–252), idem au chargement. Un ratio nul/négatif ne réinitialise pas la précédente target si le test rpmOut>0 échoue (281–284), laissant potentiellement une ancienne fréquence active.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 3,4/10 — Erreur Timer1 ×16, validation des ratios absente

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 4,4/10 — Qualité insuffisante

Fonctions stockage, capture et handlers séparées, mais globals dispersées. Copies de chaînes bornées ; ratios et montage FS non sécurisés.
