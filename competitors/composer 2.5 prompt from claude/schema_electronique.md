# Schéma électronique — Simulateur tachymètre ventilateurs Deye

**Carte :** Wemos D1 Mini (ESP8266)  
**Application :** Lecture tach Noctua NF-A9-FLX (×2) + NF-A6x25-FLX (×2) → simulation tach vers onduleur Deye SUN-8K-SG05LP1-EU-AM2-P

---

## 1. Vue d'ensemble

```
  [Ventilateur 9 cm +12V]──────┬──────────────────────────────────────────────► Deye (9 cm)
                               │
  [Ventilateur 6 cm +12V]──────┼──────────────────────────────────────────────► Deye (6 cm)
                               │
                               ▼
                    ┌──────────────────────┐
                    │  OR-ing Schottky     │
                    │  + réservoir + Buck  │
                    │  12V → 5V            │
                    └──────────┬───────────┘
                               ▼
                    ┌──────────────────────┐
                    │  Wemos D1 Mini       │
                    │  (ESP8266)           │
                    └──────────────────────┘
```

---

## 2. Alimentation 12 V → Wemos 5 V

### 2.1 OR-ing des deux connecteurs ventilateur

Les deux connecteurs 12 V des ventilateurs ne sont pas garantis actifs simultanément. Un OR-ing évite le retour de courant entre connecteurs.

| Réf. | Composant | Valeur | Justification |
|------|-----------|--------|---------------|
| D1, D2 | Diode Schottky (ex. SS14, 1N5819) | 1 A / 40 V | Chute directe ~0,3–0,4 V vs ~0,7 V pour silicium ; perte réduite |
| C1 | Condensateur électrolytique | 680 µF / 25 V | Réservoir lors du basculement d'un connecteur à l'autre ; tient ~10–20 ms à 300 mA |
| U1 | Module buck DC/DC | 12 V → 5 V, ≥1 A | Évite la dissipation du AMS1117 embarqué (12→3,3 V ≈ 2,6 W à 300 mA) |
| C2 | Céramique | 100 nF | Découplage HF entrée buck |
| C3 | Électrolytique | 220 µF / 10 V | Découplage sortie buck → broche 5V Wemos |
| C4 | Céramique | 100 nF | Découplage local près broche 3V3 ESP8266 |
| C5 | Électrolytique/tantale | 100 µF / 6,3 V | Absorption pics courant WiFi (~300–400 mA) |

### 2.2 Câblage alimentation

```
+12V_FAN_A ──►|── D1 (anode) ──┐
                               ├──► +12V_BUS ──► [U1 Buck IN] ──► 5V ──► Wemos pin 5V
+12V_FAN_B ──►|── D2 (anode) ──┘         │
                                         C1 (680µF) entre +12V_BUS et GND

GND_FAN_A ──┬── GND_COMMUN ◄── GND Wemos
GND_FAN_B ──┘
```

**Important :** Ne pas alimenter le Wemos en 12 V sur la broche 5V. Utiliser uniquement la sortie 5 V du buck.

---

## 3. Entrée tach — Noctua → ESP8266 (2 canaux identiques)

Le fil tach Noctua est **collecteur ouvert** (NPN interne). Ne jamais tirer vers 12 V (max ~5,25 V sur la broche).

### 3.1 Canal 0 (9 cm) — exemple

| Réf. | Composant | Valeur | Justification |
|------|-----------|--------|---------------|
| R1 | Résistance pull-up | 4,7 kΩ | Vers 3,3 V Wemos ; courant ~0,7 mA ; dans la plage 4,7–10 kΩ recommandée |
| C6 | Condensateur céramique | 1 nF | Filtre anti-rebond ; fc = 1/(2π×4700×1e-9) ≈ 34 kHz >> 400 Hz max sortie |
| — | Signal | TACH_NOCTUA_9 | Fil tach du ventilateur Noctua 9 cm |

```
3V3 (Wemos) ─── R1 (4,7kΩ) ───┬──► GPIO14 / D5 (entrée interruption)
                               │
                          C6 (1nF)
                               │
                              GND

TACH_NOCTUA_9 (fil jaune/blanc) ───┬── (nœud R1 / D5)
                                   │
                              (interne ventilateur → GND)
```

### 3.2 Canal 1 (6 cm)

| Réf. | Composant | Valeur |
|------|-----------|--------|
| R2 | Pull-up | 4,7 kΩ → 3,3 V |
| C7 | Filtre | 1 nF → GND |
| — | GPIO | GPIO12 / D6 |

Même schéma que canal 0, broche D6.

**Aucun transistor côté entrée** — la résistance de pull-up suffit.

---

## 4. Sortie tach — ESP8266 → Deye (collecteur ouvert simulé)

**Contrainte critique :** la tension du pull-up interne de l'onduleur est inconnue (3,3 V, 5 V ou 12 V). Interdit de connecter un GPIO directement.

### 4.1 Transistor NPN — canal 0 (9 cm)

**Transistor choisi :** BC547 (β typ. 200, Vceo 45 V, Ic 100 mA)

| Réf. | Composant | Valeur | Calcul |
|------|-----------|--------|--------|
| Q1 | BC547 (NPN) | — | Vceo 45 V >> 12 V max |
| R3 | Résistance de base | 1 kΩ | Voir calcul ci-dessous |

**Calcul résistance de base :**

- Courant collecteur estimé Ic ≈ 5 mA (pull-up onduleur typique)
- Gain β = 200 → Ib_min = Ic/β = 25 µA
- Facteur de sursaturation ×10 → Ib = 250 µA
- Vbe(sat) ≈ 0,7 V ; Vgpio = 3,3 V
- Rb = (3,3 − 0,7) / 250e-6 ≈ **10,4 kΩ** → choix **1 kΩ** (sursaturation ×40, marge large pour variations de β et Ic)

```
GPIO13 / D7 ─── R3 (1kΩ) ─── Base Q1
                              Émetteur Q1 ─── GND_COMMUN
                              Collecteur Q1 ─── TACH_DEYE_9 (fil tach onduleur)
                                              (flottant côté haut — pull-up interne Deye)
```

**Logique :**
- GPIO HIGH → Q1 saturé → collecteur tire la ligne tach à GND (impulsion)
- GPIO LOW → Q1 bloqué → ligne relâchée, tirée haut par le pull-up Deye

### 4.2 Canal 1 (6 cm)

| Réf. | Composant | Valeur |
|------|-----------|--------|
| Q2 | BC547 | — |
| R4 | Base | 1 kΩ |
| — | GPIO | GPIO5 / D1 |

Même schéma, broche D1.

---

## 5. LED d'état

| Réf. | Composant | Valeur | Justification |
|------|-----------|--------|---------------|
| LED1 | LED (optionnelle externe) | — | Ou LED intégrée D4 du Wemos |
| R5 | Résistance série | 330 Ω | I ≈ (3,3−2)/330 ≈ 4 mA |

```
GPIO2 / D4 ─── R5 (330Ω) ─── LED ─── GND
```

Sur Wemos D1 Mini, D4 pilote la LED intégrée (active LOW). R5 externe optionnelle si LED déportée.

**États (pilotés depuis `loop()`, jamais depuis ISR) :**
- **Fixe allumée** : au moins un canal simule activement
- **Clignotante (500 ms)** : aucun signal d'entrée valide (timeout 1,5 s)

---

## 6. Table de brochage complète

| Broche Wemos | GPIO | Fonction | Notes |
|--------------|------|----------|-------|
| D0 | GPIO16 | *(non utilisé)* | Pas d'interruption matérielle |
| D1 | GPIO5 | **Sortie tach CH1** (base Q2) | OK sortie |
| D2 | GPIO4 | *(libre)* | IRQ possible si extension |
| D3 | GPIO0 | *(non utilisé)* | Boot — strap FLASH |
| D4 | GPIO2 | **LED état** | Boot (strap) ; usage non critique OK |
| D5 | GPIO14 | **Entrée tach CH0** (9 cm) | Interruption FALLING |
| D6 | GPIO12 | **Entrée tach CH1** (6 cm) | Interruption FALLING |
| D7 | GPIO13 | **Sortie tach CH0** (base Q1) | OK sortie |
| D8 | GPIO15 | *(non utilisé)* | Boot — strap (pull-down requis) |

---

## 7. Liste de composants (BOM)

| Qté | Composant | Réf. suggérée |
|-----|-----------|---------------|
| 1 | Wemos D1 Mini | — |
| 2 | Diode Schottky 1A 40V | SS14 / 1N5819 |
| 1 | Module buck 12V→5V 1A | MP1584, LM2596 mini, etc. |
| 1 | Condensateur 680 µF 25V | C1 |
| 1 | Condensateur 220 µF 10V | C3 |
| 2 | Condensateur 100 µF 6,3V | C5 (+ optionnel buck) |
| 3 | Condensateur céramique 100 nF | C2, C4, (+ buck) |
| 2 | Condensateur céramique 1 nF | C6, C7 |
| 2 | Résistance 4,7 kΩ | R1, R2 |
| 2 | Résistance 1 kΩ | R3, R4 |
| 1 | Résistance 330 Ω | R5 (si LED externe) |
| 2 | Transistor NPN BC547 | Q1, Q2 |
| 1 | LED 3 mm (optionnel) | LED1 |

---

## 8. Masse et câblage ventilateurs

- **Masse commune (GND_COMMUN)** : GND des deux connecteurs ventilateur, émetteurs Q1/Q2, GND Wemos, GND buck.
- **Fil tach Noctua** : connecté au nœud pull-up (jamais au +12 V).
- **Fil tach Deye** : connecté au collecteur du transistor (jamais au GPIO directement).
- **+12 V ventilateurs** : alimentent les ventilateurs Noctua (fil rouge) ET le OR-ing vers le buck.

---

## 9. Marges et vérifications

| Paramètre | Valeur | Statut |
|-----------|--------|--------|
| Fréquence entrée max (~3000 RPM) | ~100 Hz | OK |
| Fréquence sortie max (ratio ×4) | ~400 Hz | OK ; demi-période min 1,25 ms |
| Résolution Timer1 | 50 µs | 25 ticks/demi-période min |
| Vceo BC547 | 45 V | >> 12 V pull-up Deye |
| Pull-up entrée | 3,3 V | < 5,25 V max Noctua |
| Dissipation buck vs AMS1117 | ~0,5 W vs ~2,6 W | Buck obligatoire |

---

## 10. Schéma ASCII complet (résumé)

```
                    +12V_FAN_A ──|>──┐
                                       ├── +12V_BUS ──[C1 680µ]── GND
                    +12V_FAN_B ──|>──┘         │
                                                 ▼
                                          [BUCK 12→5V]
                                                 │
                    GND ─────────────────────────┼── GND Wemos
                                                 │
                                          5V ──► Wemos 5V

  Noctua 9cm TACH ──┬── R1 4k7 ── 3V3
                    C6 1nF
                    └── D5 (GPIO14)

  Noctua 6cm TACH ──┬── R2 4k7 ── 3V3
                    C7 1nF
                    └── D6 (GPIO12)

  D7 (GPIO13) ── R3 1k ── B Q1 ── E ── GND
                              C ── TACH_DEYE_9

  D1 (GPIO5)  ── R4 1k ── B Q2 ── E ── GND
                              C ── TACH_DEYE_6

  D4 (GPIO2) ── [LED état]
```
