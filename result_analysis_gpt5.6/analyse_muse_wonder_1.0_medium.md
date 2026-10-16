# Analyse — muse_wonder_1.0_medium

**Sources analysées** : `competitors/muse_wonder_1.0_medium/deye_fan.ino`, `competitors/muse_wonder_1.0_medium/schema_electronique.md`

---

## ✅ Points techniques positifs

### Temps-réel / ISR

- Les deux ISR GPIO et le handler Timer1 portent `IRAM_ATTR`. Les ISR d'entrée se limitent à lire `ccount` et pousser un entier dans un anneau ; flottants, WiFi, EEPROM et allocations restent dans `loop()`.
- La NMI commute les GPIO par `GPOS`/`GPOC`, sans `digitalWrite()` ni appel applicatif externe. Le binaire ESP8266 3.1.2 place bien `timer1NmiIsr()` en IRAM à l'adresse `0x40100200`; les états qu'elle lit sont en DRAM. Le tableau constant des masques est optimisé en immédiats et n'apparaît pas comme accès flash.
- Les consignes 32 bits et indicateurs 8 bits sont atomiques sur Xtensa. L'ordonnanceur soustrait le temps réellement programmé aux deux canaux puis choisit l'échéance minimale.
- Vérification concrète : compilation réussie sans avertissement pour `esp8266:esp8266:d1_mini` avec le core ESP8266 Arduino 3.1.2. Occupation mesurée : 30 100/80 192 octets de RAM (37 %), 61 719/65 536 octets d'IRAM/cache (94 %) et 301 112/1 048 576 octets de flash (28 %).

### Démarrage / Boot readiness

- Les deux sorties sont forcées à LOW avant l'EEPROM et le WiFi ; avec les résistances base-émetteur de 10 kΩ, les NPN restent bloqués et les lignes Deye sont relâchées pendant reset.
- Les captures et Timer1 sont installés avant le réseau. La connexion STA est lancée en arrière-plan : aucune attente WiFi bloquante ne précède le moteur tach.
- Après obtention d'une période valide, l'activation est immédiate côté `loop()` et la NMI la prend en compte au plus tard au prochain réveil inactif, fixé à 5 ms.

### Génération sortie / Timer1

- Le code utilise bien FRC1 matériel en NMI, `TIM_DIV16`, `TIM_EDGE`, `TIM_SINGLE` et un rechargement direct de `T1L`. Les symboles et signatures employés existent dans le core 3.1.2.
- La résolution est de 0,2 µs. Aux consignes nominales d'environ 4 800 et 6 900 tr/min simulés, l'erreur de quantification d'une demi-période est négligeable.
- Chaque voie possède sa propre demi-période et sa propre phase, malgré l'unique Timer1. La sortie est un carré à 50 % et la ligne est explicitement relâchée à la désactivation.

### Fidélité au protocole tachymétrique / Fail-safe

- La mesure sur fronts descendants et la formule 2 PPR sont correctes :

```cpp
ch.rpm = 30000000.0f / ch.periodUs;
float targetPeriodUs = ch.periodUs / cfg.ratio[idx];
float half = appliedPeriodUs[idx] * (TIMER_MHZ / 2.0f);
```

  Une période électrique d'entrée multipliée en fréquence par le ratio donne donc bien `RPM_simulé = RPM_réel × ratio`.
- Le premier front ne produit aucune période : deux fronts sont requis. Une période hors de 100 µs à 150 ms est rejetée.
- Les parasites rejetés ne rafraîchissent pas `lastPulseMs`. Après 1,2 s sans impulsion valide, RPM et sortie passent à zéro et le collecteur Deye est relâché : une rafale de glitches courts ne peut pas maintenir indéfiniment l'ancien tach.
- Un saut de vitesse supérieur à la fenêtre relative est accepté seulement après quatre périodes cohérentes, ce qui distingue un changement réel d'un outlier isolé.

### Filtres / Signal

- Une validation physique, un rejet relatif de rapport inférieur à 0,70 ou supérieur à 1,43, une confirmation sur quatre périodes et une EMA à 25 % filtrent efficacement le bruit.
- Le filtre est remis à zéro après timeout ; l'ancienne période ne contamine donc pas directement la reprise.
- Le réseau d'entrée documenté respecte le collecteur ouvert Noctua : pull-up externe 10 kΩ vers 3,3 V, série 1 kΩ et condensateur 1 nF.

### WiFi / Réseau

- AP+STA simultané, AP permanent, reconnexion STA non bloquante toutes les 10 s, mDNS et `WiFi.persistent(false)` sont présents.
- Le mot de passe AP usine comporte plus de huit caractères et les mots de passe ne sont pas renvoyés par les API de lecture.
- Le serveur synchrone ne participe pas directement à la génération des fronts, prise en charge par la NMI.

### Gestion mémoire / HEAP

- La page complète est en `PROGMEM` et envoyée avec `send_P`; elle n'est pas reconstruite dans un gros `String` à chaque accès.
- `/api/status` emploie un buffer pile borné de 512 octets et `snprintf`, choix sobre en heap.
- Aucune allocation dynamique n'a lieu dans les ISR.

### Interface Web

- L'interface sombre affiche RPM réels/simulés, ratios, état des deux voies et réseau, avec actualisation AJAX toutes les 500 ms.
- Les ratios sont bornés côté HTML et revalidés côté serveur. Les mots de passe restent masqués et ne sont pas renvoyés au navigateur.
- La réponse HTTP est envoyée avant le redémarrage différé, évitant de couper la requête en cours.

### Persistance / Stockage

- Le format prévoit magic, version et CRC16, et force les quatre chaînes à être terminées après chargement.
- Les écritures ne se produisent que sur action de configuration ; l'EEPROM émulée est adaptée au faible volume si le calcul de CRC est corrigé.

### Documentation hardware/code

- Le schéma couvre l'OR-ing Schottky anti-retour des deux rails, les polyfuses, le buck 5 V, réserve 470 µF, découplage, TVS optionnelle, masse commune, BOM et procédure de test.
- Les sorties BC337 reproduisent un collecteur ouvert compatible avec un pull-up Deye de 3,3 V, 5 V ou 12 V. Les GPIO D5/D6 ne sont pas des straps de boot.
- Le document interdit correctement la mise en parallèle des deux tach Noctua d'une paire et ne raccorde qu'un ventilateur maître par canal.
- Les avertissements haute tension, garantie, température, câblage et test à l'oscilloscope sont détaillés.

### Qualité de code générale (positif)

- **Structure / organisation** : sections nettes pour configuration, acquisition, estimation, NMI, Web et réseau ; fonctions courtes et responsabilités lisibles.
- **Lisibilité / nommage** : unités et rôles apparaissent dans les noms (`periodUs`, `outHalfTicks`, `INPUT_TIMEOUT_MS`).
- **Commentaires / documentation inline** : formules 2 PPR, polarités, atomicité et ordre de boot sont expliqués près du code.
- **Gestion d'erreurs / robustesse** : plages, timeout, CRC prévu, copies bornées et sortie relâchée forment une bonne base.
- **Duplication / DRY** : structures et traitements sont partagés entre les deux canaux ; seules les ISR wrappers sont dupliquées.
- **Respect des conventions C++/Arduino** : types fixes, `volatile`, `static`, `PROGMEM`, `IRAM_ATTR` et arithmétique non signée au wrap sont bien employés.
- **Complexité / maintenabilité** : l'ordonnanceur deux voies reste compact et les paramètres sont centralisés.
- **Sécurité basique** : `snprintf` et `toCharArray` bornent les écritures ; aucun secret n'est renvoyé par l'API.
- **Testabilité** : formules et sous-systèmes sont isolés, et la documentation fournit une procédure de banc, mais aucune suite automatisée.

---

## ❌ Points techniques négatifs

### Persistance / Stockage

- **[critique]** Le CRC couvre `sizeof(cfg) - sizeof(cfg.crc)`, mais `crc` n'est pas le dernier stockage de la structure : le build Xtensa confirme `sizeof(Config) = 216` octets, avec `crc` aux octets 212–213 puis deux octets de padding. Les 214 octets hachés incluent donc le CRC lui-même :

```cpp
cfg.crc = crc16((const uint8_t *)&cfg, sizeof(cfg) - sizeof(cfg.crc));
// ...
uint16_t expected = crc16((const uint8_t *)&cfg, sizeof(cfg) - sizeof(cfg.crc));
if (expected != cfg.crc) return false;
```

  Impact : après sauvegarde, le chargement recalcule le CRC sur une valeur CRC désormais non nulle et rejette presque toujours l'enregistrement. Ratios, SSID et mots de passe reviennent aux défauts à chaque reboot ; `/api/wifi` sauvegarde puis redémarre, donc la nouvelle configuration STA/AP est en pratique inutilisable. Il faut hacher `offsetof(Config, crc)`.
- **[mineur]** Le retour de `EEPROM.commit()` est ignoré et il n'existe ni double slot ni écriture transactionnelle — impact : succès HTTP annoncé malgré un échec, et retour complet aux défauts après coupure durant l'écriture.
- **[mineur]** La validation des ratios chargés n'utilise pas `isfinite()` — impact : un NaN accompagné d'un CRC valide franchit les comparaisons de plage et peut produire une conversion de consigne indéfinie.

### Temps-réel / ISR

- **[majeur]** L'anneau de 16 timestamps n'a aucun test de saturation. L'ISR avance `isrHead` même lorsqu'il rejoint `procTail` — impact : après 16 fronts non consommés, le tampon paraît vide et les données sont silencieusement écrasées. À 3 000 tr/min et 2 PPR, un blocage de `loop()` d'environ 160 ms suffit ; un client lent du serveur synchrone peut provoquer ce cas, puis une longue période aberrante et une sortie retardée ou coupée.
- **[majeur]** Le firmware remplace directement le handler FRC1 par une NMI via une API SDK interne, sans version figée dans le projet ni abstraction — impact : conflit avec toute future utilisation de `analogWrite()`, `tone()` ou du moteur waveform, et maintenance dépendante des internals du core malgré le build 3.1.2 concluant.
- **[mineur]** L'image consomme 94 % de la zone IRAM/cache annoncée par le linker, ne laissant qu'environ 3,8 ko — impact : faible marge pour ajouter des callbacks IRAM ou changer de core.

### Démarrage / Boot readiness

- **[majeur]** Timer1 démarre tôt, mais aucune impulsion de sortie n'est émise avant le spin-up du ventilateur, deux fronts descendants, le traitement de `loop()` puis jusqu'à 5 ms d'attente NMI — impact : fenêtre de tach nul non bornée par rapport au délai d'alarme Deye ; un faux défaut reste possible au démarrage.
- **[majeur]** Le Wemos dépend des rails ventilateur et aucun signal nominal provisoire, maintien d'alimentation ou délai de grâce Deye vérifié n'est fourni — impact : si les deux rails sont absents, PWM à basse tension ou activés seulement après validation, le simulateur ne peut pas devenir prêt.
- **[mineur]** Aucun traitement ou diagnostic brownout/watchdog spécifique n'est prévu — impact : une alimentation marginale provoque des redémarrages répétés sans état explicite.

### Génération sortie / Timer1

- **[mineur]** La fréquence affichée par `simulatedRpm()` est le produit théorique entrée × ratio, alors que la sortie est bornée entre environ 60 et 30 000 tr/min — impact : l'interface peut diverger du signal électrique en cas de consigne saturée.
- **[mineur]** Aucune capture oscilloscope ni mesure de jitter sous trafic n'accompagne l'affirmation « aucun effet mesurable » du WiFi — impact : l'architecture est convaincante, mais la garantie métrologique reste non démontrée.

### Fidélité au protocole tachymétrique / Fail-safe

- **[majeur]** Un seul ventilateur maître est surveillé dans chaque paire — impact : le second Noctua peut se bloquer et supprimer environ la moitié du débit d'air sans aucune baisse du tach simulé ni alarme Deye.
- **[majeur]** Le timeout de 1,2 s continue d'émettre l'ancienne vitesse pendant jusqu'à 1,2 s après un stall — impact : détection thermique retardée de dizaines à centaines de tours simulés.
- **[mineur]** La plage d'entrée 200–30 000 tr/min est beaucoup plus large que les Noctua de 1 600/3 000 tr/min — impact : davantage de signaux parasites plausibles peuvent être acceptés.
- **[mineur]** `pulseCount` est incrémenté pour toute période traitée, y compris rejetée — impact : le diagnostic nommé « pulses » ne représente ni les fronts réels ni les seules mesures valides.

### Filtres / Signal

- **[majeur]** La limitation annoncée à « 2 % par impulsion reçue » est exécutée à chaque tour de `loop()`, pas à chaque nouvelle mesure :

```cpp
float maxStep = appliedPeriodUs[idx] * 0.02f;
float delta = targetPeriodUs - appliedPeriodUs[idx];
if (delta > maxStep) delta = maxStep;
if (delta < -maxStep) delta = -maxStep;
appliedPeriodUs[idx] += delta;
```

  Impact : la convergence dépend de la charge Web/WiFi et se fait généralement en quelques millisecondes par répétitions rapides ; le slew-rate revendiqué n'est ni déterministe ni lié au tach d'entrée.
- **[mineur]** L'en-tête promet une « médiane glissante », mais aucune médiane n'est implémentée — impact : documentation trompeuse et robustesse aux outliers inférieure à l'architecture annoncée.

### WiFi / Réseau

- **[critique]** Aucun endpoint n'a d'authentification ni de protection CSRF. Tout client de l'AP ou du LAN STA peut modifier les ratios, changer le réseau et redémarrer le module — impact : falsification directe du tach perçu par le Deye ou indisponibilité à distance.
- **[majeur]** HTTP est en clair et le mot de passe AP par défaut est publié dans le firmware et la documentation — impact : accès d'administration prévisible et interception des identifiants STA sur le réseau local.
- **[mineur]** Les retours de `WiFi.softAP()` et `WiFi.begin()` ne sont pas contrôlés — impact : l'interface peut annoncer une configuration sauvegardée alors que le réseau n'est pas opérationnel.

### Gestion mémoire / HEAP

- **[mineur]** `/api/config` construit sa réponse par concaténations de `String`, et les conversions d'adresses IP créent des temporaires — impact : fragmentation possible à long terme, même si la page principale et le statut sont correctement optimisés.
- **[mineur]** Aucun suivi du heap libre/minimal ni comportement de repli sur allocation échouée n'est exposé — impact : dérive mémoire difficile à diagnostiquer sur longue durée.

### Interface Web

- **[mineur]** Les erreurs du polling `fetch` sont toutes ignorées — impact : les RPM et états peuvent rester visuellement figés sur d'anciennes valeurs sans indicateur de perte de connexion.
- **[majeur]** Une saisie de ratio invalide est simplement ignorée et la route renvoie tout de même 303 — impact : l'utilisateur peut croire le réglage appliqué alors que l'ancienne valeur reste active.

### Documentation hardware/code

- **[majeur]** Le MP1584 proposé est un buck, pas un buck-boost. Le texte affirme un maintien jusqu'à environ 5 V d'entrée et attribue au seul 470 µF l'absorption de « micro-coupures » — impact : à proximité de 5 V ou avec un rail ventilateur PWM, le 5 V chute et le Wemos peut brown-out précisément pendant les transitions de ventilation.
- **[majeur]** Le calcul de sortie annonce jusqu'à 26 mA avec un pull-up Deye de 470 Ω, mais R3 = 4,7 kΩ et R4 = 10 kΩ ne fournissent qu'environ 0,48 mA net à la base — impact : le rapport forcé dépasse 50 dans ce pire cas et la saturation à 0,2 V n'est pas garantie. Le montage est vraisemblablement correct avec un pull-up de quelques kΩ, mais le pire cas revendiqué ne l'est pas.
- **[mineur]** La mise en commun des masses Deye est prescrite sans conditionner le raccordement à une mesure préalable de différence de potentiel — impact : risque de boucle ou courant indésirable si les retours ne sont pas réellement équipotentiels.

### Qualité de code générale (négatif)

- **[critique] Gestion d'erreurs / robustesse** : le calcul CRC fondé sur la taille plutôt que sur `offsetof` casse la persistance entière malgré une conception apparemment défensive — impact : fonctionnalité de configuration non tenue.
- **[majeur] Structure / organisation** : le fichier unique d'environ 640 lignes mélange moteur NMI, stockage, HTML, CSS, JavaScript et réseau — impact : revues et tests ciblés plus difficiles.
- **[mineur] Lisibilité / commentaires** : les promesses de médiane, de slew par impulsion et d'indépendance WiFi dépassent le comportement démontré — impact : confiance excessive lors de l'intégration.
- **[majeur] Sécurité basique** : commandes critiques anonymes, CSRF possible et transport clair — impact : contrôle réseau du signal de sûreté simulé.
- **[majeur] Testabilité** : aucun test automatisé du CRC/padding, du débordement d'anneau, du wrap, du stall ou de la fréquence — impact : le défaut de persistance et les cas concurrents n'ont pas été détectés.

---

## ⭐ Note globale : 7,7/10 — Excellent moteur compilable, persistance cassée

Le Timer1/NMI, la formule 2 PPR, le fail-safe et l'électronique placent ce candidat parmi les meilleurs des 19 analyses existantes. Il reste derrière les leaders à 8,0–8,4/10, car le CRC rend les réglages non persistants, le boot n'émet aucun tach provisoire et l'administration réseau anonyme contrôle une fonction sensible.

## ⭐ Note qualité de code : 8,1/10 — Très rigoureux, mais invariants non testés

Le code est lisible, factorisé, sobre en heap et réellement compilable sur ESP8266 3.1.2. La faute de layout dans le CRC, le buffer sans détection de saturation, le slew dépendant de `loop()` et l'absence de tests empêchent toutefois de rejoindre les meilleures notes de qualité (8,3–8,5/10).
