# Analyse — gemma4

## Sources analysées

- `competitors/gemma4/README.md`
- `competitors/gemma4/firmware_main.cpp`

## ✅ Points techniques positifs

### WiFi / Réseau

- Le mode `WIFI_AP_STA` est demandé, ce qui constitue une base correcte pour conserver un accès local tout en rejoignant un réseau existant.
- L’AP est protégé par un mot de passe de huit caractères, minimum accepté par WPA2.

### Interface Web

- La page est compacte, lisible et actualise les RPM via `/data` sans rechargement ; les champs de ratio ne perdent donc pas le focus.
- L’interface distingue les mesures 9 cm, 6 cm et une valeur simulée.

### Documentation hardware/code

- Le principe d’utiliser un NPN en collecteur ouvert pour piloter une ligne tach Deye tirée en 3,3 V, 5 V ou 12 V est pertinent.
- Le README cite des résistances d’entrée, une résistance de base et le rôle des principales broches.

### Qualité de code générale (positif)

- **Structure :** le petit fichier sépare constantes, callbacks, page web, `setup()` et `loop()`.
- **Lisibilité :** noms courts mais compréhensibles, page et serveur faciles à parcourir.
- **Commentaires :** l’intention des entrées, de la sortie NPN et de la LED est documentée.
- **Robustesse :** les compteurs partagés sont au moins déclarés `volatile`.
- **DRY :** la page web et les handlers sont concis, sans abstraction inutile.
- **Conventions C++/Arduino :** le squelette `setup()`/`loop()` et les bibliothèques ESP8266 sont employés de manière reconnaissable.
- **Maintenabilité :** le faible volume de code rend une réécriture ou correction aisée.
- **Sécurité basique :** l’AP n’est pas ouvert.
- **Testabilité :** l’endpoint `/data` pourrait servir de point d’observation une fois le firmware rendu fonctionnel.

## ❌ Points techniques négatifs

### Exactitude de compilation probable

- **[critique]** `PIN_TACH_6m` est utilisé dans `attachInterrupt`, alors que seule la constante `PIN_TACH_6cm` existe. L’identifiant non déclaré provoque une erreur de compilation certaine.
- **[critique]** Le fichier se termine par les caractères Markdown ```)` après la dernière accolade. Hors commentaire, ces tokens provoquent également une erreur de compilation.
- **[critique]** Les constantes d’entrée valent 10 et 11 avec le commentaire « Physical Pin D1/D2 ». Sur ESP8266, D1/D2 correspondent à GPIO5/GPIO4 ; GPIO10/11 appartiennent généralement à l’interface flash et ne sont pas des choix valides. Même après correction syntaxique, le binaire risquerait crash ou absence de boot.

### Temps-réel / ISR

- **[critique]** Les ISR GPIO `I_9cm()` et `I_6m()` ne portent pas `IRAM_ATTR`. Sur ESP8266, une interruption pendant une indisponibilité du cache flash peut planter le système.
- **[majeur]** `processData()` est exécuté par `Ticker`, donc en contexte de callback timer logiciel, tout en manipulant des `float` et plusieurs variables partagées sans protocole atomique. Ce n’est ni nécessaire ni robuste.
- **[majeur]** Le README affirme que `Ticker` garde le traitement cohérent pendant les pointes WiFi. `Ticker` n’est pas un Timer1 matériel de génération déterministe et ses callbacks peuvent subir latence et gigue.
- **[majeur]** Aucune section critique ne protège la lecture des compteurs pendant que les ISR les modifient.

### Démarrage / Boot readiness

- **[critique]** Aucune sortie tach n’est générée au boot, ni ultérieurement. Le Deye ne recevra jamais d’impulsions et peut lever immédiatement une alarme ventilateur.
- **[majeur]** Aucun délai de grâce, timeout d’acquisition, indicateur « première mesure valide » ou stratégie de readiness n’existe.
- **[majeur]** L’état initial explicite de `PIN_OUTPUT` n’est pas fixé par `digitalWrite` après `pinMode`; il n’existe pas non plus de pull-down base-émetteur documenté pour garantir le NPN bloqué avant et pendant le boot.

### Génération sortie / Timer1

- **[critique]** Malgré le commentaire « A timer generates a pulse train », aucune fonction ne bascule `PIN_OUTPUT`. Le produit ne simule aucun tach.
- **[critique]** Aucun Timer1 matériel FRC1 n’est configuré. La présence de `Ticker` pour le calcul et la LED ne répond pas au besoin de génération isolée du WiFi.
- **[critique]** Une seule sortie D5 est prévue pour deux entrées et `simRPM` moyenne les deux canaux. Le Deye possède deux lignes tach distinctes : une sortie partagée ne peut fournir deux fréquences indépendantes.
- **[majeur]** Aucun accès registre `GPOS/GPOC` n’est utilisé pour une sortie temps-réel ; le seul `digitalWrite()` pilote la LED.
- **[critique]** Il n’existe aucun calcul de période, rapport cyclique, limite de fréquence, correction de jitter ou prévention de drift.

### Fidélité protocole tachymétrique / Fail-safe

- **[critique]** La formule est incorrecte. Toutes les 0,5 s, `rpm9 = count9 * ratio9` n’intègre ni durée de fenêtre ni 2 impulsions/tour. La formule correcte sur 0,5 s serait `RPM = count × 60 / (2 × 0,5) = count × 60`, avant ratio.
- **[critique]** Les compteurs ne sont jamais remis à zéro. Les valeurs augmentent depuis le boot au lieu de représenter une vitesse instantanée.
- **[critique]** Aucun mécanisme de perte de signal ou stall n’arrête la simulation, ne déclenche un état fail-safe ou ne signale la panne.
- **[majeur]** Le protocole à 2 impulsions/tour n’est ni implémenté ni même explicité dans les formules du README.
- **[majeur]** La moyenne arithmétique de RPM issus de ventilateurs de tailles différentes n’a pas de sens pour les deux entrées tach indépendantes du Deye.

### Filtres / Signal

- **[majeur]** Aucun anti-rebond, filtre temporel, rejet des fronts trop rapprochés, moyenne glissante/EMA, plage plausible ou timeout n’existe.
- **[majeur]** Le README cite une résistance 10 kΩ « d’entrée » sans dire clairement si elle est série ou pull-up, sans imposer un pull-up 3,3 V ni proposer de filtre RC ; un câblage ambigu peut exposer l’ESP8266 ou rendre les niveaux instables.

### WiFi / Réseau

- **[critique]** Le code n’appelle jamais `WiFi.begin()` : le mode station est activé mais aucune connexion STA n’est configurée, contrairement à la promesse du README.
- **[critique]** Le « WiFi Manager » annoncé n’existe pas : aucun formulaire SSID/mot de passe, scan, reconnexion ou persistance réseau.
- **[majeur]** Aucun endpoint n’authentifie l’utilisateur. Tout client connecté peut lire l’état ; si des routes de configuration étaient ajoutées sur la même base, elles seraient exposées.
- **[majeur]** Le mot de passe AP `12345678` est une valeur triviale, fixe et publiée.

### Gestion mémoire / HEAP

- **[majeur]** La grande page HTML est un `const char*` sans `PROGMEM`; son stockage/usage n’est pas optimisé pour le heap limité de l’ESP8266.
- **[majeur]** Le JSON est construit par concaténations répétées de `String`, source possible de fragmentation du heap à long terme.
- **[mineur]** Aucun suivi du heap, buffer statique ou stratégie de réponse sans allocation n’est prévu.

### Interface Web

- **[critique]** Les champs ratio n’ont aucun handler de sauvegarde et ne sont jamais lus par le firmware. Les ratios affichés sont décoratifs.
- **[majeur]** L’interface promet une configuration mais ne propose ni bouton, ni POST, ni validation, ni retour d’erreur.
- **[majeur]** Le JavaScript ne gère pas les erreurs HTTP/réseau ; les valeurs restent silencieusement obsolètes.
- **[mineur]** Les unités RPM ne sont pas affichées près des valeurs.

### Persistance / Stockage

- **[critique]** Il n’existe aucune persistance, alors que le besoin implique des ratios et paramètres réseau durables.
- **[majeur]** Ni EEPROM, ni LittleFS, ni version de schéma, ni CRC ne sont utilisés. Après chaque redémarrage, même une future configuration RAM serait perdue.

### Documentation hardware/code

- **[critique]** Aucune alimentation depuis les deux connecteurs n’est conçue : pas d’ORing par diodes, donc pas de protection contre le backfeed entre deux sources 12 V potentiellement asynchrones.
- **[critique]** Aucun buck 12 V → 5 V n’est spécifié. Le chemin d’alimentation du Wemos est absent, rendant le schéma inutilisable et potentiellement dangereux si 12 V est appliqué directement.
- **[critique]** Une seule sortie NPN est documentée alors que deux sorties tach Deye sont nécessaires.
- **[majeur]** L’affirmation « isolates the Weemos from the ground/voltage » est trompeuse : un NPN non opto-isolé protège le GPIO de la tension de pull-up, mais exige une masse commune et ne fournit pas d’isolation galvanique.
- **[majeur]** Le collecteur ouvert est compatible en principe avec 3,3/5/12 V, mais le document ne donne ni raccordement émetteur/collecteur complet, ni pull-down de base, ni calcul de courant/tension du transistor, ni procédure au multimètre.
- **[majeur]** Il n’existe pas de schéma des entrées collecteur ouvert, de nomenclature complète, de découplage, de contrôle avant mise sous tension ou de procédure de calibration.
- **[mineur]** Le README orthographie « Weemos » au lieu de Wemos et emploie « Physical Pin D1 » pour une constante GPIO erronée, ce qui augmente le risque de mauvais câblage.

### Qualité de code générale (négatif)

- **[critique] Structure/robustesse :** le livrable est un prototype incomplet qui ne compile pas et n’implémente pas sa fonction principale.
- **[majeur] Lisibilité/commentaires :** plusieurs commentaires et le README annoncent Timer, WiFi Manager, ratios et sortie qui n’existent pas dans le code.
- **[mineur] DRY :** la duplication des deux ISR est bénigne, mais aucune abstraction canal ne prépare les deux sorties requises.
- **[critique] Conventions C++/Arduino :** mauvais mapping GPIO, identifiant fautif, tokens Markdown résiduels, ISR non IRAM et partages non synchronisés.
- **[critique] Maintenabilité :** exigences centrales absentes, constantes contradictoires et aucune configuration persistante.
- **[majeur] Sécurité basique :** AP à secret trivial et aucune authentification applicative.
- **[critique] Testabilité :** aucune sortie à mesurer, aucune procédure de compilation, aucun test, aucune instrumentation de fréquence ou de perte de signal.

## ⭐ Note globale : 0,5/10 — Prototype inutilisable

Le concept d’interface et de NPN est esquissé, mais le code ne compile pas, ne génère aucun tach et omet l’essentiel du matériel.

## ⭐ Note qualité de code : 1,0/10 — Implémentation incohérente

La présentation est simple, mais les erreurs certaines, les promesses non implémentées et l’absence de robustesse rendent le firmware inutilisable.
