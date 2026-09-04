# Analyse — deepseek4 flash

**Sources analysées** : `competitors/deepseek4 flash/deye_fan.ino`, `competitors/deepseek4 flash/README.md`, `competitors/deepseek4 flash/SCHEMA_ELECTRONIQUE.md`

---

## ✅ Points techniques positifs

### Temps-réel / ISR
- Dérivation directe du Timer1 matériel vers le vecteur NMI (`NmiTimSetFunc(&app_timer1_isr)`), isolant totalement la génération des signaux de sortie de la pile WiFi et des interruptions masquées.
- Mode one-shot FRC1 (`FRC1_AUTO_RELOAD = 0`) avec reprogrammation dynamique sur l'événement le plus proche, atteignant une résolution de 0,2 µs (horloge à 5 MHz avec diviseur 16).
- Manipulation directe des registres matériels `REG_GPIO_W1TS` et `REG_GPIO_W1TC` pour commuter les sorties instantanément.
- Arithmétique entièrement en virgule fixe Q16 (`g_ratioQ16[2]`) pour calculer les demi-périodes sans jamais faire appel à la bibliothèque d'émulation logicielle de virgule flottante sous interruption.
- Tout le code du moteur temps-réel est annoté `IRAM_ATTR`.

### Démarrage / Boot readiness
- Broches initialisées à l'état bas (`digitalWrite(..., LOW)`) avant la configuration matérielle, garantissant le maintien des transistors NPN à l'état bloqué.
- Démarrage du moteur d'interruption avant l'initialisation du réseau WiFi.

### Génération sortie / Timer1
- Accumulation de ticks (`g_nextTicks[i] += hp`) pour éviter la dérive temporelle cumulative.
- Plancher de sécurité de 25 ticks (5 µs) pour éviter l'écrasement du timer par reprogrammation immédiate.
- Coupure immédiate du signal et relâchement de la ligne au repos dès qu'un canal est désactivé.

### Fidélité au protocole tachymétrique / Fail-safe
- Règle des 2 impulsions par tour respectée (`PPR = 2`).
- Timeout d'inactivité à 2,0 s ou 4 fois la période mesurée : extinction automatique du signal simulé en cas de déconnexion du ventilateur.
- Bornage des ratios entre 1,0 et 4,0.

### Filtres / Signal
- Anti-rebond d'entrée par élimination des fronts trop rapprochés (< 400 µs, soit > 75 000 tr/min à 2 PPR).
- Filtrage passe-bas exponentiel sur les périodes (`EMA_SHIFT = 3`, équivalent \(\alpha = 1/8\)) entièrement calculé en nombres entiers 32 bits.

### WiFi / Réseau
- Mode `WIFI_AP_STA` simultané sans mise en veille du modem WiFi.
- Serveur mDNS intégré (`deye-fan.local`).

### Gestion mémoire / HEAP
- Interface web embarquée en Flash via `PROGMEM`.
- Endpoints de statut utilisant un buffer statique (`char g_stateBuf[640]`) pour sérialiser le JSON sans allocation dynamique sur le tas.

### Interface Web
- Interface moderne avec potentiomètres/curseurs pour ajuster les ratios en direct via des requêtes AJAX non bloquantes (`/api/set?c=0&r=250`).
- Affichage dynamique de l'état des ventilateurs et des métriques en temps réel.

### Persistance / Stockage
- Sauvegarde en EEPROM protégée par un mot magique de 4 octets (`DF10`), un numéro de version et un contrôle d'intégrité par CRC8.

### Documentation hardware/code
- Document `SCHEMA_ELECTRONIQUE.md` très soigné : OU-diode avec Schottky 1N5822/SS34, convertisseur buck externe MP1584/LM2596 (12V vers 5V), et protection d'entrée avec diodes de clamp BAT54S et résistance de 4,7 kΩ tolérant du 12V direct.

### Qualité de code générale (positif)
- **Structure / organisation** : Séparation claire entre les couches matérielles et réseau.
- **Lisibilité / nommage** : Nommage concis et cohérent.
- **Commentaires / documentation inline** : Excellente documentation matérielle et explications claires des choix NMI.
- **Gestion d'erreurs / robustesse** : Pas de crash possible par division par zéro grâce aux bornages systématiques.
- **Respect des conventions C++/Arduino** : Gestion fine des accès atomiques avec `volatile` et registres bas niveau.

---

## ❌ Points techniques négatifs

### Temps-réel / ISR (mineur)
- [mineur] : L'utilisation de définitions de registres brutes au lieu des constantes standard du core ESP8266 (`REG_GPIO_W1TS` au lieu de `GPOS`) réduit la portabilité sur d'autres versions du core Arduino — impact : risque mineur de conflit de définition selon la version de la toolchain.

### Fidélité au protocole tachymétrique / Fail-safe (majeur)
- [majeur] : L'affichage des RPM dans `loop()` repose sur un compteur d'impulsions évalué sur une fenêtre fixe d'une seconde (`g_pulseCount`), alors que la période simulée en sortie est régulée instantanément par l'EMA. Cela crée une discordance temporelle d'affichage de 1 000 ms par rapport à la consigne réelle envoyée à l'onduleur — impact : latence visuelle importante sur l'interface web lors des montées en régime du ventilateur.

### Persistance / Stockage (mineur)
- [mineur] : Utilisation d'un simple CRC8 pour valider la structure EEPROM au lieu d'un CRC32 ou CRC16 — impact : probabilité plus élevée (1/256) de collision lors d'une corruption aléatoire de flash.

### Qualité de code générale (négatif)
- [mineur] : Code source regroupé dans un fichier `.ino` unique assez dense, mélangeant les primitives assembleur/registres et les handlers HTTP — impact : maintenabilité plus complexe qu'une architecture modulaire en plusieurs fichiers.

---

## ⭐ Note globale : 5.5/10 — Performances brutes excellentes mais compromis d'affichage
Une solution très affûtée au niveau du moteur NMI et de la virgule fixe Q16, accompagnée d'un schéma électronique complet, mais pénalisée par une fenêtre de mesure RPM décalée à 1 seconde et une ergonomie de code monolithique.

## ⭐ Note qualité de code : 6.2/10 — Code bas niveau efficace mais compact
Code très technique et performant, bien pensé pour l'embarqué sans FPU, mais manquant d'une séparation modulaire claire.
