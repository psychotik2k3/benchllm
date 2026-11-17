# Analyse — Swift_1.5_Qwen3.8-27B_medium-IQ3S

**Sources analysées** : [deye_fan.ino](<../competitors/Swift_1.5_Qwen3.8-27B_medium-IQ3S/deye_fan.ino>) ; [schema_electronique.txt](<../competitors/Swift_1.5_Qwen3.8-27B_medium-IQ3S/schema_electronique.txt>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Démarrage / Boot readiness

GPIO au repos et chargement de paramètres prévus avant le réseau ; l’absence de build valide empêche de mesurer un premier tach exploitable. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Filtres / Signal

Intention d’EMA de vitesse 1/10 et traitement toutes les 10 ms.

### WiFi / Réseau

Intention AP+STA et paramètres configurables dans une page HTTP.

### Interface Web

Page de mesures et réglages de ratios/WiFi ; statut JSON prévu.

### Persistance / Stockage

Intention de stockage Preferences avec défauts de ratios.

### Documentation hardware/code

Schéma texte fourni avec OU de diodes et sorties NPN, mais alimentation erronée.

### Qualité de code générale (positif)

- **Structure / organisation** : Channel regroupe les données mais mélange capture, commande et génération sans contrat de partage.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : Quelques bornes HTTP, aucun contrôle cohérent de disponibilité des API ou de vieillissement.
- **Duplication / DRY** : Deux ISR similaires ; boucle commune pour les calculs et la sortie.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Channel regroupe les données mais mélange capture, commande et génération sans contrat de partage.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Génération sortie / Timer1

- [critique] Timer0 est programmé une seule fois avec 100 (267–269), sans réarmement dans onTick, et timer0_enable n’est pas l’API exposée par le cœur. Le code suppose une base en microsecondes alors que Timer0 compare des cycles absolus.
- [majeur] Le scheduler ajoute pulseWidth à une attente égale à la période complète, avec deux ticks supplémentaires de countdown. La fréquence réelle serait inférieure à la consigne même avec le timer réparé.

### Temps-réel / ISR

- [critique] onTick/onIn1/onIn2 ne sont pas IRAM_ATTR, Channel n’est pas volatile malgré des champs partagés entre ISR et loop. digitalWriteFast et WiFi.connect ne sont pas fournis par la livraison.

### Fidélité au protocole tachymétrique / Fail-safe

- [critique] Le timeout est dans un else-if déclenché seulement si p est hors plage (312–318). Après une mesure valide, p reste valide pour toujours : un ventilateur arrêté continue à être annoncé actif et sa fréquence reste émise.

### Documentation hardware/code

- [critique] schema_electronique.txt 28–34 place 100 kΩ en série avant le LDO depuis 12 V, limitant le courant à <120 µA ; impossible d’alimenter un ESP8266 consommant des dizaines de mA. Le schéma propose aussi AP2112 directement sur 12 V, hors de sa plage d’entrée.

### WiFi / Réseau

- [majeur] Modifications en GET, mots de passe dans l’URL, absence d’authentification et rechargement complet chaque seconde, qui gêne la saisie.

### Qualité de code générale (négatif)

- [critique] server est déclaré WiFiServer (149) puis utilisé comme ESP8266WebServer : send/on/hasArg/arg/handleClient (216–302) ne sont pas ses méthodes. ESP8266HTTPServer.h et Preferences.h ne sont pas des dépendances du cœur ESP8266 standard annoncé.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 0,8/10 — API réseau et timer incompatibles

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 2,5/10 — Qualité insuffisante

Channel regroupe les données mais mélange capture, commande et génération sans contrat de partage. Quelques bornes HTTP, aucun contrôle cohérent de disponibilité des API ou de vieillissement.
