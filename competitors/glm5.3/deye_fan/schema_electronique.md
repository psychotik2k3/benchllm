# Schéma électronique complet — « Deye Fan Tach » (Wemos D1 mini ↔ onduleur Deye SUN-8K-SG05LP1)

Ce document décrit **toute l'installation** : alimentation, circuits d'entrée (Noctua → ESP8266), circuits de
sortie (ESP8266 → Deye), câblage complet, nomenclature (BOM), contrôles avant mise sous tension et calibration.

## 0. Contraintes prises en compte

| Contrainte | Réponse matérielle |
|---|---|
| Tension de pull-up tach de l'onduleur **inconnue** (3,3 / 5 / 12 V) | Sorties en **collecteur ouvert** (NPN : collecteur sur la broche tach, émetteur à la masse) : le transistor ne voit que la tension de pull-up de l'onduleur et la ramène simplement à la masse. Tous les NPN recommandés tiennent ≥ 25 V (la plupart ≥ 40 V). Rien à configurer, rien à modifier côté onduleur. |
| Les 2 connecteurs ventilateurs ne sont **pas alimentés en même temps**, ni forcément à la même tension | Alimentation par **OU logique de diodes Schottky** (SS14) depuis chaque connecteur, suivie d'un buck 5 V large gamme. Quelle que soit la broche alimentée (ou les deux), le module est alimenté ; le basculement est transparent. |
| Alimentation du Wemos **depuis les connecteurs ventilateurs (+12 V)** | §2 : Schottky → découplage → buck 5 V → broche 5V du D1 mini (son régulateur interne fabrique le 3,3 V). |
| Signal de sortie **stable, sans gigue**, isolé du WiFi/web | Assuré par le firmware (timer matériel FRC1 + horodatage sur compteur de cycles CPU, voir README). Le matériel reste volontairement simple : un transistor par sortie suffit. |
| Tach Noctua à collecteur ouvert | Entrées tirées au **3,3 V uniquement** (10 kΩ) — jamais au 12 V. Les deux tach d'une même taille sont câblés en parallèle (wire-OR, parfaitement supporté par des sorties à collecteur ouvert). |

Hypothèses (à vérifier au multimètre, voir §6) : les connecteurs ventilateurs de l'onduleur sont des 3 broches
+12 V / GND / tach ; la broche tach de l'onduleur est tirée en interne vers ≤ 12 V par une résistance de
quelques kΩ.

## 1. Architecture générale

```
                +------------------------------------------------------------------------+
 Connecteur     |                          WEMOS D1 MINI (ESP8266)                       |
 Deye 92 mm     |                                                                        |
  +12V o----->|-|---+                                 GPIO14 (D5) <=== IN 1 <=== tach des 2 NF-A9-FLX
  GND  o-------|---|----------------+                  |            (circuit entrée 1, pull-up 3V3)
  tach o-----------|-----------------|--------------+   GPIO12 (D6) ===> OUT 1 ===> NPN --> broche tach C1
                |  5V               |   +--------+ |                  (circuit sortie 1)
                |  GND <------------+---| Buck 5V |-+    GPIO13 (D7) <=== IN 2 <=== tach des 2 NF-A6x25-FLX
                |                       | 5V/1A   |      |
 Connecteur     |                       +--------+       GPIO5  (D1) ===> OUT 2 ===> NPN --> broche tach C2
 Deye 60 mm     |                                        |
  +12V o----->|-|---+                                    GPIO2  (D4) ===> LED embarquée (état)
  GND  o-------|---|----------------+                     |
  tach o-----------------------------+   MASSE COMMUNE (onduleur = modules = ESP8266)
                +------------------------------------------------------------------------+
```

Points essentiels :

1. **Masse commune** : le GND de l'ESP8266 est relié aux GND des deux connecteurs ventilateurs de l'onduleur
   (c'est la même masse que l'électronique de l'onduleur). C'est ce qui permet la mesure du tach entrant et la
   commande du tach sortant.
2. **Aucune connexion directe** entre une broche ESP8266 et l'onduleur : tout passe par les circuits décrits
   en §3 (entrées) et §4 (sorties).
3. Les tach des **deux Noctua de même taille** sont câblés en parallèle sur un seul canal : le firmware compte
   les fronts combinés et divise par le nombre de ventilateurs déclaré dans l'interface web.

## 2. Alimentation (buck 5 V alimenté par les 2 connecteurs ventilateurs)

```
 +12V (C1 92 mm) o---[D1 SS14]---+
                                 +---+ o VRAW
 +12V (C2 60 mm) o---[D2 SS14]---+   |
                                     |   +---------------------------------------+
                        C3 470µF/25V |   |  Buck 5 V  (module type "Mini-560",   |
                        D5 SMBJ14A   |   |  MP1584EN, entrée 4,5..26 V)          |
                          à la masse |   |                                       |
                              GND ---+---| IN+                        OUT+ ------+----> +5V --> broche 5V D1 mini
                                         | IN-                        OUT- --+        C4 100µF/16V + C5 100nF
                                         +---------------------------------|--------------------- GND (commun)
                                                                           GND
```

Nomenclature :

| Repère | Composant | Valeur / type | Rôle |
|---|---|---|---|
| D1, D2 | SS14 (ou 1N5819) | Schottky 1 A / 40 V, Vf ≈ 0,35 V | OU logique : chaque connecteur alimente le module indépendamment ; un connecteur hors tension n'est pas rétro-alimenté |
| C3 | Condensateur électrolytique | 470 µF / 25 V, faible ESR | Réserve de courant (pointes WiFi ≈ 300 mA) et lissage des variations de la tension ventilateur |
| D5 | TVS SMBJ14A (optionnel mais conseillé) | standoff 14 V | Écrête les transitoires de la ligne 12 V de l'onduleur (relais, démarrages) |
| Buck | Module MP1584EN type « Mini-560 » (ou LM2596 mini) | Réglé à **5,0–5,2 V** | Abaisse VRAW vers 5 V ; large gamme d'entrée |
| C4, C5 | 100 µF/16 V + 100 nF | — | Découplage de la sortie buck, à proximité du D1 mini |

**Dimensionnement et comportement :**

- Consommation : D1 mini ≈ 80 mA au repos, pointes ≈ 250–300 mA en émission WiFi ; un buck 1 A est très large.
- Les NMB d'origine sont des ventilateurs 12 V. Si l'onduleur module leur vitesse en abaissant la tension
  (contrôle thermique), VRAW suit : le buck maintient 5 V tant que VRAW ≥ ≈ 5,5 V (ventilateurs ≥ ≈ 6 V).
- En dessous (ventilateurs presque arrêtés ou coupés par l'onduleur), le module peut redémarrer. Deux
  protections logicielles existent : le mode **cache-panne** (fréquence fixe configurable) et le fait qu'un
  ventilateur arrêté par l'onduleur n'est pas attendu comme « tournant » par celui-ci.
- La LED embarquée et l'interface web permettent de vérifier en permanence que le module est alimenté et actif.

## 3. Circuits d'entrée — tach Noctua → ESP8266 (identique pour les 2 canaux)

Chaque canal reçoit le tach (fil **vert** sur les Noctua) des **deux** ventilateurs de la taille concernée,
câblés **en parallèle** (les deux sorties à collecteur ouvert se combinent en wire-OR : dès qu'un ventilateur
tire la ligne à la masse, elle est à la masse — c'est le fonctionnement normal et supporté de ce type de sortie).

```
 Canal 1 (92 mm)                              Wemos D1 mini

 tach NF-A9 n°1 o------+---[R3 1 k]---+---------+-------> GPIO14 (D5)
 tach NF-A9 n°2 o------+              |         |
                                     [R1 10k] [C2 1nF]
                                        |         |
                                       3V3       GND

 Canal 2 (60 mm) : tach NF-A6x25 n°1/n°2 --> R4 1 k --> GPIO13 (D7), R2 10 k vers 3V3, C6 1 nF
```

| Repère | Valeur | Rôle |
|---|---|---|
| R1, R2 | 10 kΩ vers 3V3 | Pull-up des lignes tach (collecteur ouvert). **Toujours au 3,3 V**, jamais au 12 V : la broche ESP8266 n'est pas tolérante 5 V/12 V, et Noctua spécifie le tach pour un pull-up ≤ 5 V. |
| R3, R4 | 1 kΩ en série | Protection (limitation de courant dans les diodes de clamp internes de l'ESP8266 en cas de transitoire), sans gêner les fronts (impulsions tach ≈ 1–2 ms). |
| C2, C6 | 1 nF (optionnel) | Filtrage HF des parasites ; RC ≈ 10 µs, sans effet sur des impulsions de l'ordre de la milliseconde. |

**Alimentation des ventilateurs** : les 4 Noctua restent alimentées **directement** par les connecteurs
ventilateurs de l'onduleur (chaque paire sur son connecteur d'origine, via un petit domino ou une adaptation
de connecteur). Courant : NF-A9-FLX ≈ 0,10 A et NF-A6x25-FLX ≈ 0,12 A, soit ≈ 0,25 A par connecteur — très
en dessous de ce que fournissait le NMB d'origine.

**Repérage des fils** (à confirmer au multimètre) :
- Noctua : noir = GND, jaune = +12 V, vert = tach (signal) ;
- NMB d'origine (connecteur de l'onduleur) : noir = GND, rouge = +12 V, tach selon la couleur du câble (souvent
  jaune/blanc).

Règle d'or : **ne jamais** connecter le fil tach d'une Noctua à la broche tach de l'onduleur, et **ne jamais**
tirer un tach Noctua vers le 12 V.

## 4. Circuits de sortie — ESP8266 → broche tach de l'onduleur (identique pour les 2 canaux)

La broche tach du connecteur ventilateur de l'onduleur est tirée vers le haut par **une résistance interne dont
on ne connaît ni la valeur ni la tension** (3,3 / 5 ou 12 V). On la « pilotera » donc en **collecteur ouvert** :
un transistor NPN qui relie la broche tach à la masse. La tension de pull-up de l'onduleur n'a alors **aucune
importance** (tous les transistors ci-dessous tiennent ≥ 25 V), et au repos (transistor bloqué) la ligne
reprend exactement l'état « haute » qu'aurait un vrai tach.

```
 Canal 1 (92 mm) :                                  Canal 2 (60 mm) :

                 R7 3,3 kΩ        T1 = BC337                      R8 3,3 kΩ        T2 = BC337
 GPIO12 (D6) o--/\/\/\--+------------ B            GPIO5 (D1) o--/\/\/\--+------------ B
                        |                             C o---------------------------> broche TACH du C2
                        |        C o-------------------------------> broche TACH du C1
                     R9 10 kΩ        |                                             |
                        |            E                                         R10 10 kΩ     E
                       GND ----------+------------> GND (commun)               |            |
                                                                              GND ---------+--> GND (commun)
```

| Repère | Valeur | Rôle |
|---|---|---|
| R7, R8 | 3,3 kΩ | Résistance de base : Ib = (3,3 V − 0,7 V) / 3,3 kΩ ≈ **0,8 mA** |
| R9, R10 | 10 kΩ | Pull-down base-émetteur : maintient le transistor **bloqué** pendant le boot de l'ESP8266 (broches flottantes) et en cas de fil débranché → ligne tach au repos haute, état bénin |

**Dimensionnement :**

- Courant absorbé dans le tach de l'onduleur lors d'un front : I = V_pullup / R_pullup. Pire cas plausible :
  12 V / 1 kΩ = 12 mA ; cas courant : 5 V / 10 kΩ = 0,5 mA. Avec Ib ≈ 0,8 mA, le transistor est largement
  saturé (β forcé ≈ 15 à 12 mA, VCEsat ≈ 0,2 V) → front propre, niveau bas fiable.
- Transistors compatibles (tous ≥ 25 V VCEO, la plupart ≥ 40 V) : **BC337-25 (recommandé)**, 2N2222A, 2N3904,
  BC547, BC548, S8050, SS8050, S9013, S9014, S9018, C1815, C945, 2N5551. Brochage à vérifier selon le modèle
  (BC337 : face plate vers vous, E-C-B de gauche à droite ; 2N2222/2N3904 en boîtier TO-92 : E-B-C).

**Comportements importants :**

- **Au repos / boot** : GPIO bas ou flottant → transistor bloqué → ligne tach **haute** = même état qu'un
  ventilateur à l'arrêt : aucun « faux signal » n'est envoyé à l'onduleur.
- **En simulation** : carré 50 % — l'onduleur compte les fronts descendants (fréquence = tr/min / 30), le
  rapport cyclique n'a pas d'importance.
- **Jamais de connexion directe** entre une broche ESP8266 et l'onduleur : toujours ce transistor.

## 5. Câblage complet (table de raccordement)

**Côté Wemos D1 mini :**

| Broche Wemos | Fonction | Vers / composants |
|---|---|---|
| **5V** | Alimentation | Buck OUT+ (réglé 5,0–5,2 V) + C4 (100 µF) / C5 (100 nF) au plus près du module |
| **GND** | Masse | Buck OUT− ; GND des connecteurs C1 et C2 de l'onduleur ; R1/R2 (pull-up 3V3 côté 3V3 mais retour masse des C2/C6) ; émetteurs T1/T2 ; R9/R10 |
| **D5 (GPIO14)** | Entrée tach 92 mm | R3 (1 kΩ) ← nœud {tach NF-A9 n°1, tach NF-A9 n°2, R1 10 kΩ → 3V3, C2 1 nF → GND} |
| **D6 (GPIO12)** | Sortie tach 92 mm | R7 (3,3 kΩ) → base T1 ; collecteur T1 → broche tach du connecteur C1 ; émetteur T1 → GND ; R9 (10 kΩ) entre base et émetteur |
| **D7 (GPIO13)** | Entrée tach 60 mm | R4 (1 kΩ) ← nœud {tach NF-A6x25 n°1, tach NF-A6x25 n°2, R2 10 kΩ → 3V3, C6 1 nF → GND} |
| **D1 (GPIO5)** | Sortie tach 60 mm | R8 (3,3 kΩ) → base T2 ; collecteur T2 → broche tach du connecteur C2 ; émetteur T2 → GND ; R10 (10 kΩ) entre base et émetteur |
| **D4 (GPIO2)** | LED d'état | LED embarquée du module — aucun câblage externe |
| 3V3 | Référence des pull-up R1/R2 | Uniquement les 10 kΩ des entrées tach |
| D2 (GPIO4), D3 (GPIO0), D0 (GPIO16), A0, RX, TX | Libres | RX/TX = console de debug 115200 bauds. NB : D0/GPIO16 ne gère pas les interruptions → NE PAS l'utiliser pour un tach |

**Côté ventilateurs Noctua :**

| Ventilateur | +12 V (fil jaune) | GND (fil noir) | Tach (fil vert) |
|---|---|---|---|
| NF-A9-FLX n°1 (92 mm) | +12 V du connecteur C1 (domino) | GND du C1 | nœud d'entrée 1 (D5) |
| NF-A9-FLX n°2 (92 mm) | +12 V du connecteur C1 (domino) | GND du C1 | nœud d'entrée 1 (D5) |
| NF-A6x25-FLX n°1 (60 mm) | +12 V du connecteur C2 (domino) | GND du C2 | nœud d'entrée 2 (D7) |
| NF-A6x25-FLX n°2 (60 mm) | +12 V du connecteur C2 (domino) | GND du C2 | nœud d'entrée 2 (D7) |

**Côté onduleur (connecteurs C1 = 92 mm, C2 = 60 mm — ne rien modifier sur l'onduleur) :**

| Broche du connecteur | Raccordement |
|---|---|
| +12 V | → anode de D1 (SS14, canal 1) / anode de D2 (SS14, canal 2) → VRAW → buck |
| GND | → masse commune du montage |
| Tach | → collecteur de T1 (C1) / collecteur de T2 (C2). **Rien d'autre.** |

**Fil conducteur** : fils de raccordement courts (≤ 20 cm), paire tach+GND torsadée de préférence ; le montage
peut être réalisé sur plaque d'essai soudée ou protoboard, dans un petit boîtier placé près des ventilateurs.

## 6. Contrôles AVANT mise sous tension (multimètre)

1. **Continuité des masses** : GND des connecteurs C1 et C2 de l'onduleur, OUT− du buck, GND du D1 mini,
   émetteurs T1/T2 = un seul et même nœud.
2. **Pas de court-circuit** entre +12 V et GND (connecteurs et buck).
3. **Polarité des diodes** D1/D2 : anode côté connecteur onduleur (+12 V), cathode côté VRAW.
4. **Régler le buck SEUL** : l'alimenter (par ex. depuis un chargeur 12 V) et régler OUT+ à **5,0–5,2 V**
   AVANT de le brancher sur la broche 5V du D1 mini.
5. **Module débranché, ventilateurs branchés** : mesurer la tension entre la broche tach de C1/C2 et GND →
   c'est la tension de pull-up de l'onduleur (3,3 / 5 ou 12 V). Noter la valeur : le circuit de sortie la
   supporte de toute façon (transistors ≥ 25 V), mais c'est une donnée utile.
6. **Module alimenté** (ventilateurs en marche ou simple chargeur USB sur le D1 mini) : vérifier 5 V sur la
   broche 5V, 3,3 V sur la broche 3V3, et la LED qui clignote (état « attente de signal »).
7. **Test banc recommandé** (sans l'onduleur) : activer temporairement le mode cache-panne d'un canal
   (interface web) et mesurer au fréquencemètre la fréquence sur le collecteur de T1 : elle doit valoir
   blindRpm / 30 (ex : 4000 tr/min → 133,3 Hz). Désactiver ensuite le mode cache-panne.
8. Raccorder les collecteurs de T1/T2 aux broches tach de C1/C2, remettre sous tension : dès que les ventilateurs
   tournent, la LED passe en fixe et l'interface web affiche les RPM.

## 7. Nomenclature (BOM)

| Qté | Composant | Valeur / type | Repères |
|---|---|---|---|
| 1 | Wemos D1 mini | V2.3.0 (ESP8266 ESP-12S) | — |
| 1 | Module buck 5 V | MP1584EN type « Mini-560 » (ou équivalent LM2596 mini) | — |
| 2 | Diode Schottky | SS14 (1N5819 acceptable) | D1, D2 |
| 1 | TVS | SMBJ14A (optionnel) | D5 |
| 1 | Condensateur électrolytique | 470 µF / 25 V, faible ESR | C3 |
| 1 | Condensateur électrolytique | 100 µF / 16 V | C4 |
| 2 | Condensateur céramique | 100 nF | C5 (+ découplage) |
| 2 | Condensateur céramique | 1 nF (optionnel) | C2, C6 |
| 4 | Résistance | 10 kΩ (1/4 W) | R1, R2, R9, R10 |
| 2 | Résistance | 3,3 kΩ (1/4 W) | R7, R8 |
| 2 | Résistance | 1 kΩ (1/4 W) | R3, R4 |
| 2 | Transistor NPN | BC337-25 (ou 2N2222A, 2N3904, BC547/548, S8050, S9013/14, C1815, C945, 2N5551) | T1, T2 |
| 1 | Domino / bornier | 8 sections | raccordements +12 V / GND |
| — | Connectique | embases 2,54 mm ou dupont, gaine thermorétractable, plaque à pastilles, boîtier | — |

## 8. Montage et intégration

- Réaliser le montage sur plaque à pastilles, dans un petit boîtier placé **près des ventilateurs** (fils courts).
- Laisser le port USB du D1 mini accessible (reprogrammation sans démonter) ou prévoir un connecteur 2×2.
- Éloigner le buck et le D1 mini du flux d'air chaud direct et des vibrations (pads adhésifs / vis).
- Paire tach + GND torsadée pour chaque entrée ; ne pas faire courir les fils tach le long des fils 230 V.
- Étiqueter les deux canaux (92 mm / 60 mm) : les ratios sont distincts.

## 9. Calibration des ratios (récapitulatif)

```
ratio = RPM attendus par l'onduleur (tach du NMB d'origine) / RPM réels du Noctua
```

1. Noter les RPM réels affichés par l'interface web à pleine tension (12 V) : ≈ 1600 tr/min (NF-A9-FLX) et
   ≈ 3100 tr/min (NF-A6x25-FLX).
2. Renseigner les ratios par défaut (2,5 et 2,3) puis observer le comportement de l'onduleur.
3. Si une alarme ventilateur apparaît : augmenter le ratio par pas de 0,25. Si l'onduleur semble tolérant,
   on peut réduire le ratio (moins de "RPM" fictifs, comportement plus fidèle au refroidissement réel).
4. Vérifier le champ « Nº de tach en parallèle » : il doit valoir le nombre de ventilateurs dont le tach est
   câblé sur le canal (2 par défaut ici). Il ne change PAS la fréquence envoyée à l'onduleur, il corrige
   seulement l'affichage des RPM mesurés (les tach en parallèle additionnent leurs fronts).
5. Le mode « cache-panne » : à réserver aux cas où l'onduleur lève une alarme à chaque arrêt/variation de
   ventilation. Il masque aussi les pannes réelles de ventilateur — à activer en connaissance de cause.

## 10. Hypothèses et notes

- **Vitesses NMB** : les RPM exacts des NMB 06025VE-12N-CL et 09225VE-12N-CU ne sont pas publiés de façon
  fiable ; les valeurs ~7000 et ~4000 tr/min utilisées pour les ratios par défaut sont des hypothèses de
  classe usuelle pour ces formats 6025/9225 en 12 V. La calibration web rend la donnée non critique.
- **Tach** : les ventilateurs 3 fils (NMB comme Noctua) délivrent 2 impulsions par tour (Hz = tr/min / 30).
- **Pilotage des ventilateurs par l'onduleur** : inconnu (tout-ou-rien 12 V ou tension variable). La solution
  gère les deux cas : alimentation par OU de diodes + buck large gamme, et simulation proportionnelle à la
  vitesse réelle mesurée.
- **Pull-up tach de l'onduleur** : inconnu (3,3 / 5 / 12 V) → sortie à collecteur ouvert, aucune importance.
- Le comportement exact de la surveillance tach du Deye (seuil, délai d'alarme) n'est pas documenté
  publiquement : la calibration in-situ (§9) est la méthode fiable.

---
*Document généré pour le projet « Deye Fan Tach » (deye_fan.ino v1.0). Licence MIT, fourni sans garantie.*







