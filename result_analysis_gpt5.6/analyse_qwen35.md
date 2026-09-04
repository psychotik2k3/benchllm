# Analyse — qwen35

Sources analysées (liste exacte) :
- `competitors/qwen35/firmware_main.cpp`
- `competitors/qwen35/schematic.md`
- `competitors/qwen35/README.md`
- `competitors/qwen35/calibration_guide.md`
- `competitors/qwen35/SUMMARY.md`
- `competitors/qwen35/INDEX.md`
- `competitors/qwen35/MEMORY.md`
- `competitors/qwen35/memory/anti_rebound_gpio_optimization.md`

## ✅ Points techniques positifs

### Temps-réel / ISR
- Les deux ISR sont annotées `IRAM_ATTR`, courtes et sans log, allocation ou appel réseau.
- L'anti-rebond temporel utilise une soustraction non signée de `micros()`, compatible avec son débordement.

### Démarrage / Boot readiness
- L'AP est créé pendant `setup()` même sans identifiants STA, ce qui rend théoriquement l'interface accessible localement.
- La tentative STA est bornée à environ 15 s.

### Filtres / Signal
- L'intention d'un rejet des fronts séparés de moins de 1 ms et d'une moyenne glissante sur quatre échantillons va dans la bonne direction.

### WiFi / Réseau
- Le code demande `WIFI_AP_STA` et fournit un AP protégé WPA par un mot de passe de huit caractères.
- Les routes HTTP séparent page, données, configuration et statut.

### Interface Web
- La page est lisible, responsive et prévoit deux ratios configurables et un rafraîchissement asynchrone.
- Les ratios reçus sont bornés côté serveur.

### Documentation hardware/code
- Le dépôt contient un README, un index, un résumé, un guide de calibration, un schéma et une note d'optimisation.
- Les avertissements de sécurité sur l'onduleur haute tension sont appropriés.

### Qualité de code générale (positif)
- **Structure :** sections clairement titrées.
- **Lisibilité :** noms et objectif général compréhensibles.
- **Commentaires :** intention des ISR, ratios, WiFi et interface abondamment décrite.
- **Robustesse :** quelques bornes de ratios et un timeout STA existent.
- **DRY :** l'interface partage une structure visuelle cohérente, mais le firmware factorise peu les canaux.
- **Conventions C++/Arduino :** certains types fixes et `volatile` sont présents.
- **Maintenabilité :** constantes et broches sont regroupées.
- **Sécurité basique :** AP avec mot de passe, mais aucune protection applicative.
- **Testabilité :** les routes JSON pourraient servir à des tests fonctionnels si le firmware compilait.

## ❌ Points techniques négatifs

### Exactitude de compilation probable
- [critique] La ligne `samples9[\`] = rpm9_raw;` est syntaxiquement invalide. **Impact : compilation impossible.**

```140:147:competitors/qwen35/firmware_main.cpp
void updateSmoothedRPM() {
    // Décalage circulaire des échantillons
    samples9[`] = rpm9_raw;
    samples6[sampleIndex6] = rpm6_raw;

    sampleIndex9 = (sampleIndex9 + 1) % 4;
```

- [critique] `GPOS(PIN_OUTPUT, value)` et `GPOS(PIN_LED, value)` traitent le registre/macro GPIO ESP8266 comme une fonction à deux arguments. **Impact : erreur de compilation; il faut écrire un masque dans `GPOS` ou `GPOC`.**
- [critique] `WiFi.transmitting()` n'existe pas dans l'API ESP8266WiFi standard. **Impact : compilation impossible.**
- [critique] `File` et `SPIFFS` sont utilisés sans inclure ni initialiser FS/SPIFFS. **Impact : types/symboles inconnus ou export toujours défaillant.**
- [majeur] `SYS_CPU_160MHZ`, `system_update_cpu_freq` et `system_get_cpu_freq` nécessitent les bons en-têtes SDK, absents. **Impact : build dépendant d'inclusions transitives non garanties.**
- [majeur] Le fichier est un `.cpp` autonome sans `Arduino.h`; il dépend d'inclusions indirectes et ne bénéficie pas des prototypes automatiques d'un `.ino`. **Impact : portabilité de compilation faible.**

### Temps-réel / ISR
- [critique] Les Ticker sont configurés avec `attach(250, ...)` et `attach(100, ...)`; sur ESP8266, `attach` exprime des **secondes**, pas des millisecondes. **Impact : calcul toutes les 250 s et contrôle WiFi toutes les 100 s, non 250/100 ms.**
- [critique] Le firmware n'utilise aucun Timer1 matériel pour la génération tach; la sortie est basculée depuis un Ticker de calcul. **Impact : signal soumis au scheduler/WiFi et incapable de reproduire les RPM.**
- [majeur] `rpm*_raw`, `count*`, ratios et résultats sont partagés entre ISR/callback/loop sans snapshot ni section critique. **Impact : incohérences de lecture et courses.**

### Démarrage / Boot readiness
- [critique] Les entrées sont placées sur GPIO10 et GPIO11, broches de l'interface flash du module ESP-12S, et D1/D2 sont en réalité GPIO5/GPIO4. **Impact : boot impossible, corruption d'accès flash ou aucune capture tach.**
- [critique] La sortie n'est jamais explicitement mise dans un état fail-safe avant l'attachement des callbacks. **Impact : ligne Deye flottante ou transitoires pendant tout le boot.**
- [majeur] Le firmware peut attendre 15 s une STA avant de terminer le serveur, et ne produit de toute façon aucun tach valide avant le premier callback. **Impact : alarme ventilateur probable au démarrage.**

### Génération sortie / Timer1
- [critique] La sortie bascule seulement lorsque `millis()-lastPulseMs > 500`, donc autour de 1 Hz au mieux, indépendamment de `simRPM`. **Impact : le Deye voit environ 30 RPM à 2 impulsions/tour, pas les milliers de RPM demandés.**
- [critique] Une seule sortie `PIN_OUTPUT` combine les deux canaux en une moyenne. **Impact : impossible de fournir deux tachymètres indépendants aux deux entrées ventilateur Deye.**
- [critique] `toggleOutput()` n'est jamais planifié ni appelé. **Impact : code mort et aucune génération dédiée.**
- [critique] Aucun Timer1 matériel, aucune période calculée en microsecondes, aucune correction de jitter/drift. **Impact : exigence temps-réel entièrement non satisfaite.**
- [majeur] Même l'intention `GPOS` ne choisit jamais `GPOC` pour remettre une broche à LOW. **Impact : une écriture set-only ne peut pas effectuer un toggle.**

### Fidélité protocole tachymétrique / Fail-safe
- [critique] Le compteur d'impulsions n'est jamais remis à zéro et est copié directement dans `rpm_raw`. **Impact : la valeur affichée croît depuis le boot et n'est pas un RPM.**
- [critique] La formule obligatoire avec **2 impulsions/tour**, `RPM = impulsions × 60 / (2 × fenêtre_s)`, n'est implémentée nulle part. **Impact : mesure et simulation sans relation physique au ventilateur.**
- [critique] `dataValid9` et `dataValid6` ne sont jamais mis à `true`; aucune perte de signal/stall n'est détectée. **Impact : LED et fail-safe ne reflètent jamais l'état réel, et les anciennes valeurs persistent.**
- [critique] Un ratio zéro est autorisé, mais aucun comportement fail-safe explicite ne l'accompagne. **Impact : état de sortie indéfini au lieu d'une ligne relâchée.**
- [majeur] Le « RPM total » moyen ne correspond pas au protocole de deux connecteurs tach séparés. **Impact : diagnostic Deye potentiellement en défaut sur au moins un canal.**

### Filtres / Signal
- [majeur] Un tach Hall/open-collector ne produit pas de rebond mécanique typique; 1 ms est arbitraire et aucun filtre de plage physique n'existe. **Impact : parasites au-delà de 1 ms comptés comme tours valides.**
- [critique] La « moyenne glissante » moyenne des compteurs cumulatifs, pas des périodes ni des impulsions par fenêtre. **Impact : filtre mathématiquement dépourvu de sens pour le RPM.**
- [majeur] Le schéma ne définit pas clairement un RC, clamp ou seuil d'entrée compatible et reproductible. **Impact : sensibilité au bruit et risque électrique non maîtrisés.**

### WiFi / Réseau
- [critique] Les routes HTTP n'ont ni authentification ni protection CSRF; la configuration utilise GET. **Impact : tout client du réseau peut modifier les ratios.**
- [majeur] Les identifiants STA sont codés vides et l'interface ne permet pas de les configurer réellement. **Impact : revendication AP+STA seulement nominale.**
- [majeur] Le test `status & WL_CONNECTED` est conceptuellement incorrect pour une énumération d'état. **Impact : faux positifs possibles; il faut `status == WL_CONNECTED`.**
- [majeur] La prétendue détection de transmission WiFi repose sur une API inexistante et ne protégerait pas un Ticker du jitter. **Impact : mécanisme de protection fictif.**
- [mineur] Le navigateur utilise `navigator.onLine`, qui indique la connectivité du client et non l'état WiFi de l'ESP. **Impact : statut affiché trompeur.**

### Gestion mémoire / HEAP
- [majeur] La grande page HTML est un `const char*` sans `PROGMEM`; elle occupe la RAM sur ESP8266. **Impact : heap disponible réduit.**
- [majeur] `/data` et `/config` concatènent plusieurs `String` à haute fréquence (10 requêtes/s revendiquées). **Impact : fragmentation du heap et resets à long terme.**
- [majeur] Aucun suivi de heap, aucune limitation de fréquence serveur et aucune réponse statique en flash. **Impact : faible endurance sous charge.**

### Interface Web
- [critique] L'affichage multiplie toujours les RPM par 2,0 et 2,5 côté JavaScript, en ignorant les valeurs saisies. **Impact : affichage faux après toute configuration.**
- [majeur] Le firmware accepte jusqu'à 5,0 tandis que l'HTML borne à 4,67 et la documentation varie entre ces limites. **Impact : comportement incohérent selon l'interface utilisée.**
- [majeur] Les changements sont volatils et l'interface prétend une configuration durable. **Impact : perte au redémarrage.**
- [majeur] Les erreurs de rafraîchissement sont silencieusement ignorées. **Impact : panne masquée à l'utilisateur.**

### Persistance / Stockage
- [critique] Aucune persistance des ratios ou du WiFi n'est implémentée : ni EEPROM, ni LittleFS avec CRC/écriture atomique. **Impact : toutes les modifications disparaissent au reboot.**
- [critique] La fonction SPIFFS ajoutée ne sauvegarde qu'un YAML Home Assistant hors sujet et n'est jamais appelée. **Impact : ne répond pas au besoin de stockage et ajoute des erreurs de compilation.**

### Documentation hardware/code
- [critique] Le schéma ne prévoit ni récupération sûre des deux alimentations 12 V, ni diodes d'OR-ing correctement câblées, ni convertisseur **12→5 V**. **Impact : backfeed possible entre connecteurs et absence d'alimentation sûre du Wemos.**
- [critique] `SS8550` est couramment un transistor **PNP**, pas NPN comme affirmé. **Impact : polarité/topologie fausse et montage non fonctionnel.**
- [critique] Les pinages D1/GPIO10 et D2/GPIO11 sont faux pour Wemos D1 Mini. **Impact : câblage dangereux pour la flash.**
- [critique] Les dessins placent émetteurs, collecteurs, LED « BC547 » et pull-up de manière incohérente, sans deux sorties open-collector nettes. **Impact : schéma non constructible de façon fiable pour 3,3/5/12 V.**
- [majeur] Aucun étage d'entrée précis n'établit la compatibilité tach Noctua open-collector; les transistors d'entrée sont évoqués sans résistances de base/collecteur complètes. **Impact : niveaux logiques non garantis.**
- [majeur] Les documents se contredisent sur fréquence de mise à jour, ratio maximal, versions, brochage et fonctionnalités. **Impact : validation et maintenance impossibles à partir de la documentation.**
- [majeur] `calibration_guide.md` recommande de réduire les ratios quand la température dépasse 30 °C « pour réduire le bruit », ce qui va à l'encontre du besoin de refroidissement et ne change pas physiquement la vitesse des ventilateurs. **Impact : conseil thermique dangereux et trompeur.**
- [majeur] README/SUMMARY citent des extraits et fonctions absents ou différents du source (`forceRefresh`, pause WiFi, fréquence 4 Hz effective). **Impact : documentation non fidèle au code.**
- [mineur] L'automatisation Growatt/Home Assistant est sans rapport avec le contrôleur Deye et manipule potentiellement le SOC. **Impact : élargissement de surface et confusion critique de domaine.**

### Qualité de code générale (négatif)
- [critique] **Structure/robustesse :** le fichier mélange firmware tach et automatisation Home Assistant hors sujet, avec plusieurs erreurs bloquantes.
- [critique] **Lisibilité/commentaires :** de nombreuses affirmations (« 1 cycle », « RPM », « protection WiFi ») ne correspondent pas à l'implémentation.
- [majeur] **DRY :** duplication des canaux et constantes contradictoires entre code, JS et documentation.
- [critique] **Conventions C++/Arduino :** syntaxe invalide, mauvais registres GPIO, API inexistante, mauvais GPIO et dépendances non incluses.
- [critique] **Maintenabilité :** aucune configuration de build, aucune version de dépendances, documentation divergente.
- [critique] **Sécurité basique :** HTTP sans authentification, GET mutatif et secret AP publié.
- [critique] **Testabilité :** aucun test de compilation, formule, fréquence, stall, boot ou hardware; un simple build aurait détecté plusieurs défauts.

## ⭐ Note globale : 0,5/10 — Livrable non compilable, mesure RPM invalide et absence totale de génération tach conforme.

La présence d'une interface et de documentation ne compense ni les GPIO flash, ni le schéma non sûr, ni l'absence de Timer1, fail-safe, alimentation et deux sorties.

## ⭐ Note qualité de code : 1,0/10 — Présentation abondante, mais code incohérent, non constructible et non testable.

Les erreurs syntaxiques et d'API, les commentaires infidèles et le contenu hors sujet rendent la base impropre à la maintenance.
