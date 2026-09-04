# Analyse — claude opus 5 medium

Sources analysées (liste exhaustive du dossier) :
- `competitors\claude opus 5 medium\deye_fan\deye_fan.ino`
- `competitors\claude opus 5 medium\SCHEMA.md`

## ✅ Points techniques positifs

### Temps-réel / ISR

- Le moteur affecte Timer1 au vecteur NMI de niveau 3, donc il peut préempter la pile WiFi de niveau 1 et n’est pas bloqué par `noInterrupts()`. C’est l’architecture la plus rigoureuse des trois dossiers pour limiter la gigue ESP8266.
- Toutes les fonctions appelées depuis la NMI sont annotées `IRAM_ATTR`, les données sont en DRAM, et l’ISR évite flottants, allocation, `Serial`, SDK réseau et constantes flash.
- La lecture des entrées et l’écriture des sorties se font directement par `GPI`, `GPOS` et `GPOC`, sans `digitalRead()`/`digitalWrite()` dans le moteur.
- Le partage NMI → `loop()` utilise un seqlock et le sens inverse des mots 32 bits alignés atomiques, choix cohérent puisque la NMI n’est pas masquable.

```232:247:competitors/claude opus 5 medium/deye_fan/deye_fan.ino
static inline uint32_t IRAM_ATTR ccyNow() {
  uint32_t c;
  __asm__ __volatile__("rsr %0,ccount" : "=a"(c));
  return c;
}
static inline bool IRAM_ATTR ccyReached(uint32_t now, uint32_t deadline) {
  return (int32_t)(now - deadline) >= 0;
}
```

### Démarrage / Boot readiness

- `engineBegin()` est lancé avant WiFi, HTTP et mDNS. Les sorties sont forcées à LOW avant passage en `OUTPUT`, ce qui bloque les NPN et évite une impulsion parasite au boot.
- Une entrée n’est « primed » qu’au premier front ; la première période n’est publiée qu’au second. Cela évite la fausse mesure depuis l’instant zéro observée dans des conceptions plus naïves.

### Génération sortie / Timer1

- Il s’agit bien du Timer1 matériel en mode `TIM_SINGLE`, pas d’un `Ticker` ni d’un polling dans `loop()`.
- L’ordonnanceur programme le prochain événement réel et accumule `nextEdge += halfPeriod`. Un retard ponctuel ne devient donc pas une dérive cumulative ; une boucle de rattrapage bornée absorbe jusqu’à 16 fronts.
- Le changement de fréquence n’est adopté qu’à une frontière de front, évitant les impulsions tronquées.
- Le plafond de fréquence et `maxSimRpm` bornent les commandes aberrantes.

```350:365:competitors/claude opus 5 medium/deye_fan/deye_fan.ino
const uint32_t h = oc->cmdHalfCcy;
if (h != 0 && h != oc->halfCcy) {
  oc->halfCcy = h;
}
oc->nextEdgeCcy += oc->halfCcy;
if (++iter >= 16u) {
  oc->nextEdgeCcy = now + oc->halfCcy;
  break;
}
```

### Fidélité protocole tachymétrique / Fail-safe

- Entrée et sortie ont chacune un nombre d’impulsions/tour configurable, avec valeur par défaut correcte de 2. Les formules `RPM = fréquence × 60 / PPR` et `fréquence_sortie = RPM_simulé × PPR / 60` sont correctes.
- La perte de fronts pendant 1,2 s coupe la sortie et relâche le collecteur ouvert ; aucune vitesse n’est inventée en cas de stall.
- Les périodes hors plage sont rejetées, comptées et forcent un réamorçage, empêchant une période incohérente de rester active.
- Le NPN de sortie en collecteur ouvert est correctement dimensionné pour un pull-up Deye de 3,3 V, 5 V ou 12 V ; la résistance base-émetteur garantit l’arrêt pendant boot/reset.

### Filtres / Signal

- L’entrée recommandée respecte la nature open-collector Noctua : pull-up externe vers 3,3 V, résistance série et condensateur. Le calcul des seuils et constantes de temps est explicite.
- Le filtre numérique exige trois échantillons identiques à 40 µs, puis applique une EMA entière de coefficient 1/8 sur la période.
- La mesure porte sur des fronts descendants ; le front montant ralenti par le RC ne biaise donc pas directement la période mesurée.

### WiFi / Réseau

- AP+STA simultané, STA désactivable, reconnexion périodique non bloquante et mDNS sont présents.
- `WiFi.persistent(false)` évite des écritures SDK parasites ; modem-sleep est désactivé pour réduire la variabilité temporelle.
- Le SSID AP inclut l’identifiant de puce, ce qui évite les collisions entre modules.

### Gestion mémoire / HEAP

- La page Web statique est placée en `PROGMEM` et envoyée par `send_P`.
- Les grosses chaînes construites dynamiquement réservent leur capacité (`j.reserve(1400)`), et les fragments constants utilisent `F()`.
- Le heap libre est exposé dans l’état système, utile pour diagnostiquer une dérive mémoire.

### Interface Web

- L’interface est complète : RPM lus/simulés, fréquence, ratio, PPR entrée/sortie, écrêtage, compteurs de fronts/rejets, cadence NMI, CPU, heap, AP/STA et uptime.
- Les paramètres sont validés côté serveur et appliqués sans redémarrer le moteur ; les réponses 400 différencient les erreurs.

### Persistance / Stockage

- La configuration EEPROM possède magic, version et vrai CRC32.
- Le chargement effectue un bornage défensif même après CRC valide, et les valeurs par défaut incluent ratio, PPR et limite RPM.
- EEPROM émulée est proportionnée à cette petite structure fixe ; LittleFS n’apporterait pas ici d’avantage décisif.

### Documentation hardware/code

- Le document couvre alimentation dangereuse, mesure des masses, solution optocouplée si elles diffèrent, BOM, fusibles, réglage du buck et mise en service instrumentée.
- Le OU Schottky depuis les deux connecteurs empêche le backfeed ; le buck 12 V → 5 V est explicitement obligatoire et évite d’appliquer 12 V au LDO du Wemos.
- Les entrées open-collector sont correctement tirées à 3,3 V et les sorties reproduisent une charge open-collector compatible 3,3/5/12 V.
- Les tests proposés au fréquencemètre et sous saturation HTTP ciblent directement fréquence, jitter et isolation WiFi.

### Qualité de code générale (positif)

- **Structure :** découpage très net entre primitives bas niveau, moteur, état partagé, application, stockage, Web et réseau.
- **Lisibilité :** noms précis, constantes typées et invariants proches du code concerné.
- **Commentaires :** détaillent les choix temporels, le wraparound, l’atomicité et les contraintes NMI.
- **Robustesse :** seqlock, garde de rattrapage, bornages, CRC/version, timeout et état de sortie sûr.
- **DRY :** moteur générique à deux canaux, formulaires générés par boucle et fonctions réseau centralisées.
- **Conventions C++/Arduino :** types fixes, `static`, `const`, `IRAM_ATTR`, `F()`/`PROGMEM`, arithmétique wrap-safe et absence de travail lourd dans `loop()`.
- **Maintenabilité :** paramètres centralisés, configuration versionnée, diagnostics riches et commentaires d’architecture.
- **Sécurité basique :** validation stricte des longueurs et plages, mot de passe AP minimal de huit caractères.
- **Testabilité :** compteurs NMI/fronts/rejets, mode Timer1 niveau 1 de diagnostic et procédure de test instrumentée facilitent la vérification.

## ❌ Points techniques négatifs

### Exactitude de compilation probable

- [majeur] Aucune version exacte du core ESP8266 n’est figée et aucune compilation reproductible n’est fournie. `ETS_FRC_TIMER1_NMI_INTR_ATTACH`, `T1C`, `TEIE`, `T1I` et l’hypothèse que `timer1_write()` est appelable depuis NMI dépendent d’API internes du core ; une mise à jour peut produire une erreur de compilation ou, plus grave, un acquittement Timer1 incorrect.
- [mineur] Aucun `platformio.ini`, lock de bibliothèque ni CI ne confirme les signatures de `send_P`, mDNS et des macros NMI. La syntaxe générale paraît plausible, mais elle n’a pas pu être validée localement en l’absence d’`arduino-cli`.

### Temps-réel / ISR

- [majeur] Une NMI à environ 25 kHz qui inspecte deux entrées, deux sorties, un seqlock et reprogramme Timer1 est agressive. Le coût annoncé de 4 % CPU et 1,5 µs n’est étayé par aucune mesure incluse ; une surcharge NMI peut affamer des sections critiques radio plutôt que seulement les préempter.
- [majeur] `timer1_write()` est présenté comme garanti IRAM, mais cette propriété n’est pas vérifiée statiquement pour le core ciblé. Si l’implémentation ou un trampoline réside en flash, un `EEPROM.commit()` avec cache désactivé peut planter.
- [mineur] Après 64 échecs, `engineReadInput()` retourne tout de même le dernier snapshot, potentiellement incohérent. La collision est très improbable, mais un booléen d’échec ou une reprise au cycle suivant serait plus sûr.

### Démarrage / Boot readiness

- [majeur] « Le moteur démarre avant le WiFi » ne signifie pas qu’un tach est immédiatement émis : il faut que le ventilateur démarre, puis deux fronts descendants valides, puis un passage de `updateChannel()`. Il n’existe pas de signal de boot provisoire ni de délai d’alarme Deye vérifié.
- [mineur] `lastEdgeMs` est initialisé après WiFi, serveur et mDNS. Cela reste sûr car `periodCcy` doit être non nul, mais le commentaire « premières centaines de millisecondes » ne quantifie ni spin-up ventilateur ni fenêtre d’alarme.

### Génération sortie / Timer1

- [mineur] Le plancher de 10 ticks correspond à seulement 2 µs. En cas de rafale de rattrapage ou d’échéances très proches, la NMI peut se rappeler à 500 kHz, ce qui mérite une mesure de charge réelle.
- [mineur] Le compteur de cycles wrappe toutes les 53,7 s à 80 MHz. Les comparaisons signées sont correctement conçues tant que les échéances restent à moins de 2³¹ cycles, invariant vrai avec les limites actuelles mais non affirmé par `static_assert` ou validation dédiée.

### Fidélité protocole tachymétrique / Fail-safe

- [majeur] Comme un seul tach est lu pour chaque paire de deux Noctua, la panne du second ventilateur est masquée. L’impact est une perte potentielle de la moitié du débit d’air sans alarme Deye.
- [majeur] Un timeout de 1,2 s reste suffisamment long pour émettre une ancienne vitesse après blocage ; aucune corrélation thermique ni seuil spécifique à chaque canal n’est proposée.
- [mineur] Autoriser 1 à 8 PPR est flexible mais dangereux sur une interface non authentifiée : une modification erronée conserve un signal régulier tout en falsifiant fortement les RPM.

### Filtres / Signal

- [majeur] Le commentaire d’en-tête du code décrit `1 kΩ`, `10 kΩ`, `47 nF`, alors que `SCHEMA.md` recommande `470 Ω`, `4,7 kΩ`, `22 nF`. Ces réseaux n’ont pas la même impédance ni exactement la même dynamique ; l’ambiguïté peut produire deux montages différents.
- [mineur] L’EMA ajoute environ huit périodes de latence. C’est raisonnable, mais aucun compromis entre stabilité, réponse à une accélération et délai du fail-safe n’est configurable.

### WiFi / Réseau

- [critique] Aucun endpoint HTTP n’est authentifié. Tout client sur l’AP ou le LAN STA peut modifier ratios/PPR/limites, changer le réseau ou redémarrer le module ; l’impact est une falsification du tach ou une indisponibilité.
- [critique] `/api/status` renvoie en clair `apPass` avec le SSID et les informations réseau.

```924:934:competitors/claude opus 5 medium/deye_fan/deye_fan.ino
j += F("],\"wifi\":{");
j += F("\"apSsid\":\""); j += g_cfg.apSsid;
j += F("\",\"apPass\":\""); j += g_cfg.apPass;
j += F("\",\"apIp\":\""); j += WiFi.softAPIP().toString();
j += F("\",\"apClients\":"); j += WiFi.softAPgetStationNum();
j += F(",\"staSsid\":\""); j += g_cfg.staSsid;
```

  Impact : un simple GET révèle le secret AP ; aucune authentification, session ni protection CSRF ne compense cette fuite.
- [majeur] Les chaînes injectées dans JSON et HTML ne sont pas échappées. Un SSID contenant `"`, `\`, `<` ou `&` peut casser la réponse ou injecter du balisage dans l’interface.
- [mineur] Le mot de passe AP usine est fixe et publié.

### Gestion mémoire / HEAP

- [majeur] `handleStatus()` construit environ toutes les 600 ms un objet `String` JSON dynamique, puis ajoute plusieurs `String` temporaires d’adresses IP et d’uptime. `reserve()` limite les réallocations, mais l’usage prolongé peut fragmenter le petit heap ESP8266.
- [mineur] Le heap est affiché, mais aucun seuil d’alerte, minimum historique ou compteur d’échec d’allocation n’est géré.

### Interface Web

- [majeur] Le formulaire reçoit et réaffiche le mot de passe AP en clair. Cela facilite sa fuite visuelle, via API ou via inspection du DOM.
- [majeur] Les opérations sensibles ne requièrent ni authentification ni confirmation ; l’exposition STA rend le contrôleur modifiable depuis tout le réseau local.
- [mineur] Les erreurs de polling sont silencieusement ignorées, ce qui peut laisser à l’écran des RPM anciens sans indicateur de perte de connexion.

### Persistance / Stockage

- [mineur] EEPROM avec CRC32/version est robuste, mais il n’y a pas de double slot ou écriture transactionnelle. Une coupure pendant `commit()` ramène toute la configuration aux défauts au boot suivant.
- [mineur] Les secrets WiFi restent en clair dans la flash, limite classique de l’ESP8266 mais à documenter.

### Documentation hardware/code

- [majeur] La contradiction de valeurs RC entre le commentaire source et `SCHEMA.md` nuit directement à la reproductibilité.
- [majeur] La documentation détaille bien le risque haute tension, mais le contournement du contrôle ventilateur reste fondamental : elle ne propose aucun capteur thermique indépendant ni surveillance des quatre ventilateurs.
- [mineur] Les polyfuses 500 mA sont proches du total crête estimé à 0,48 A si une seule source porte toute la charge ; à chaud, leur courant de maintien peut provoquer des coupures intempestives. Un dimensionnement avec courbe température/courant est nécessaire.

### Qualité de code générale (négatif)

- **Structure :** excellente, mais complexe pour deux canaux et fortement couplée aux internals du core ESP8266.
- **Lisibilité :** bonne pour un lecteur expert ; NMI, seqlock et registres rendent néanmoins l’audit difficile.
- **Commentaires :** riches, mais quelques affirmations de garantie et mesures CPU ne sont pas accompagnées de résultats.
- **Robustesse :** très bonne côté signal ; faible côté sécurité HTTP et atomicité de la persistance.
- **DRY :** bon ; la construction JSON manuelle reste longue et fragile.
- **Conventions C++/Arduino :** solides, avec dépendance non encapsulée à des API privées et absence de version de toolchain.
- **Maintenabilité :** bonne dans le code, diminuée par l’instabilité potentielle des hooks NMI et la divergence code/schéma.
- **Sécurité basique :** insuffisante : secret AP exposé et commandes critiques anonymes.
- **Testabilité :** instrumentation exemplaire, mais aucun test automatisé, aucune capture de jitter ni preuve de compilation jointe.

## ⭐ Note globale : 8,0/10 — Moteur temps-réel excellent, sécurité réseau à corriger

La conception NMI, les filtres, le fail-safe et le hardware sont techniquement avancés, mais l’API NMI non figée, l’absence d’authentification et la fuite du mot de passe empêchent un déploiement sans corrections.

## ⭐ Note qualité de code : 8,5/10 — Très rigoureux, complexe et insuffisamment sécurisé

La structure, les invariants et la documentation du moteur sont de haut niveau ; la qualité baisse surtout sur la sécurité Web, les chaînes dynamiques et la reproductibilité de compilation.
