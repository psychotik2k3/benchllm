# Analyse — Claude_Opus_5.5_xhigh

**Sources analysées** : [deye_fan/deye_fan.ino](<../competitors/Claude_Opus_5.5_xhigh/deye_fan/deye_fan.ino>) ; [README.md](<../competitors/Claude_Opus_5.5_xhigh/README.md>) ; [SCHEMA.md](<../competitors/Claude_Opus_5.5_xhigh/SCHEMA.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

`tachNmi` (433–470) porte l’échantillonnage, le calcul de cible, la génération et le timeout. Helpers forcés inline, arithmétique Q16 et données séparées E/S ; pas de flottant ni appel réseau dans la NMI.

### Démarrage / Boot readiness

GPIO relâchés dès 1233–1237, EEPROM puis `engineStart` avant WiFi (1247–1260). Aucune vitesse inventée avant deux fronts. À 1 600 RPM, acquisition typique 18,75–37,5 ms après activation, avec validation numérique de 150 µs et amorçage de sortie d’environ 10 µs ; durée power-on non mesurée. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Génération sortie / Timer1

Échéances absolues en cycles, anticipation de 8 µs et attente active bornée avant le front ; les rapports 80/160 MHz sont pris en compte. Diagnostics de durée NMI et retard maximum.

### Fidélité au protocole tachymétrique / Fail-safe

PPR=2 des deux côtés, ratio 0,25–8 ; délai de perte 500 ms géré dans le moteur, arrêt à une frontière de sortie (368–428). Reprise avec capture réamorcée.

### Filtres / Signal

Trois échantillons à 50 µs, rejet des fronts <2 ms, moyenne adaptative de 1/2/4/8 périodes puis IIR 1/4.

### WiFi / Réseau

AP permanent, machine d’état STA, sommeil désactivé, scan asynchrone, DNS captif, mDNS et mise à jour HTTP avec mot de passe. Authentification administrateur configurable.

### Gestion mémoire / HEAP

HTML PROGMEM ; moteur sans allocation ; String status réservé à 1 400 octets, échappement JSON explicite.

### Interface Web

Polling 500 ms avec garde busy, configuration chargée séparément, aperçu du ratio, diagnostics et boutons explicites.

### Persistance / Stockage

Signature, version, longueur, CRC32 par offsetof, contrôle des bornes et terminaison des chaînes ; static_assert sur la taille EEPROM.

### Documentation hardware/code

README/SCHEMA distinguent hypothèses et essais déclarés, détaillent alimentation, sorties NPN, masses communes et un seul tach par paire. Les essais annoncés par le compétiteur n’ont pas été reproduits ici.

### Qualité de code générale (positif)

- **Structure / organisation** : Engine, Shared, Config et machine d’état STA séparent clairement responsabilités et propriété des données.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : CRC, chaînes terminées et mises à jour de configuration par candidat temporaire ; limites sur commit et parsing.
- **Duplication / DRY** : Canaux génériques, helpers numériques partagés et fonctions de service séparées.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Engine, Shared, Config et machine d’état STA séparent clairement responsabilités et propriété des données.
- **Sécurité basique** : Échappement JSON, copies bornées et authentification HTTP administrateur optionnelle ; les routes vérifient authOK.
- **Testabilité** : mulQ16, CRC et conversion de ratios sont isolables. Le README annonce des simulations, mais leurs scripts de test ne sont pas livrés ; le script Python fourni génère le schéma, pas un banc du moteur.

---

## ❌ Points techniques négatifs

### Filtres / Signal

- [mineur] Le cumul moyenne de huit périodes + IIR 1/4 (334–345) ralentit les transitions. À 1 600 RPM, la fenêtre couvre 150 ms et la constante IIR est de l’ordre de 65 ms ; l’établissement prend plusieurs centaines de ms.

### WiFi / Réseau

- [majeur] `authOK` retourne true quand adminPass est vide (850–854), état par défaut. Les clients du LAN peuvent alors configurer, réinitialiser ou redémarrer le contrôleur. L’authentification optionnelle doit être activée avant déploiement.

### Persistance / Stockage

- [mineur] `EEPROM.commit` n’est pas contrôlé (591–597) ; pas de copie transactionnelle ni de réduction des sauvegardes répétées.

### Temps-réel / ISR

- [mineur] Architecture NMI complexe et polling 20 kHz ; les compteurs internes mesurent le retard logiciel, pas la forme électrique au collecteur. Aucun binaire, map IRAM ou essai oscilloscope reproduit dans cette réanalyse : les chiffres métrologiques du README restent des déclarations.

### Qualité de code générale (négatif)

- [mineur] `parseRatio` utilise String.toFloat (963–970), qui accepte un préfixe numérique suivi de texte ; par exemple « 2abc » est traité comme 2. La plage est validée, pas la syntaxe complète.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 8,6/10 — Meilleure base temps réel, validation physique requise

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 8,5/10 — Structure solide

Engine, Shared, Config et machine d’état STA séparent clairement responsabilités et propriété des données. CRC, chaînes terminées et mises à jour de configuration par candidat temporaire ; limites sur commit et parsing.
