# Schéma électronique — variante Wemos seul

## Conditions de validité

Ce schéma fonctionne pour deux alimentations positives indépendantes **avec retour GND réellement commun et non commuté** et des entrées tach Deye à tirage positif limité en courant. Les deux tensions positives n'ont pas besoin d'être égales ou présentes ensemble. Mesurer les retours et leur commande avant de réunir les masses : certains appareils coupent le côté négatif des ventilateurs. Une simple continuité mesurée hors tension ne suffit pas à exclure une commande par transistor côté masse.

**Si un retour est flottant ou commuté côté masse, ne pas réaliser ce montage à masse commune.** Relier les retours pourrait contourner la commande, alimenter une voie arrêtée ou endommager l'électronique. Il faut alors l'adaptation isolée décrite en fin de document ; son matériel n'est pas dans la liste disponible.

Le brochage physique Deye est inconnu : J9 et J6 désignent des **fonctions mesurées**, pas des numéros de broches supposés. Les quatre ventilateurs restent alimentés par leur connecteur de taille correspondant, sans mise en commun des positifs.

## 1. Vue d'ensemble et alimentation

```text
J9 +FAN9 ----+---- rouges des deux NF-A9 en parallèle
            |
            +-- F1 -- A D1 K ------+
                       SS34       |
                                  +---- V_OR ---- VIN U1 S13V10F5
J6 +FAN6 ----+-- F2 -- A D2 K ------+                GND ------ GND
            |          SS34       |                VOUT = +5 V
            |                     |                     |
            +---- rouges des      C1 470 µF/25 V         SW1
                  deux NF-A6      C2 100 nF             |
                  en parallèle    |                     +---- Wemos 5V
                                  |                     |
                                  |                C3 470 µF/10 V
                                  |                C4 100 nF
GND commun -----------------------+---------------------+---- Wemos GND
   |       |             |               |
 J9 GND  J6 GND       GND des deux A9   GND des deux A6
```

Chaque condensateur dessiné en parallèle représente une branche entre son rail et GND ; C1 et C2 sont en parallèle entre eux, comme C3 et C4. La netlist ci-dessous précise chaque borne. **A = anode, K = cathode (bague)**. D1 et D2 ont leurs cathodes réunies à V_OR. Elles ne servent qu'à alimenter l'électronique, pas les moteurs.

| Élément / borne | Connexion exacte |
|---|---|
| J9 +FAN9 | rouges des deux A9, entrée F1 |
| J6 +FAN6 | rouges des deux A6, entrée F2 |
| F1 sortie | anode D1 |
| F2 sortie | anode D2 |
| D1 cathode, D2 cathode | V_OR, U1 VIN, C1 positif, C2 borne 1 |
| C1 négatif, C2 borne 2 | GND commun |
| U1 GND | GND commun |
| U1 VOUT | entrée interrupteur SW1 |
| SW1 sortie | Wemos 5V, C3 positif, C4 borne 1 |
| C3 négatif, C4 borne 2 | GND commun |
| Wemos 3V3 | uniquement circuits logiques / résistances de rappel |

U1 : module buck-boost 5 V **Pololu S13V10F5**, plage d'entrée publiée 2,8–22 V ; respecter son courant de sortie disponible en fonction de VIN, sa dissipation et les instructions du fabricant. Dimensionner pour les pointes de consommation de la carte, réserve visée 500 mA côté 5 V. La plage exploitable pour l'installation est limitée aussi par les ventilateurs et les composants : ne pas considérer la plage du module comme une autorisation de surtension des Noctua. F1/F2 : fusibles ou PTC pour branches électroniques, valeur initiale de conception 0,75 A, à ajuster d'après courant de démarrage, capacité des connecteurs et essais. D1/D2 : SS34, 3 A / 40 V. C1/C3 : électrolytiques faible ESR, en parallèle avec les 100 nF, proches du module et du Wemos respectivement. Vérifier la stabilité du module avec les capacités ajoutées.

Si +FAN9 = 12 V et +FAN6 = 8 V, D1 fournit l'essentiel du courant électronique et D2 bloque. Si J9 est coupé, D2 prend le relais. Aucune sortie positive ne réinjecte vers l'autre par cette branche. Les moteurs ne changent pas de source. Si les deux sources sont absentes ou trop faibles, la carte s'arrête : les résistances base-émetteur bloquent alors les sorties NPN. Le montage ne dispose pas d'une énergie autonome. En cas de forte modulation de l'alimentation des ventilateurs, mesurer V_OR et 5 V à l'oscilloscope ; C1 ne garantit pas le maintien pour des coupures arbitraires.

Le buck-boost évite la perte de régulation d'un simple buck lorsque la tension ventilateur baisse autour de 5 V. Il ne permet pas aux moteurs 12 V de fonctionner à une tension arbitraire : vérifier le démarrage et la plage propre aux fans. Aucun régulateur ne peut extraire de puissance d'un connecteur éteint.

SW1 déconnecte l'alimentation externe du Wemos. Pour programmer, débrancher entièrement l'installation de l'onduleur et alimenter par USB sur établi. Ne pas mettre en parallèle USB 5 V et U1 : le chemin USB exact des clones V2.3.0 n'est pas présumé protégé contre les retours de courant. Ne pas connecter une masse USB de PC à un onduleur sous tension sans avoir établi son isolation électrique.

## 2. Entrées tach Noctua, à réaliser deux fois

```text
Wemos 3V3 -------- R1 4,7 kΩ -------o T9
                                   |
NF-A9 n°1 jaune (broche 3) ---------+
                                   |
                                   +---- R2 1 kΩ ---- D1 / GPIO5 (entrée)

Wemos 3V3 -------- R3 4,7 kΩ -------o T6
                                   |
NF-A6 n°1 jaune (broche 3) ---------+
                                   |
                                   +---- R4 1 kΩ ---- D2 / GPIO4 (entrée)

NF-A9 n°2 jaune ---- isolé séparément, non raccordé
NF-A6 n°2 jaune ---- isolé séparément, non raccordé

Noirs des quatre Noctua = GND commun (si condition de masse validée).
Rouges = alimentation de la paire correspondante, voir section 1.
```

Le brochage trois broches Noctua FLX est noir GND, rouge alimentation, jaune RPM. Vérifier le détrompeur et la fiche du modèle, pas la disposition NMB. Le tach Noctua à collecteur ouvert tire T9/T6 vers zéro et le rappel externe le tire **uniquement à 3,3 V** : courant de rappel environ 0,7 mA. R2/R4 sont des résistances série d'amortissement, **pas** des convertisseurs de tension. Ne raccorder ni le tach Deye ni 12 V à ces nœuds.

Pas de condensateur de filtrage arbitraire sur les GPIO : il ralentirait les fronts sans hystérésis. Employer câbles courts, paire tach/GND, séparation des fils moteurs et du convertisseur, condensateurs de découplage appropriés. Pour un environnement très bruité, ajouter un vrai buffer Schmitt 3,3 V et des protections ESD adaptées, puis mesurer les seuils et les fronts. La liste disponible de résistances et NPN ne suffit pas à garantir une immunité CEM quelconque. Vérifier aussi la sortie du fan non alimenté : le rappel 3,3 V ne doit pas alimenter sa logique via un chemin parasite significatif.

## 3. Sorties vers l'onduleur, à réaliser deux fois

```text
                      DANS LE DEYE (valeur inconnue)
                      +3,3 V / +5 V / +12 V
                                |
                              Rpull
                                |
J9 TACH_DEYE9 -------------------+---------- C Q1 2N3904
                                            E -------- GND commun
                                            B
                                            |
Wemos D5 / GPIO14 ------- R5 2,2 kΩ ----------+
                                            |
                                         R6 47 kΩ
                                            |
GND commun ---------------------------------+

J6 TACH_DEYE6 ------------------------------ C Q2 2N3904
                                            E -------- GND commun
                                            B
                                            |
Wemos D6 / GPIO12 ------- R7 2,2 kΩ ----------+
                                            |
                                         R8 47 kΩ
                                            |
GND commun ---------------------------------+
```

Chaque collecteur va **uniquement** à l'entrée tach Deye correspondante. Ne pas ajouter de rappel vers 3,3 V, 5 V ou 12 V sur ces collecteurs : le rappel doit venir du Deye. Le code produit un HIGH GPIO pour saturer le NPN et tirer la ligne Deye à LOW ; LOW GPIO bloque le NPN et laisse le Deye choisir son niveau haut. L'inversion ne change pas la fréquence.

Le 2N3904 accepte une tension collecteur-émetteur de 40 V selon la fiche onsemi citée. Le même étage fonctionne donc sans modification matérielle pour un rappel à 3,3 V, 5 V ou 12 V, **dans les limites de courant ci-dessous**. Il protège le GPIO contre ce niveau en fonctionnement normal ; ce n'est pas une isolation galvanique ni une protection contre toute surtension transitoire.

Avec R5/R7 = 2,2 kΩ, le courant base disponible est voisin de 1 mA. En conception conservatrice, limiter le courant tiré à **10 mA maximum**, vérifier `VCE_sat` et la tension LOW mesurés. Mesurer Rpull ou le courant avec une résistance de test connue ; **ne pas court-circuiter une broche inconnue avec un ampèremètre**. Le fait que la tension soit 12 V ne révèle pas le courant disponible. Si l'entrée est une sortie push-pull ou une alimentation 12 V dure, le montage n'est pas applicable sans caractérisation et adaptation.

Les résistances de 47 kΩ garantissent un NPN bloqué pendant le reset et lorsque l'ESP est hors tension. Au boot et en l'absence de tach valide, la ligne est libérée, pas forcée à produire des RPM. Consulter le brochage E/B/C de **l'exemplaire exact** : les NPN de la liste ne sont pas tous interchangeables broche à broche. Un BC547 peut convenir électriquement dans cette plage mais nécessite de respecter son propre brochage et ses caractéristiques.

## 4. LED et bouton de récupération

La LED intégrée reliée à GPIO2/D4 avec sa résistance sur la carte est utilisée ; rien à ajouter. Elle est active à l'état bas, allumée pendant la synthèse. Ne pas ajouter une charge qui maintiendrait GPIO2 à LOW au boot.

```text
Wemos 3V3 -------- R9 10 kΩ -------- D0 / GPIO16
                                      |
                                  BP1 normalement ouvert
                                      |
GND ----------------------------------+
```

BP1 est facultatif ; R9 reste recommandé pour que l'entrée ne flotte pas. GPIO16 n'est pas utilisé en interruption. D3/GPIO0, D8/GPIO15 et RX/TX sont laissés libres pour ne pas compromettre l'amorçage et la programmation.

## 5. Nomenclature

| Repère | Quantité | Valeur / fonction |
|---|---:|---|
| Wemos | 1 | D1 mini V2.3.0, ESP8266 |
| A9 | 2 | Noctua NF-A9-FLX, sans adaptateur LNA/ULNA pour commencer |
| A6 | 2 | Noctua NF-A6x25-FLX, sans adaptateur LNA/ULNA pour commencer |
| Q1, Q2 | 2 | 2N3904, brochage vérifié |
| R1, R3 | 2 | 4,7 kΩ, 1/4 W |
| R2, R4 | 2 | 1 kΩ, 1/4 W |
| R5, R7 | 2 | 2,2 kΩ, 1/4 W |
| R6, R8 | 2 | 47 kΩ, 1/4 W |
| R9 | 1 | 10 kΩ, 1/4 W |
| D1, D2 | 2 | SS34 Schottky 3 A / 40 V, composants à acheter |
| U1 | 1 | Module S13V10F5 buck-boost 5 V, à acheter |
| F1, F2 | 2 | Protection branche électronique, 0,75 A initial à valider |
| C1 | 1 | 470 µF / 25 V, polarisé |
| C3 | 1 | 470 µF / 10 V, polarisé |
| C2, C4 | 2 | 100 nF céramique |
| SW1 | 1 | Interrupteur alimentation externe Wemos |
| BP1 | 1 | Bouton momentané normalement ouvert, facultatif |

Les résistances et les NPN seuls ne peuvent pas fournir une alimentation 12 → 5 V régulée efficace, ni faire un OR de puissance correctement protégé avec les garanties demandées. Les diodes, condensateurs, protections et régulateur sont donc des composants supplémentaires indispensables à ce schéma.

Le courant moteur maximal des deux A6 est environ 0,16 A selon la fiche actuelle (0,08 A par fan). La fiche A9 indique 0,07 A nominal par fan, avec sa tolérance. Ajouter la consommation électronique et les pointes de démarrage : chaque connecteur doit pouvoir fournir **seul** sa paire et toute l'électronique, puisque l'autre peut être éteint. Vérifier le courant admissible du Deye ; ne pas le déduire de ces seuls chiffres. Si le budget ne suffit pas, il faut une alimentation auxiliaire convenable, en contradiction avec l'alimentation uniquement sur les connecteurs.

## 6. Retours flottants ou coupure côté masse : adaptation nécessaire

Pour préserver deux domaines ventilateurs réellement indépendants, il faut :

1. Un convertisseur **galvaniquement isolé par connecteur**, primaire entre son +FAN et son propre retour ; plage primaire couvrant les tensions réellement mesurées. OR des deux sorties secondaires régulées pour le Wemos, sans réunir les retours primaires. Pas le S13V10F5, qui n'est pas isolé.
2. Une isolation de chaque tach d'entrée : rappel dans le domaine du ventilateur, transmetteur optique/logique puis sortie 3,3 V côté Wemos. Dimensionner les courants, le CTR minimal ou le seuil logique, les vitesses et les états hors tension.
3. Une sortie à transistor isolée par canal, collecteur sur tach Deye et émetteur sur le retour local de ce connecteur. Un optocoupleur approprié peut réaliser le collecteur ouvert ; spécifier sa tenue en tension et sa saturation au courant de rappel Deye mesuré.

Cette adaptation dépend de tensions et de références non fournies. Elle est décrite pour empêcher l'erreur consistant à réunir des retours commutés ; ce n'est pas un second schéma certifié ni une isolation de sécurité secteur. Le schéma principal ne doit pas être branché si sa condition de masse commune n'est pas vérifiée.
