# Analyse — glm_5.2_high

**Sources analysées** (liste exhaustive du dossier) :

- `competitors/glm_5.2_high/deye_fan.ino`
- `competitors/glm_5.2_high/schema_electronique.md`

---

## ✅ Points techniques positifs

### Temps-réel / ISR

- Les deux ISR d’entrée portent `IRAM_ATTR`, ne font ni allocation ni calcul flottant, et `micros()` est lui-même placé en IRAM dans le core ESP8266 3.1.2.
- Le snapshot des timestamps et du drapeau `edgeFresh` est protégé par une section critique courte.
- L’acquisition emploie le compteur de cycles CPU, de résolution bien supérieure aux périodes tach attendues.

### Démarrage / Boot readiness

- Les deux GPIO de sortie sont configurés à LOW, donc NPN bloqués, avant LittleFS et le réseau.
- Les résistances base-émetteur de 10 kΩ documentées maintiennent les transistors bloqués pendant le reset, lorsque les GPIO sont en haute impédance.
- La connexion STA n’est pas attendue de façon bloquante : l’AP, le serveur et la boucle démarrent sans boucle d’association WiFi.

### Génération sortie / Timer1

- `startWaveform(pin, highUs, lowUs)` et `stopWaveform(pin)` correspondent bien à l’API du core ESP8266 3.1.2. Le moteur partage Timer1 entre plusieurs GPIO et applique les changements sans générateur logiciel dans `loop()`.
- Le signal demandé est carré à 50 %, et l’inversion par le NPN conserve correctement la fréquence.
- L’arrêt de voie appelle `stopWaveform()` puis force la base à LOW, ce qui relâche explicitement la ligne tach Deye.

### Fidélité au protocole tachymétrique / Fail-safe

- La constante `PULSES_PAR_REV = 2` et la formule théorique `RPM = Hz × 60 / 2` sont correctes pour un tach PC Noctua.
- Les deux voies ont des mesures, ratios et sorties distincts.
- L’intention fail-safe est saine : après 1,5 s sans front, la fréquence de sortie demandée devient nulle au lieu de conserver indéfiniment un faux régime.

### Filtres / Signal

- Les périodes hors de la fenêtre 1 ms–1 s sont prévues pour être rejetées.
- Une moyenne glissante de quatre périodes réduit le bruit de mesure.
- Une hystérésis évite en principe de reprogrammer le moteur waveform à chaque petite variation.

### WiFi / Réseau

- Le mode `WIFI_AP_STA` conserve un point d’accès local même si la station ne se connecte pas.
- La configuration STA ne bloque pas le traitement tach et l’AP peut être reconfiguré à chaud.
- Les copies SSID/mot de passe utilisent `strncpy` et imposent la terminaison NUL.

### Gestion mémoire / HEAP

- Les grandes chaînes HTML constantes utilisent `F()`, et `pageHTML()` réserve environ 3 ko avant concaténation.
- Les ISR n’utilisent ni `String`, ni allocation, ni objet réseau.
- La configuration et les buffers de périodes ont une taille statique bornée.

### Interface Web

- L’interface expose les RPM lus/simulés, le ratio et la présence du signal pour les deux voies, avec polling JSON toutes les secondes.
- Les ratios sont validés côté serveur dans la plage 0–100 avant sauvegarde.
- Les SSID réaffichés passent par un échappement des principaux caractères HTML.

### Persistance / Stockage

- LittleFS évite l’écriture brute répétitive d’une EEPROM émulée et la sauvegarde n’a lieu qu’après une action utilisateur.
- La taille de structure, un marqueur magique et les bornes des ratios écartent certains fichiers manifestement invalides.

### Documentation hardware/code

- Le document fournit une BOM, les correspondances de broches, le câblage des entrées 3,3 V, les sorties NPN collecteur ouvert et les condensateurs du convertisseur.
- La sortie collecteur ouvert est le bon principe pour une tension de pull-up Deye inconnue ; le BC337 offre une marge de tension suffisante.
- Les pull-up d’entrée 4,7 kΩ vers 3,3 V et résistances série 1 kΩ protègent correctement les GPIO si un seul tach est raccordé par canal.

### Qualité de code générale (positif)

- **Structure / organisation** : fonctions distinctes pour configuration, WiFi, acquisition, génération, web et LED.
- **Lisibilité / nommage** : unités généralement explicites et état regroupé par canal.
- **Commentaires / documentation inline** : formules, polarités et intentions temps-réel sont abondamment expliquées.
- **Gestion d’erreurs / robustesse** : bornes de ratio, timeout et arrêt explicite de sortie sont prévus.
- **Duplication / DRY** : les deux canaux partagent structures et fonctions paramétrées ; seules les petites ISR sont dupliquées.
- **Respect des conventions C++/Arduino** : types de largeur fixe et `volatile` sont employés pour les données d’ISR.
- **Complexité / maintenabilité** : les constantes physiques et mappings sont centralisés.
- **Sécurité basique** : copies réseau bornées et échappement HTML partiel.
- **Testabilité** : le calcul et l’application de sortie sont séparés, même si aucun test n’est fourni.

---

## ❌ Points techniques négatifs

### Compilation et API ESP8266

- [critique] Le sketch contient trois parenthèses fermantes surnuméraires dans `pageHTML()` :

```cpp
s += escapeHtml(cfg.apPass) + "'><br><br>");
s += escapeHtml(cfg.staSsid) + "'><br><br>");
s += escapeHtml(cfg.staPass) + "'><br><br>");
```

  — impact : erreurs de syntaxe C++ certaines, avant édition de liens.
- [critique] `GPIO12`, `GPIO14`, `GPIO13`, `GPIO4` et `GPIO5` ne sont pas définis par la variante officielle `d1_mini` du core ESP8266 3.1.2 ; celle-ci expose `D1`…`D8` ou des numéros entiers — impact : au moins cinq identifiants inconnus supplémentaires empêchent la compilation.
- [majeur] Aucun core/version, fichier de build ou compilation CI n’est fourni — impact : la dépendance à l’en-tête interne `core_esp8266_waveform.h` n’est pas verrouillée. La signature utilisée est valide en 3.1.2, mais sa stabilité future n’est pas garantie.
- [mineur] Les retours de `startWaveform()` et `stopWaveform()` sont ignorés — impact : l’état logiciel peut annoncer une fréquence appliquée alors que l’API a refusé la broche ou les paramètres.

### Temps-réel / ISR

- [critique] Le calcul `(deltaCycles * 1000000UL) / CPU_HZ` multiplie deux valeurs 32 bits. Le numérateur déborde et, après wrap, ne peut produire qu’une valeur de 0 à 53 µs, toujours rejetée par la borne minimale de 1 000 µs — impact : **aucune période tach ne peut jamais devenir valide et aucune sortie ne démarre**, même après correction des erreurs de compilation.

```cpp
uint32_t deltaCycles = c1 - c0;
uint32_t periodUsLocal =
    (deltaCycles * 1000000UL) / CPU_HZ;
if (periodUsLocal >= 1000 && periodUsLocal <= 1000000) {
```

- [majeur] Le test `c1 > c0` n’est pas robuste au wrap du compteur de cycles, qui survient environ toutes les 53,7 s à 80 MHz — impact : un intervalle est rejeté périodiquement ; une soustraction non signée suffisait.
- [mineur] Les commentaires affirment que deux timestamps pris dans les ISR rendent la mesure « exacte » malgré le WiFi. Une latence d’entrée variable se soustrait bien entre deux appels et crée encore une erreur différentielle — impact : garantie de précision trop absolue, sans mesure de jitter.

### Démarrage / Boot readiness

- [critique] En l’état, les erreurs de compilation puis le calcul de période impossible garantissent l’absence permanente de tach de sortie — impact : le Deye voit deux ventilateurs arrêtés.
- [majeur] Même après correction, aucun signal provisoire n’est généré avant deux fronts valides et un passage dans `loop()` ; LittleFS, un éventuel formatage, le WiFi et le serveur sont initialisés avant l’attachement des interruptions — impact : fenêtre de boot sans tach non bornée, susceptible de déclencher l’alarme Deye.
- [majeur] Sur échec de montage, `LittleFS.format()` est lancé au boot sans watchdog applicatif ni gestion du second échec — impact : démarrage prolongé, perte de configuration et absence de tach pendant toute l’opération.
- [majeur] L’alimentation du Wemos dépend des rails ventilateur et n’a ni réserve énergétique ni source auxiliaire — impact : aucun signal de boot ou fail-safe ne peut être maintenu si les deux rails sont coupés ou fortement modulés.

### Génération sortie / Timer1

- [majeur] L’« hystérésis » vaut `freqOutApplied × 0,2 + 0,05`, soit environ 20 %, malgré un nom évoquant 200 mHz — impact : une variation graduelle de 10–19 % laisse une fréquence de sortie obsolète ; la réactivité tach n’est pas fidèle.
- [majeur] Le code stocke `freqOutApplied = f` même si la période a été saturée à 50 µs ou 4 s — impact : l’UI et l’hystérésis utilisent la demande théorique, pas la fréquence réellement générée.
- [mineur] Les promesses « aucune activité réseau ne peut introduire de jitter » et « élimine la jitter résiduelle » ne sont accompagnées d’aucune capture oscilloscope ni borne mesurée — impact : conformité temporelle non démontrée sur le montage réel.

### Fidélité au protocole tachymétrique / Fail-safe

- [critique] Le schéma recommande de mettre en parallèle les deux sorties tach Noctua d’un canal. Deux collecteurs ouverts non synchronisés réalisent un OR de niveaux bas, pas une somme propre de fréquences ; les impulsions se chevauchent et des fronts disparaissent — impact : RPM irrégulier ou sous-estimé et panne d’un ventilateur masquée par l’autre.
- [majeur] Un seul intervalle dans la plage suffit à déclarer `signalPresent`, sans compteur de confirmation ni cohérence inter-périodes — impact : après correction arithmétique, deux parasites peuvent créer un faux tach pendant jusqu’à 1,5 s.
- [majeur] Le timeout de 1,5 s conserve l’ancienne waveform durant des dizaines de tours attendus — impact : un ventilateur réellement calé reste présenté au Deye comme tournant pendant cette fenêtre.
- [majeur] Le buffer de moyenne n’est pas vidé après timeout — impact : à la reprise, jusqu’à trois anciennes périodes sur quatre contaminent la sortie.
- [mineur] La fenêtre acceptée correspond à 30–30 000 RPM à 2 PPR, très au-delà des Noctua cités — impact : de nombreux parasites lents ou survitesses irréalistes restent admissibles.

### Filtres / Signal

- [majeur] La moyenne de périodes n’a ni médiane, ni rejet relatif, ni remise à zéro après stall — impact : un front manqué ou un outlier valide influence quatre mesures.
- [mineur] La moyenne rectangulaire de quatre échantillons retarde les changements brusques, puis l’hystérésis de 20 % ajoute une seconde quantification — impact : sortie par paliers et réponse lente.

### WiFi / Réseau

- [critique] L’administration HTTP n’a aucune authentification ni protection CSRF — impact : tout client de l’AP ou du LAN STA peut modifier ratios et identifiants, puis rendre le simulateur indisponible.
- [critique] Les mots de passe AP et STA sont réinjectés en clair dans la page HTML — impact : tout client ouvrant `/` récupère les secrets enregistrés.
- [majeur] L’AP utilise un secret par défaut publié (`deye12345`) et accepte aussi un AP ouvert — impact : prise de contrôle locale prévisible.
- [majeur] La longueur du mot de passe AP n’est pas validée selon les contraintes ESP8266 (8–63 caractères lorsqu’il n’est pas vide), et le retour de `WiFi.softAP()` est ignoré — impact : un mot de passe de 1 à 7 caractères peut couper l’AP tout en renvoyant HTTP 303 comme si l’opération avait réussi.
- [mineur] `WiFi.persistent(true)` duplique les identifiants STA dans la zone SDK alors qu’ils sont déjà dans LittleFS — impact : écritures flash et effacement usine incomplets.

### Gestion mémoire / HEAP

- [majeur] Chaque requête `/` reconstruit environ 3 ko ou plus par concaténations `String`, et chaque statut JSON crée plusieurs temporaires — impact : fragmentation progressive et besoin d’un bloc heap contigu sur ESP8266.
- [mineur] Aucun suivi du heap libre/minimal ni réponse de repli n’existe — impact : une allocation échouée se traduit par une page tronquée ou une instabilité difficile à diagnostiquer.

### Interface Web

- [majeur] Le polling réécrit le ratio affiché chaque seconde sans protéger un champ ayant le focus — impact : l’utilisateur peut voir sa saisie remplacée pendant l’édition.
- [majeur] `toFloat()` transforme une chaîne non numérique en `0`, mais une requête partielle peut modifier un canal avant de découvrir l’erreur de l’autre ; la réponse reste une redirection sans message — impact : configuration RAM partiellement appliquée et non persistée.
- [mineur] Le statut ne distingue pas premier front, période rejetée, stall et entrée débranchée — impact : diagnostic terrain insuffisant.

### Persistance / Stockage

- [majeur] La structure binaire brute n’a ni version, ni CRC, ni sauvegarde atomique — impact : une corruption partielle conservant `magic` peut charger SSID, mots de passe ou booléen invalides.
- [majeur] Les retours de `configSauver()`, du formatage et du second `LittleFS.begin()` sont ignorés par les handlers — impact : succès HTTP annoncé malgré un échec de persistance.
- [mineur] Le format dépend du padding et de la représentation du compilateur — impact : évolution de structure ou de toolchain non migrable.

### Documentation hardware/code

- [critique] Dans le schéma détaillé d’alimentation, les deux entrées +12 V sont reliées par le segment vertical **avant** D1/D2 ; ce dessin court-circuite les connecteurs et annule l’OR-ing — impact : backfeed entre sorties Deye, avec risque de dommage matériel. Le résumé ultérieur montre un autre câblage, donc le document est contradictoire.
- [critique] Le document affirme qu’un LM2596 réglé à 5 V régule encore avec une entrée de 5 V. C’est un buck non élévateur et la diode Schottky laisse environ 4,7 V avant ses pertes — impact : 5 V non régulé, brownout ou boucle de reboot du Wemos à basse tension.
- [majeur] Le calcul de saturation du BC337 emploie un gain forcé d’environ 100 et conclut que 2,6 mA de base suffisent à 260 mA ; un dimensionnement de commutation robuste utilise typiquement un gain forcé bien plus faible — impact : la garantie annoncée pour une pull-up Deye de 220 Ω/55 mA n’est pas établie.
- [majeur] Le parallèle des tach est présenté comme lisible parce que les ventilateurs « tournent à la même vitesse » ; leur phase n’est pourtant pas synchronisée — impact : recommandation de câblage fonctionnellement fausse.
- [mineur] Absence de fusible/PTC, TVS, mesure de masses, implantation CEM et validation à l’oscilloscope — impact : robustesse dans l’environnement de l’onduleur non démontrée.

### Qualité de code générale (négatif)

- [critique] **Gestion d’erreurs / robustesse** : erreurs de syntaxe, symboles GPIO inexistants et overflow arithmétique central n’ont pas été détectés — impact : livrable non compilable puis non fonctionnel après correction superficielle.
- [majeur] **Commentaires / documentation inline** : plusieurs affirmations absolues contredisent le code ou la physique (`exact`, absence totale de jitter, LM2596 à 5 V, tach parallèles) — impact : revue et intégration induites en erreur.
- [majeur] **Sécurité basique** : secrets affichés, API ouverte, absence de CSRF et AP prévisible — impact : contrôle réseau complet des paramètres fonctionnels.
- [majeur] **Testabilité** : aucun build reproductible, test unitaire du calcul de période, banc waveform ni capture oscilloscope — impact : des défauts déterministes élémentaires ont atteint le livrable.
- [mineur] **Structure / organisation** : un fichier de plus de 600 lignes mélange moteur temps-réel, stockage, HTML, JavaScript et réseau — impact : revues et tests ciblés plus coûteux.
- [mineur] **Respect des conventions C++/Arduino** : constantes de broches non portables et arithmétique sans élargissement explicite — impact : dépendance implicite à des symboles inexistants et overflow.
- [mineur] **Maintenabilité** : la dépendance à une API interne du core et le format binaire persistant ne sont pas versionnés — impact : mises à jour fragiles.

---

## ⭐ Note globale : 1,8/10 — Non compilable et chaîne tach inopérante

L’architecture visée est crédible et mieux documentée que de nombreux prototypes, mais les erreurs C++ empêchent le build et le débordement 32 bits garantit ensuite le rejet de toutes les périodes. Le parallèle tach et le schéma d’OR-ing rendent aussi le montage risqué sans refonte.

## ⭐ Note qualité de code : 4,8/10 — Bien structuré, insuffisamment vérifié

Le découpage, le nommage et les intentions temps-réel sont convenables, mais des fautes de compilation, un calcul central impossible, des garanties trompeuses et l’absence de tests abaissent fortement la qualité livrable.
