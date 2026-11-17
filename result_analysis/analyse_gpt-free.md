# Analyse — gpt-free

**Sources analysées** : [gpt_deye_fan.ino](<../competitors/gpt-free/gpt_deye_fan.ino>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

ISR d’entrée IRAM et copie atomique newPeriod/period ; somme du filtre réalisée en uint64_t hors interruptions.

### Démarrage / Boot readiness

Sorties basses et pause explicite de 100 ms avant EEPROM, capture puis réseau. Premier signal après deux fronts et premier processChannel ; aucune vitesse nominale avant mesure, durée power-on non mesurée. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Génération sortie / Timer1

startWaveformClockCycles (479–487) utilise le moteur NMI du cœur, temps haut/bas égaux, deux sorties indépendantes.

### Fidélité au protocole tachymétrique / Fail-safe

PPR=2 ; fréquence de sortie proportionnelle au ratio, timeout logiciel de 500 ms avec drapeau signalPresent.

### Filtres / Signal

Périodes bornées et EMA 1/8 sur nouvelles mesures.

### WiFi / Réseau

AP+STA sans attente de connexion ; SSID/identifiants configurables.

### Gestion mémoire / HEAP

Aucune allocation dans les ISR, réservation explicite des String HTML/JSON.

### Interface Web

Dark theme et polling 500 ms des seules mesures ; formulaire chargé séparément.

### Persistance / Stockage

EEPROM structurée et signature, copies HTTP via toCharArray borné.

### Documentation hardware/code

Les commentaires du firmware expliquent les voies, les formules et les contraintes waveform ; pas de schéma annexe livré.

### Qualité de code générale (positif)

- **Structure / organisation** : Fonctions lisibles et sections explicatives, mais fichier de 1 575 lignes et état global.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : Sections critiques et copies bornées présentes ; CRC, finitude, résultat waveform et reconfiguration manquent.
- **Duplication / DRY** : processChannel partagé, mais les deux ISR et l’interface sont très répétitifs.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Fonctions lisibles et sections explicatives, mais fichier de 1 575 lignes et état global.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Persistance / Stockage

- [majeur] Seul magic est vérifié (225–253), sans CRC ni version ni fin de chaîne. constrain ne rejette pas NaN : des ratios EEPROM corrompus peuvent parvenir aux calculs et conversions numériques.

### WiFi / Réseau

- [majeur] handleGetConfig renvoie staPassword/apPassword (1065–1074), sans authentification et sans échappement JSON. Lecture des secrets et configuration possibles depuis le réseau ; guillemets/backslashes cassent la réponse.

### Fidélité au protocole tachymétrique / Fail-safe

- [majeur] lastValidEdgeUs est daté lors de processChannel (579), pas lors du front. Un traitement HTTP lent peut rendre fraîche une ancienne mesure puis prolonger la sortie de 500 ms supplémentaires. Le timeout ne s’exécute que dans loop.

### Filtres / Signal

- [mineur] Le filtre filteredPeriodCycles n’est pas remis à zéro lors de l’arrêt (609–621). La reprise mélange une nouvelle vitesse avec l’ancien régime et nécessite plusieurs périodes pour converger.

### Interface Web

- [majeur] Les paramètres WiFi sont sauvegardés (1181) mais startWiFi n’est pas rappelé et aucun reboot n’est planifié. La nouvelle configuration radio n’est effective qu’après redémarrage manuel.

### Qualité de code générale (négatif)

- [mineur] Retour de startWaveformClockCycles ignoré, page construite en RAM avec reserve(9000), deux grosses implémentations parallèles de capture. Le statut actif ne prouve pas que l’onde a démarré.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 6,1/10 — Moteur waveform pertinent, configuration fragile

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 6,4/10 — Maintenabilité à améliorer

Fonctions lisibles et sections explicatives, mais fichier de 1 575 lignes et état global. Sections critiques et copies bornées présentes ; CRC, finitude, résultat waveform et reconfiguration manquent.
