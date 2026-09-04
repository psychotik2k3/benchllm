# Analyse — claude-sonnet5-free

Sources analysées (liste exhaustive du dossier) :
- `competitors\claude-sonnet5-free\deye_fan.ino`
- `competitors\claude-sonnet5-free\schema_electronique.md`

## ✅ Points techniques positifs

### Temps-réel / ISR

- Les trois ISR sont explicitement placées en IRAM via `ICACHE_RAM_ATTR`. L’ISR Timer1 est courte, sans allocation ni `digitalWrite()`, et la copie des mesures multioctets vers `loop()` est protégée par `noInterrupts()`.
- La génération est indépendante de `loop()` et du serveur HTTP : Timer1 tourne à 10 kHz avec un pas annoncé et effectivement configuré à 100 µs (`TIM_DIV16`, 500 ticks à 5 MHz).

```215:228:competitors/claude-sonnet5-free/deye_fan.ino
void ICACHE_RAM_ATTR onTimer1ISR() {
  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    if (!chOutputActive[ch]) continue;
    if (chCountdown100us[ch] <= 0) {
      uint8_t pin = (ch == CH_A) ? PIN_TACH_OUT_A : PIN_TACH_OUT_B;
      chPinState[ch] ^= 1;
      if (chPinState[ch]) {
        GPOS = (1UL << pin);
```

- La veille modem est désactivée et le CPU est réglé à 160 MHz. Cela réduit la gigue, sans toutefois rendre une interruption Timer1 de niveau 1 non masquable.

### Génération sortie / Timer1

- Le code emploie bien le Timer1 matériel ESP8266, pas `Ticker`, `tone()`, `delay()` ni du polling dans `loop()`.
- Les fronts de sortie utilisent les registres atomiques `GPOS`/`GPOC`, adaptés au chemin temps-réel et supérieurs à `digitalWrite()` pour la latence.
- La période de sortie est dérivée directement de la période d’entrée et du ratio, avec plafond à environ 15 000 RPM simulés.

### Fidélité protocole tachymétrique / Fail-safe

- La constante `PULSES_PER_REV = 2` et la formule `RPM = 60 000 000 / période_us / 2` sont correctes pour un tach Noctua à deux impulsions par tour.
- En perte de signal détectée après 3 s, la simulation est arrêtée et le NPN est bloqué : la ligne Deye est relâchée au lieu d’émettre une vitesse inventée.
- La sortie matérielle NPN en collecteur ouvert reproduit correctement un tach et accepte un pull-up Deye à 3,3 V, 5 V ou 12 V. La résistance base-émetteur de 10 kΩ maintient le transistor bloqué pendant le reset.

### Filtres / Signal

- Le logiciel rejette les intervalles inférieurs à 2 ms, ce qui constitue un filtre simple contre les doubles fronts et parasites rapides.
- Le schéma prévoit 1 nF au collecteur de l’adaptateur d’entrée et 220 Ω en série vers le GPIO.

### WiFi / Réseau

- Le mode `WIFI_AP_STA` maintient un AP de secours tout en permettant une connexion domestique STA.
- La connexion STA est non bloquante ; un échec réseau n’empêche pas le moteur de signal de fonctionner.

### Gestion mémoire / HEAP

- La page HTML/CSS/JS volumineuse est stockée en `PROGMEM`.
- Les réponses d’état et de configuration utilisent des tableaux `char` bornés plutôt qu’une longue concaténation de `String`, ce qui limite la fragmentation du heap.

### Interface Web

- L’interface donne les RPM mesurés et simulés, l’état de chaque canal, les deux ratios et la configuration AP/STA.
- Les ratios sont validés côté serveur entre 0,1 et 10,0, indépendamment des attributs HTML.

### Persistance / Stockage

- La structure EEPROM possède un magic et une somme de contrôle ; une configuration invalide revient aux valeurs par défaut.
- Les écritures ne se font qu’à la sauvegarde explicite, pas périodiquement, ce qui limite l’usure de la flash.

### Documentation hardware/code

- Le brochage évite les GPIO de strap de boot et documente précisément les étages de sortie, le OU à deux Schottky anti-backfeed et le buck 12 V → 5 V.
- L’alimentation depuis l’un ou l’autre connecteur est correctement conçue : deux diodes empêchent le retour d’énergie entre sorties ventilateur, puis un convertisseur abaisseur fournit 5 V au Wemos.
- Le document explique correctement le risque d’un pull-up Deye inconnu et le choix du collecteur ouvert.

### Qualité de code générale (positif)

- **Structure :** sections nettes matériel, capture, génération, Web, persistance et réseau.
- **Lisibilité :** noms explicites, constantes regroupées, flux `setup()`/`loop()` facile à suivre.
- **Commentaires :** architecture et hypothèses temporelles abondamment expliquées.
- **Robustesse :** bornage des ratios, timeout, état de repos explicite et checksum de configuration.
- **DRY :** tableaux à deux canaux et fonctions paramétrées évitent presque toute duplication.
- **Conventions C++/Arduino :** emploi cohérent de `volatile`, types de taille fixe, `PROGMEM`, API ESP8266 et arithmétique non signée tolérant les wraparounds.
- **Maintenabilité :** constantes centralisées et responsabilités bien séparées.
- **Sécurité basique :** mot de passe AP de huit caractères et mots de passe absents des réponses JSON.
- **Testabilité :** les conversions et la mise à jour d’un canal sont isolées en fonctions, même si aucun test automatisé n’est livré.

## ❌ Points techniques négatifs

### Exactitude de compilation probable

- [mineur] Aucune compilation réelle n’est démontrée et aucun manifeste de version du core ESP8266 n’est fourni. Le code paraît syntaxiquement compilable avec un core ESP8266 compatible avec `ICACHE_RAM_ATTR`, `GPOS/GPOC` et l’API Timer1, mais cette dépendance de version réduit la reproductibilité.

### Temps-réel / ISR

- [majeur] Timer1 reste une interruption de niveau 1, donc la pile WiFi peut la retarder. Désactiver modem-sleep et monter le CPU à 160 MHz réduit la probabilité, mais ne garantit pas l’affirmation « totalement autonome » : des fronts peuvent subir une gigue de plusieurs dizaines de microsecondes sous charge RF.
- [majeur] Les ISR GPIO appellent `micros()` et `millis()` par l’intermédiaire de `handleTachEdge()`. Même si ces API sont généralement utilisables avec le core visé, l’absence de contrainte de version et de vérification de placement IRAM de toutes les dépendances rend le fonctionnement pendant une opération flash moins sûr qu’un horodatage registre/cycle entièrement IRAM.

### Démarrage / Boot readiness

- [critique] Le premier front valide calcule une « période » depuis le boot, car `lastEdgeMicros` vaut zéro. Il peut donc activer immédiatement une fréquence synthétique fausse au lieu d’attendre deux fronts réels.

```188:196:competitors/claude-sonnet5-free/deye_fan.ino
uint32_t now = micros();
uint32_t delta = now - lastEdgeMicros[ch];
if (delta >= TACH_MIN_PERIOD_US) {
  lastPeriodMicros[ch] = delta;
  lastEdgeMicros[ch]   = now;
  lastPulseMillis[ch]  = millis();
}
```

  Impact : au démarrage, le Deye peut recevoir pendant environ 150 ms une cadence liée au temps de boot plutôt qu’au ventilateur. Inversement, avant ce premier front la sortie reste absente ; aucune fenêtre de grâce coordonnée avec le délai d’alarme Deye n’est documentée.

### Génération sortie / Timer1

- [majeur] Le compteur introduit une erreur systématique d’un tick : après un basculement il est chargé à `N`, puis le basculement suivant n’arrive qu’après le passage `N … 0`, soit `N+1` ticks. À la limite basse `N=10`, la demi-période réelle vaut 1,1 ms au lieu de 1,0 ms, presque 9,1 % de RPM en moins.
- [majeur] Les échéances sont reconstruites à partir du nombre de ticks effectivement servis, sans échéance absolue (`nextEdge += période`). Un retard d’ISR décale donc tous les fronts suivants : la gigue WiFi peut se transformer en dérive de phase cumulative.
- [mineur] La résolution de 100 µs quantifie fortement les ratios aux hautes vitesses ; l’arrondi est toujours tronqué avant plafonnement, ce qui biaise la fréquence vers le haut hors erreur du compteur.

### Fidélité protocole tachymétrique / Fail-safe

- [critique] Le schéma d’entrée suppose à tort que le tach Noctua open-collector est tiré en interne vers 12 V. Une sortie open-collector nécessite normalement un pull-up externe ; ici, lorsque le transistor du ventilateur est ouvert, la base de Q1 n’a aucune source de courant, et lorsqu’il conduit elle est à 0 V. Q1 reste donc bloqué dans les deux états.

```107:116:competitors/claude-sonnet5-free/schema_electronique.md
Le tach Noctua est un signal **collecteur ouvert**, tiré au niveau haut par
une résistance interne du ventilateur vers son propre +12V
...
TACH Noctua (repos ~12V, impulsions vers 0V)
        |
      [Rb 10k]
```

  Impact : dans le câblage documenté, les GPIO risquent de ne voir aucun front et le simulateur de ne jamais produire de tach. Il faut tirer le tach vers 3,3 V ou concevoir explicitement un étage adapté à une vraie source 12 V vérifiée.
- [majeur] Un seul tach est observé par paire de deux ventilateurs. Le blocage du second ventilateur reste invisible ; le système masque alors potentiellement une panne de refroidissement réelle au Deye.
- [majeur] Le timeout de 3 s est long pour un fail-safe thermique/alarme et tout parasite valide reçu avant l’échéance le repousse. Il n’existe ni compteur de cadence cohérente, ni seuil de fronts consécutifs, ni diagnostic des rejets.

### Filtres / Signal

- [majeur] Le filtre matériel de 1 nF est associé à un étage d’entrée électriquement incorrect et ne résout pas l’absence de pull-up du tach.
- [mineur] Il n’y a ni moyenne, ni médiane, ni EMA sur les périodes valides. Une seule période perturbée, tant qu’elle dépasse 2 ms, modifie immédiatement la fréquence de sortie et l’affichage.

### WiFi / Réseau

- [majeur] Aucune authentification HTTP, aucun contrôle d’origine et aucun jeton CSRF ne protègent `/save`. Tout client AP ou STA peut changer ratios et identifiants puis redémarrer le contrôleur, avec impact direct sur le signal présenté à l’onduleur.
- [mineur] Le mot de passe AP par défaut est fixe et publié dans le code/documentation ; l’AP permanent élargit la surface d’attaque.
- [mineur] Il n’y a pas de stratégie explicite de reconnexion STA périodique ni de mDNS.

### Gestion mémoire / HEAP

- [mineur] `server.arg()` crée plusieurs objets `String` temporaires lors d’une sauvegarde. Ce n’est pas critique à cette fréquence, mais aucune mesure de heap ni compteur de fragmentation n’est exposé.
- [majeur] Les SSID sont interpolés dans du JSON sans échappement. Un guillemet ou antislash dans un SSID produit une réponse invalide ; la page ne peut alors plus charger la configuration.

### Interface Web

- [majeur] Les entrées issues du réseau sont copiées par `strncpy(..., taille - 1)` sans imposer explicitement le dernier octet à `'\0'`. L’initialisation à zéro atténue le premier usage, mais le contrat reste fragile lors d’évolutions ou de données EEPROM anciennes.
- [mineur] Les erreurs HTTP et validations rejetées ne sont pas détaillées à l’utilisateur ; une valeur hors plage est simplement ignorée tout en répondant « Enregistré ».

### Persistance / Stockage

- [mineur] La « checksum » est une somme pondérée, nettement moins robuste qu’un CRC16/CRC32 face aux corruptions multiples.
- [mineur] EEPROM émulée convient à une petite structure, mais aucune version de format n’est stockée. Une évolution de `Config` invalidera implicitement les données sans migration ; LittleFS n’est pas nécessaire ici, mais une version et un vrai CRC seraient préférables.

### Documentation hardware/code

- [critique] L’étage d’entrée principal est fondé sur une propriété tach Noctua erronée, ce qui invalide la mise en service malgré la qualité du reste du schéma.
- [majeur] La documentation reconnaît que la simulation masque les pannes, mais ne propose ni lecture des deux ventilateurs de chaque paire, ni relais d’alarme, ni coupure de simulation en cas de surchauffe.
- [mineur] Aucun protocole de test à l’oscilloscope/fréquencemètre sous forte charge WiFi n’est fourni, alors que la stabilité temporelle est une exigence centrale.

### Qualité de code générale (négatif)

- **Structure :** bonne globalement, mais la logique d’amorçage de mesure n’est pas modélisée comme un état distinct.
- **Lisibilité :** les commentaires promettent parfois une isolation plus forte que celle garantie par une ISR niveau 1.
- **Commentaires :** très complets, mais l’erreur physique sur le pull-up tach est répétée et peut induire le monteur en erreur.
- **Robustesse :** manque d’échantillons consécutifs, de filtre de période et de boot grace ; défaut compteur `N+1`.
- **DRY :** satisfaisant ; aucun défaut notable.
- **Conventions C++/Arduino :** dépendance aux symboles bas niveau du core non versionnée et chaînes C pas toujours terminées défensivement.
- **Maintenabilité :** le format EEPROM sans version et les hypothèses matérielles non vérifiées compliquent les évolutions.
- **Sécurité basique :** contrôle Web totalement non authentifié sur AP et STA.
- **Testabilité :** aucun test unitaire, test de compilation automatisé, mesure de jitter ou procédure instrumentée.

## ⭐ Note globale : 4,5/10 — Bonne architecture firmware, mais entrée tach probablement inopérante

La génération Timer1 et le collecteur ouvert sont bien pensés, mais l’absence de vrai pull-up d’entrée et les défauts d’amorçage/temporisation empêchent de considérer la solution fiable telle quelle.

## ⭐ Note qualité de code : 7,0/10 — Clair et structuré, insuffisamment validé

Le code est lisible, documenté et relativement maintenable, mais les promesses temps-réel, la sécurité Web et plusieurs invariants critiques ne sont ni testés ni complètement respectés.
