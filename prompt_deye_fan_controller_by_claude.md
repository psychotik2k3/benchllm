# Prompt technique — Simulateur de tachymètre ventilateurs Deye (ESP8266 / Wemos D1 Mini)

Tu es un expert en électronique embarquée et en programmation Arduino/ESP8266. Tu dois produire deux livrables : le code Arduino complet et le schéma électronique complet. Suis STRICTEMENT les directives d'architecture ci-dessous : elles ne sont pas optionnelles, elles répondent à des contraintes physiques et logicielles réelles du projet.

## Contexte fonctionnel

Un onduleur solaire Deye SUN-8K-SG05LP1-EU-AM2-P voit ses ventilateurs d'origine (NMB 06025VE-12N-CL 6cm et NMB 09225VE-12N-CU 9cm, 12V, 3 fils GND/+12V/tach) remplacés par des Noctua NF-A6x25-FLX (x2) et NF-A9-FLX (x2), plus silencieux mais tournant nettement moins vite. Il faut lire le tach réel de chaque type de Noctua, le multiplier par un ratio réglable, et réémettre un signal tach simulé vers l'onduleur pour qu'il ne détecte pas de sous-régime.

## 1. Spécification du signal tach

- Un ventilateur Noctua délivre **2 impulsions par tour**, sortie **collecteur ouvert** (transistor NPN interne qui tire la ligne à la masse, jamais de tension appliquée activement).
- RPM = (fréquence des impulsions / 2) × 60.
- Le signal simulé en sortie doit reproduire le même format (collecteur ouvert, 2 impulsions/tour) pour être compatible avec ce que l'onduleur attend d'un vrai capteur Hall de ventilateur.
- Fréquences attendues en entrée : quelques Hz à ~80-90 Hz max (NF-A9-FLX ~2500 tr/min, NF-A6x25-FLX ~3000 tr/min). En sortie, avec un ratio pouvant aller jusqu'à x3-x4, prévoir jusqu'à ~350-400 Hz.

## 2. Étage d'entrée (lecture Noctua → ESP8266) — 2 canaux indépendants

- Le fil tach Noctua est collecteur ouvert : il faut une résistance de tirage (pull-up) de 4,7 à 10 kΩ **vers le 3,3V de l'ESP8266, jamais vers le 12V**. Noctua spécifie explicitement de ne pas dépasser ~5,25V sur cette broche : ne jamais tirer vers 12V.
- Ajouter un petit condensateur de filtrage (quelques nF) en parallèle pour absorber les rebonds/bruit électrique, dimensionné pour que la constante de temps RC reste largement inférieure à la période minimale du signal à mesurer (fréquence de coupure au moins 10x supérieure à la fréquence max attendue), afin de ne pas déformer le timing des fronts.
- Utiliser des broches GPIO **compatibles interruption** du Wemos D1 Mini (éviter GPIO16/D0 qui ne supporte pas les interruptions ; éviter GPIO0/D3, GPIO2/D4, GPIO15/D8 qui sont liées au boot). Recommander par exemple D5/D6/D7.
- Aucun transistor n'est nécessaire côté entrée : la résistance de pull-up suffit.

## 3. Étage de sortie (ESP8266 → Deye) — 2 canaux indépendants

- **Contrainte critique : la tension appliquée par l'onduleur sur sa broche tach est inconnue (3,3V, 5V ou 12V selon le firmware/le pull-up interne).** Il est donc interdit de connecter une sortie GPIO de l'ESP8266 directement sur cette ligne.
- Solution obligatoire : reproduire un étage **collecteur ouvert** avec un transistor NPN (au choix parmi 2N2222, 2N3904, BC547, BC548, S9013, S9014, S8050, SS8050, C1815, C945, 2N5551, BC337, S9018) :
  - Base : reliée au GPIO de l'ESP8266 via une résistance de base dimensionnée pour saturer le transistor avec une marge suffisante (calcule Ib à partir du gain β typique du transistor choisi et d'un courant collecteur estimé de quelques mA, avec un facteur de sursaturation ~5-10 pour garantir la commutation même si l'onduleur tire plus de courant que prévu).
  - Émetteur : relié à la masse commune.
  - Collecteur : relié directement au fil tach de l'onduleur, **laissé flottant côté haut** — c'est le pull-up interne de l'onduleur (quel qu'il soit) qui définira le niveau logique haut. Cela résout naturellement la contrainte de tension inconnue et isole le domaine 3,3V de l'ESP8266 du domaine potentiellement 12V de l'onduleur.
  - Vérifier que le Vceo du transistor choisi (généralement 30-45V pour ces références) supporte largement 12V.
- Il faut donc 2 transistors NPN au total (un par canal de sortie), plus leurs résistances de base.

## 4. Alimentation du Wemos D1 Mini

- Contrainte : le Wemos doit être alimenté depuis les connecteurs ventilateurs 12V, sachant que les deux connecteurs ne sont pas garantis actifs simultanément ni à tension identique.
- Prévoir un **OR-ing d'alimentation** : additionner explicitement 2 diodes Schottky (à ajouter à la liste de composants, hors liste stricte fournie par l'utilisateur — le justifier clairement) entre chaque +12V ventilateur et un nœud d'alimentation commun, pour éviter tout retour de courant d'un connecteur vers l'autre si l'un des deux n'est pas alimenté ou à une tension différente.
- Après ce nœud commun, prévoir un condensateur de réservoir (470-1000 µF) pour lisser une éventuelle coupure/bascule d'un connecteur à l'autre.
- Ne PAS compter sur le régulateur linéaire embarqué (AMS1117) du Wemos pour dissiper 12V → 3,3V en direct : avec les pointes de courant WiFi (jusqu'à ~300-400 mA en émission), la dissipation thermique serait excessive et risquerait des brown-outs. Recommander un convertisseur DC/DC step-down (buck) externe 12V → 5V, injecté sur la broche 5V du Wemos (qui dispose déjà de son propre régulateur 5V→3,3V).
- Ajouter un découplage local proche de l'ESP8266 (100 nF céramique + 100-220 µF électrolytique/tantale) pour absorber les appels de courant transitoires du WiFi et éviter les resets par brown-out, problème classique et bien documenté sur ESP8266.

## 5. Architecture logicielle — isolation stricte du signal vis-à-vis du WiFi/serveur web

C'est le point le plus critique du projet : le WiFi, le serveur web et la persistance de configuration NE DOIVENT JAMAIS impacter la précision du signal tach généré. L'architecture logicielle doit donc séparer strictement deux mondes :

### a) Monde temps-réel (priorité haute, piloté par interruptions/matériel)
- **Capture d'entrée** : utiliser `attachInterrupt()` en mode `FALLING` sur les 2 broches tach d'entrée. La routine d'interruption doit être minimale (placée en RAM avec `ICACHE_RAM_ATTR`/`IRAM_ATTR`), se contenter d'horodater le front avec `micros()`, calculer la période depuis le front précédent, et stocker le résultat dans une variable `volatile`. Aucun calcul lourd, aucun log, aucun accès flash dans l'ISR.
- **Génération de sortie** : NE PAS utiliser `delay()`/boucle `loop()` pour générer les impulsions (le `loop()` est perturbé par la pile WiFi, le serveur web, etc.). Utiliser le **timer matériel Timer1 de l'ESP8266** (via la librairie `Ticker` ou l'API bas niveau `timer1_attachInterrupt`/`timer1_write`) configuré en interruption périodique rapide et fixe (par exemple toutes les 50 µs). Cette interruption agit comme un ordonnanceur logiciel qui bascule l'état de chaque sortie en fonction d'une prochaine date de bascule précalculée pour chaque canal (technique de type « pulse scheduler »/« soft PWM » à 2 canaux indépendants), indépendamment de la charge du `loop()` ou du WiFi.
- Toutes les variables partagées entre ISR et `loop()` (périodes mesurées, ratios, périodes cibles de sortie) doivent être déclarées `volatile`, de préférence en `uint32_t` seul (accès atomique sur cette architecture) pour éviter d'avoir à désactiver les interruptions ; si une donnée composite doit être lue/écrite en plusieurs étapes, encadrer l'accès par une courte section critique (`noInterrupts()`/`interrupts()`).
- Filtrage/lissage du RPM mesuré : moyenne glissante sur les 3-4 dernières périodes pour absorber la gigue mécanique du ventilateur, tout en restant réactif.
- Détection de décrochage : si aucun front n'est reçu sur un canal pendant un délai (~1-2 s), considérer RPM = 0 sur ce canal et basculer la LED en mode « attente ».
- Éviter les changements brusques de fréquence de sortie (lisser/limiter la pente de variation de la période cible) pour ne pas produire un signal qui semble erratique aux yeux de l'onduleur.

### b) Monde best-effort (priorité basse, dans `loop()`)
- Serveur web (idéalement `ESPAsyncWebServer` + `ESPAsyncTCP`, non bloquant, plutôt que `ESP8266WebServer` synchrone qui bloquerait le `loop()` pendant le traitement des requêtes HTTP).
- Gestion WiFi (mode `WIFI_AP_STA` simultané, connexion STA en tâche de fond sans bloquer le boot ni la génération du signal si la connexion échoue).
- Mise à jour de la LED d'état.
- Sauvegarde de configuration (voir point 6) : jamais déclenchée à haute fréquence, jamais dans un contexte d'interruption.
- Aucun appel `delay()` de plus de quelques millisecondes n'importe où dans ce monde ; utiliser des machines à état non bloquantes basées sur `millis()`.
- Rappeler que les opérations flash (écriture LittleFS, connexion/scan WiFi) peuvent brièvement désactiver les interruptions ou introduire de la latence : elles doivent rester rares et jamais dans un chemin qui pourrait coïncider avec une bascule de sortie critique — le découplage timer matériel + ISR minimales décrit en (a) est justement ce qui garantit qu'elles n'ont aucun impact mesurable sur le signal.

## 6. Configuration persistante et interface web

- Utiliser **LittleFS** (pas SPIFFS, obsolète) pour stocker un fichier JSON (via `ArduinoJson`) contenant : ratio par canal, identifiants WiFi STA, paramètres AP.
- Chargement au boot ; sauvegarde uniquement sur action explicite de l'utilisateur via l'interface web (pas d'écriture périodique).
- Page web permettant : réglage des 2 ratios (sans recompilation), configuration WiFi (SSID/mot de passe STA, paramètres AP), affichage en temps réel des RPM lus et RPM simulés par canal (rafraîchissement via requête périodique ou websocket léger, sans jamais bloquer la génération de signal).

## 7. Indicateur LED

- Une LED (avec résistance série adaptée) doit refléter l'état de simulation :
  - Allumée fixe : au moins un canal reçoit un signal valide et simule activement.
  - Clignotante : aucun signal d'entrée valide détecté (timeout dépassé), mode attente/repli.
- Préciser sur quelle broche GPIO (éviter les broches sensibles au boot) et comment elle est pilotée depuis le monde best-effort (pas depuis l'ISR).

## 8. Plan de brochage à proposer (Wemos D1 Mini)

Proposer explicitement une table d'affectation des broches D0-D8 couvrant : 2 entrées tach (interruption), 2 sorties tach (vers base transistor), 1 sortie LED, en excluant les broches à comportement spécial au boot (D3/GPIO0, D4/GPIO2, D8/GPIO15) pour les usages critiques, et en rappelant que D0/GPIO16 ne supporte pas les interruptions.

## 9. Livrables attendus

1. **`deye_fan.ino`** : code Arduino complet, commenté, compilable directement pour la carte **LOLIN(WEMOS) D1 mini** sous Arduino IDE, respectant scrupuleusement la séparation temps-réel (ISR + Timer1) / best-effort (loop, WiFi, web) décrite ci-dessus.
2. **Schéma électronique complet** décrivant : la section alimentation (2 connecteurs 12V → OR-ing diodes → réservoir → buck 12V→5V → Wemos 5V → découplage), les 2 circuits d'entrée (Noctua → pull-up 3,3V + filtrage → GPIO interruption), et les 2 circuits de sortie (GPIO → résistance de base → transistor NPN collecteur ouvert → fil tach Deye), avec valeurs de composants chiffrées et justifiées (calculs de résistances de base et de pull-up à l'appui).

Respecte l'intégralité de ces contraintes dans ta réponse ; ne simplifie aucun des points d'isolation logicielle ni d'étage collecteur ouvert, ce sont les points durs du projet.
