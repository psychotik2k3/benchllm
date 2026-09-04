# Analyse — deepseek4 flash

Sources analysées (liste exhaustive du dossier) :
- `competitors/deepseek4 flash/deye_fan.ino`
- `competitors/deepseek4 flash/README.md`
- `competitors/deepseek4 flash/SCHEMA_ELECTRONIQUE.md`

## ✅ Points techniques positifs

### Temps-réel / ISR

- La conception vise explicitement le Timer1 matériel FRC1, et non `Ticker` ou un polling dans `loop()`.
- L’ISR de génération est marquée `IRAM_ATTR`, n’appelle ni bibliothèque Arduino, ni flottant, ni flash et écrit les registres GPIO `W1TS/W1TC`.
- Les bascules GPIO sont atomiques et groupées : si deux canaux expirent ensemble, leurs bits peuvent être appliqués dans la même ISR.
- Le mode par défaut utilise le vecteur NMI Timer1, plus résistant aux sections critiques WiFi qu’une interruption masquable normale.
- Les calculs de ratio coûteux restent dans `loop()` ; l’ISR manipule uniquement des entiers et un calendrier d’événements.

### Démarrage / Boot readiness

- Les sorties sont forcées `LOW` avant l’initialisation du timer et du réseau ; avec les NPN décrits, cela relâche les deux lignes tach.
- Timer1 et les interruptions d’entrée sont initialisés avant le WiFi, mDNS et le serveur HTTP.
- Le système ne démarre normalement une voie de sortie qu’après l’existence d’une période d’entrée.

### Génération sortie / Timer1

- L’event-calendar réarme FRC1 sur la prochaine échéance utile au lieu d’imposer 20 000 interruptions par seconde ; cela réduit fortement la charge CPU par rapport à un tick fixe de 50 µs.
- La résolution annoncée de 0,2 µs correspond à l’APB 80 MHz divisée par 16.
- La prochaine échéance progresse avec `g_nextTicks[i] += hp` : une latence ponctuelle ne décale pas systématiquement toute la phase future.
- Les ratios sont convertis en Q16 ; aucun calcul flottant n’est nécessaire dans le chemin temps-réel.
- Chaque canal a sa demi-période et sa prochaine échéance indépendantes.

### Fidélité protocole tachymétrique / Fail-safe

- `PULSES_PER_REV` vaut explicitement 2 et intervient dans les deux conversions RPM.
- La formule `60 000 000 / (période × 2)` est correcte pour deux impulsions tachymétriques par tour.
- Le signal de sortie est arrêté lors d’une perte d’entrée ; le système ne cherche pas volontairement à masquer un ventilateur arrêté.
- Le timeout adaptatif `max(2 s, 4 × période)` tient mieux compte des régimes lents qu’une valeur fixe.
- Les périodes inférieures à 400 µs sont rejetées comme parasites.

### Filtres / Signal

- Le filtre EMA `α = 1/8` est peu coûteux, sans allocation et lisse les variations de période.
- La documentation propose un condensateur 1 nF optionnel et un clamp d’entrée pour limiter les parasites et surtensions.

### WiFi / Réseau

- AP et STA fonctionnent simultanément, avec AP disponible même sans configuration STA.
- La connexion STA est non bloquante et une reconnexion est tentée toutes les 20 s.
- mDNS fournit un nom local en plus de l’adresse AP.

### Gestion mémoire / HEAP

- La page HTML est en `PROGMEM`.
- La réponse d’état utilise un tampon global borné de 640 octets et `snprintf_P`, limitant les allocations répétées.
- Configuration, snapshots et état temps-réel sont statiques ; aucune allocation n’est effectuée dans les ISR.
- Les chaînes chargées depuis EEPROM sont explicitement terminées par NUL.

### Interface Web

- L’interface présente état live, période, RPM réel, ratio et RPM simulé pour les deux canaux.
- Les sliders modifient immédiatement chaque ratio et un formulaire séparé rend les changements persistants.
- Le JavaScript échappe les chaînes avant leur insertion dans le tableau WiFi.

### Persistance / Stockage

- La structure EEPROM inclut magic, version et octet de contrôle, puis borne à nouveau les ratios après lecture.
- Les paramètres par défaut sont immédiatement sauvegardés lorsque le contenu est invalide.
- `EEPROM.put()` réduit généralement les modifications inutiles par rapport à une réécriture octet par octet.

### Documentation hardware/code

- README et schéma couvrent compilation, mise en route, brochage, BOM, raccordement et procédure de réglage.
- Les sorties NPN en collecteur ouvert reproduisent correctement le comportement tach et tolèrent une pull-up Deye à 3,3 V, 5 V ou 12 V.
- Les deux alimentations 12 V sont réunies par OR-ing Schottky ; cela évite le backfeed entre connecteurs.
- Le buck 12 V → 5 V est correctement exigé pour alimenter `VIN` sans dissipation excessive.
- La documentation donne les courants de base/collecteur et demande une masse commune.

### Qualité de code générale (positif)

- **Structure :** sections clairement délimitées pour registres, ISR, persistance, Web, timer, WiFi et boucle.
- **Lisibilité :** noms explicites, unités dans les identifiants et commentaires abondants.
- **Commentaires :** architecture NMI/event-calendar, formules et polarités sont détaillées.
- **Robustesse :** bornes, timeout adaptatif, saturation des compteurs et configuration versionnée montrent une intention défensive.
- **DRY :** les données par canal sont regroupées en tableaux et le traitement principal est factorisé dans `updateOutChannel`.
- **Conventions C++/Arduino :** `IRAM_ATTR`, `PROGMEM`, `F()`, largeur d’entiers fixe et arithmétique 64 bits sont employés à bon escient.
- **Maintenabilité :** constantes de réglage centralisées et séparation des fonctions facilitent l’évolution.
- **Sécurité basique :** mot de passe AP activé par défaut et bornage des entrées numériques.
- **Testabilité :** fonctions de conversion et CRC sont isolées ; le fallback `USE_NMI_TIMER=0` facilite une comparaison expérimentale.

## ❌ Points techniques négatifs

### Exactitude de compilation probable

- [critique] `handleState()` tente `s = g_snap` alors que `g_snap` est `volatile` et `s` ne l’est pas. En C++, l’opérateur d’affectation implicite de `SnapT` n’accepte pas une source `volatile`; impact : erreur de compilation probable du type « discards qualifiers » sur les cores standards.

```410:418:competitors/deepseek4 flash/deye_fan.ino
static void handleState()
{
    uint32_t ratios[2];
    SnapT s;
    noInterrupts();
    s = g_snap;
    ratios[0] = g_ratioX100[0];
    ratios[1] = g_ratioX100[1];
```

- [majeur] Le mode NMI dépend de symboles ROM et de détails privés (`NmiTimSetFunc`, adresses FRC1, `ETS_FRC_TIMER1_INUM`) susceptibles de changer entre versions du core ; impact : compilation/édition de liens ou fonctionnement non portable malgré la revendication « 2.7.4 ou plus ».
- [majeur] Aucune configuration reproductible ne verrouille la carte, le core et les options ; impact : un firmware compilant avec une ancienne version peut échouer ou changer de comportement avec une version récente.

### Temps-réel / ISR

- [critique] Le code affirme protéger les données partagées avec `noInterrupts()`, mais une NMI est précisément non masquable par cette fonction. `g_halfTicks`, `g_nextTicks`, `g_outLevel` et les registres FRC1 peuvent donc être lus/modifiés par la NMI au milieu d’une mise à jour depuis `loop()` ; impact : échéance incohérente, impulsion perdue ou timer réarmé avec une ancienne valeur.
- [majeur] `updateOutChannel()` réécrit directement `REG_FRC1_LOAD/COUNT` pendant que la NMI peut aussi les écrire ; impact : course matérielle sur le compteur et intervalle de sortie ponctuellement faux.
- [majeur] Les accès 32 bits unitaires sont atomiques, mais l’invariant « prochaine échéance + demi-période + compteur réveillé » ne l’est pas ; impact : le commentaire de sûreté masque une vraie concurrence inter-contexte.
- [mineur] « jitter ~0 » est techniquement faux : latence NMI, temps d’exécution de l’ISR, contention bus et échéances simultanées subsistent ; impact : spécification impossible à vérifier telle quelle.

### Génération sortie / Timer1

- [critique] Lorsqu’aucune voie n’est active, l’ISR arme une attente maximale et place cette valeur dans `g_lastDelta`. Pour réveiller une voie, `updateOutChannel()` force ensuite LOAD/COUNT à 2 ticks mais ne remplace pas `g_lastDelta`. À l’ISR suivante, l’horloge logique avance donc d’environ 1,677 s alors que 0,4 µs seulement s’est écoulée ; impact : calendrier désynchronisé et sortie pouvant basculer à une cadence proche de l’attente maximale au lieu de la fréquence demandée.
- [critique] En mode NMI, la NMI peut se produire après le réveil du compteur mais avant `g_halfTicks[ch] = half`. Elle ne voit alors aucune voie active et rendort FRC1 pour la durée maximale ; impact : première impulsion retardée d’environ 1,677 s ou activation erratique.
- [critique] L’écriture de `REG_FRC1_COUNT` est non documentée comme mécanisme de rechargement dans l’API Arduino ESP8266 ; selon le silicium/core, le registre COUNT peut être en lecture seule ou avoir une sémantique différente. L’implémentation contourne l’API éprouvée sans preuve sur cible ; impact : timer pouvant ne pas redémarrer ou présenter des intervalles erronés.
- [majeur] Si une ISR est suffisamment tardive pour manquer plusieurs échéances d’un canal, le code ne bascule qu’une fois et ajoute une seule demi-période ; impact : impulsions manquantes et calendrier restant en retard, avec rafale d’ISR minimales jusqu’au rattrapage.
- [majeur] La borne maximale de demi-période est 0x00400000 ticks, soit environ 0,839 s, alors que l’entrée accepte jusqu’à 120 s ; impact : aux très faibles régimes la sortie est artificiellement accélérée, en contradiction avec le ratio demandé.
- [mineur] Le ratio maximal ×20 peut générer des fréquences irréalistes et solliciter fortement la NMI ; impact : charge CPU et signal hors plage de ce que l’onduleur attend.

### Démarrage / Boot readiness

- [critique] Le premier front peut être accepté comme une période entre le boot (`g_inLastUs = 0`) et ce premier front dès que le délai dépasse 400 µs. Ce n’est pas une période tach réelle ; impact : émission initiale d’un RPM arbitraire avant le deuxième front.
- [majeur] EEPROM est lue et éventuellement écrite avant l’activation du timer et des entrées ; impact : fenêtre sans tach au boot, susceptible de déclencher une alarme Deye.
- [majeur] Le schéma prétend que des bases de NPN flottantes « restent bloquées », sans résistance base-émetteur ; impact : impulsion parasite possible avant que les GPIO soient configurés.
- [majeur] Aucun temps de boot maximal ni délai d’alarme de l’onduleur n’est caractérisé ; impact : boot readiness non démontrée.

### Fidélité protocole tachymétrique / Fail-safe

- [majeur] L’ISR met `g_inLastUs` à jour même lorsqu’une période est rejetée ; des parasites espacés de moins de 400 µs peuvent repousser indéfiniment le timeout tout en conservant l’ancienne période valide ; impact : simulation pouvant rester active après perte du vrai tach.
- [majeur] `g_inEdgeCnt` compte aussi le premier front et les fronts dont la période est invalide ; impact : RPM affiché sur la fenêtre d’une seconde peut signaler une rotation inexistante ou surestimée.
- [majeur] La corroboration `live` emploie seulement `STALL_MIN_US` (2 s), alors que l’arrêt effectif utilise `max(2 s, 4 × période)` ; impact : interface indiquant « mort » alors que la sortie continue encore.
- [mineur] Le comptage RPM suppose une fenêtre exactement égale à une seconde, mais `tWindow` est remis à `now` après une boucle potentiellement retardée ; impact : erreur proportionnelle au retard de boucle.

### Filtres / Signal

- [majeur] L’EMA est recalculée à chaque passage de `loop()` avec la même dernière période, même lorsqu’aucun nouveau front n’est arrivé ; impact : le filtre ne représente pas huit mesures et sa constante de temps dépend de la vitesse de boucle.
- [majeur] Le clamp à diode vers le rail 3,3 V injecte jusqu’à environ 1,8 mA dans ce rail si une vraie pull-up 12 V existe. Aucun dispositif d’absorption de ce courant/back-power n’est spécifié ; impact : élévation du 3,3 V ou alimentation parasite de l’ESP lorsque celui-ci est éteint.
- [mineur] Le texte mélange tach Noctua à collecteur ouvert et « pull-up interne 12 V ». Un Noctua standard fournit normalement un collecteur ouvert nécessitant une pull-up externe ; impact : schéma conceptuellement ambigu et diagnostic plus difficile.

### WiFi / Réseau

- [critique] Aucun contrôle d’accès HTTP n’est présent ; toute personne sur AP ou STA peut changer les ratios, identifiants et persistance ; impact : falsification distante de l’état ventilateur.
- [critique] `/api/state` renvoie en clair les mots de passe STA et AP à tout client non authentifié ; impact : fuite directe des identifiants du réseau domestique et du point d’accès.

```430:436:competitors/deepseek4 flash/deye_fan.ino
    int n = snprintf_P(g_stateBuf, sizeof(g_stateBuf),
        PSTR("{\"ch\":[{\"live\":%u,\"r\":%lu,\"rpm\":%lu,\"sim\":%lu,\"inUs\":%lu,\"outUs\":%lu},"
             "{\"live\":%u,\"r\":%lu,\"rpm\":%lu,\"sim\":%lu,\"inUs\":%lu,\"outUs\":%lu}],"),
        (unsigned)s.live[0], (unsigned long)ratios[0], (unsigned long)s.rpmIn[0], (unsigned long)s.rpmOut[0], (unsigned long)s.inUs[0], (unsigned long)s.outUs[0],
        (unsigned)s.live[1], (unsigned long)ratios[1], (unsigned long)s.rpmIn[1], (unsigned long)s.rpmOut[1], (unsigned long)s.inUs[1], (unsigned long)s.outUs[1]);
    if (n < 0) n = 0;
```

- [majeur] `/api/set` modifie l’état via GET, sans CSRF ni persistance annoncée clairement ; impact : une simple image/lien Web peut altérer un ratio.
- [majeur] Les SSID et mots de passe sont insérés dans du JSON avec `%s` sans échappement JSON ; impact : guillemet, barre oblique ou caractère de contrôle casse la réponse et peut injecter du contenu.
- [mineur] La conversion manuelle de `WiFi.localIP()` en entier dépend de l’endianness ; impact : adresse STA potentiellement affichée à l’envers.

### Gestion mémoire / HEAP

- [majeur] `ESP8266WebServer::arg()` crée de nombreuses `String`; impact : fragmentation du heap lors de réglages répétés, même si l’état périodique utilise un tampon statique.
- [majeur] Le second `snprintf_P` peut tronquer silencieusement la fin du JSON, notamment avec quatre chaînes longues ; impact : réponse invalide et interface inutilisable.
- [mineur] Aucun suivi de heap libre/minimal n’est disponible ; impact : fuite ou fragmentation difficile à diagnostiquer.

### Interface Web

- [critique] L’interface préremplit dans le DOM les mots de passe STA et AP renvoyés par l’API ; impact : tout utilisateur de la page, script local ou capture d’écran peut récupérer les secrets.
- [majeur] Le champ uptime affiche `Date.now()/1000`, c’est-à-dire le temps Unix du navigateur et non l’uptime ESP ; impact : information système fausse malgré l’existence inutilisée de `g_snapUptime`.
- [majeur] Le slider déclenche une requête à chaque événement `input` sans debounce ; impact : rafale HTTP, charge CPU/heap et changement de ratio concurrent avec le timer.
- [mineur] Les erreurs de sauvegarde EEPROM et de connexion WiFi ne sont pas remontées à l’utilisateur ; impact : faux succès opérationnel.

### Persistance / Stockage

- [majeur] La fonction appelée `crc8` n’est qu’une somme modulo 256, beaucoup moins robuste qu’un CRC-8 ; impact : certaines corruptions multioctets se compensent et restent indétectées.
- [majeur] EEPROM émulée écrit dans la flash sans stratégie de double slot/journal ; impact : configuration perdue lors d’une coupure pendant `commit()`.
- [mineur] Aucun compteur/version de migration autre qu’un octet fixe n’est traité ; impact : évolution future de structure nécessitant un reset complet.

### Documentation hardware/code

- [majeur] Le schéma de clamp présente un signal source 12 V alors que les ventilateurs cités ont une sortie open collector ; il ne montre pas clairement la pull-up locale nécessaire lorsque cette source est flottante ; impact : entrée pouvant rester indéterminée malgré `INPUT_PULLUP` interne relativement faible.
- [majeur] Les références de transistors sont interchangeables dans la BOM sans pinout propre à chacune ; impact : câblage collecteur/base/émetteur erroné, car BC547 et 2N2222 n’ont pas nécessairement le même ordre de broches.
- [majeur] Aucune résistance base-émetteur, protection ESD de connecteur, fusible obligatoire ou validation des masses lors d’alimentations asymétriques ; impact : comportement de boot et robustesse terrain non garantis.
- [mineur] « jitter quasi nul » et compatibilité large du core ne sont soutenus par aucune mesure oscilloscope ni matrice de compilation ; impact : documentation promotionnelle plutôt que vérification reproductible.

### Qualité de code générale (négatif)

- **Structure :** architecture ambitieuse mais fondée sur des registres privés et des invariants concurrents non encapsulés ; [majeur], impact : correction risquée.
- **Lisibilité :** très nombreux commentaires, parfois inexacts, masquent les points critiques NMI ; [majeur], impact : revue trompée par l’intention.
- **Commentaires :** revendications « NMI non retardable » et « jitter ~0 » ne constituent pas une borne mesurée ; [majeur], impact : garanties non auditables.
- **Robustesse :** build probablement cassé, premier front invalide et fail-safe contournable par parasites ; [critique], impact : solution non déployable telle quelle.
- **DRY :** les ISR d’entrée sont dupliquées et peuvent diverger ; [mineur], impact : maintenance plus coûteuse.
- **Conventions C++/Arduino :** copie incorrecte d’un objet volatile et contournement d’API par adresses absolues ; [critique], impact : non-portabilité et erreur de compilation.
- **Maintenabilité :** dépendance aux détails ROM/FRC1 non versionnés ; [majeur], impact : mises à jour du core dangereuses.
- **Sécurité basique :** exposition des mots de passe et API sans authentification ; [critique], impact : compromission réseau et fonctionnelle.
- **Testabilité :** aucun test unitaire, build CI, test de course NMI ou capture de jitter ; [majeur], impact : propriétés centrales non prouvées.

## ⭐ Note globale : 3,4/10 — Excellente intention temps-réel et hardware complet, mais le réveil FRC1 désynchronise le calendrier, le build est probablement cassé et les secrets sont exposés.

L’event-calendar sur registres est techniquement intéressant, mais son passage idle→actif est fonctionnellement incorrect et doit être reconçu, compilé puis validé sur oscilloscope.

## ⭐ Note qualité de code : 3,9/10 — Code très documenté mais trop dépendant d’internals, avec une erreur C++ probable, une course NMI centrale et des défauts critiques de sécurité.

La présentation est soignée, toutefois robustesse, conventions de concurrence, reproductibilité et testabilité restent insuffisantes.
