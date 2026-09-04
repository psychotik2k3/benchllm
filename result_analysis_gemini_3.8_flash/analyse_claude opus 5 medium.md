# Analyse — claude opus 5 medium

**Sources analysées** : `competitors/claude opus 5 medium/deye_fan/deye_fan.ino`, `competitors/claude opus 5 medium/SCHEMA.md`

---

## ✅ Points techniques positifs

### Temps-réel / ISR
- Dérivation du Timer1 directement vers le vecteur NMI (niveau 3) via `ETS_FRC_TIMER1_NMI_INTR_ATTACH(fanEngineIsr)` et `NmiTimSetFunc()`. La NMI préempte la pile WiFi du SDK Espressif (niveau 1) et n'est pas masquée par `noInterrupts()`.
- Tout le code exécuté sous NMI est annoté `IRAM_ATTR` ; toutes les données accédées sont en DRAM (aucun accès en mémoire Flash qui crasherait lors d'une écriture EEPROM ou d'un appel WiFi).
- Aucun calcul flottant, aucune allocation dynamique, aucun appel aux fonctions du SDK dans l'ISR NMI.
- Horodatage par le registre matériel de cycles CPU `ccount` via l'instruction assembleur `rsr %0,ccount` (`ccyNow()`), garantissant une précision à l'instruction près.
- Protocole seqlock (`InputChannel.seq`) pour publier les instantanés multi-mots vers la boucle `loop()` sans aucun verrou bloquant ni désactivation d'interruption.

### Démarrage / Boot readiness
- `engineBegin()` est exécuté avant `applyWifi()` et `serverBegin()` dans `setup()` : le signal tach de sortie est prêt et actif dès les premières centaines de millisecondes, évitant à l'onduleur Deye de détecter un défaut de rotation au boot.
- Les broches de sortie sont explicitement écrites à l'état bas (`digitalWrite(..., LOW)`) avant la configuration en `pinMode(..., OUTPUT)` afin de garantir le blocage du transistor NPN dès le reset.
- Résistance physique de 10 kΩ entre base et émetteur préconisée dans le schéma pour maintenir le transistor bloqué pendant la haute impédance du bootloader ESP8266.

### Génération sortie / Timer1
- Ordonnanceur one-shot à plus proche échéance (*earliest deadline*) : Timer1 configuré en `TIM_SINGLE` et reprogrammé dynamiquement à chaque front avec un plancher de sécurité (`TIMER_MIN_TICKS = 10` ticks de 0,2 µs).
- Manipulation directe des registres matériels `GPOS` et `GPOC` pour basculer les sorties en une seule instruction d'horloge.
- Accumulation de phase (`nextEdgeCcy += oc->halfCcy`) évitant toute dérive temporelle cumulative, couplée à une boucle de rattrapage bornée à 16 fronts.
- Prise en compte des nouvelles demi-périodes exclusivement aux frontières de fronts, empêchant toute impulsion tronquée ou glitch de sortie.

### Fidélité au protocole tachymétrique / Fail-safe
- Ratios et impulsions par tour configurables par canal (2 PPR par défaut pour Noctua et Deye).
- Bornage strict de la fréquence de sortie entre 0,50 Hz et 600 Hz (correspondant à 18 000 tr/min à 2 PPR).
- Timeout d'inactivité de 1 200 ms (`IN_TIMEOUT_MS`) : en cas d'arrêt ou de déconnexion du ventilateur d'entrée, la sortie est immédiatement désactivée et la ligne relâchée au niveau haut par le pull-up de l'onduleur.
- Écrêtage de sécurité configurable (`maxSimRpm`, 12 000 tr/min par défaut) protégeant l'onduleur contre toute valeur aberrante.

### Filtres / Signal
- Échantillonnage périodique des entrées à 25 kHz (40 µs) avec un déparasiteur temporel exigeant 3 échantillons consécutifs identiques (120 µs de rejet de glitch).
- Filtrage passe-bas exponentiel entier (`EMA_SHIFT = 3`, équivalent \(\alpha = 1/8\)) calculé sans aucun flottant dans l'ISR.
- Fenêtre de plausibilité stricte sur la période mesurée : 2 000 µs (15 000 tr/min) à 200 000 µs (150 tr/min). Les fronts hors bornes incrémentent un compteur de rejet de bruit et réamorcent le filtre.

### WiFi / Réseau
- Mode `WIFI_AP_STA` simultané sans mise en veille du modem (`WIFI_NONE_SLEEP`) pour éviter toute gigue d'horloge induite par l'économie d'énergie.
- Connexion Station non bloquante dans `loop()` avec reconnexion automatique espacée de 30 secondes.
- Publication du service mDNS (`http://deye-fan.local`).

### Gestion mémoire / HEAP
- Interface web stockée en mémoire Flash via `PROGMEM` (`PAGE_INDEX[] PROGMEM`).
- Buffer JSON pré-alloué (`j.reserve(1400)`) limitant la fragmentation mémoire lors du polling régulier de `/api/status`.

### Interface Web
- Interface utilisateur complète avec thème sombre soigné, métriques en temps réel, indicateurs visuels d'état (badges "SIMULATION ACTIVE", "ATTENTE DE SIGNAL", "ECRETE").
- Polling AJAX asynchrone toutes les 600 ms, sans rechargement de page.
- Configuration dynamique des ratios, du nombre d'impulsions par tour, des seuils de sécurité et des identifiants WiFi, avec commande de redémarrage dédiée.

### Persistance / Stockage
- Persistance en EEPROM (512 octets) avec signature magique `0x44594E31`, numéro de version et calcul d'intégrité par CRC32 complet.
- Écriture déclenchée uniquement sur requête explicite de l'utilisateur.

### Documentation hardware/code
- Document `SCHEMA.md` exhaustif, comprenant un synoptique ASCII clair, le schéma de l'alimentation par OU-diode Schottky (1N5819/SS34) et buck DC/DC externe MP1584EN réglé à 5,0 V.
- Calculs précis de dimensionnement : courant de base du transistor de sortie, filtre d'entrée RC (4,7 kΩ / 470 Ω / 22 nF), constante de temps et consommation globale détaillée.

### Qualité de code générale (positif)
- **Structure / organisation** : Architecture exemplaire avec séparation stricte entre le moteur temps-réel NMI, la boucle applicative et le serveur web.
- **Lisibilité / nommage** : Nommage explicite, constantes préfixées, typage entier rigoureux (`uint32_t`, `uint16_t`).
- **Commentaires / documentation inline** : Commentaires techniques de très haut niveau explicitant les choix matériels et registres Xtensa.
- **Gestion d'erreurs / robustesse** : Bornage défensif systématique des paramètres chargés d'EEPROM et des entrées HTTP.
- **Duplication / DRY** : Tableaux de canaux et structures itérées proprement par boucles `for`.
- **Respect des conventions C++/Arduino** : Utilisation adéquate de `volatile`, barrières mémoire assembleur `__asm__ __volatile__("" ::: "memory")` pour le seqlock.
- **Complexité / maintenabilité** : Code modulaire, facile à étendre à un troisième canal si nécessaire.
- **Sécurité basique** : Validation stricte des plages numériques sur les requêtes POST.
- **Testabilité** : Métriques internes exposées en diagnostic (nombre de réveils NMI par seconde, compteurs de fronts rejetés).

---

## ❌ Points techniques négatifs

### WiFi / Réseau (mineur)
- [mineur] : L'endpoint `/api/status` expose le mot de passe du point d'accès en clair (`apPass`) dans le flux JSON reçu par n'importe quel client local — impact : faille de confidentialité mineure sur le mot de passe de l'AP local.

### Temps-réel / ISR (mineur)
- [mineur] : L'utilisation de l'API NMI non documentée du SDK Espressif (`ETS_FRC_TIMER1_NMI_INTR_ATTACH`) et le détournement complet du Timer1 empêchent l'usage des fonctions Arduino `analogWrite()`, `tone()` et de la bibliothèque `Servo` — impact : incompatibilité avec ces modules, bien que documentée dans l'en-tête.

### Qualité de code générale (négatif)
- [mineur] : La fonction `handleStatus()` utilise la classe `String` pour composer la réponse JSON au lieu de sérialiser sur un buffer statique ou via un stream direct — impact : risque mineur de fragmentation du tas à long terme lors de sessions de monitoring continu.

---

## ⭐ Note globale : 8.3/10 — Excellent compétiteur
Implémentation logicielle la plus pointue du banc d'essai grâce au vecteur Timer1/NMI et au seqlock sans verrou, accompagnée d'une documentation électronique irréprochable ; seul un détail de sécurité sur l'API web et la complexité NMI la séparent de la première marche.

## ⭐ Note qualité de code : 8.7/10 — Qualité exceptionnelle
Code d'une rigueur d'ingénierie remarquable, respectant à la lettre les contraintes temps-réel embarquées sur architecture Xtensa monocoeur.
