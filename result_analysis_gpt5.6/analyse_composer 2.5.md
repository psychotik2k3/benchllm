# Analyse — composer 2.5

Sources analysées (liste exhaustive du dossier) :
- `competitors\composer 2.5\deye_fan\deye_fan.ino`
- `competitors\composer 2.5\deye_fan\schema_electronique.md`

## ✅ Points techniques positifs

### Temps-réel / ISR

- Les ISR Timer1 et GPIO portent `IRAM_ATTR`, et la génération est séparée du serveur Web et de `loop()`.
- Le chemin de sortie évite `digitalWrite()` et utilise un accès registre, ce qui vise une latence plus faible.
- Les variables partagées principales sont `volatile` et les grandeurs 32 bits alignées sont atomiques sur Xtensa dans le cas nominal.

### Démarrage / Boot readiness

- Les sorties sont mises à LOW avant Timer1 et le moteur est lancé avant WiFi/HTTP.
- La première interruption tach ne produit pas de période valide si elle tombe hors plage depuis le boot ; dans le cas habituel, deux fronts sont nécessaires avant une cadence exploitable.

### Génération sortie / Timer1

- L’intention architecturale est d’utiliser Timer1 matériel en `TIM_LOOP`, et non un `Ticker` ou du polling dans `loop()`.
- Les deux canaux ont des demi-périodes indépendantes, dérivées de la période d’entrée et du ratio.

### Fidélité protocole tachymétrique / Fail-safe

- `PULSES_PER_REV` vaut correctement 2 et la conversion `60 000 000 / (période × 2)` est correcte pour les RPM d’entrée.
- En l’absence de tach pendant 3 s, `active` est désactivé, la demi-période est annulée et le NPN est relâché.
- La sortie NPN collecteur ouvert est compatible avec un pull-up Deye à 3,3 V, 5 V ou 12 V, sans imposer de niveau haut.

### Filtres / Signal

- Les périodes hors de `[500 µs, 600 ms]` sont rejetées.
- Le schéma prévoit un pull-up d’entrée externe de 4,7 kΩ vers 3,3 V, conforme à une vraie sortie tach Noctua open-collector, ainsi qu’un condensateur optionnel.

### WiFi / Réseau

- AP+STA sont simultanés et la connexion STA est non bloquante.
- La persistance WiFi du SDK est désactivée afin que les écritures soient maîtrisées par l’application.

### Interface Web

- La page affiche RPM lus et simulés, ratios, état des canaux et adresses AP/STA.
- Les ratios sont validés côté serveur dans la plage 1,0 à 6,0.

### Persistance / Stockage

- La structure EEPROM comporte un magic et un CRC16, et revient aux valeurs par défaut si la validation échoue.
- La sauvegarde ne se produit qu’après une action utilisateur, ce qui limite l’usure flash.

### Documentation hardware/code

- Le principe collecteur ouvert côté entrée et sortie est correctement expliqué.
- Le schéma prévoit un OU à diodes Schottky depuis les deux connecteurs, ce qui empêche en principe le backfeed entre alimentations.
- Le brochage, la LED et les étapes d’accès AP sont faciles à suivre.

### Qualité de code générale (positif)

- **Structure :** sections cohérentes pour ISR, conversion, LED, EEPROM, WiFi, Web et initialisation.
- **Lisibilité :** noms compréhensibles et logique courte.
- **Commentaires :** nombreux commentaires pédagogiques sur les fronts et l’état open-collector.
- **Robustesse :** plages de ratio/période, timeout et CRC sont prévus.
- **DRY :** tableau `channels[2]` et ISR paramétrée réduisent la duplication.
- **Conventions C++/Arduino :** types fixes, `volatile`, constantes regroupées et chaînes flash `F()` sont utilisés.
- **Maintenabilité :** solution compacte, peu de dépendances et responsabilités identifiables.
- **Sécurité basique :** l’AP exige au moins un mot de passe WPA de huit caractères.
- **Testabilité :** fonctions de conversion et de CRC isolées, mais sans tests fournis.

## ❌ Points techniques négatifs

### Exactitude de compilation probable

- [critique] `#define EEPROM_MAGIC 0xDEYE` n’est pas un littéral hexadécimal valide : `Y` n’appartient pas à `[0-9A-F]`. Le sketch ne compile donc pas tel quel.

```49:55:competitors/composer 2.5/deye_fan/deye_fan.ino
#define EEPROM_SIZE         512
#define EEPROM_MAGIC        0xDEYE
#define AP_SSID_DEFAULT     "DeyeFanSim"
#define AP_PASS_DEFAULT     "deye1234"
#define AP_CHANNEL          6
```

- [critique] `WiFi.setSleepMode(WIFI_NONE)` ne correspond vraisemblablement pas à l’énumération du core ESP8266, qui utilise `WIFI_NONE_SLEEP`. C’est un second bloqueur de compilation probable.
- [majeur] La séquence Timer1 omet `timer1_isr_init()`, normalement appelée avant `timer1_attachInterrupt()` sur les versions classiques du core ESP8266. Selon la version, cela peut empêcher l’initialisation correcte même après correction des erreurs de compilation.
- [majeur] Aucune version de core, aucun projet reproductible et aucune CI ne sont fournis. La compilation réelle n’a pas pu être lancée localement faute d’`arduino-cli`, mais les deux erreurs précédentes sont visibles statiquement.

### Temps-réel / ISR

- [critique] `handleTachEdge()` exécute une division flottante dans une ISR GPIO. Sur ESP8266, cela peut appeler des helpers logiciels non garantis en IRAM et allonge fortement l’interruption ; sous accès flash/WiFi, le risque va du jitter au crash.

```164:174:competitors/composer 2.5/deye_fan/deye_fan.ino
float ratio = c->ratio;
if (ratio < RATIO_MIN) ratio = RATIO_MIN;
if (ratio > RATIO_MAX) ratio = RATIO_MAX;
uint32_t outPeriodUs = (uint32_t)(period / ratio);
uint32_t halfUs = outPeriodUs / 2;
uint32_t ticks = halfUs / TIMER_TICK_US;
```

- [majeur] Timer1 reste une interruption niveau 1 partageant la priorité avec WiFi. Le commentaire « totalement isolé » est faux : le trafic radio peut la retarder et introduire de la gigue.
- [majeur] Les helpers GPIO font une lecture-modification-écriture du registre complet. Une modification concurrente d’un autre GPIO entre lecture et écriture peut être perdue ; `GPOS`/`GPOC` atomiques sont préférables.
- [majeur] `ratio` est un `float` modifié dans `loop()` et lu dans l’ISR sans protocole de publication. Une écriture 32 bits est atomique, mais l’ordre entre mise à jour EEPROM/application et ISR n’est pas formalisé.

### Démarrage / Boot readiness

- [majeur] Le lancement de Timer1 avant WiFi ne fournit pas de signal avant deux fronts tach et la montée en régime du ventilateur. Aucune fenêtre de grâce, valeur de boot contrôlée ni comparaison avec le délai d’alarme Deye n’est fournie.
- [majeur] Le schéma de sortie n’a pas de résistance base-émetteur. Pendant reset, les GPIO sont en haute impédance et la base peut flotter, injectant des impulsions parasites malgré le LOW appliqué plus tard par `setup()`.

### Génération sortie / Timer1

- [critique] `timer1_write()` reçoit des ticks Timer1, pas des microsecondes. Avec `TIM_DIV16`, un tick vaut 0,2 µs ; écrire `10` programme donc environ 2 µs, soit 500 kHz, pas 10 µs/100 kHz comme annoncé.

```406:412:competitors/composer 2.5/deye_fan/deye_fan.ino
void setupTimer1() {
  timer1_attachInterrupt(onTimer1);
  // timer1_write() prend des microsecondes : interruption toutes les 10 µs
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
  timer1_write(TIMER_TICK_US);
  timerRunning = true;
}
```

  Impact : le compteur `halfPeriodTicks`, calculé en unités de 10 µs, est consommé toutes les 2 µs ; la fréquence de sortie est environ cinq fois trop élevée, tandis que l’ISR tourne à une cadence extrêmement coûteuse.
- [majeur] Même si 50 ticks étaient utilisés, une ISR fixe à 100 kHz effectuant une boucle sur deux canaux est disproportionnée. Elle consomme du CPU et augmente la contention WiFi alors qu’un Timer1 one-shot sur la prochaine échéance serait beaucoup plus efficace.
- [majeur] Les compteurs redémarrent à zéro après chaque retard effectif. Il n’y a pas d’échéance absolue accumulée, donc chaque retard ISR décale la phase et peut créer une dérive cumulative.
- [mineur] La troncature successive `period / ratio`, `/2`, puis `/10` biaise la fréquence, surtout aux petites demi-périodes.

### Fidélité protocole tachymétrique / Fail-safe

- [critique] À cause de l’unité Timer1 erronée, les RPM réellement présentés au Deye ne correspondent ni aux RPM calculés ni à la formule 2 impulsions/tour affichée dans l’interface.
- [majeur] `lastEdgeUs` est mis à jour avant de valider la période. Des parasites hors plage répétés peuvent donc repousser indéfiniment le timeout alors que `active` et l’ancienne fréquence restent vrais.
- [majeur] Un seul ventilateur est prévu et mesuré par canal, alors que le besoin matériel évoque deux Noctua par connecteur dans les autres conceptions de référence. S’il y en a effectivement deux, la panne du non-mesuré est invisible.
- [majeur] Le timeout de 3 s conserve longtemps une ancienne cadence après stall, retardant l’alarme et masquant une perte de refroidissement.

### Filtres / Signal

- [majeur] Le condensateur de 100 nF avec 4,7 kΩ donne environ 470 µs de constante de temps sur la remontée. C’est très supérieur aux filtres de quelques dizaines de nanofarads mieux justifiés ; à vitesse élevée ou avec un duty-cycle atypique, la marge de seuil se réduit.
- [majeur] Il n’y a aucun EMA, médiane ni exigence de plusieurs périodes cohérentes. Toute période isolée dans la large plage autorisée modifie instantanément la sortie.
- [mineur] Le schéma qualifie le condensateur d’optionnel sans définir quelle variante a été réellement analysée/testée.

### WiFi / Réseau

- [critique] Aucun endpoint n’est authentifié. Tout client AP ou STA peut changer les ratios et credentials WiFi, donc modifier indirectement le signal de sécurité présenté à l’onduleur.
- [majeur] Le mot de passe AP est fixe, public dans le code et non configurable.
- [majeur] Le formulaire réinjecte le mot de passe STA en clair dans l’attribut `value` de la page. Toute personne pouvant charger `/` récupère les credentials domestiques.
- [mineur] Il n’y a ni reconnexion STA périodique, ni mDNS, ni indicateur fiable d’échec de sauvegarde.

### Gestion mémoire / HEAP

- [majeur] La page HTML entière est reconstruite dans un `String` à chaque GET et se recharge automatiquement toutes les deux secondes. Les nombreuses concaténations, dont des nombres et adresses IP, favorisent fragmentation et pics de heap.
- [mineur] Aucune capacité n’est réservée et aucun heap libre/minimal n’est exposé.
- [majeur] Les SSID et mots de passe sont injectés dans le HTML sans échappement, permettant de casser les attributs ou d’injecter du contenu.

### Interface Web

- [critique] L’interface affiche le mot de passe STA dans le HTML et n’a aucune authentification.
- [majeur] Les RPM simulés affichés sont calculés comme `rpmIn × ratio`, indépendamment des ticks réellement générés. L’interface annonce donc une valeur fausse à cause de l’erreur Timer1.
- [mineur] Une valeur de ratio invalide est ignorée, mais la configuration est tout de même sauvegardée et une redirection de succès est renvoyée sans diagnostic.

### Persistance / Stockage

- [critique] Le magic invalide empêche toute compilation et donc toute persistance.
- [majeur] Le CRC protège la structure, mais aucun champ de version ne permet une migration de format.
- [mineur] Il n’y a pas de double copie transactionnelle ; une coupure durant `EEPROM.commit()` peut faire perdre tous les réglages. LittleFS n’est pas obligatoire pour ce faible volume, mais version, CRC valide et stratégie atomique le seraient.

### Documentation hardware/code

- [critique] La documentation ordonne de connecter le nœud issu des Schottky, encore proche de 12 V, directement à la broche `5V` du Wemos et affirme que l’AMS1117 accepte cela. Une D1 mini ne doit pas recevoir 12 V sur `5V`; son régulateur et les composants du rail peuvent être détruits.

```61:68:competitors/composer 2.5/deye_fan/schema_electronique.md
**Branchement Wemos :** connecter VIN+ au pin **5V** du Wemos D1 mini.
Le régulateur AMS1117-3.3 embarqué sur la carte accepte jusqu'à ~15 V en entrée.
Consommation typique : 80–150 mA (WiFi actif).
> **Note thermique :** à 12 V d'entrée, le régulateur linéaire dissipe ~1 W.
> C'est acceptable en permanence pour un ESP8266.
```

  Impact : risque immédiat de destruction, surchauffe ou incendie. Le buck 12 V → 5 V doit être obligatoire, pas conditionnel.
- [majeur] Le schéma de sortie omet les résistances base-émetteur nécessaires à un état sûr pendant boot.
- [majeur] L’alimentation OR-ing empêche bien le backfeed en théorie, mais aucun fusible, réglage préalable du 5 V, vérification des masses ou dimensionnement thermique sérieux n’est fourni.
- [mineur] Aucun test à l’oscilloscope/fréquencemètre ni test de charge WiFi ne valide les affirmations de fréquence et de jitter.

### Qualité de code générale (négatif)

- **Structure :** claire, mais les calculs critiques sont placés dans l’ISR au lieu d’une couche applicative.
- **Lisibilité :** bonne en surface ; plusieurs commentaires factuellement faux sur Timer1 et l’isolation WiFi rendent le code trompeur.
- **Commentaires :** abondants mais non vérifiés, avec contradiction majeure entre unités et API.
- **Robustesse :** compilation bloquée, fréquence ×5, timeout contournable par bruit et boot matériel non sécurisé.
- **DRY :** globalement bon ; la génération HTML manuelle est longue et répétitive.
- **Conventions C++/Arduino :** littéral invalide, mauvais symbole WiFi probable, RMW GPIO et flottants en ISR sont de mauvaises pratiques.
- **Maintenabilité :** absence de version EEPROM/toolchain et dépendance à des hypothèses incorrectes.
- **Sécurité basique :** inexistante côté HTTP, avec fuite explicite du mot de passe STA.
- **Testabilité :** aucun test, compteur de jitter, instrumentation Timer1, preuve de compilation ou procédure de validation électrique.

## ⭐ Note globale : 2,0/10 — Non compilable, fréquence erronée et alimentation dangereuse

L’intention fonctionnelle est reconnaissable, mais plusieurs bloqueurs indépendants rendent cette proposition inutilisable et risquée sans refonte.

## ⭐ Note qualité de code : 4,0/10 — Lisible, mais erreurs fondamentales non détectées

La présentation est ordonnée et pédagogique, toutefois la compilation, les invariants temps-réel, la sécurité et la concordance code-commentaires sont insuffisants.
