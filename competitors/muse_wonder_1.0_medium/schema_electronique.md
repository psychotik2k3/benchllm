# Schéma électronique complet — Simulateur tach Deye / Noctua

**Projet :** remplacement des ventilateurs NMB d'un onduleur Deye SUN-8K-SG05LP1-EU-AM2-P
par des Noctua, avec multiplication électronique du signal tachymétrique (RPM).
**Microcontrôleur :** Wemos D1 Mini V2.3.0 (ESP-12S / ESP8266).
**Code associé :** `deye_fan.ino` (même dossier).

> ⚠️ **Sécurité** — N'intervenir que sur les connecteurs **basse tension 12 V** des
> ventilateurs, onduleur **hors tension, débranché du secteur et des panneaux**,
> condensateurs laissés à décharger selon la notice Deye. Ne jamais ouvrir le
> compartiment puissance (230 V / DC haute tension). Cette modification peut
> affecter la garantie constructeur et le refroidissement : après mise en service,
> surveiller les températures internes de l'onduleur en charge et par forte chaleur.

---

## 1. Vue d'ensemble

```
                        ┌───────────────────────── ONDEULEUR DEYE ─────────────────────────┐
                        │ Connecteur vent. 92 mm        Connecteur vent. 60 mm             │
                        │  GND   +12V   TACH_D9           GND   +12V   TACH_D6             │
                        └──┬──────┬──────┬────────────────┬──────┬──────┬──────────────────┘
                           │      │      │                │      │      │
              ┌────────────┘      │      │                │      │      └────────────┐
              │      ┌────────────┘      │                │      └───────┐          │
              │      │                   │                │              │          │
        Alim. ventilateurs (directe,     │          Alim. ventilateurs   │     Tach simulés
        le Wemos n'est PAS en série      │          (directe)            │     (collecteur ouvert)
        avec la puissance des ventilos)  │                │              │          │
              │      │                   │                │              │          │
     ┌────────▼──────▼───┐      ┌────────▼──────┐  ┌──────▼──────▼───┐    │    ┌─────▼─────┐
     │ 2× NF-A9 FLX      │      │  (vers §2 :   │  │ 2× NF-A6x25 FLX │    │    │ Transistors│
     │ (92 mm, en        │      │  OR-ing +     │  │ (60 mm, en      │    │    │ de sortie  │
     │  parallèle)       │      │  buck 5 V)    │  │  parallèle)     │    │    │ Q3 / Q4    │
     └────────┬──────────┘      └──────┬────────┘  └───────┬─────────┘    │    └─────▲─────┘
              │ tach maître            │ +5V               │ tach maître  │          │
              │ (1 seul fil jaune,     │                   │ (1 seul fil  │     D5 / D6 (GPIO)
              │  voir §3)              │                   │  jaune)     │          │
              ▼                        ▼                   ▼             │    ┌─────┴──────────┐
        ┌───────────┐          ┌──────────────┐     ┌───────────┐       └──┤  WEMOS D1 MINI  │
        │ Entrée §3 │─────────▶│ Alimentation │     │ Entrée §3 │──────────│  D1 / D2 (GPIO) │
        │ canal 9cm │  D1      │  (§2) → 5 V   │     │ canal 6cm │   D2     │                 │
        └───────────┘          └──────────────┘     └───────────┘          │  LED D4 = état  │
                                                                           └─────────────────┘
        GND : masse COMMUNE et unique (étoile) entre onduleur, ventilateurs et Wemos (§6).
```

**Principe :** le Wemos n'a que trois rôles : (1) écouter le tach de **un** Noctua
par canal, (2) ré-émettre vers l'onduleur un tach de fréquence multipliée par un
ratio réglable (interface web), (3) s'alimenter proprement depuis le +12 V des
connecteurs ventilateurs. **Il ne commute jamais la puissance des ventilateurs.**

---

## 2. Alimentation du Wemos depuis les deux connecteurs 12 V

### Problème à résoudre
- Le Wemos exige du 5 V (broche `5V`, régulateur 3,3 V embarqué) ; les connecteurs
  ventilateurs fournissent ~12 V (7…13,8 V possibles selon la régulation Deye).
- Les deux connecteurs **ne sont pas alimentés en même temps ni forcément à la
  même tension** : il faut alimenter le Wemos dès qu'**au moins un** connecteur
  est sous tension, **sans jamais renvoyer de courant** d'un connecteur vers
  l'autre (back-feeding), et sans mettre deux sources 12 V en parallèle direct.

### Solution : OR-ing à diodes Schottky + fusible + buck 12 V → 5 V

```
 Conn. 92 mm                    Conn. 60 mm
 +12V_D9                        +12V_D6
   │                              │
   ├──[ F1 : polyfuse 0,5 A ]     ├──[ F2 : polyfuse 0,5 A ]
   │                              │
   ├──▶| D1 : SS14 ──┐   ┌── |◀──├── D2 : SS14  (Schottky 1 A / 40 V)
   │    (anode→cath.)│   │        │    sens passant vers le rail commun
   │                 └───┴──┬─────┘
   │                        │  RAIL +VIN (7…13,8 V selon source active)
   │              ┌─────────┼──────────────────┐
   │              │         │                  │
   │           C1 470 µF  TVS1 SMBJ15A     (optionnel mais recommandé :
   │           /25 V      (cath. au +VIN,    écrête les transitoires
   │           + 100 nF    anode au GND)     du bus ventilateur)
   │              │         │                  │
   │              └─────────┴────────┬─────────┘
   │                                 ▼
   │                    ┌──────────────────────────┐
   │                    │ BUCK DC-DC réglable ou   │
   │                    │ fixe 12 V → 5,0 V        │
   │                    │ ≥ 1 A (ex. module MP1584 │
   │                    │ ou LM2596 mini), réglé   │
   │                    │ à 5,0 V AVANT branchement│
   │                    └───────┬──────────┬───────┘
   │                         +5V          GND
   │                          │            │
   │                          ▼            │
   │                   Wemos : broche `5V`  broche `G`
   │
 GND_D9 ──┬───────────────┬────────────────┴──── GND commun (§6)
 GND_D6 ──┘               └──── masses ventilateurs
```

### Pourquoi ces choix
| Élément | Rôle / justification |
|---|---|
| **D1, D2 = SS14** | OR-ing : le rail +VIN suit `max(V_D9, V_D6) − 0,4 V`. Si un seul connecteur est alimenté, sa diode conduit et l'autre, polarisée en inverse, **empêche tout retour de courant** vers le connecteur éteint (pas de back-feeding vers l'électronique Deye ni vers les ventilateurs de l'autre canal). Schottky = faible chute (0,4 V) et 40 V de tenue. |
| **F1, F2 = polyfuse 0,5 A** | Un court-circuit côté Wemos/buck ne doit pas écrouler le 12 V d'un canal ventilateur ni solliciter la protection interne du Deye. |
| **Buck 5 V ≥ 1 A** | Le Wemos consomme ~80 mA en moyenne à 3,3 V mais **plusieurs centaines de mA en pics** lors des émissions WiFi. Un buck (rendement ~85 %) ne dissipe que ~0,3 W. **Ne pas utiliser un 7805** : à 12 V → 5 V / 350 mA de pic, il dissiperait ~2,5 W, exigerait un radiateur sérieux et risquerait la coupure thermique dans un boîtier fermé et chaud. |
| **C1 = 470 µF / 25 V + 100 nF** | Réserve locale : absorbe les appels de courant WiFi et le démarrage, évite que le buck « voie » les micro-coupures du bus 12 V quand le Deye hache/modifie l'alimentation de ses ventilateurs. |
| **TVS1 (optionnel)** | Protection contre les surtensions transitoires du bus ventilateur (démarrages, selfs). |

> **Composants à ajouter à la liste fournie (assumés et nécessaires)** : les diodes
> SS14, les polyfuses, les condensateurs et le module buck ne figurent pas dans la
> liste « résistances + transistors NPN ». Il n'existe **aucune** solution fiable
> pour faire du 5 V / 350 mA pics à partir de 7…14 V avec seulement des résistances
> et des NPN (un suiveur émetteur « régulé » par diviseur n'a ni référence stable,
> ni tenue aux pics WiFi, et dissiperait plusieurs watts). Ces quelques composants
> passifs/alimentation sont donc indispensables ; tout le reste du schéma n'utilise
> que des résistances et un NPN de la liste autorisée (BC337).

**Cas limites vérifiés par ce montage :**
- Un seul connecteur alimenté (l'autre à 0 V) → fonctionnement normal, aucun retour.
- Deux connecteurs à des tensions différentes → le plus haut alimente, l'autre est bloqué par sa diode ; bascule automatique si les tensions s'inversent.
- 12 V absent partout (onduleur éteint) → Wemos éteint, ventilateurs Noctua également à l'arrêt puisque leur +12 V vient des mêmes connecteurs : cohérent.
- Tension ventilateur réduite (~7 V, si le Deye régule en tension) → le buck MP1584 fonctionne encore (entrée min. ~4,5 V) ; le Wemos reste alimenté.

---

## 3. Circuits d'entrée — Noctua → ESP8266 (× 2 canaux identiques)

### Propriétés du signal Noctua (données constructeur)
- Sortie tach = **collecteur ouvert**, **2 impulsions par tour**, courant max. **5 mA**.
- Le fil tach (jaune) ne fournit **aucune tension** : c'est le récepteur qui doit
  fournir le pull-up. On choisit donc **notre** tension : 3,3 V, directement
  compatible ESP8266 — le 12 V du ventilateur n'atteint jamais le GPIO.

```
 Noctua maître (1 par canal)              Côté Wemos
 ┌──────────────────────────┐
 │ Noir  GND ───────────────┼──────────────────────────── GND commun
 │ Rouge +12V ──────────────┼──── +12V du connecteur Deye correspondant (§1)
 │ Jaune TACH ───┬──────────┼────┬────[ R2 : 1 kΩ ]──── D1 (GPIO5) canal 9 cm
 │  (collecteur  │          │    │                       D2 (GPIO4) canal 6 cm
 │   ouvert)     │          │    │
 └───────────────┘          │   [ R1 : 10 kΩ ]            [ C2 : 1 nF ]
                            │    pull-up vers +3,3 V       vers GND
                            └────┬───────────────┬─────────┘
                              +3,3 V (broche 3V3 du Wemos)
```

| Élément | Valeur | Justification |
|---|---|---|
| **R1 pull-up** | 10 kΩ → 3,3 V | Courant collecteur Noctua = 3,3 V / 10 kΩ = **0,33 mA** (≪ 5 mA max). Fronts suffisamment raides à ces fréquences (tach ≤ ~100 Hz réels). Le pull-up interne de l'ESP8266 est aussi activé dans le code, en redondance. |
| **R2 série** | 1 kΩ | Limite le courant en cas de fausse manœuvre / transitoire sur le fil tach et découple la capacité du câble. |
| **C2** | 1 nF → GND | Filtre HF d'antiparasitage (câbles longs dans un environnement bruyant d'onduleur). Avec R1+R2, τ ≈ 11 µs : négligeable devant la période tach (≥ 10 ms), aucun effet sur la mesure. Ne pas augmenter au-delà de quelques nF. |

### ⚠️ Doublement des ventilateurs : un seul tach par canal
Chaque canal Deye reçoit **deux** Noctua en parallèle sur GND/+12 V, mais
**les deux fils jaunes ne doivent JAMAIS être reliés ensemble ni au même GPIO** :
deux collecteurs ouverts en parallèle mélangeraient leurs impulsions (fréquence
fausse et illisible). **Un seul** ventilateur par canal — le « maître » — voit son
fil jaune raccordé au circuit ci-dessus ; le fil jaune du second ventilateur est
**isolé** (gaine thermorétractable) et laissé en l'air. Hypothèse de travail
(explicite) : les deux ventilateurs identiques d'un même canal, alimentés par la
même tension, tournent à la même vitesse à quelques % près ; le signal du maître
les représente donc tous les deux vis-à-vis de l'onduleur.

---

## 4. Circuits de sortie — ESP8266 → Deye (× 2 canaux identiques)

### Problème à résoudre
La tension que l'onduleur applique sur sa broche tach (son pull-up interne) est
**inconnue : 3,3 V, 5 V ou 12 V**. La seule interface qui fonctionne dans les trois
cas **sans modification matérielle** est une sortie **collecteur ouvert** à
transistor NPN dimensionné pour 12 V : le transistor ne fait que tirer la ligne
vers GND ou la relâcher ; c'est le pull-up **du Deye** qui fixe le niveau haut,
quel qu'il soit. C'est d'ailleurs l'interface que les ventilateurs d'origine
présentent eux-mêmes à l'onduleur (collecteurs ouverts NMB : VCE max 15 V).

```
 Côté Wemos                                   Côté onduleur Deye
 D5 (GPIO14, canal 9 cm) ──[ R3 : 4,7 kΩ ]──┬── Base   Q3 : BC337
 D6 (GPIO12, canal 6 cm) ──[ R3 : 4,7 kΩ ]──┬── Base   Q4 : BC337
                                             │
                                  [ R4 : 10 kΩ ]  (base → GND : transistor
                                             │     bloqué si GPIO en l'air,
                                             ▼     reset ou boot du Wemos)
                                        Émetteur ─────────────── GND commun
                                        Collecteur ───────────── broche TACH
                                                                   du connecteur
                                                                   Deye (9 cm ou
                                                                   6 cm selon canal)
                                        (AUCUN pull-up côté Wemos : le niveau
                                         haut est imposé par le Deye : 3,3/5/12 V)
 Logique : GPIO HAUT → Q saturé → collecteur ≈ 0,2 V → ligne tach au niveau BAS.
           GPIO BAS  → Q bloqué → ligne relâchée → niveau HAUT par le pull-up Deye.
           Le code génère un rapport cyclique de 50 % (standard tach PC).
```

### Choix du transistor parmi la liste autorisée : **BC337**
| Critère | Exigence | BC337 | Autres candidats de la liste |
|---|---|---|---|
| VCEO | > 12,6 V (cas pull-up 12 V) avec marge | **45 V** ✅ | 2N2222 : 40 V ✅ ; BC547 : 45 V ✅ mais… |
| IC | Courant du pull-up Deye inconnu ; hypothèse pire cas : pull-up 470 Ω @ 12 V ≈ 26 mA, voire court-circuit franc de la ligne | **800 mA** ✅, VCE(sat) faible | BC547/BC548 : 100 mA seulement — trop juste ; 2N3904 : 200 mA, acceptable mais inférieur |
| IB nécessaire | GPIO ESP8266 : limiter à quelques mA | (3,3 − 0,7) V / 4,7 kΩ ≈ **0,55 mA**, β forcé ≈ 10…50 → saturation franche dès ~5 mA de collecteur ✅ | — |

Le BC337 est donc le meilleur choix de la liste ; le **2N2222** est un repli
acceptable (40 V / 800 mA). R3 = 4,7 kΩ protège le GPIO, R4 = 10 kΩ garantit
l'état relâché (sûr) pendant le démarrage : tant que le firmware n'a pas démarré,
l'onduleur voit une ligne tach « au repos », exactement comme un ventilateur à
l'arrêt dont le collecteur serait ouvert.

**Robustesse au cas 12 V :** collecteur bloqué, Q3/Q4 supportent 12,6 V en permanence
(45 V max). Collecteur passant, ils ne dissipent que VCE(sat) × IC ≈ 0,2 V × quelques
mA : négligeable. Aucune tension externe ne peut remonter vers le GPIO : le seul
chemin serait collecteur→base, bloqué en inverse. Le circuit est donc inchangé et
sûr que le Deye tire sa ligne à 3,3 V, 5 V ou 12 V — **aucun strap, aucun réglage**.

---

## 5. Affectation des broches et LED d'état

| Signal | Broche Wemos | GPIO | Direction | Remarque |
|---|---|---|---|---|
| Tach IN canal 9 cm | D1 | GPIO5 | Entrée (pull-up int. + R1 ext.) | Interruption sur front descendant |
| Tach IN canal 6 cm | D2 | GPIO4 | Entrée (pull-up int. + R1 ext.) | Interruption sur front descendant |
| Tach OUT canal 9 cm | D5 | GPIO14 | Sortie → base Q3 | Pas une broche de strap : sûr au boot |
| Tach OUT canal 6 cm | D6 | GPIO12 | Sortie → base Q4 | Pas une broche de strap : sûr au boot |
| LED d'état | D4 | GPIO2 | LED bleue embarquée, active LOW | Fixe = simulation sur 2 canaux ; clignotant lent = attente de signal ; double flash = 1 seul canal actif |
| Alimentation | `5V` / `G` | — | Depuis le buck (§2) | Ne **jamais** brancher l'USB et le buck 5 V en même temps sans retirer l'un des deux (ou insérer une diode Schottky série dans le +5 V du buck) |

---

## 6. Masse commune, câblage, implantation

- **Masse unique en étoile :** GND des deux connecteurs Deye, GND des 4 Noctua,
  GND du buck et GND du Wemos sont reliés ensemble. C'est indispensable : les
  signaux tach (entrée comme sortie) sont référencés à cette masse. Le montage
  est donc **non isolé galvaniquement** côté basse tension — c'est acceptable et
  inévitable ici, les ventilateurs et leurs tach partageant déjà la masse de
  l'onduleur (une isolation optique exigerait des optocoupleurs, hors liste).
- **Puissance ventilateurs en direct :** chaque paire de Noctua est alimentée par
  un « Y » depuis son connecteur Deye (GND/+12 V en parallèle, tach maître seul
  raccordé, tach esclave isolé). Courants : 2 × 0,07 A (NF-A9) et 2 × 0,08 A
  (NF-A6x25) — très inférieurs aux originaux (NMB 92 mm ≈ 0,9 A ; 60 mm ≈ 0,2 A),
  donc largement dans l'enveloppe des connecteurs Deye. **Le Wemos n'est jamais
  en série avec cette puissance.**
- **Repérage connecteurs 3 broches** (standard PC, **vérifier sur vos pièces**,
  ne pas se fier qu'aux couleurs côté Deye) : broche 1 = GND (noir), broche 2 =
  +12 V (rouge), broche 3 = tach (jaune). Mesurer au multimètre avant raccordement.
- **Câblage :** paires torsadées tach+GND si le câble dépasse ~30 cm, éloigner les
  fils tach des câbles de puissance de l'onduleur, connecteurs sertis ou soudés +
  gaine thermo (aucune épissure volante dans l'onduleur).
- **Implantation :** module à monter sur plaquette pastillée ou PCB, transistors
  et R1…R4 au plus près du Wemos, C1 près de l'entrée du buck, le tout dans un
  petit boîtier isolant ventilé, fixé hors des zones chaudes et des ailettes.
  Aucun composant ne chauffe significativement (buck ≈ 0,3 W, transistors ≈ mW).

---

## 7. Nomenclature (BOM)

| Réf. | Composant | Qté | Provenance |
|---|---|---|---|
| U1 | Wemos D1 Mini V2.3.0 (ESP-12S) | 1 | fourni |
| Q3, Q4 | **BC337** (NPN 45 V / 800 mA) — repli : 2N2222 | 2 | liste autorisée |
| R1 | 10 kΩ (pull-up tach entrée → 3,3 V) | 2 | liste autorisée |
| R2 | 1 kΩ (série entrée tach) | 2 | liste autorisée |
| R3 | 4,7 kΩ (base transistor de sortie) | 2 | liste autorisée |
| R4 | 10 kΩ (base → GND, état sûr au boot) | 2 | liste autorisée |
| C2 | 1 nF céramique (antiparasite entrée) | 2 | passif d'appoint |
| D1, D2 | SS14 (Schottky 1 A / 40 V) — OR-ing alimentation | 2 | **à ajouter** (§2) |
| F1, F2 | Polyfuse réarmable 0,5 A | 2 | **à ajouter** |
| C1 | 470 µF / 25 V électrolytique + 100 nF céramique | 1+1 | **à ajouter** |
| TVS1 | SMBJ15A (optionnel, protection transitoires) | 1 | **à ajouter** |
| U2 | Module buck DC-DC 12 V → 5,0 V ≥ 1 A (MP1584 / LM2596 mini), réglé à 5,0 V | 1 | **à ajouter** |
| — | Câbles en Y 3 broches, gaine thermo, boîtier, plaquette | — | mécanique |

---

## 8. Mise en service et vérification (avant remontage dans l'onduleur)

1. **Buck seul :** alimenter U2 en 12 V (alimentation de laboratoire), régler la
   sortie à **5,0 V à vide**, vérifier, puis seulement brancher le Wemos.
2. **OR-ing :** alimenter tour à tour chaque entrée 12 V, puis les deux à des
   tensions différentes (ex. 12 V et 9 V) : le 5 V doit rester stable et aucun
   courant ne doit refluer vers l'entrée la plus basse (multimètre en série).
3. **Entrées :** brancher un Noctua alimenté en 12 V labo, fil jaune sur le circuit
   §3 ; ouvrir `http://192.168.4.1/` : le RPM réel doit s'afficher (≈ 1 600 ou
   ≈ 3 000 tr/min selon le modèle, ±10 %).
4. **Sorties — test des 3 tensions :** relier le collecteur de Q3 à une résistance
   de test de 2,7 kΩ tirée successivement vers **3,3 V, 5 V puis 12 V** ; à
   l'oscilloscope (ou au fréquencemètre), vérifier un signal carré 0 V ↔ Vpull-up,
   rapport cyclique ≈ 50 %, fréquence = RPM_réel × ratio / 30 (Hz).
   Pendant le test, lancer un transfert WiFi / rafraîchir la page en continu :
   **la période du signal ne doit pas bouger** (c'est le Timer1 en NMI qui émet,
   indépendamment du WiFi et du serveur web — voir l'en-tête de `deye_fan.ino`).
5. **Ratios de départ :** 3,00 (9 cm) et 2,30 (6 cm), puis ajuster via l'interface
   web jusqu'à ce que le Deye affiche des vitesses plausibles et ne lève aucune
   alarme, en commençant bas et en augmentant par pas de 0,25.
6. **Arrêt ventilateur :** débrancher le +12 V d'un Noctua maître → après ~1,2 s,
   sa sortie simulée doit s'arrêter (ligne relâchée) et la LED repasser en
   indication d'attente : l'onduleur doit alors détecter le défaut, comme prévu.
7. **Remontage final :** onduleur hors tension, raccorder les deux connecteurs,
   refermer, remettre en service, contrôler alarmes et températures internes
   pendant plusieurs heures de production solaire.

---

## 9. Limites et hypothèses assumées

- **2 impulsions/tour** des deux côtés (Noctua et standard tach PC/Deye) : si un
  ventilateur ou l'entrée Deye différait, seul le facteur d'échelle changerait.
- **Un tach maître par canal** représente sa paire de ventilateurs (§3).
- Le pull-up tach du Deye est supposé exister côté onduleur (c'est lui qui
  alimentait les collecteurs ouverts des NMB d'origine) ; sa valeur exacte et sa
  tension (3,3/5/12 V) sont volontairement **inutiles** au dimensionnement (§4).
- Si le Deye régule la vitesse de ses ventilateurs en faisant varier le 12 V,
  les Noctua ralentiront de la même façon que l'auraient fait les NMB, le ratio
  appliqué restera valable, et le buck maintiendra le Wemos en service jusqu'à
  ~5 V d'entrée.
