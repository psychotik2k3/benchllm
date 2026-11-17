# Analyse — composer 2.5 prompt from claude

**Sources analysées** : [deye_fan.ino](<../competitors/composer 2.5 prompt from claude/deye_fan.ino>) ; [schema_electronique.md](<../competitors/composer 2.5 prompt from claude/schema_electronique.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

ISR de capture en IRAM et instantanés sous noInterrupts ; calculs flottants réalisés hors ISR.

### Démarrage / Boot readiness

GPIO inactifs, montage LittleFS puis capture/Timer1 avant WiFi (461–493). Pas de nominal artificiel ; deux fronts puis premier traitement loop nécessaires. Formatage automatique éventuel avant capture : délai non borné par le code. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Génération sortie / Timer1

Tick DIV16 de 250 ticks = 50 µs ; sorties indépendantes, nouvelle échéance après chaque toggle.

### Fidélité au protocole tachymétrique / Fail-safe

RPM=30 000 000/période, ratio 1–4 et arrêt après 1,5 s sans front, ligne relâchée dans le traitement (241–250).

### Filtres / Signal

Périodes 2,5–500 ms, moyenne de quatre valeurs et limitation de variation de 15 % par appel.

### WiFi / Réseau

ESPAsyncWebServer, AP+STA et machine de connexion sans attente bloquante.

### Gestion mémoire / HEAP

HTML PROGMEM, documents JSON statiques ; réponse sérialisée dans un String.

### Interface Web

Polling status séparé des formulaires, sauvegarde de ratios et WiFi par POST.

### Persistance / Stockage

LittleFS et JSON avec retour aux défauts après erreur de lecture/désérialisation.

### Documentation hardware/code

OU Schottky + buck 5 V, pull-up d’entrée à 3,3 V et sorties BC547 avec résistance de base dimensionnée.

### Qualité de code générale (positif)

- **Structure / organisation** : Séparation capture, filtrage, stockage et handlers HTTP ; globals nombreuses.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : JSON invalide géré, mais chaînes, erreurs d’écriture et données non finies à mieux contrôler.
- **Duplication / DRY** : Boucles par canal ; prise d’instantané explicitement dupliquée pour les deux voies.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Séparation capture, filtrage, stockage et handlers HTTP ; globals nombreuses.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Temps-réel / ISR

- [majeur] Timer1 masquable : les écritures LittleFS peuvent retarder les fronts et les entrées. DigitalWrite et échéances `now + half` (112–113) ne compensent ni le retard ni l’arrondi au tick ; biais de période jusqu’à presque 100 µs, hors latences supplémentaires.

### Filtres / Signal

- [mineur] La moyenne et le slew sont recalculés à chaque loop, pas à chaque nouvel échantillon ; g_edgeCount n’est jamais consommé. Les quatre cases peuvent contenir quatre copies de la même mesure, la vitesse du filtre dépend de la charge réseau.

### Fidélité au protocole tachymétrique / Fail-safe

- [majeur] L’horodatage g_lastEdgeUs est actualisé avant validation (79–87). Un bruit continu hors plage peut conserver une ancienne mesure active et repousser le timeout.

### WiFi / Réseau

- [majeur] Pas d’authentification ; handler asynchrone WiFi avec delay(300) puis ESP.restart (451–454), sans report dans loop. Risque de réponse interrompue ou de blocage du contexte réseau.

### Persistance / Stockage

- [mineur] Réécriture directe de config.json, sans rename ni sauvegarde ; les retours saveConfig ne sont pas propagés. Une coupure pendant sauvegarde peut perdre toute la configuration.

### Qualité de code générale (négatif)

- [majeur] Les strncpy (150–153 et 440–449) utilisent toute la capacité, sans forcer le NUL final. Un SSID de 33 octets ou un mot de passe de 65 octets produit une chaîne non terminée et une lecture hors tampon au stockage/WiFi.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 5,5/10 — Base fonctionnelle, robustesse réseau et flash insuffisante

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 5,8/10 — Maintenabilité à améliorer

Séparation capture, filtrage, stockage et handlers HTTP ; globals nombreuses. JSON invalide géré, mais chaînes, erreurs d’écriture et données non finies à mieux contrôler.
