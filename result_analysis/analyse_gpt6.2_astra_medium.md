# Analyse — gpt6.2_astra_medium

**Sources analysées** : [deye_fan/deye_fan.ino](../competitors/gpt6.2_astra_medium/deye_fan/deye_fan.ino), [fpga/tach.v](../competitors/gpt6.2_astra_medium/fpga/tach.v), [fpga/icebreaker.pcf](../competitors/gpt6.2_astra_medium/fpga/icebreaker.pcf), [SCHEMA.md](../competitors/gpt6.2_astra_medium/SCHEMA.md). Le bitstream [fpga/tach.bin](../competitors/gpt6.2_astra_medium/fpga/tach.bin) est inventorié et haché, sans exécution ni correspondance source/binaire démontrée par reconstruction.

**Analyse du 18 novembre 2026.** Les lignes .ino et .v sont distinguées ci-dessous. Seul ce nouveau compétiteur a été lu ; la comparaison finale repose sur les rapports existants. Les [14 contrôles reproduits](./_audit/gpt6.2_astra_medium_verification.json) portent sur un modèle Python, le protocole, le brochage et la syntaxe JavaScript ; aucune compilation Arduino/Verilog, simulation HDL, programmation FPGA ou mesure physique n’a été effectuée.

**Périmètre matériel différent** : cette version ajoute une carte iCEBreaker et quatre optocoupleurs, deux convertisseurs isolés et un multiplexeur d’alimentation. Elle constitue une solution complète à matériel supplémentaire, pas un firmware substituable sur un Wemos seul. Le classement global doit se lire avec cette réserve ; le meilleur choix Wemos seul reste indiqué séparément.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

**Capture et génération matérielles indépendantes du Wemos.** Aucun front tach n’est traité par une ISR ESP8266. Dans `channel` (.v 1–56), deux bascules synchronisent l’entrée au domaine 12 MHz ; la qualification, la mesure, la synthèse et l’expiration fonctionnent en logique synchrone. `top` mémorise les ratios après validation SPI. Une pause au milieu d’une transaction ou une écriture LittleFS ne suspend donc pas ce moteur, tant que le FPGA reste alimenté et cadencé.

Le code isole ainsi le calcul tach du logiciel réseau par construction. Cela n’établit pas l’absence de couplage électrique/RF dans le montage réel et ne remplace pas les mesures CEM, d’horloge et de fronts aux collecteurs.

### Démarrage / Boot readiness

Les registres de sortie et de période sont initialisés à zéro, ainsi que les ratios r9/r6 (.v 3–7/62). Le schéma fournit des rappels base-émetteur pour laisser les collecteurs ouverts avant configuration du FPGA. L’ESP charge la configuration puis réalise un échange avant de lancer le réseau (.ino 197–199) ; aucun délai de connexion STA ne bloque l’activation.

Le FPGA doit être configuré depuis sa flash et recevoir une commande valide avant d’émettre. Si le premier échange arrive trop tôt, le polling nominal de 250 ms renvoie les ratios (.ino 205). Deux fronts qualifiés permettent d’acquérir une période. À 1 600 RPM, cela représente environ 18,75–37,5 ms après disponibilité du moteur, plus la qualification de quelques ticks et l’amorçage de sortie. À cela s’ajoutent la configuration FPGA et le setup Wemos, non chronométrés : **aucun temps power-on exact n’est démontré**. Aucun nominal fictif n’est émis avant mesure. Pas de watchdog matériel supplémentaire ni de stratégie dédiée de brownout livrés.

### Génération sortie / Timer1

Timer1 n’est pas utilisé : le diviseur FPGA produit un tick toutes les 96 périodes de l’horloge 12 MHz, soit 8 µs (.v 59–61). Pour une période d’entrée P en ticks et un ratio R en centièmes, l’accumulateur ajoute 2R à chaque tick et bascule à P×100 (.v 14–15/41–52). La fréquence moyenne vaut donc fréquence d’entrée × R/100.

Le reste fractionnaire est conservé ; à consigne constante, les demi-périodes numériques alternent entre deux nombres de ticks consécutifs. Le modèle reproduit 500 ticks par demi-période pour 50 Hz ×2,50, et 357/358 ticks pour 100 Hz ×1,75. Les deux voies disposent d’un état séparé. Le FPGA mesure aussi l’intervalle entre deux activations de chaque sortie : la télémétrie n’est pas un simple produit RPM×ratio.

### Fidélité au protocole tachymétrique / Fail-safe

Deux voies et convention 2 PPR : la page convertit une période de ticks de 8 µs par 3 750 000/P (.ino 149–150). La mesure rejette des intervalles inférieurs à 2 ms et accepte des périodes inférieures à 500 ms. Les compteurs saturent au lieu de revenir à zéro ; après environ 500 ms sans front, période et génération sont invalidées et drive est libéré (.v 22–38). La reprise doit réamorcer la capture.

Le maintien du dernier ratio si le Wemos disparaît ne masque pas une perte de tach : le timeout propre au FPGA reste actif. Des commandes de 72 bits sont validées sur signature, longueur, XOR, octets réservés et bornes 100–800 avant publication simultanée des deux ratios (.v 81–85).

### Filtres / Signal

Synchronisation de domaine d’horloge puis qualification par trois échantillons identiques, à 8 µs ; bornage des intervalles de fronts et vieillissement saturant. Les modèles de perturbation basse de un et deux ticks sont rejetés. Pas de flottant, d’allocation ou d’appel logiciel dans le filtrage.

### WiFi / Réseau

AP+STA, connexion STA sans boucle d’attente et WiFi.persistent(false). Configuration réseau appliquée 1,2 s après la réponse pour éviter une coupure immédiate. Serveur synchrone, dont les délais ne cadencent aucun front FPGA.

Authentification HTTP obligatoire avec mot de passe AP/admin **généré aléatoirement**, sans dérivation du SSID (.ino 54–55/95–99). Les mutations passent par POST et jeton de session. Les mots de passe ne sont pas exposés dans `/live`. SPI volontairement lent, synchronisé côté FPGA ; le débit de commande n’impose pas celui de conversion.

### Gestion mémoire / HEAP

États FPGA et buffers SPI de taille fixe ; aucune allocation dans le chemin des signaux. HTML source en PROGMEM. Échappement des attributs HTML, y compris `@` pour ne pas injecter de pseudo-placeholders durant les substitutions (.ino 128–138). Les chaînes dynamiques restent confinées au serveur Wemos.

### Interface Web

Polling récursif à 750 ms, sans rechargement complet du formulaire ; seules les cellules de mesures et le statut changent. Valeurs insérées par textContent. Absence du convertisseur affichée explicitement, et séparation entre période mesurée dans le FPGA et niveau physique reçu par le Deye. Le JavaScript extrait passe `node --check` ; aucun parcours navigateur réel n’a été effectué.

### Persistance / Stockage

LittleFS, fichier temporaire `/next` puis renommage vers `/settings`, CRC32 par offsetof et vérification de version/taille à la lecture (.ino 25–58). La configuration candidate est validée, enregistrée avec contrôle de taille écrite et renommage réussi, puis publiée en RAM et envoyée au FPGA (.ino 168–189). Une erreur d’enregistrement renvoie HTTP 500 ; le changement n’est pas présenté comme réussi.

La sauvegarde peut avoir lieu pendant une ventilation continue : la conversion reste dans le FPGA. Cela supprime la nécessité de différer indéfiniment le commit jusqu’à l’arrêt des ventilateurs. La garantie exacte en cas de coupure doit néanmoins être vérifiée avec le filesystem et la chaîne d’alimentation réels.

### Documentation hardware/code

SCHEMA.md fournit schémas, brochages, plages de tension, procédure de reconstruction FPGA et scénarios de réception au banc. L’isolation est traitée pour les deux alimentations et les deux directions tach : retours G9/G6 distincts de la logique, convertisseurs isolés et optocoupleurs. Le second tach de chaque paire reste isolé.

Les dix affectations du PCF correspondent au [brochage officiel iCEBreaker](https://github.com/icebreaker-fpga/icebreaker-examples/blob/main/icebreaker/icebreaker.pcf). Le brochage et l’activation du [LM66200](https://www.ti.com/lit/ds/symlink/lm66200.pdf) concordent avec le schéma. La gamme et le brochage simple sortie des [Traco TDN 5WI](https://www.tracopower.com/sites/default/files/products/datasheets/tdn5wi_datasheet.pdf) correspondent aux indications du document ; l’ouverture directe du PDF a été refusée, mais ses données indexées et le catalogue fabricant ont été consultés. Aucune validation électrique indépendante n’est affirmée.

### Qualité de code générale (positif)

- **Structure / organisation** : module HDL channel instancié deux fois, module top pour le protocole ; firmware Wemos réduit à configuration, SPI, stockage et réseau.
- **Lisibilité / nommage** : petite livraison, environ 209 lignes Arduino et 97 lignes Verilog ; unités de ratio et période documentées.
- **Commentaires / documentation inline** : autonomie des deux moteurs, précision numérique, limites de l’alimentation et réception physique distinguées.
- **Gestion d’erreurs / robustesse** : CRC et taille de stockage, candidate temporaire, écriture vérifiée et HTTP 500 ; commandes FPGA bornées et rejetées atomiquement si invalides.
- **Duplication / DRY** : moteur channel partagé par les deux voies ; fonctions communes d’autorisation, parsing et stockage.
- **Respect des conventions C++/Arduino** : constexpr, types fixes et offsetof ; aucune fausse ISR IRAM ou API Timer1 sur le Wemos. Initialisations et affectations non bloquantes explicites en HDL.
- **Complexité / maintenabilité** : séparation réseau/moteur claire et peu d’états ; modification de matériel requise, mais interfaces petites et identifiables.
- **Sécurité basique** : clé initiale aléatoire, authentification sur page/status/configuration, jeton POST, bornes de paramètres et échappement des attributs.
- **Testabilité** : HDL et contraintes de broches fournis, commandes de synthèse explicites ; chaque canal et le protocole peuvent être testés isolément. Le banc de test automatisé manque dans la livraison.

---

## ❌ Points techniques négatifs

### Documentation hardware/code

- [majeur] La variante exige une carte FPGA et plusieurs composants d’alimentation/isolation supplémentaires. Elle ne satisfait pas un déploiement limité au seul Wemos, résistances et NPN. Avant choix, accepter explicitement ce changement d’architecture ; dans le classement « Wemos seul », cette version est hors catégorie. Aucune adaptation de cette logique au Wemos seul n’est fournie.
- [majeur] La précision de 8 µs concerne le moteur numérique, pas les collecteurs. Les VO617A-3 sont des phototransistors saturables : la [fiche Vishay](https://www.vishay.com/docs/83430/vo617a.pdf) donne notamment un temps d’extinction typique de 25 µs dans ses conditions d’essai, sans maximum universel garantissant ce montage. L’alimentation, le courant LED, le CTR, la température et le pull-up peuvent modifier délais et largeur de pulse. Aucun bilan de temps au pire cas ni essai électrique ne qualifie la plage entière annoncée ; ne pas appeler « jitter physique ≤8 µs » ce qui est seulement une propriété numérique.

### Fidélité au protocole tachymétrique / Fail-safe

- [mineur] L’ESP déclare le FPGA connecté sur le seul octet 0xD3 (.ino 91), sans checksum de réponse ni validation des quatre périodes. Une corruption des octets de mesures peut conserver ok=true et afficher de faux RPM. Cela ne modifie pas directement la génération, mais affaiblit son diagnostic.
- [mineur] Le XOR de commande détecte une erreur de bit isolée, mais pas toutes les erreurs multiples : modifier le même bit dans deux octets conserve le contrôle. Le modèle accepte ainsi des ratios 2,51/1,74 au lieu de 2,50/1,75 après deux altérations coordonnées, toujours dans les bornes. Préférer CRC et numéro de séquence pour diagnostiquer une liaison bruitée.
- [mineur] Le timeout est fixe à environ 500 ms, même à haute vitesse ; pas de contrôle de plausibilité propre à chaque Noctua ni de plafond de RPM Deye indépendant du ratio. La plage d’entrée acceptée atteint 15 000 RPM et le ratio 8 permet théoriquement 120 000 RPM de sortie, demi-période numérique 125 µs. La tolérance réelle de l’onduleur et des optocoupleurs à ces extrêmes n’est pas établie.

### Filtres / Signal

- [mineur] Aucun lissage multi-périodes ni compensation de l’asymétrie entre pôles : chaque intervalle accepté remplace la mesure (.v 24). Les variations mécaniques ou de seuil optique se répercutent sur la synthèse. Le filtre de largeur et le seuil 2 ms rejettent des parasites courts, pas un bruit périodique plausible.

### Persistance / Stockage

- [mineur] loadSettings accepte version/taille/CRC sans vérifier la terminaison des quatre chaînes ni les bornes des ratios (.ino 45–50). Les chemins HTTP produisent des valeurs bornées, mais un fichier CRC-valide mal formé peut provoquer une lecture de chaîne hors tampon ou une commande FPGA systématiquement rejetée. Ajouter une validation sémantique après lecture.

### Gestion mémoire / HEAP

- [mineur] Malgré PROGMEM, page copie tout le HTML dans un String et enchaîne des substitutions (.ino 133–139). live et escapeAttr créent aussi des chaînes dynamiques sans réserver leur capacité. Risque de pics RAM et fragmentation à qualifier sous charge ; le FPGA préserve la conversion, mais pas la disponibilité de l’interface.

### Qualité de code générale (négatif)

- [majeur] Aucun testbench Verilog, journal de simulation, rapport de timing, version de synthèse/placement ni manifeste reliant le bitstream au RTL livrés. Le fichier tach.bin et les commandes de reconstruction existent, mais sa correspondance au code et le respect de 12 MHz ne sont pas démontrés par une reconstruction indépendante ici. Les modèles Python passent sans constituer une simulation du RTL ou des synchroniseurs à 12 MHz.
- [mineur] Plusieurs constantes HDL restent anonymes (249, 62499, 62500, 100, 95), sans paramètre commun au C++ et au HDL. Changer l’horloge ou le pas de temps peut désynchroniser conversion RPM, filtres, timeout et interface. Les noms très abrégés rendent le protocole moins immédiatement lisible.
- [mineur] Pas de récupération locale de clé prévue hormis l’effacement de la flash ; résultat de l’enregistrement initial ignoré et erreur de montage LittleFS peu visible au premier accès. Une initialisation non persistée génère une autre clé au prochain boot. Le formulaire remonte mieux les erreurs d’enregistrement ultérieures.

Les impacts chiffrés ci-dessus sont calculés ou tirés de conditions de fiche fabricant, pas mesurés sur l’installation. La documentation reconnaît que les essais temporels et thermiques restent à faire. Aucun ancien rapport n’a été réévalué à partir de ses sources pendant cet ajout.

---

## ⭐ Note globale : 8,8/10 — Meilleure architecture autonome, avec FPGA supplémentaire

Première solution du corpus à livrer ici un moteur matériel autonome en RTL, sa liaison de commande et l’interface Wemos, avec arrêt matériel et sauvegarde indépendante de la synthèse. Elle prend la tête du classement fonctionnel si ce matériel supplémentaire est accepté. Le choix « Wemos seul » reste distinct ; la correspondance du bitstream, les délais optiques et les essais physiques doivent encore être validés.

## ⭐ Note qualité de code : 8,1/10 — Séparation nette, validation reproductible incomplète

Bonne architecture et contrôles de configuration, avec un HDL compact partagé entre canaux. L’absence de testbench, de rapports de timing et de validation sémantique complète du stockage limite la qualité démontrée, malgré l’intérêt fonctionnel du matériel autonome.
