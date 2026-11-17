# Analyse — gpt6.1sol_medium

**Sources analysées** : [deye_fan/deye_fan.ino](../competitors/gpt6.1sol_medium/deye_fan/deye_fan.ino) ; [README.md](../competitors/gpt6.1sol_medium/README.md) ; [SCHEMA.md](../competitors/gpt6.1sol_medium/SCHEMA.md) ; [ARCHITECTURE_STRICTE.md](../competitors/gpt6.1sol_medium/ARCHITECTURE_STRICTE.md) ; [VERIFICATION.md](../competitors/gpt6.1sol_medium/VERIFICATION.md) ; fichiers de configuration et comptes rendus dans [verification/](../competitors/gpt6.1sol_medium/verification/). Le binaire fourni est contrôlé par SHA-256, pas désassemblé ni exécuté.

**Analyse ajoutée le 17 novembre 2026.** Revue statique du nouveau compétiteur, sans relecture des sources des autres versions. Les références de lignes sans nom de fichier désignent le sketch. Les journaux de compilation livrés sont des preuves fournies par le compétiteur ; aucune recompilation indépendante n’a été effectuée ici. [Contrôles reproduits](./_audit/gpt6.1sol_medium_verification.json) et [méthodologie commune](METHODOLOGIE_2026-11-17.md).

---

## ✅ Points techniques positifs

### Temps-réel / ISR

`capture`, `tach9`, `tach6`, `timeoutUs` et `tachTimer` sont annotées IRAM_ATTR. Pas de flottant, d’allocation ou d’appel réseau dans ces fonctions. Les mesures, la synthèse, le timeout et la LED se traitent en interruption ; les paramètres sont publiés sous sections critiques courtes (202–208). Le serveur n’a pas à exécuter une fonction d’arrêt pour libérer la sortie.

`verification/iram-symbols.txt` fournit les adresses IRAM de capture, tachTimer, micros, timer1_write et des deux wrappers GPIO. Le journal indique 30 172 octets de données RAM et 28 803 octets de code IRAM, auxquels s’ajoutent 32 768 octets réservés au cache. Les empreintes du sketch et du binaire correspondent aux valeurs livrées. Cela établit leur intégrité par rapport au manifeste, sans prouver à lui seul la provenance du build ou le placement de tous les appels indirects.

### Démarrage / Boot readiness

`setup` met les sorties basses avant de les configurer, charge l’EEPROM puis attache capture et Timer1 avant le réseau (409–428). La connexion STA n’attend pas son établissement. Sorties relâchées tant qu’aucune mesure fiable n’est acquise ; ratio initial 1, aucune vitesse nominale inventée.

Trois périodes valides, donc quatre fronts, sont nécessaires avant activation (83–100/111). À 1 600 RPM, qualification après 56,25 ms depuis le premier front, ou environ 56,25–75 ms depuis l’activation de capture selon la phase ; à 3 000 RPM, environ 30–40 ms. S’ajoutent le service du timer, nominalement sous 2 ms, puis la première demi-période. Ces chiffres sont calculés pour un signal régulier déjà présent, sans latence exceptionnelle ; **le délai power-on réel n’est pas mesuré**. Pas de gestion dédiée de brownout ou de watchdog de boot repérée ; le montage s’appuie sur le cœur et l’alimentation.

### Génération sortie / Timer1

Timer1 DIV16 en mode one-shot est initialisé avec les API du cœur, puis rechargé avec `waitUs * 5` (424–427/164). Les unités sont cohérentes à 80 et 160 MHz puisque les échéances sont exprimées en microsecondes, et le timer matériel reste à 5 MHz. Accès GPOS/GPOC, échéances absolues mises à jour par accumulation ; pas de train de rattrapage artificiel après un grand retard (145–150).

L’ordonnanceur réévalue les deux canaux au plus toutes les 2 ms ; un retard constaté est comptabilisé, et une demi-période de 250 µs borne la fréquence. Il expose retard maximum et resynchronisations sans prétendre mesurer le fil physique.

### Fidélité au protocole tachymétrique / Fail-safe

Deux canaux indépendants, convention 2 PPR explicitement documentée : RPM = 30 000 000 / période et RPM de sortie théorique = 15 000 000 / demi-période. Inverse du ratio arrondi en Q12, ratios 1–8. Le produit entier maximal autorisé, arrondi compris, vaut 2 048 004 096 et tient dans uint32_t.

Timeout adaptatif `max(300 ms, 3 × période filtrée)`, jusqu’à 1,5 s, contrôlé dans l’ISR. La capture est invalidée après 1,5 s sans front : seen/good/period sont effacés (119–123), ce qui évite le redémarrage fantôme d’une ancienne mesure au wrap de micros en l’absence de nouveaux fronts. La reprise doit satisfaire de nouveau la qualification. L’interface calcule la sortie à partir de la demi-période effectivement programmée et indique l’écrêtage.

### Filtres / Signal

Rejet des fronts espacés de moins de 2 ms sans déplacer l’horodatage accepté ; période maximale 500 ms, soit une plage nominale d’entrée de 60–15 000 RPM. Premier front utilisé seulement pour amorcer la mesure. EMA entière de coefficient 1/4 appliquée aux fronts acceptés, avec trois périodes de qualification.

### WiFi / Réseau

AP+STA permanent, sommeil désactivé, auto-reconnexion et WiFi.persistent(false). Reconfiguration différée d’une seconde depuis loop pour laisser partir la réponse.

**Authentification HTTP obligatoire et protection CSRF** : authorized vérifie le compte admin et, pour les mutations, un jeton aléatoire collecté via X-CSRF-Token (232–242/429–431). Mutations en POST ; aucun mot de passe enregistré ne revient dans le status. Limite explicitée : Basic sur HTTP n’apporte pas de chiffrement.

### Gestion mémoire / HEAP

HTML en PROGMEM envoyé par send_P ; pas d’allocation dans le trajet ISR. Snapshot cohérent des deux canaux sous section critique et String status réservé à 900 octets. Échappement JSON des guillemets, backslashes et caractères de contrôle, avec snprintf borné pour les séquences Unicode.

### Interface Web

Polling toutes les secondes protégé par busy ; champs chargés une seule fois, donc saisie préservée. Écriture de contenu par textContent et création de nœuds DOM, sans injection des SSID via innerHTML. La page distingue RAM, sauvegarde en attente, échec de sauvegarde et RPM théoriques. Le script embarqué passe `node --check` ; aucun parcours navigateur sur carte n’a été exécuté.

### Persistance / Stockage

Signature, version, taille et CRC32 calculé avec offsetof ; contrôle de la terminaison NUL, des ratios finis et des bornes au chargement. `static_assert` de taille EEPROM (418). Le résultat commit est contrôlé et une erreur devient visible dans l’interface (375–393). La politique de report d’écriture vise à éviter un effacement flash pendant la synthèse et réduit les écritures rapprochées ; sa restriction majeure est exposée ci-dessous.

### Documentation hardware/code

SCHEMA.md fournit schémas et netlists, nomenclature, OU Schottky pour l’électronique, un seul tach par paire et sorties 2N3904 avec rappel base-émetteur. Dimensionnement conservateur du courant de collecteur à 10 mA, sans confondre hFE et saturation garantie. Le buck-boost 5 V proposé couvre aussi les alimentations autour de 5 V ; ses limites de courant restent à vérifier selon la charge. [Référence fabricant Pololu](https://www.pololu.com/product/4083).

Le document traite les masses commutées, les limites de l’USB et l’éventuelle isolation de chaque domaine. ARCHITECTURE_STRICTE sépare clairement le prototype ESP8266 de la variante autonome FPGA/PIO/capture-compare, qui n’est pas livrée. VERIFICATION distingue compilation, inspection de symboles et essais physiques non réalisés.

### Qualité de code générale (positif)

- **Structure / organisation** : Channel et Configuration distincts ; capture, génération, validation, routes HTTP, persistance et bouton de récupération disposent de fonctions dédiées.
- **Lisibilité / nommage** : code compact de 446 lignes, constantes de durée et unités explicites ; quelques noms abrégés mais contexte accessible.
- **Commentaires / documentation inline** : compromis réseau, sauvegarde et mesure réelle expliqués ; les limites ne sont pas présentées comme des performances démontrées.
- **Gestion d’erreurs / robustesse** : contrôle complet du CRC, des chaînes et des ratios ; résultat commit publié ; état de capture invalidé après perte.
- **Duplication / DRY** : fonctions communes aux deux canaux et wrappers ISR minimaux ; échappement JSON et autorisation centralisés.
- **Respect des conventions C++/Arduino** : constexpr, types fixes, volatile sur l’état partagé, offsetof et static_assert de taille ; sections critiques limitées aux échanges nécessaires.
- **Complexité / maintenabilité** : moteur court, séparation des traitements réseau et ISR ; pas de machine de modes ou de coprocesseur simulé inutilement complexe.
- **Sécurité basique** : authentification obligatoire, jeton CSRF, parsing strtof complet, copies bornées et secrets non exposés par le status.
- **Testabilité** : binaire, journal, empreintes et symboles livrés ; formules simples à isoler. Les contrôles reproduits ici restent des modèles, pas des tests du firmware compilé.

---

## ❌ Points techniques négatifs

### Persistance / Stockage

- [majeur] La sauvegarde est interdite tant qu’un canal tourne ou qu’une entrée est récente (385–389). Avec ventilation continue, pendingSave peut rester vrai indéfiniment. Si les deux alimentations ventilateurs coupent ensemble, l’ESP peut s’éteindre avant la fenêtre calme : les nouveaux ratios et identifiants sont perdus au reboot. Le README le reconnaît ; l’exigence de réglages persistants pendant l’usage n’est donc pas pleinement satisfaite par ce montage.
- [mineur] Un seul bloc EEPROM, sans copie de secours transactionnelle ; coupure pendant commit susceptible de perdre aussi l’ancienne configuration. Après un échec commit, servicePersistence ne réessaie plus avant une nouvelle mutation (376). L’état d’erreur est visible, mais la reprise automatique manque.

### Temps-réel / ISR

- [majeur] Le moteur utilise timer1_attachInterrupt, donc une interruption normale, pas une NMI (424–426). GPIO et génération restent retardables par des sections critiques SDK/flash ; déplacer le timeout dans l’ISR supprime la dépendance directe à loop, sans garantir un arrêt lorsque le CPU ou les IRQ sont bloqués. La variante stricte matérielle n’est qu’une proposition, sans HDL ni firmware de coprocesseur. [Implémentation Timer1 du cœur 3.1.2](https://github.com/esp8266/Arduino/blob/3.1.2/cores/esp8266/core_esp8266_timer.cpp).
- [mineur] La fenêtre d’armement minimale de 25 µs peut décaler un front proche d’une autre échéance, avant même les retards SDK. À demi-période minimale 250 µs, 25 µs représentent 10 % de cette demi-période ; ce rapport ne constitue pas une mesure de jitter. Les compteurs internes ne couvrent pas les fronts GPIO manqués ni le signal après transistor.

### Filtres / Signal

- [mineur] Le rejet d’intervalle et l’EMA ne valident pas la largeur d’impulsion ni sa provenance : un bruit périodique cohérent à plus de 2 ms peut satisfaire les trois périodes requises. Pas de filtre médian ou Schmitt matériel dans le schéma de base ; la tenue CEM doit être établie dans l’onduleur.
- [mineur] L’EMA alpha=1/4 introduit une constante de temps d’environ 3,48 périodes, soit 65 ms à 1 600 RPM et 35 ms à 3 000 RPM. Un changement brusque nécessite environ onze nouvelles périodes pour atteindre 95 % du nouvel écart, hors quantification et traitement.

### WiFi / Réseau

- [majeur] Le mot de passe initial AP/admin se déduit directement du SSID et du chip ID (184–185). La protection HTTP est active mais les credentials initiaux sont prévisibles ; tout client à portée peut les reconstruire avant personnalisation. Ce défaut n’annule pas les contrôles CSRF, mais impose un changement de clé initiale.

### Qualité de code générale (négatif)

- [mineur] Aucun test automatisé du scheduler ou de la capture livré : le compte rendu et le binaire ne couvrent pas par tests les transitions de perte/reprise, le wrap ou les conditions d’écriture. Le compilateur et l’ELF ont été retirés ; les instructions de reproduction sont fournies, mais les preuves livrées ne remplacent pas une recompilation indépendante.
- [mineur] Les retours de EEPROM.begin et de la configuration réseau ne sont pas tous exploités ; l’échec softAP est seulement journalisé (224–225), sans stratégie de récupération. Les erreurs commit sont mieux traitées que les erreurs d’initialisation.

Les impacts chiffrés sont des calculs et modèles, pas des mesures sur onduleur. Les 14 contrôles de cet ajout passent : deux empreintes, onze vérifications numériques/de transitions et un contrôle de syntaxe JavaScript. Aucun flash, essai matériel, désassemblage ou recompilation n’a été réalisé pendant cette analyse.

---

## ⭐ Note globale : 8,0/10 — Bonne base fonctionnelle, persistance conditionnelle

Génération double canal cohérente, arrêt traité dans le timer, reprise propre et interface protégée. La sauvegarde potentiellement indéfiniment différée et l’interruption masquable empêchent d’en faire la meilleure base temps réel du corpus. Le journal et le binaire fournis renforcent la traçabilité sans qualifier le comportement physique.

## ⭐ Note qualité de code : 8,4/10 — Compact, lisible et défensif

Bonne séparation des responsabilités, parsing strict, contrôles d’intégrité et traitement explicite des erreurs de sauvegarde. Il manque surtout un banc automatisé reproductible et une stratégie de persistance qui satisfasse l’usage continu sans compromettre le signal.
