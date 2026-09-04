# Schéma électronique — simulateur tach Deye (Wemos D1 Mini)

Carte : **LOLIN (WEMOS) D1 Mini V2.3.0** (ESP-12S / ESP8266).  
Transistors NPN : **2N3904**, **S8050** ou **2N2222** (tous convennent).  
Ne **jamais** injecter 12 V sur la broche **5V** du D1 Mini : le LDO embarqué (RT9013 / ME6211) est prévu pour ~5 V.

---

## 1. Principe

```
  Deye 9 cm ──+12V──► 2× Noctua NF-A9-FLX          tach lue (1 ventilo) ──► ESP
  Deye 6 cm ──+12V──► 2× Noctua NF-A6x25-FLX       tach lue (1 ventilo) ──► ESP

  ESP ── NPN collecteur ouvert ──► broche tach Deye 9 cm
  ESP ── NPN collecteur ouvert ──► broche tach Deye 6 cm

  +12V_9 ── Schottky ─┐
                      ├─► buck 12 V → 5 V ──► Wemos 5V
  +12V_6 ── Schottky ─┘
```

Les fils tach d’origine (Deye ↔ NMB) sont **coupés**. L’onduleur ne voit plus que le collecteur ouvert du NPN. Les Noctua ne voient que le pull-up 3,3 V de l’ESP.

---

## 2. Alimentation (deux rails 12 V indépendants)

Les deux connecteurs ventilateur de l’onduleur ne sont **pas** forcément alimentés en même temps, ni à la même tension. On **OU-diode** uniquement l’alimentation du Wemos. Chaque taille de ventilateur reste sur **son** rail.

```
  [Deye 9 cm +12V]────┬──── +12V_FAN9  (vers les deux NF-A9-FLX)
                      │
                     D1  1N5819 (Schottky)  anode = rail 9 cm
                      │ cathode
                      ├──────────── +12V_OR
                      │
                     D2  1N5819            anode = rail 6 cm
                      │
  [Deye 6 cm +12V]────┴──── +12V_FAN6  (vers les deux NF-A6x25-FLX)


  +12V_OR ──── VIN  [ buck DC-DC  ]  VOUT 5,0 V ──── Wemos broche 5V
                   [ MP1584 / Mini360 / ]
                   [  réglé à 5,00 V     ]
                   GND ──────────────────────────── Wemos GND

  [Deye 9 cm GND] ──┐
                    ├── GND commun (étoile) ── Wemos GND
  [Deye 6 cm GND] ──┘                          ── émetteurs NPN
                                               ── GND Noctua
```

| Composant | Rôle |
|-----------|------|
| D1, D2 Schottky (1N5819, SS14, BAT46…) | OU-diode : le rail le plus haut alimente le buck ; pas de retour vers l’autre connecteur |
| Buck 12 V → 5 V (≥ 500 mA, 1 A recommandé) | Seule alimentation sûre du D1 Mini. Wi-Fi ≈ 80–250 mA crête |
| Option : 100 µF / 25 V après les diodes | Réserve si un rail disparaît brièvement |

**À ne pas faire**

- 12 V direct sur `5V` / USB du Wemos (surchauffe / destruction du LDO).
- Faire passer le courant des ventilateurs **dans** le Wemos.
- Relier `+12V_FAN9` à `+12V_FAN6` sans diodes (rétro-alimentation de l’onduleur).

Les deux Noctua 9 cm sont en parallèle sur `+12V_FAN9` / GND. Idem 6 cm sur `+12V_FAN6`.

---

## 3. Entrées tach (Noctua → ESP8266)

Les NF-A6x25-FLX et NF-A9-FLX : **collecteur ouvert**, **2 impulsions / tour**, Ic max 5 mA. Ils **ne génèrent pas** de tension : ils tirent à GND. Un pull-up 3,3 V suffit et reste sûr même si l’autre ventilo de la paire n’est pas câblé.

On ne lit **qu’un** tach par taille (le second ventilo de chaque paire n’a pas besoin d’être monitoré).

```
  Noctua 9 cm  (un des deux NF-A9-FLX)
    GND  ──────────────────────────── GND
    +12V ──────────────────────────── +12V_FAN9
    TACH ──── R1 220 Ω ────┬──────── D5  (GPIO14)
                           │
                          R2 4,7 kΩ  vers 3V3 Wemos
                           │
                          C1 100 pF  vers GND   (optionnel, anti-HF)

  Noctua 6 cm  (un des deux NF-A6x25-FLX)
    GND  ──────────────────────────── GND
    +12V ──────────────────────────── +12V_FAN6
    TACH ──── R3 220 Ω ────┬──────── D6  (GPIO12)
                           │
                          R4 4,7 kΩ  vers 3V3
                           │
                          C2 100 pF  vers GND   (optionnel)
```

Le firmware active aussi le pull-up interne ; R2/R4 (externes) donnent un front plus propre. Courant de fuite tach ≈ 3,3 V / 4,7 kΩ ≈ 0,7 mA ≪ 5 mA.

Le second ventilo de chaque taille : **GND + 12 V seulement** (fil tach isolé / non connecté).

---

## 4. Sorties tach (ESP8266 → Deye) — 3,3 V / 5 V / 12 V sans modif

La broche tach de l’onduleur a un **pull-up interne à une tension inconnue** (3,3 / 5 / 12 V). Un **collecteur ouvert NPN** accepte les trois cas : on ne fait que tirer à GND, on ne sort jamais 3,3 V vers l’onduleur.

**Ne pas** clampter le collecteur vers 3V3 (zener / diode vers 3V3) : cela fausserait la lecture 12 V de l’onduleur.

```
  Wemos D1 (GPIO5) ── R5 4,7 kΩ ──┬── base  Q1  2N3904 / S8050 / 2N2222
                                  │         E ── GND
                                 R6 10 kΩ   C ── R7 100 Ω ──── tach Deye 9 cm
                                  │
                                 GND

  Wemos D2 (GPIO4) ── R8 4,7 kΩ ──┬── base  Q2  même type
                                  │         E ── GND
                                 R9 10 kΩ   C ── R10 100 Ω ─── tach Deye 6 cm
                                  │
                                 GND
```

| Résistance | Valeur | Rôle |
|------------|--------|------|
| R5, R8 | 4,7 kΩ | Ib ≈ (3,3 − 0,7) / 4,7 k ≈ 0,55 mA. Suffisant pour Ic ≤ 5 mA (spec tach NMB/Intel) |
| R6, R9 | 10 kΩ | Garantit Q off si GPIO flotte au reset |
| R7, R10 | 100 Ω | Limite un court-circuit / ESD sur le fil tach |

Vce(sat) < 0,3 V : l’onduleur voit un niveau bas valide, que son pull-up soit à 3,3 V, 5 V ou 12 V.  
Vceo des 2N3904 / S8050 / 2N2222 ≫ 12 V.

GPIO HIGH → Q saturé → tach onduleur = GND (impulsion).  
GPIO LOW  → Q bloqué → pull-up onduleur seul (niveau haut).

---

## 5. LED d’état

La LED **onboard** du D1 Mini (GPIO2 / D4, **cathode vers GPIO**, active LOW) est utilisée par le firmware :

- **Allumée fixe** : au moins un canal simule (signal tach Noctua présent).
- **Clignotante (~2 Hz)** : aucun signal, attente.

LED externe optionnelle : GPIO2 → 330 Ω → LED → 3V3 (même polarité active LOW).

---

## 6. Brochage résumé

| Wemos | GPIO | Signal |
|-------|------|--------|
| 5V | — | 5 V **après** le buck uniquement |
| GND | — | GND commun |
| 3V3 | — | Pull-up tach Noctua (R2, R4) |
| D5 | 14 | Tach IN 9 cm |
| D6 | 12 | Tach IN 6 cm |
| D1 | 5 | Base Q1 (OUT 9 cm) |
| D2 | 4 | Base Q2 (OUT 6 cm) |
| D4 | 2 | LED onboard |

Laisser **libre** : D3 (GPIO0 boot), D8 (GPIO15 boot), D0 (GPIO16, pas d’IRQ).

---

## 7. Câblage des connecteurs (vue système)

```
  Connecteur Deye 9 cm (ex-NMB 09225)          Connecteur Deye 6 cm (ex-NMB 06025)
  ┌─────────────────────────┐                  ┌─────────────────────────┐
  │ GND  ───────────────────┼── GND commun     │ GND  ───────────────────┼── GND commun
  │ +12V ──┬────────────────┼── 2× NF-A9 +12V  │ +12V ──┬────────────────┼── 2× NF-A6 +12V
  │        └─ D1 Schottky ─┼── +12V_OR         │        └─ D2 Schottky ─┼── +12V_OR
  │ TACH ── (fil Deye) ─────┼── R7 ── C Q1     │ TACH ── (fil Deye) ─────┼── R10 ── C Q2
  └─────────────────────────┘                  └─────────────────────────┘
         ▲ ne plus relier au Noctua                    ▲ idem
```

Polarité typique des 3 fils ventilateur (à vérifier au multimètre **avant** branchement) :

- Noir = GND  
- Rouge / jaune = +12 V  
- Jaune / blanc / bleu = tach  

Les Noctua 3 broches suivent en général noir / brun(+12) / jaune(tach) — **contrôler** sur le harnais fourni.

---

## 8. Nomenclature

| Qté | Réf | Valeur / type |
|-----|-----|----------------|
| 1 | — | Wemos D1 Mini V2.3.0 |
| 1 | U-buck | Module buck 12 V → 5 V, ≥ 1 A, réglé à 5,00 V |
| 2 | D1 D2 | Schottky 1 A (1N5819 / SS14) |
| 2 | Q1 Q2 | NPN 2N3904, S8050, 2N2222, BC547… |
| 2 | R1 R3 | 220 Ω |
| 4 | R2 R4 R5 R8 | 4,7 kΩ |
| 2 | R6 R9 | 10 kΩ |
| 2 | R7 R10 | 100 Ω |
| 2 | C1 C2 | 100 pF (optionnel) |
| 1 | C-bus | 100 µF / 25 V (optionnel sur +12V_OR) |
| 2 | — | Noctua NF-A9-FLX |
| 2 | — | Noctua NF-A6x25-FLX |

---

## 9. Mise en service

1. Régler le buck à **5,00 V à vide**, puis seulement brancher le Wemos.  
2. Flasher `deye_fan.ino` (carte **LOLIN(WEMOS) D1 mini**, CPU 80 MHz).  
3. Point d’accès `DeyeFan-xxxx` / mot de passe `deyefan12` → http://192.168.4.1  
4. Ajuster les ratios sans recompiler (départ **2,00**).  
   - NF-A9-FLX ≈ 2000 tr/min vs NMB 92 mm souvent 3500–5600 tr/min → ratio typique **1,8–2,8**.  
   - NF-A6x25-FLX ≈ 3000 tr/min vs NMB 60 mm plus rapide → ratio typique **1,5–2,5**.  
5. Contrôler à l’oscillo : sortie collecteur = créneau ~50 %, jitter indépendant du trafic HTTP (Timer1 **NMI**).
