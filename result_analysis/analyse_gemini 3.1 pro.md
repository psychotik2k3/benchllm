# Analyse — gemini 3.1 pro

**Sources analysées** : [deye_fan.ino](<../competitors/gemini 3.1 pro/deye_fan.ino>) ; [schema_et_explications.md](<../competitors/gemini 3.1 pro/schema_et_explications.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

Compteurs d’entrée et ISR Timer1 en IRAM, instantané des compteurs sous section critique.

### Démarrage / Boot readiness

Timer1 prévu avant WiFi, sorties au repos ; premier calcul après 1 s de loop. En l’état, pas de binaire compilable. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Génération sortie / Timer1

DIV16 chargé à 250 = 50 µs, compteurs de demi-période par canal ; conversion `300000/rpmOut` cohérente avec 2 PPR.

### WiFi / Réseau

AP+STA et démarrage non bloquant de la station.

### Gestion mémoire / HEAP

HTML source en PROGMEM, mais copie dans String à chaque page.

### Interface Web

Routes status et save distinctes ; mesures mises à jour sans recompiler.

### Persistance / Stockage

Paramètres regroupés en Config et signature EEPROM.

### Documentation hardware/code

Schéma d’entrées/sorties et proposition d’un buck 5 V.

### Qualité de code générale (positif)

- **Structure / organisation** : Petites fonctions dédiées mais état global et traitements dupliqués par voie.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : Validation serveur limitée à une borne basse des ratios ; aucune protection strcpy.
- **Duplication / DRY** : Compteurs, sorties et calculs copiés entre les deux voies.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Petites fonctions dédiées mais état global et traitements dupliqués par voie.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Fidélité au protocole tachymétrique / Fail-safe

- [majeur] Le comptage par fenêtre utilise p×30 sans diviser par la durée réelle écoulée (261–275). Si HTTP retarde une fenêtre à 1,5 s, la vitesse est surestimée de 50 % ; aucune protection contre le bruit et aucune borne supérieure des ratios côté serveur.

### Temps-réel / ISR

- [majeur] La documentation parle de NMI-like et de zéro jitter, mais le code utilise une interruption Timer1 ordinaire. Les ticks perdus ne sont pas compensés et la sortie attend le calcul à 1 Hz.

### Persistance / Stockage

- [majeur] Seule la signature protège l’EEPROM ; ratios et chaînes ne sont pas validés au chargement. Une configuration partiellement corrompue peut déclencher lectures hors tampon ou conversions invalides.

### WiFi / Réseau

- [majeur] Aucune authentification ; la page HTML réinsère SSID et mot de passe sans échappement (174–176), exposant le secret et une injection HTML.

### Qualité de code générale (négatif)

- [critique] EEPROM_MAGIC=0xDEYE0001 (8) contient Y : entier hexadécimal invalide. Toute comparaison/affectation de cette macro empêche la compilation.
- [critique] `saveConfig` copie les arguments HTTP non bornés par strcpy dans ssid[32] et pass[64] (161–163). Une requête de plus de 31/63 caractères écrase les champs adjacents en RAM.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 2,0/10 — Compilation bloquée, sécurité mémoire insuffisante

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 3,5/10 — Qualité insuffisante

Petites fonctions dédiées mais état global et traitements dupliqués par voie. Validation serveur limitée à une borne basse des ratios ; aucune protection strcpy.
