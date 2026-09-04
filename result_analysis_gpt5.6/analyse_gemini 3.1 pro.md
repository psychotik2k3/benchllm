# Analyse — gemini 3.1 pro

## Sources analysées

- `competitors/gemini 3.1 pro/deye_fan.ino`
- `competitors/gemini 3.1 pro/schema_et_explications.md`

## ✅ Points techniques positifs

### Temps-réel / ISR

- Les deux ISR GPIO et l’ISR Timer1 portent bien `IRAM_ATTR`, exigence importante sur ESP8266 lorsque le cache flash est indisponible.
- Les ISR d’entrée sont minimales : elles se limitent à incrémenter un compteur `volatile`.
- La copie puis remise à zéro des compteurs est protégée par `noInterrupts()` / `interrupts()`.
- Les sorties sont commutées par accès direct aux registres `GPOS` et `GPOC`, nettement plus déterministe et plus rapide que `digitalWrite()` dans l’ISR.

### Génération sortie / Timer1

- La sortie repose réellement sur le Timer1 matériel FRC1 de l’ESP8266 (`timer1_enable`), et non sur `Ticker`, `delay()` ou du polling dans `loop()`.
- Le timer cadencé à 50 µs produit deux sorties indépendantes à rapport cyclique proche de 50 %. La conversion prévue est cohérente avec 2 impulsions/tour : fréquence tach = RPM/30, donc demi-période en ticks de 50 µs = `300000/RPM`.
- La charge WiFi n’introduit pas de dérive liée à la boucle principale dans chaque période déjà programmée.

### Fidélité protocole tachymétrique / Fail-safe

- La mesure compte les fronts descendants, conformément à une sortie tach à collecteur ouvert.
- L’arrêt d’entrée finit par imposer une sortie à zéro à la fenêtre suivante ; l’état de repos commande le NPN bloqué et laisse la ligne tach Deye remonter via son pull-up.
- Le document prévoit deux NPN en collecteur ouvert, un par canal, ce qui rend les sorties compatibles avec des pull-up Deye à 3,3 V, 5 V ou 12 V sans appliquer cette tension aux GPIO.

### WiFi / Réseau

- Le mode `WIFI_AP_STA` garantit un AP de secours tout en permettant une connexion station.
- Le serveur expose une page de configuration et un endpoint JSON de statut simples.

### Gestion mémoire / HEAP

- La page HTML statique est placée en `PROGMEM`, ce qui évite d’occuper en permanence la RAM avec son contenu source.
- Les structures et états temps-réel ont une taille fixe et faible.

### Interface Web

- L’interface est lisible sur mobile, affiche les quatre valeurs RPM et actualise le statut sans recharger la page.
- Les ratios sont modifiables sans recompilation.

### Persistance / Stockage

- L’EEPROM n’est écrite que lors d’un enregistrement explicite, ce qui limite l’usure flash.
- Un champ magique est prévu pour initialiser des valeurs par défaut.

### Documentation hardware/code

- Le schéma explique correctement le principe de l’OU d’alimentation depuis les deux connecteurs, afin d’éviter le backfeed entre les deux sorties 12 V.
- Il impose à juste titre un buck 12 V → 5 V ; alimenter directement le Wemos en 12 V ou dissiper la chute dans un petit NPN/résistor serait dangereux.
- Les entrées tach Noctua sont décrites comme des collecteurs ouverts tirés à 3,3 V, avec masse commune.
- Les sorties NPN, résistances de base, masses et raccordements sont décrits clairement.

### Qualité de code générale (positif)

- **Structure :** sections fonctionnelles nettes (configuration, web, ISR, démarrage).
- **Lisibilité :** noms de variables et commentaires rendent le flux principal facile à suivre.
- **Commentaires :** les formules RPM/période et le rôle du Timer1 sont explicités.
- **Robustesse :** compteurs partagés et consignes du timer sont modifiés sous section critique.
- **DRY :** l’implémentation reste courte, sans duplication web excessive ; la duplication des deux canaux demeure compréhensible.
- **Conventions C++/Arduino :** emploi cohérent de `setup()`, `loop()`, `volatile`, `PROGMEM` et des API ESP8266.
- **Maintenabilité :** broches et paramètres sont centralisés en début de fichier.
- **Sécurité basique :** l’AP possède un mot de passe et le formulaire impose un minimum fonctionnel aux ratios.
- **Testabilité :** `/status` et l’affichage séparé entrée/sortie permettent une vérification fonctionnelle élémentaire.

## ❌ Points techniques négatifs

### Exactitude de compilation probable

- **[critique]** `#define EEPROM_MAGIC 0xDEYE0001` n’est pas un littéral hexadécimal C++ valide : `Y` n’appartient pas à `[0-9A-F]`. Le firmware ne compile donc pas en l’état, avant même toute validation matérielle.
- **[mineur]** Le fichier dépend explicitement des API Timer1 spécifiques au cœur ESP8266, mais la documentation ne fixe aucune version de cœur ni carte Arduino cible ; une évolution d’API peut produire une erreur de compilation supplémentaire.

### Temps-réel / ISR

- **[majeur]** Une interruption fixe à 20 kHz s’exécute même pour des sorties lentes ou arrêtées. Elle consomme inutilement une part importante du CPU et augmente la pression sur la pile WiFi et le watchdog.
- **[majeur]** L’affirmation documentaire « zéro jitter » / « absolue stabilité métrologique » est fausse. Les interruptions ESP8266 peuvent être retardées par des sections critiques système ou des opérations flash. Le timer réduit fortement la gigue, mais ne l’annule pas.
- **[majeur]** Le générateur incrémente un compteur relatif. Un retard d’ISR reporte le front, puis toutes les périodes suivantes repartent de ce front retardé : la phase dérive cumulativement, contrairement à une planification sur temps absolu.

### Démarrage / Boot readiness

- **[majeur]** Aucune impulsion n’est générée avant la première fenêtre de mesure d’environ une seconde. Le démarrage du WiFi, de l’AP et du serveur se produit avant que `loop()` calcule une consigne ; si le Deye vérifie rapidement le tach, cela peut déclencher une alarme au boot.
- **[majeur]** Aucun délai de grâce, état « boot ready », signal de précharge configurable ou stratégie documentée vis-à-vis du délai d’alarme Deye n’est prévu.
- **[mineur]** Le NPN est commandé GPIO bas au boot logiciel, mais le schéma omet une résistance base-émetteur de rappel. Pendant la phase où le GPIO flotte avant `pinMode`, un faux niveau reste électriquement possible.

### Génération sortie / Timer1

- **[majeur]** La résolution de 50 µs quantifie la période. À 10 000 RPM, `target_ticks=30`, soit 1,5 ms par demi-période ; l’arrondi entier impose une erreur de fréquence pouvant devenir sensible aux hautes vitesses.
- **[critique]** Les ratios n’ont pas de maximum. Si `rpm_out > 300000`, la division entière `300000 / rpm_out` donne zéro, valeur interprétée comme « sortie arrêtée » : une saisie excessive provoque exactement un stall tach.
- **[majeur]** `rpm_in * cfg.ratio` est converti vers `uint32_t` sans contrôle de finitude ni saturation. Une valeur très grande, corrompue ou non finie peut produire un comportement non portable ou aberrant.
- **[mineur]** Une modification de `target_ticks` ne remet pas `ticks` à l’échelle ; le premier front après changement peut conserver une fraction arbitraire de l’ancienne période.

### Fidélité protocole tachymétrique / Fail-safe

- **[majeur]** Le calcul `RPM = pulses * 30` suppose une fenêtre exactement égale à 1,000 s. Or `lastCalcTime = now` et la charge web peuvent allonger la fenêtre ; les impulsions de toute la durée sont alors multipliées comme si elle durait une seconde, surestimant les RPM.
- **[majeur]** La détection de perte n’est qu’implicite à la fenêtre suivante, avec une latence de 0 à plus de 1 s, sans seuil explicite, hystérésis, timeout configurable ni diagnostic de stall.
- **[majeur]** Deux impulsions par tour sont codées en dur. C’est cohérent pour les ventilateurs visés, mais non validé ni configurable ; un tach différent donnerait directement un RPM faux.
- **[majeur]** Aucun bornage de fréquence ne protège le Deye contre un signal simulé physiquement invraisemblable.
- **[mineur]** La fréquence est estimée par comptage sur une seule seconde : faible résolution à bas régime et réponse en échelon au changement de vitesse.

### Filtres / Signal

- **[majeur]** Aucun anti-rebond logiciel, rejet d’impulsions trop rapprochées, filtre médian/EMA ni validation de plage n’est appliqué. Un parasite électrique augmente immédiatement la vitesse calculée.
- **[majeur]** Le schéma ne prévoit qu’une résistance série de 1 kΩ et le pull-up interne, sans pull-up externe défini ni condensateur RC. La qualité du signal dépend donc du câblage et de la valeur peu précise du pull-up interne.

### WiFi / Réseau

- **[majeur]** Aucune authentification HTTP ni protection CSRF : tout client AP ou STA peut lire les identifiants affichés, modifier WiFi/ratios et redémarrer l’équipement.
- **[critique]** Le mot de passe STA est réinjecté en clair dans la page HTML (`%PWD%`). Il est donc exposé à tout utilisateur réseau ouvrant `/`.
- **[majeur]** Le SSID et le mot de passe AP sont fixes et publiés dans le code/documentation ; le mot de passe `deyesetup` n’offre qu’une protection partagée et prévisible.
- **[mineur]** Pas de reconnexion STA supervisée, portail captif, mDNS, diagnostic réseau ni contrôle des erreurs de `softAP()`/`begin()`.

### Gestion mémoire / HEAP

- **[critique]** `strcpy(cfg.ssid, s.c_str())` et `strcpy(cfg.pass, p.c_str())` n’imposent aucune longueur maximale côté serveur. Une requête POST forgée peut déborder `cfg`, corrompre les ratios ou la mémoire et provoquer crash ou exécution indéterminée.
- **[majeur]** La page PROGMEM est tout de même copiée intégralement dans un `String` puis modifiée plusieurs fois ; les réponses JSON utilisent aussi des concaténations `String`. Sur ESP8266, les usages répétés peuvent fragmenter le heap.
- **[mineur]** Aucun suivi du heap ni traitement d’échec d’allocation n’est exposé.

### Interface Web

- **[majeur]** L’interface révèle le mot de passe WiFi existant dans le champ HTML et ne distingue pas « conserver le secret » d’une nouvelle valeur.
- **[majeur]** La validation est essentiellement côté HTML ; une requête directe peut envoyer chaînes surdimensionnées, `nan`, `inf` ou ratios excessifs.
- **[mineur]** Le JavaScript n’a aucune gestion d’erreur : une panne réseau produit des rejets silencieux et un statut figé.

### Persistance / Stockage

- **[majeur]** Un simple magic ne détecte ni corruption partielle ni incompatibilité de structure. Il manque version, taille et CRC.
- **[majeur]** Le retour de `EEPROM.commit()` est ignoré ; l’interface annonce toujours un succès même si la sauvegarde échoue.
- **[mineur]** EEPROM émulée convient à cette petite configuration fixe, mais LittleFS aurait facilité une évolution de schéma. En l’absence de CRC/version, l’EEPROM choisie est ici insuffisamment sécurisée.

### Documentation hardware/code

- **[critique]** La proposition de remplacer les diodes d’ORing par des NPN « base soudée au collecteur » est techniquement dangereuse : la jonction base-émetteur a une faible tenue en inverse et le transistor n’est pas un substitut garanti à une diode de puissance. L’impact est un risque de panne, d’échauffement ou de backfeed.
- **[majeur]** Les 1N4148 proposées pour l’alimentation peuvent être sous-dimensionnées pour les pointes WiFi et n’offrent pas la marge d’une Schottky 1 A. Une SS14/1N5819 est plus appropriée.
- **[majeur]** Le schéma n’ajoute ni condensateurs de réserve/découplage dimensionnés ni protection contre transitoires 12 V ; le Wemos peut brown-out lors des pointes WiFi ou commutations de ventilateurs.
- **[majeur]** La sortie collecteur ouvert est conceptuellement correcte pour 3,3/5/12 V, mais aucune résistance base-émetteur, aucun calcul de courant de collecteur et aucune vérification de la tension réelle du pull-up Deye ne sont fournis.
- **[mineur]** Aucun BOM complet, protocole de contrôle avant mise sous tension ou test au fréquencemètre n’est donné.

### Qualité de code générale (négatif)

- **[critique] Structure/robustesse :** une constante invalide empêche toute compilation, ce qui invalide le livrable exécutable.
- **[majeur] Lisibilité/commentaires :** les commentaires « zéro jitter » et « absolue stabilité » survendent les garanties réelles et peuvent induire un intégrateur en erreur.
- **[majeur] DRY :** les deux canaux du générateur sont dupliqués, augmentant le risque de correction incohérente.
- **[majeur] Conventions C++/Arduino :** emploi de `strcpy` non borné et de macros plutôt que de constantes typées ; validation insuffisante aux frontières.
- **[majeur] Maintenabilité :** absence de version de configuration, de CRC et de séparation claire entre modèle, contrôle et transport web.
- **[critique] Sécurité basique :** secrets exposés, endpoints non authentifiés et débordement mémoire distant.
- **[majeur] Testabilité :** aucun test, aucune procédure de compilation reproductible, aucun mode de génération fixe ni mesure de jitter annoncée.

## ⭐ Note globale : 3,5/10 — Non déployable

Bonne intention temps-réel et électronique de sortie pertinente, mais le firmware ne compile pas et plusieurs défauts peuvent provoquer alarmes, dérive ou compromission.

## ⭐ Note qualité de code : 3,0/10 — Fragile malgré sa clarté

Le code est lisible et bien sectionné, mais l’échec de compilation, le débordement distant et l’absence de validations dominent l’évaluation.
