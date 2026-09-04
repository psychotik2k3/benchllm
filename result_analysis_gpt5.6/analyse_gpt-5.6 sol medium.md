# Analyse — gpt-5.6 sol medium

Sources analysées (liste exhaustive du dossier) :

- `competitors/gpt-5.6 sol medium/deye_fan.ino`
- `competitors/gpt-5.6 sol medium/SCHEMA_ET_INSTALLATION.md`

## ✅ Points techniques positifs

### Temps-réel / ISR

- Les deux ISR GPIO et l’ISR Timer1 portent `IRAM_ATTR`. Les données parcourues par l’ISR de sortie, notamment les masques GPIO, sont volontairement en DRAM. Il n’y a dans l’ISR Timer1 ni allocation, ni flottant, ni `String`, ni accès réseau.
- La sortie est commutée par registres `GPIO_OUT_W1TS/W1TC`, pas par `digitalWrite()`. C’est adapté à une ISR ESP8266 et réduit latence et jitter logiciel :

```cpp
if (outputPinHigh[channel]) {
  GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, mask);
  outputPinHigh[channel] = false;
} else {
  GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, mask);
  outputPinHigh[channel] = true;
}
```

- Le calcul flottant et le filtre restent dans `loop()`. Le partage principal vers l’ISR est un entier 32 bits aligné, dont l’écriture est atomique sur ESP8266.
- La soustraction non signée des timestamps rend le calcul de période robuste au débordement de `micros()`.

### Démarrage / Boot readiness

- Le moteur GPIO/interruptions/Timer1 est initialisé avant le WiFi et le serveur HTTP. Une association lente ou du trafic HTTP ne retarde donc pas l’installation du chemin temps-réel.
- Les sorties sont mises à LOW avant activation du timer ; avec les résistances de rappel de base documentées, les NPN restent bloqués pendant reset et boot.
- La documentation reconnaît correctement la limite physique : si les deux alimentations ventilateur sont coupées, le Wemos ne peut pas produire un tach sans alimentation auxiliaire.

### Génération sortie / Timer1

- La génération utilise directement le Timer1 matériel ESP8266 en mode one-shot (`TIM_SINGLE`), et non `Ticker`, une boucle de polling, `delayMicroseconds()` ou le traitement HTTP.
- L’ordonnanceur à échéance la plus proche partage correctement l’unique Timer1 entre deux canaux : le temps armé est soustrait aux deux compteurs, puis la prochaine échéance minimale est programmée.
- La période demandée n’est appliquée qu’à un front, ce qui évite un tronquage asynchrone de demi-période.
- La formule de sortie est cohérente : pour une période d’entrée entre impulsions et un ratio `r`, la demi-période carrée vaut `Tentrée/(2r)`. La quantification est de 0,2 µs avec `TIM_DIV16`, très inférieure aux périodes tach usuelles.
- L’architecture limite la dérive cumulative : chaque échéance est relative au timer matériel précédent, non à la cadence irrégulière de `loop()`. Un retard ISR éventuel reste possible sur ESP8266, mais le WiFi ne fabrique pas directement les fronts.

### Fidélité protocole tachymétrique / Fail-safe

- Le code et la documentation appliquent explicitement les 2 impulsions par tour. La formule `RPM = 30 000 000 / période_us` est exacte lorsque l’on mesure un même type de front :

```cpp
state.measuredRpm = 30000000.0f / state.filteredPeriodUs;  // 2 PPR.
state.simulatedRpm = state.measuredRpm * config.ratio[channel];
```

- Les deux premiers fronts sont nécessaires avant publication d’une période, ce qui évite de générer à partir d’une mesure incomplète.
- Les périodes hors plage 500 µs–1,5 s sont rejetées. En cas de perte de signal, un timeout de 2 s met RPM et sortie à zéro et relâche le collecteur.
- Une sortie désactivée est explicitement ramenée LOW côté base NPN : la ligne Deye est relâchée, état fail-safe électrique cohérent.

### Filtres / Signal

- Un filtre IIR léger sur la période (`75 %` historique, `25 %` nouvelle mesure) atténue le bruit tout en restant plus réactif qu’une moyenne longue.
- Le filtre est réinitialisé après perte de signal, empêchant une ancienne vitesse de contaminer la reprise.
- Le filtre temporel matériel documenté (transistor d’adaptation, pull-up 10 kΩ) isole le GPIO de la tension tach du ventilateur.

### WiFi / Réseau

- Le mode permanent `WIFI_AP_STA` garantit un accès local même si la station ne se connecte pas.
- Les identifiants WiFi sont gérés par la configuration CRC plutôt que dupliqués dans la NVRAM SDK (`WiFi.persistent(false)`).
- Le mot de passe AP est validé à 8–63 caractères ; l’AP par défaut est protégé.

### Gestion mémoire / HEAP

- Les longues constantes HTML et chaînes protocolaires utilisent `F()`, donc la majorité du contenu littéral reste en flash.
- Les `String` dynamiques réservent leur capacité (`reserve`) avant concaténation, ce qui limite les réallocations et la fragmentation du heap.
- Les buffers persistants sont de taille fixe et les copies utilisent `strlcpy` avec contrôle préalable de capacité.

### Interface Web

- L’interface est responsive, expose les RPM lus/simulés, l’état des deux canaux, les ratios et le statut STA.
- Les ratios ont des bornes côté navigateur et sont revalidés côté serveur ; une entrée non finie ou hors plage est rejetée avec HTTP 400.
- Les SSID réaffichés dans le HTML passent par un échappement de `& < > " '`. Les mots de passe ne sont pas renvoyés au formulaire.
- Un changement de ratio s’applique sans reboot ; un changement WiFi est suivi d’un redémarrage différé après réponse HTTP.

### Persistance / Stockage

- L’EEPROM émulée contient `magic`, version, taille et CRC32. Les ratios sont contrôlés avec `isfinite()` et bornes avant acceptation.
- Une configuration invalide est remplacée par des valeurs par défaut puis enregistrée. C’est nettement plus robuste qu’un simple marqueur magique.
- Le faible volume et la fréquence d’écriture à l’action utilisateur rendent EEPROM acceptable ici ; LittleFS n’apporterait pas d’avantage décisif pour un unique enregistrement binaire.

### Documentation hardware/code

- Le document couvre l’alimentation depuis les deux connecteurs par diodes Schottky anti-backfeed, PTC, TVS, condensateurs et conversion vers 5 V par buck-boost avec UVLO. Il interdit explicitement le 12 V sur le Wemos.
- L’entrée tach est compatible avec un collecteur ouvert Noctua et empêche une tension 5/12 V d’atteindre le GPIO 3,3 V grâce au NPN d’adaptation.
- La sortie NPN en collecteur ouvert est correctement conçue pour un pull-up Deye à 3,3, 5 ou 12 V, avec résistance de base et pull-down de boot.
- La documentation précise qu’un seul tach par paire doit être lu, que les deux fils tach ne doivent jamais être reliés, et donne une procédure de mesure/validation à l’oscilloscope.
- Les risques d’alimentation, de tension PV/batterie et l’absence de garantie thermique déduite du seul tach sont clairement signalés.

### Qualité de code générale (positif)

- **Structure :** séparation nette configuration, acquisition, Timer1, réseau, web et traitement par canal ; encapsulation dans un namespace.
- **Lisibilité :** noms explicites, constantes dimensionnées et commentaires centrés sur les invariants temps-réel et électriques.
- **Commentaires :** les formules 2-PPR, l’inversion NPN, l’atomicité et les contraintes IRAM sont documentées près du code concerné.
- **Robustesse :** contrôles de bornes, CRC, gestion du wrap temporel, arrêt sur signal périmé et sorties relâchées.
- **DRY :** les canaux partagent structures et fonctions paramétrées ; seules les petites ISR wrappers sont dupliquées pour `attachInterrupt`.
- **Conventions C++/Arduino :** `constexpr`, types de largeur fixe, `reinterpret_cast`, références et initialisations explicites sont employés de façon cohérente.
- **Maintenabilité :** limites et broches sont centralisées, et l’état temps-réel est clairement distingué de la configuration persistée.
- **Sécurité basique :** échappement HTML, mots de passe non réaffichés, validation serveur et AP protégé par défaut.
- **Testabilité :** les conversions et validations sont isolées en fonctions, et la documentation fournit des tests électriques et à l’oscilloscope reproductibles.

## ❌ Points techniques négatifs

### Temps-réel / ISR

- **[majeur]** `recordInputEdge()` est marqué IRAM mais appelle `micros()`. Sa sûreté dépend de l’implémentation IRAM du core ESP8266 3.x ciblé ; le projet ne fige ni version exacte ni test de map mémoire. Un changement de core peut provoquer un accès flash depuis ISR pendant cache indisponible, avec crash ou mesure perdue.
- **[mineur]** Les sections `noInterrupts()/interrupts()` réactivent toujours les interruptions au lieu de restaurer l’état antérieur. Dans le seul `loop()` actuel l’impact est faible, mais cela rend la fonction moins sûre à réutiliser dans un contexte déjà critique.
- **[mineur]** L’ISR Timer1 boucle sur deux canaux et effectue plusieurs accès `volatile`. C’est acceptable aux fréquences visées, mais aucune mesure de pire cas ni budget de latence n’est fourni.

### Démarrage / Boot readiness

- **[majeur]** « Timer initialisé avant WiFi » ne signifie pas « tach disponible au boot » : `requestedHalfTicks` reste nul jusqu’à deux fronts valides puis un passage dans `loop()`. Si le Deye vérifie le tach avant la montée en vitesse du Noctua, il peut lever une alarme malgré l’ordre d’initialisation.
- **[majeur]** Si les rails ventilateur sont absents ou hachés au démarrage, le contrôleur alimenté par ces mêmes rails ne peut pas garantir un signal anticipé. Le document le reconnaît mais ne fournit pas de mode boot synthétique ni circuit auxiliaire complet.

### Génération sortie / Timer1

- **[majeur]** La sortie est plafonnée silencieusement par `MIN_HALF_PERIOD_TICKS = 250` (50 µs), soit 10 kHz de tach ou 300 000 RPM à 2 PPR. Un ratio/période demandant davantage affiche encore le RPM théorique non plafonné : l’UI peut donc annoncer une vitesse différente du signal réellement généré.
- **[mineur]** Lors de l’activation, la première demi-période est forcée à 50 µs au lieu de la période cible. Le premier pulse est transitoirement trop court, ce qui peut perturber une fenêtre de mesure très brève.
- **[mineur]** La promesse d’indépendance au WiFi est raisonnable mais pas absolue sur ce SoC monocœur. Aucun relevé de jitter maximal, percentile ou dérive sous trafic n’est livré ; seul un conseil de vérification oscilloscope figure dans la documentation.

### Fidélité protocole tachymétrique / Fail-safe

- **[majeur]** Le timeout de 2 s est long pour une protection ventilateur : après un stall, l’ancien tach continue d’être simulé pendant jusqu’à deux secondes. L’onduleur peut croire le ventilateur actif pendant cette fenêtre.
- **[majeur]** Un unique intervalle valide suffit à maintenir la sortie jusqu’au timeout, sans confirmation de plusieurs pulses cohérents. Une impulsion parasite isolée après une ancienne mesure peut réactiver brièvement un tach crédible.
- **[mineur]** Les bornes de période correspondent à 20–60 000 RPM à 2 PPR ; elles sont très larges par rapport aux ventilateurs cités et laissent passer davantage de bruit basse fréquence qu’une plage contextualisée.
- **[mineur]** Aucun diagnostic distinct ne sépare stall, entrée débranchée, période hors plage et simple attente ; cela limite l’investigation d’un défaut réel.

### Filtres / Signal

- **[mineur]** Le filtre IIR fixe introduit un retard lors d’une variation rapide de vitesse et n’a ni seuil de saut ni médiane contre un outlier. Une période valide aberrante peut déplacer la sortie sur plusieurs mesures.
- **[mineur]** Il n’existe pas d’hystérésis de présence ni de compteur de validation ; la reprise repose sur une seule période dans une plage très large.

### WiFi / Réseau

- **[majeur]** Le serveur HTTP et l’API de configuration n’ont aucune authentification. Tout client connecté à l’AP ou au LAN STA peut modifier ratios et identifiants, puis redémarrer le contrôleur ; l’impact est une falsification du tach ou une perte d’accès.
- **[majeur]** Le trafic est en HTTP clair : les identifiants saisis peuvent être observés sur le réseau local.
- **[majeur]** Il n’y a aucune protection CSRF ni contrôle d’origine. Une page visitée par un client ayant accès à l’équipement peut soumettre des paramètres à `/api/config`.
- **[mineur]** Aucune stratégie explicite de reconnexion STA n’est implémentée après une déconnexion prolongée ; le comportement dépend du core WiFi.

### Gestion mémoire / HEAP

- **[majeur]** Chaque accès à `/` construit environ 7 ko dans un `String`. Même avec `reserve()` et `F()`, le buffer final occupe le heap et exige un bloc contigu ; sous connexions répétées, cela peut échouer ou fragmenter la mémoire limitée de l’ESP8266.
- **[mineur]** Les réponses JSON utilisent encore plusieurs objets `String` temporaires (`WiFi.*IP().toString()`, conversions flottantes). L’impact est surtout une pression heap évitable, hors ISR.
- **[mineur]** Aucun suivi du heap libre/minimal ni réponse de repli en cas d’échec d’allocation n’est prévu.

### Interface Web

- **[majeur]** L’interface ne signale pas les erreurs de `fetch()` ni les statuts HTTP lors de la sauvegarde ; elle affiche le corps comme message mais ne distingue pas clairement succès et échec. Une configuration non appliquée peut être prise pour acquise.
- **[mineur]** Le RPM affiché comme « simulé » est calculé avant saturation Timer1 ; il peut diverger du signal électrique réellement émis.
- **[mineur]** Aucun bouton explicite n’efface le mot de passe AP ou ne restaure la configuration usine ; la récupération après mauvaise configuration dépend d’un reflashing ou d’une écriture EEPROM invalide.

### Persistance / Stockage

- **[mineur]** L’EEPROM émulée est réécrite à chaque POST valide, même si seul un ratio identique est soumis. Des sauvegardes automatisées répétées peuvent user inutilement la flash.
- **[mineur]** Il n’y a ni double-slot ni écriture transactionnelle. Une coupure pendant `EEPROM.commit()` est détectée au prochain boot par CRC, mais entraîne alors la perte complète de la configuration et le retour aux valeurs par défaut.

### Documentation hardware/code

- **[majeur]** Le composant U2 est décrit génériquement comme « buck-boost 3–18 V » sans référence qualifiée, courant de crête, comportement d’enablement ni seuil UVLO chiffré. Une implémentation avec un module médiocre peut brown-out sous les pointes WiFi.
- **[majeur]** Le montage suppose des masses communes entre les deux connecteurs Deye. La procédure demande des mesures, mais ne conditionne pas explicitement le raccordement à la vérification d’absence de différence de potentiel entre masses ; une mauvaise hypothèse peut créer un courant de boucle.
- **[mineur]** Le schéma est textuel et ne donne ni PCB, ni distances, ni implantation de masse/TVS. La compatibilité CEM dans un onduleur reste à démontrer.
- **[mineur]** Aucun test automatisé, sketch de banc ou capture oscilloscope mesurée n’accompagne les affirmations de fréquence et de jitter.

### Qualité de code générale (négatif)

- **Structure : [mineur]** le fichier unique de plus de 500 lignes mélange firmware, HTML, CSS et JavaScript, ce qui alourdit les revues et les modifications.
- **Lisibilité : [mineur]** quelques noms (`outputPinHigh`) décrivent le niveau GPIO de base et non le niveau tach collecteur, source possible de confusion logique.
- **Commentaires : [mineur]** ils sont globalement bons, mais certaines garanties (« exclusivement hardware Timer1 », indépendance WiFi) sont formulées plus fortement que ce qui est démontré.
- **Robustesse : [majeur]** l’absence d’authentification et le timeout stall long sont les principales lacunes fonctionnelles.
- **DRY : [mineur]** les fragments de construction HTML restent très volumineux dans le code malgré l’usage de fonctions communes.
- **Conventions C++/Arduino : [mineur]** l’état partagé gagnerait à utiliser une section critique restauratrice ou des helpers atomiques documentés plutôt que le couple global `noInterrupts()/interrupts()`.
- **Maintenabilité : [mineur]** la dépendance aux primitives Timer1 spécifiques ESP8266 n’est pas isolée derrière une abstraction ni verrouillée par une version de core.
- **Sécurité basique : [majeur]** aucune auth, aucun CSRF et HTTP clair donnent à un accès réseau local le contrôle des paramètres de sécurité fonctionnelle.
- **Testabilité : [majeur]** aucune suite de tests hôte, aucune simulation de wrap/stall/saturation et aucune mesure instrumentée de jitter ne sont fournies.

## ⭐ Note globale : 8,4/10 — Très solide techniquement, mais le boot sans mesure, le délai de stall et l’absence d’authentification empêchent une confiance de niveau industriel.

La chaîne Timer1/IRAM/collecteur ouvert et la documentation électrique sont les plus complètes des éléments évalués, sous réserve de validation réelle au scope.

## ⭐ Note qualité de code : 8,2/10 — Code structuré et défensif, pénalisé par le monolithe web embarqué, la sécurité réseau et l’absence de tests automatisés.

La qualité d’implémentation est élevée pour un sketch ESP8266, mais les garanties temps-réel et de sûreté doivent encore être mesurées et testées.
