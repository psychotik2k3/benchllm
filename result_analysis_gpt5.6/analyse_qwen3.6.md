# Analyse — qwen3.6

Sources analysées (liste exacte) :
- `competitors/qwen3.6/deye_fan.ino`
- `competitors/qwen3.6/SCHÉMA_CIRCUIT.md`

## ✅ Points techniques positifs

### Temps-réel / ISR
- Les deux captures tachymétriques sont bien déclarées `IRAM_ATTR`, courtes, sans allocation, journalisation ni accès réseau.
- Les données partagées critiques sont majoritairement `volatile` et de largeur 32 bits, donc atomiques sur ESP8266. Le calcul de période par soustraction non signée tolère le débordement de `micros()`.

### Démarrage / Boot readiness
- L'intention de maintenir une sortie inactive avant la première mesure est explicite : `fan1Active` et `fan2Active` démarrent à `false` et aucun faux RPM n'est affiché comme actif.

### Génération sortie / Timer1
- L'architecture visée — capture GPIO et génération indépendante de `loop()` — est pertinente pour isoler le tach du serveur HTTP.
- Deux échéances par canal sont prévues plutôt qu'une fréquence commune, ce qui permettrait des sorties réellement indépendantes si l'API et le réarmement étaient corrigés.

### Fidélité protocole tachymétrique / Fail-safe
- La constante de **2 impulsions par tour** est correcte et la conversion d'affichage `RPM = 30 000 000 / période_us` est juste.
- Un timeout de perte de signal de 2 s est prévu par canal.
- La sortie matérielle proposée avec NPN à collecteur ouvert est, dans son principe, compatible avec un pull-up Deye à 3,3 V, 5 V ou 12 V.

### Filtres / Signal
- Validation de période et rejet des fronts trop rapides sont prévus.
- Le schéma propose résistance série, clamp et petit condensateur pour limiter surtensions et bruit HF.

### WiFi / Réseau
- L'intention est un mode `WIFI_AP_STA`, avec AP de secours et API d'état.
- Les traitements réseau restent hors ISR.

### Interface Web
- L'interface expose séparément périodes, RPM réels, RPM simulés, ratios et configuration STA.
- Les ratios reçus sont bornés avant emploi.

### Persistance / Stockage
- Une signature magique et des bornes de validité évitent d'utiliser directement des ratios EEPROM manifestement invalides.
- Les écritures ne sont pas effectuées depuis une ISR.

### Documentation hardware/code
- La documentation décrit brochage, BOM, principe open-collector, assemblage et tests.
- La résistance de base de 4,7 kΩ limite correctement le courant GPIO, et l'OR-ing par deux Schottky vise à empêcher le backfeed entre les deux connecteurs 12 V.

### Qualité de code générale (positif)
- **Structure :** découpage clair par responsabilités (EEPROM, ISR, WiFi, HTTP, matériel).
- **Lisibilité :** noms explicites et commentaires abondants.
- **Commentaires :** formules et intention temps-réel sont documentées.
- **Robustesse :** bornes de ratios, timeout et valeurs par défaut existent.
- **DRY :** l'organisation est cohérente, même si les deux canaux restent dupliqués.
- **Conventions C++/Arduino :** types entiers de taille fixe, `static` et constantes sont correctement employés à plusieurs endroits.
- **Maintenabilité :** les broches et paramètres sont centralisés.
- **Sécurité basique :** validation numérique présente, mais sécurité réseau insuffisante (voir négatifs).
- **Testabilité :** l'API d'état et les logs faciliteraient les essais sur cible.

## ❌ Points techniques négatifs

### Exactitude de compilation probable
- [critique] Le fichier ne compile probablement pas pour une Wemos D1 Mini ESP8266 : `hw_timer_t`, `timerBegin(1, prescaler, true)`, `timerRead`, `timerWrite` et cette forme de `timerAttachInterrupt` sont des API ESP32, pas l'API Timer1 ESP8266. **Impact : firmware non constructible pour la cible annoncée.**
- [critique] `eepromLoadRatios()` ferme sa fonction avant `EEPROM.end();`, qui se retrouve au niveau global, suivi d'une accolade surnuméraire. **Impact : erreur syntaxique certaine.**

```295:303:competitors/qwen3.6/deye_fan.ino
static void IRAM_ATTR fan1TachISR() {
    uint32_t now = micros();
    
    // Anti-rebond / validation de période minimale
    if (lastFan1IRQ_us == 0) {
        lastFan1IRQ_us = now;
        return;
    }
```

### Temps-réel / ISR
- [majeur] `micros()`, la multiplication 64 bits et surtout `digitalWrite()` dans les ISR ne garantissent pas que toute la chaîne appelée réside en IRAM sur ESP8266. **Impact : crash ou latence lorsque le cache flash est indisponible.**
- [majeur] L'affirmation « interruptions jamais masquées par le WiFi » est fausse sur ESP8266 ; les écritures flash et certaines sections système désactivent les interruptions. **Impact : jitter non borné et mesures manquées.**
- [majeur] Plusieurs états liés (`active`, période, prochaine échéance, ratio) sont modifiés sans section critique ni snapshot cohérent. **Impact : une ISR peut observer un mélange d'anciennes et nouvelles valeurs.**

### Démarrage / Boot readiness
- [critique] Les interruptions tach et la génération ne sont activées qu'après une tentative STA pouvant bloquer 15 s. **Impact : l'onduleur peut déclarer une alarme ventilateur avant la première mesure/sortie.**
- [critique] GPIO15/D8 est une broche de strap qui doit être LOW au reset ; le tach externe peut la tirer HIGH. **Impact : échec de boot selon l'état du ventilateur.**
- [critique] Avec un NPN base-résistance, `GPIO HIGH` allume le transistor et tire le tach Deye à LOW, contrairement aux commentaires qui le disent bloqué. **Impact : état de boot/fail-safe inversé et ligne potentiellement maintenue basse.**
- [majeur] Le premier passage de `loop()` efface toute mesure capturée juste après l'attachement des interruptions. **Impact : délai supplémentaire d'au moins deux fronts avant une sortie valide.**

### Génération sortie / Timer1
- [critique] Le calcul ISR multiplie la période par le ratio au lieu de la diviser :

```317:324:competitors/qwen3.6/deye_fan.ino
// simPeriod = period / ratio = period * 10 / ratioInt10
uint32_t simPeriodU32 =
    (uint32_t)((uint64_t)period * fan1RatioInt10 / 10);
```

  **Impact : avec ×2,5, la sortie est 2,5 fois plus lente au lieu d'être 2,5 fois plus rapide.**
- [critique] Lorsqu'un canal est inactif, son échéance devient `UINT32_MAX`; l'ISR d'entrée ne la réarme jamais. **Impact : après arrêt initial ou stall, la génération de ce canal ne redémarre pas.**
- [critique] Le code appelle `timerWrite()` comme une programmation d'alarme absolue, alors que l'API visée ne fonctionne pas ainsi. **Impact : timing impossible ou timer incorrect même après adaptation superficielle.**
- [majeur] `digitalWrite()` dans le chemin de sortie ajoute une latence et un jitter importants ; les registres `GPOS/GPOC` seraient adaptés. **Impact : fronts moins déterministes.**
- [majeur] Le prochain front est calculé depuis `now`, pas depuis l'échéance précédente. **Impact : toute latence s'accumule en dérive de fréquence.**

### Fidélité protocole tachymétrique / Fail-safe
- [critique] `simPeriodUsFanX` est commentée « demi-période », mais elle est calculée à partir de la période entre impulsions et utilisée directement entre chaque toggle. Même avec la division de ratio corrigée, il faut programmer `period/(2×ratio)`. **Impact : fréquence de sortie divisée par deux.**
- [critique] La polarité NPN est incohérente dans tout le code : LOW bloque le NPN, HIGH le conduit. **Impact : le fail-safe et les impulsions sont inversés par rapport aux commentaires.**
- [majeur] Une période invalide conserve la dernière consigne valide. **Impact : un signal bruité/hors plage peut continuer à simuler une vitesse ancienne pendant jusqu'à 2 s.**

### Filtres / Signal
- [critique] Le schéma d'entrée mélange la sortie tach Noctua et le pull-up supposé de l'onduleur, alors que ce sont fonctionnellement une entrée de mesure et une sortie simulée distinctes. **Impact : câblage ambigu pouvant relier deux domaines qui ne doivent pas l'être.**
- [critique] L'orientation du clamp est contradictoire entre texte, dessin et netlist (certaines indications le relient même à `VIN_COM` 12 V). **Impact : aucune protection 5/12 V fiable, avec risque de surtension GPIO.**
- [majeur] Aucun filtrage logiciel statistique n'est appliqué ; une seule période valide remplace immédiatement la consigne. **Impact : jitter d'entrée transmis à la sortie.**

### WiFi / Réseau
- [critique] L'AP est ouvert (`WiFi.softAP(..., "")`) et l'HTTP n'a aucune authentification. **Impact : toute personne à portée peut changer ratios et identifiants WiFi, puis redémarrer le contrôleur.**
- [majeur] Les identifiants et ratios sont modifiés par GET, donc via URL et historiques/proxies. **Impact : fuite de secrets et modifications involontaires.**
- [mineur] `uint8_t level = WiFi.RSSI()` tronque un RSSI négatif. **Impact : affichage dBm erroné.**

### Gestion mémoire / HEAP
- [majeur] La page HTML et les JSON sont construits par concaténations répétées de `String`, et le HTML n'est pas en `PROGMEM`. **Impact : fragmentation du heap et risque de reset après fonctionnement prolongé.**
- [majeur] Les lectures EEPROM reconstruisent aussi plusieurs `String` à chaque page. **Impact : allocations récurrentes inutiles.**

### Persistance / Stockage
- [critique] Sauvegarder les ratios avec `eepromWriteRatios()` n'écrit jamais la signature magique. Après un premier boot vierge, les ratios sauvegardés restent considérés invalides, sauf si le WiFi a déjà écrit le magic. **Impact : configuration perdue au redémarrage.**
- [majeur] Pas de version, CRC, double-buffer ni écriture atomique. **Impact : coupure pendant `commit()` pouvant rendre toute la configuration inutilisable.**
- [majeur] `eepromWriteWiFi()` contient des indexations conditionnelles fragiles (`ssid[31]`, `pass[63]`) et un terminateur décalé. **Impact : format incohérent et risque de lecture de données hors longueur logique.**

### Documentation hardware/code
- [critique] Alimenter directement le Wemos en 12 V via « VIN/AMS1117 » est dangereux : les D1 Mini sont conçues pour une alimentation 5 V de carte, et leur régulateur 3,3 V n'offre ni la marge thermique ni toujours la tension d'entrée admissible requise. **Impact : surchauffe, brown-out ou destruction. Il faut un vrai buck 12→5 V.**
- [critique] Le document additionne à tort le courant des ventilateurs à celui dissipé par le régulateur du Wemos : les ventilateurs 12 V ne passent pas dans le régulateur. **Impact : calcul thermique incohérent qui masque néanmoins le vrai danger du 12 V direct.**
- [majeur] BAT54/BAT54S est trop peu dimensionnée ou ambiguë pour l'OR-ing d'une alimentation complète selon le courant et les pointes; SS34 est plus crédible. **Impact : échauffement/chute de tension.**
- [majeur] Plusieurs erreurs de cathode/anode et de références D1–D4 rendent la procédure non reproductible. **Impact : risque de montage inversé.**

### Interface Web
- [majeur] Les valeurs HTML (SSID notamment) ne sont pas échappées. **Impact : injection HTML/JavaScript persistante locale.**
- [majeur] `/api/status` autorise CORS `*` sans authentification. **Impact : lecture par n'importe quelle page web accessible au client connecté.**

### Qualité de code générale (négatif)
- [critique] **Robustesse/compilation :** mélange d'API ESP32 et ESP8266 et erreur d'accolade rendent le livrable inexécutable.
- [majeur] **Structure/DRY :** duplication complète des deux canaux multiplie les risques de divergence.
- [majeur] **Lisibilité/commentaires :** de nombreux commentaires très affirmatifs contredisent le code et l'électronique (polarité, ratio, indépendance WiFi).
- [majeur] **Conventions :** macros plutôt que `constexpr`, état global abondant et absence de section critique.
- [majeur] **Maintenabilité :** aucune configuration de build reproductible ni test.
- [critique] **Sécurité basique :** AP/HTTP sans authentification et secrets via GET.
- [majeur] **Testabilité :** aucun test unitaire, test de timing, mesure de jitter ou scénario de boot/stall automatisé.

## ⭐ Note globale : 2,0/10 — Architecture annoncée pertinente, mais firmware non compilable et plusieurs inversions critiques rendent le montage inutilisable.

La note technique reste basse malgré une documentation riche, car compilation, génération de fréquence, redémarrage après stall, boot et alimentation ne sont pas sûrs.

## ⭐ Note qualité de code : 3,0/10 — Présentation soignée, mais exactitude, cohérence et robustesse sont insuffisantes.

Le découpage aide la lecture, sans compenser les erreurs d'API, les commentaires trompeurs, la duplication et l'absence de tests.
