# Analyse — glm5.3

## Sources analysées

- `competitors/glm5.3/deye_fan/deye_fan.ino`
- `competitors/glm5.3/deye_fan/schema_electronique.md`
- `competitors/glm5.3/deye_fan/README.md`

## ✅ Points techniques positifs

### Temps-réel / ISR

- Les wrappers ISR GPIO, la fonction commune `inEdge()` et l’ISR de sortie portent `IRAM_ATTR`.
- Les ISR d’entrée ne font qu’horodater et alimenter un tampon circulaire fixe ; le calcul flottant et le filtrage restent dans `loop()`.
- Le générateur utilise le Timer1 matériel FRC1 en mode `TIM_SINGLE`, pas `Ticker` ni du polling.
- Les commutations de sortie passent directement par `GPOS/GPOC`, donc sans le coût ni la variabilité de `digitalWrite()` dans l’ISR.
- La lecture des tampons d’entrée est protégée par une section critique puis traitée hors interruption.

### Démarrage / Boot readiness

- Les broches de sortie, interruptions d’entrée et Timer1 sont initialisés avant le réseau ; un WiFi lent ne retarde donc pas artificiellement l’installation du mécanisme tach.
- Les sorties sont forcées GPIO bas, donc NPN bloqués, avant l’initialisation réseau. Le schéma ajoute des résistances base-émetteur de 10 kΩ pour maintenir cet état même lorsque les GPIO flottent avant `setup()`.
- La sortie démarre dès qu’au moins quatre fronts permettent une mesure valide, avec une boucle de contrôle à 100 ms ; à vitesse normale, l’acquisition peut être bien inférieure à une seconde.

### Génération sortie / Timer1

- Chaque canal possède sa propre phase et sa propre demi-période.
- Les échéances sont exprimées en temps absolu via le compteur de cycles CPU. `nextEdge += half` évite la dérive cumulative normale d’un réarmement « maintenant + période ».
- En cas de retard, l’ISR rattrape au plus trois fronts puis resynchronise, ce qui borne son temps d’exécution.
- La fréquence est limitée à 5–1500 Hz et le réarmement Timer1 respecte la limite 23 bits.
- Le rapport cyclique est de 50 % et chaque période complète crée un front descendant tach, cohérent avec un capteur standard.

### Fidélité protocole tachymétrique / Fail-safe

- La relation 2 impulsions/tour est correctement et explicitement implémentée : `Hz = RPM/30` et `RPM = Hz×30`.
- La fréquence est mesurée sur les timestamps réellement observés, et non en supposant qu’une tâche périodique s’est exécutée exactement à l’heure.
- Un signal âgé de plus de 2,5 s devient invalide et la sortie est arrêtée par défaut, ligne tach relâchée au niveau haut.
- Le mode cache-panne est explicite, configurable par canal et désactivé par défaut ; la documentation avertit correctement qu’il masque une panne réelle.
- Canaux, ratios, activation et RPM de secours sont indépendants.

### Filtres / Signal

- Le rejet des fronts espacés de moins de 400 µs filtre les parasites rapides.
- La mesure exploite jusqu’à 128 timestamps sur une fenêtre maximale de deux secondes, améliorant la résolution à bas régime.
- Une EMA de coefficient 0,25 stabilise la consigne réémise.
- Le matériel prévoit un pull-up externe 10 kΩ vers 3,3 V, une résistance série 1 kΩ et un condensateur optionnel 1 nF, soit un filtrage RC léger sans tirer l’entrée vers 5/12 V.

### WiFi / Réseau

- AP et STA fonctionnent simultanément ; l’AP reste disponible comme accès de secours.
- Sont prévus : reconnexion STA, portail captif DNS, mDNS, scan asynchrone et désactivation de la persistance WiFi SDK afin d’éviter des écritures flash cachées.
- Les secrets WiFi ne sont pas renvoyés par l’API de configuration ; un champ vide conserve le mot de passe existant.

### Gestion mémoire / HEAP

- La page HTML est en `PROGMEM` et servie directement par `send_P`.
- Les endpoints principaux emploient des buffers `char` fixes, `snprintf`, `strlcpy` et un échappement JSON borné.
- Le statut expose le heap libre, utile pour détecter une dégradation en exploitation.
- Les tampons de timestamps ont une taille fixe et n’allouent rien dans les ISR.

### Interface Web

- L’interface couvre les deux canaux, fréquences et RPM d’entrée/sortie, états normal/cache-panne/attente, réseau, scan, reboot et reset usine.
- Les valeurs sont rafraîchies sans rechargement et les formulaires valident des plages cohérentes côté serveur.
- Les erreurs de validation et d’écriture EEPROM sont renvoyées avec des statuts HTTP adaptés.
- L’échappement HTML côté navigateur et JSON côté firmware réduit les risques d’injection via SSID/hostname.

### Persistance / Stockage

- Le format EEPROM inclut magic, version, taille et CRC32 ; une donnée corrompue ou incompatible revient proprement aux valeurs par défaut.
- Le résultat de `EEPROM.commit()` est vérifié et propagé à l’interface.
- Pour une petite structure fixe rarement écrite, EEPROM émulée est plus simple et appropriée que LittleFS ; aucune écriture périodique n’use la flash.

### Documentation hardware/code

- L’ORing de deux alimentations est correctement réalisé avec deux Schottky SS14/1N5819, empêchant le backfeed entre connecteurs.
- Le chemin d’alimentation complet est spécifié : 12 V des connecteurs → diodes → réserve 470 µF/TVS optionnelle → buck 5 V → broche 5V du Wemos, avec découplage.
- Les sorties sont de vrais collecteurs ouverts NPN avec résistances de base et base-émetteur, compatibles avec une lecture Deye tirée en 3,3 V, 5 V ou 12 V.
- Le document fournit calcul de courant, BOM, tableaux de câblage, contrôle au multimètre, test au fréquencemètre et procédure de calibration.
- Les hypothèses non confirmées sur les RPM NMB et le pilotage du Deye sont signalées au lieu d’être présentées comme des certitudes.

### Qualité de code générale (positif)

- **Structure :** organisation claire en douze sections et modèles distincts pour entrée, mesure, sortie, contrôle, réseau et web.
- **Lisibilité :** noms expressifs, constantes regroupées et commentaires décrivant les invariants temporels.
- **Commentaires :** formules, niveaux électriques, état de repos et stratégie de perte sont documentés près du code.
- **Robustesse :** buffers bornés, CRC, validation serveur, limites de fréquence et rattrapage borné de l’ISR.
- **DRY :** tableaux de deux canaux et fonctions paramétrées évitent presque toute duplication métier.
- **Conventions C++/Arduino :** constantes typées, `static`, `volatile`, `IRAM_ATTR`, `F()`/`PROGMEM`, calculs wrap-safe et API ESP8266 appropriées.
- **Maintenabilité :** configuration versionnée et séparation des responsabilités facilitent l’évolution.
- **Sécurité basique :** AP protégé, secrets non relus via API, validation des tailles et échappement des contenus réseau.
- **Testabilité :** API de statut détaillée, mode fixe cache-panne, logs série, reset usine et procédure de test sur banc rendent les vérifications reproductibles.

## ❌ Points techniques négatifs

### Exactitude de compilation probable

- **[mineur]** Le code a de bonnes chances de compiler avec la carte et le cœur ESP8266 3.1.x documentés, mais il dépend d’API bas niveau (`timer1_isr_init`, `ESP.setCpuFreqMHz`, FRC1) spécifiques à ce cœur. Aucune compilation CI ni version verrouillée ne prouve la compatibilité ; un autre cœur/carte échouera probablement.
- **[mineur]** L’appel `offsetof(Settings, crc)` repose normalement sur `<stddef.h>` inclus transitivement par Arduino. Une inclusion explicite rendrait cette dépendance de compilation plus claire.

### Temps-réel / ISR

- **[majeur]** La documentation promet « aucune influence » du WiFi et environ 1 µs de gigue sans mesure fournie. Sur ESP8266, des sections critiques système et écritures flash peuvent masquer ou retarder FRC1 ; l’architecture évite la dérive cumulative, pas toute gigue.
- **[majeur]** Lors d’un retard, plusieurs basculements peuvent être exécutés presque consécutivement dans la même ISR. Ces impulsions extrêmement étroites peuvent ne pas être reconnues par l’entrée Deye ; la resynchronisation temporelle ne reconstitue donc pas forcément le nombre d’impulsions physiquement observables.
- **[mineur]** La boîte aux lettres `pend*` n’emploie ni section critique ni barrière explicite. Les mots 32 bits alignés sont atomiques sur ESP8266 et `pendFlag` est écrit en dernier, mais une demande précédente encore pendante peut être consommée pendant l’écriture d’une nouvelle demande et mélanger transitoirement des champs.

### Démarrage / Boot readiness

- **[majeur]** Même si l’infrastructure timer est prête tôt, le firmware n’émet rien avant quatre fronts valides. Il n’existe aucun délai de grâce connu côté Deye ni signal de boot optionnel immédiat ; à très basse vitesse, lors d’un démarrage lent ou sans tach, une alarme avant readiness reste possible.
- **[majeur]** Le mode cache-panne n’intervient qu’après environ 7,5 s depuis la dernière mesure valide et, au premier boot sans front, depuis `millis()==0`. Si le délai d’alarme Deye est plus court, ce mode ne protège pas le démarrage.
- **[mineur]** Le README demande d’attendre environ 20 s pour l’interface alors que le signal peut être prêt avant ; aucun indicateur explicite « acquisition initiale terminée » n’est exposé séparément de `valid/run`.

### Génération sortie / Timer1

- **[mineur]** La demi-période est tronquée en entier. À 80 MHz l’erreur de quantification est toutefois très faible dans la plage autorisée.
- **[majeur]** Un changement de ratio applique une nouvelle demi-période au front suivant sans recalcul de phase. C’est raisonnable, mais un changement brutal peut produire une période de transition qui ne correspond exactement ni à l’ancienne ni à la nouvelle fréquence.
- **[mineur]** Une cadence d’ISR de veille à 1 kHz reste active quand aucun canal ne tourne. Elle est beaucoup moins coûteuse qu’un tick fixe 20 kHz, mais un mécanisme événementiel ou une cadence plus lente pourrait réduire la charge idle.

### Fidélité protocole tachymétrique / Fail-safe

- **[critique]** Le postulat « deux tach en parallèle s’additionnent puis on divise par le nombre de ventilateurs » est faux en général. Des collecteurs ouverts en wire-OR peuvent être câblés ensemble électriquement, mais des impulsions de ventilateurs non synchronisés se chevauchent et masquent des fronts ; la fréquence combinée n’est pas une somme fiable. Les RPM par ventilateur et donc la sortie peuvent être sous-estimés ou irréguliers.
- **[critique]** Avec deux tach en parallèle, l’arrêt d’un seul ventilateur ne peut pas être détecté correctement. Le ventilateur restant maintient `valid=true`; la division fixe par deux réduit la valeur, mais ne distingue pas panne, désynchronisation et baisse réelle de régime.
- **[majeur]** Après une perte longue, l’état de l’EMA n’est jamais réinitialisé. Au retour du signal, la sortie converge depuis l’ancienne fréquence au lieu de repartir immédiatement de la nouvelle mesure.
- **[majeur]** Avec cache-panne activé, la sortie normale s’arrête dès que la mesure devient invalide vers 2,5 s, puis la fréquence fixe ne démarre qu’après 7,5 s. Ce trou d’environ 5 s est susceptible de déclencher précisément l’alarme que le cache-panne veut éviter.
- **[majeur]** Le délai de perte de 2,5 s est fixe et relativement long ; une panne réelle peut rester masquée par la dernière sortie durant ce délai.
- **[mineur]** Les 2 impulsions/tour sont globales et non configurables. Elles conviennent aux Noctua/NMB décrits, mais limitent la réutilisation.

### Filtres / Signal

- **[majeur]** Le seuil anti-rebond fixe de 400 µs rejette toute fréquence supérieure à 2,5 kHz ; la sortie est bornée à 1,5 kHz, mais la relation entre filtre, nombre de ventilateurs parallèles et fréquence combinée maximale n’est pas validée formellement.
- **[majeur]** L’EMA stabilise le signal mais ajoute une latence lors des changements de ventilation. Aucun compromis mesuré entre réaction thermique, délai Deye et lissage n’est fourni.
- **[mineur]** Le condensateur 1 nF est optionnel et le filtrage dépend fortement de la longueur/implantation des câbles ; aucune capture oscilloscope ne valide les fronts dans l’environnement bruyant de l’onduleur.

### WiFi / Réseau

- **[critique]** Il n’existe aucune authentification HTTP. Tout appareil sur l’AP ou le LAN STA peut changer ratios, activer le cache-panne, modifier le WiFi, redémarrer ou réinitialiser le module.
- **[majeur]** Aucune protection CSRF ni contrôle `Origin` : une page web malveillante visitée par un utilisateur du même réseau peut envoyer des POST au module.
- **[majeur]** Le mot de passe AP par défaut `noctua12v` est commun, documenté et non forcé à changer au premier démarrage.
- **[mineur]** HTTP et identifiants sont transmis sans TLS. Sur ESP8266 cela peut être un compromis acceptable, mais le LAN/AP doit alors être considéré comme une frontière de confiance explicite.
- **[mineur]** Les retours de `WiFi.softAP`, `DNSServer.start`, `MDNS.begin` et `server.begin` sont peu ou pas exploités pour diagnostiquer un échec partiel.

### Gestion mémoire / HEAP

- **[majeur]** `handleScanJson()` construit encore la réponse dans un `String` avec concaténations répétées ; des scans fréquents peuvent fragmenter le heap malgré les buffers fixes des autres endpoints.
- **[majeur]** `handleWifiPost()` crée plusieurs `String`, puis un `String clean` agrandi caractère par caractère. L’opération est rare, mais une construction dans un buffer fixe serait plus cohérente avec la stratégie mémoire annoncée.
- **[mineur]** Le commentaire « réponses construites dans des buffers statiques » n’est donc pas universellement exact.

### Interface Web

- **[majeur]** Les actions critiques n’exigent ni session ni confirmation serveur ; seules reboot/reset ont une confirmation JavaScript contournable.
- **[majeur]** `loadCfg()` n’est appelé qu’au chargement. Après l’enregistrement partiel d’un canal, l’écran ne relit pas la configuration persistée et peut conserver une valeur locale normalisée différemment.
- **[mineur]** Les `catch` vides de `loadCfg()` et du démarrage de scan masquent la cause d’une erreur.
- **[mineur]** L’interface ne montre pas l’âge du dernier front, le délai avant fail-safe ni le compteur d’erreurs/parasites, informations utiles au diagnostic.

### Persistance / Stockage

- **[majeur]** Lorsqu’une EEPROM invalide est détectée, les valeurs par défaut sont chargées mais pas immédiatement sauvegardées. Ce n’est pas bloquant, mais chaque reboot répète l’état « EEPROM vierge » jusqu’à une sauvegarde utilisateur.
- **[majeur]** Le CRC protège les erreurs accidentelles, pas la confidentialité ni l’authenticité : mots de passe STA/AP sont stockés en clair dans la flash.
- **[mineur]** LittleFS n’est pas nécessaire pour ce schéma fixe, mais une migration future devra incrémenter `SETTINGS_VERSION` et fournir une stratégie de reprise ; aujourd’hui toute évolution incompatible efface logiquement la configuration.

### Documentation hardware/code

- **[critique]** La documentation qualifie le parallèle des sorties tach de deux ventilateurs de « parfaitement supporté » et suppose l’addition des fronts. L’électrique est acceptable, mais la mesure de fréquence ne l’est pas : les recouvrements d’impulsions rendent le résultat non déterministe.
- **[majeur]** Le schéma affirme que le réglage « tach en parallèle » ne corrige que l’affichage et « ne change PAS la fréquence envoyée ». Le code divise pourtant `emaHz` par `parallelFans` avant de calculer la sortie : ce réglage modifie directement la fréquence envoyée au Deye. Cette contradiction peut conduire à une calibration erronée d’un facteur allant jusqu’à quatre.
- **[majeur]** L’ORing évite correctement le backfeed, mais si les connecteurs sont pilotés par tension/PWM et tombent sous le dropout du buck, le Wemos peut redémarrer. Le cache-panne logiciel ne fonctionne évidemment plus sans alimentation ; la documentation le présente pourtant parmi les « protections » de ce cas.
- **[majeur]** La TVS SMBJ14A sur un rail 12 V automobile/industriel peut commencer à conduire selon tolérances et transitoires normaux ; le choix exact devrait être confirmé par la tension maximale réelle des connecteurs et la fiche du buck.
- **[majeur]** Le schéma ASCII des sorties est visuellement ambigu autour des résistances base-émetteur et collecteurs. Le tableau corrige l’intention, mais un vrai schéma électrique réduirait le risque de montage erroné.
- **[mineur]** La procédure recommande 5,0–5,2 V sans préciser la tolérance maximale de l’entrée 5V du modèle exact de D1 mini ; 5,0 V réglé sous charge est le choix le plus prudent.
- **[mineur]** Les revendications de gigue (~1 µs) et d’absence d’influence WiFi ne sont accompagnées d’aucune trace oscilloscope, durée de test ou condition d’écriture flash.

### Qualité de code générale (négatif)

- **[majeur] Structure/robustesse :** architecture solide, mais la boîte aux lettres ISR mériterait un protocole atomique formalisé et le fail-safe cache-panne contient un trou de sortie.
- **[mineur] Lisibilité/commentaires :** excellente dans l’ensemble, mais plusieurs formulations absolues (« aucune influence », « parfaitement supporté ») dépassent ce que démontre le code.
- **[mineur] DRY :** bonne factorisation ; les quelques traitements réseau `String` pourraient être unifiés avec les helpers bornés.
- **[mineur] Conventions C++/Arduino :** dépendances bas niveau correctement ciblées mais peu encapsulées, et plusieurs `#define` pourraient être des `constexpr`.
- **[majeur] Maintenabilité :** forte dépendance aux internes ESP8266 Timer1 et absence de tests automatisés/compilation CI.
- **[critique] Sécurité basique :** toutes les commandes d’administration sont sans authentification ni CSRF.
- **[majeur] Testabilité :** bonne observabilité manuelle, mais aucun test unitaire des calculs wrap-around/CRC, aucun banc automatisé et aucune preuve instrumentée du jitter ou des tach parallèles.

## ⭐ Note globale : 7,5/10 — Solide mais risqué

Solution riche et techniquement avancée, pénalisée par le faux modèle des tach parallèles, le trou du cache-panne et l’administration web ouverte.

## ⭐ Note qualité de code : 8,0/10 — Bonne ingénierie générale

Code structuré, lisible et défensif, avec une dette ciblée sur sécurité, concurrence ISR et validation automatisée.
