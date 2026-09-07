/*
 * ============================================================================
 *  SCHÉMA ÉLECTRONIQUE COMPLET — DEYE FAN SIMULATOR
 *  Deye SUN-8K-SG05LP1 × Noctua NF-A6x25-FLX / NF-A9-FLX × 4
 *  Wemos D1 Mini V2.3.0 (ESP-12S / ESP8266)
 * ============================================================================
 *
 *  Ce fichier décrit l'ensemble du schéma électronique :
 *  1. Alimentation (12V → 3.3V, diode-OR + buck converter)
 *  2. Circuit d'entrée (Noctua tach → ESP8266 GPIO13)
 *  3. Circuit de sortie (ESP8266 GPIO12/4 → Deye tach input)
 *  4. Câblage des ventilateurs Noctua
 *  5. Câblage vers l'onduleur Deye
 *
 *  Les composants sont sélectionnés parmi les valeurs standard E24/E96.
 *  Tous les transistors NPN listés fonctionnent (2N2222, BC547, etc.).
 *
 * ============================================================================
 */

/*
 *
 * ╔══════════════════════════════════════════════════════════════════════╗
 * ║                    SCHÉMA ÉLECTRONIQUE COMPLET                      ║
 * ╚══════════════════════════════════════════════════════════════════════╝
 *
 *
 *  ┌──────────────────────────────────────────────────────────────────────┐
 *  │                     SECTION 1 : ALIMENTATION                         │
 *  │                                                                      │
 *  │  (Deye +12V Fan 1)      (Deye +12V Fan 2)                           │
 *  │        │                         │                                    │
 *  │        │ 12V                     │ 12V                               │
 *  │        │                         │                                    │
 *  │     ┌──┴──┐                  ┌───┴──┐                                │
 *  │     │ D1  │                  │ D2   │                                │
 *  │     │Schottky               │ Schottky                             │
 *  │     │1N5819                  │ 1N5819                               │
 *  │     │ Anode  ────┐          │ Anode  ────┐                         │
 *  │     │ Cathode────┼──────────┤ Cathode────┼────────────────┐         │
 *  │     └───────────┘          └────────────┘                │         │
 *  │                                                        │         │
 *  │                                              +11.7V ───┼────┐    │
 *  │                                             (diode drop)  │    │    │
 *  │                                                        │    │    │
 *  │                                              ┌─────────┴────┴────┴───┐
 *  │                                              │                       │
 *  │                                              │  XL4015 / LM2596      │
 *  │                                              │  Buck Converter       │
 *  │                                              │  (12V → 3.3V, 2A)     │
 *  │                                              │                       │
 *  │                                              └────┬────────────────┘    │
 *  │                                                   │                     │
 *  │                                               3.3V ───────────────────┐ │
 *  │                                                   │                     │ │
 *  │  ┌────────────────────────────────────────────────┴─────────────────────┼─┤
 *  │  │                        ALIMENTATION ESP8266                          │ │
 *  │  │                                                                        │ │
 *  │  │  Wemos D1 Mini :                                                       │ │
 *  │  │    VIN (5V pin)  ──→ 3.3V  (alimente le régulateur 3.3V interne)      │ │
 *  │  │    GND           ──→ GND  (masse commune)                              │ │
 *  │  │    3V3           │  (distribution 3.3V interne)                        │ │
 *  │  │    GND           │                                                     │ │
 *  │  └──────────────────┘                                                     │ │
 *  │           │                                                                 │ │
 *  │  ┌────────┴────────────────────────────────────────────────────────────────┼─┤
 *  │  │                           GND COMMUN                                   │ │
 *  │  │  ───────────────────────────────────────────────────────────────────────┼─┤
 *  │  │  (Relie : masse des ventilateurs Deye, cathodes D1/D2, GND buck,       │ │
 *  │  │   GND Wemos D1 Mini, émetteurs transistors de sortie, masse Deye)      │ │
 *  │  └─────────────────────────────────────────────────────────────────────────┘ │
 *  └──────────────────────────────────────────────────────────────────────────────┘
 *
 *
 *  ┌──────────────────────────────────────────────────────────────────────┐
 *  │                  SECTION 2 : CIRCUIT D'ENTRÉE                        │
 *  │          Noctua Tach (3 broches) → ESP8266 GPIO13                    │
 *  │                                                                      │
 *  │  Les deux ventilateurs Noctua partagent la même entrée GPIO13        │
 *  │  car leurs signaux tach sont OR-logiques (open-collector).           │
 *  │                                                                      │
 *  │  Câblage Noctua (3 broches) :                                        │
 *  │    Noir  = GND (masse)                                               │
 *  │    Rouge = +12V (alimentation)                                       │
 *  │    Jaune = TACH (signal, fil central, open-collector)                │
 *  │                                                                      │
 *  │  Les 4 fils TACH (2× NF-A9 + 2× NF-A6) sont mis en parallèle        │
 *  │  via des résistances 1kΩ de limitation de courant.                    │
 *  └──────────────────────────────────────────────────────────────────────┘
 *
 *  Circuit détaillé d'entrée (pour UN canal Noctua) :
 *
 *    Noctua TACH (jaune) ──┬── 1kΩ ───────────────────────┐
 *    Noctua TACH (jaune) ──┼── 1kΩ ───────────────────────┤
 *    Noctua TACH (jaune) ──┼── 1kΩ ───────────────────────┤
 *    Noctua TACH (jaune) ──┴── 1kΩ ───────────────────────┤
 *                                                        │
 *                                                        ├───┐
 *                                                        │   │ 47kΩ
 *                                                        │   │ (pull-up)
 *                                                        │   │
 *                                                     +12V┤───┘
 *                                                        │
 *                                                        │
 *                                                    ┌───┴───┐
 *                                                    │       │
 *                                                   39kΩ    │
 *                                                    │       │
 *                                                    │     ─┴──┐
 *                                                    │     │   │
 *                                                    └─────┤   ├───→ GPIO13 (D7)
 *                                                          │   │     ESP8266
 *                                                         10kΩ  │
 *                                                          │   │
 *                                                    ──────┤   │
 *                                                    GND   └───┘
 *
 *  Notes d'entrée :
 *    • Le signal tach Noctua est open-collector (collecteur transistor
 *      interne vers l'émetteur, base pilotée par le circuit de mesure
 *      de vélocité).
 *    • Le pull-up 47kΩ sur +12V est nécessaire car le Noctua ne fournit
 *      pas de pull-up interne sur le fil tach (certains modèles oui,
 *      d'autres non — on le met par sécurité).
 *    • Le diviseur 39kΩ + 10kΩ réduit la tension :
 *      12V → 12 × 10/(39+10) = 2.45V (sûr pour ESP8266, max 3.6V)
 *      5V  → 5  × 10/(39+10) = 1.02V  (> 2.0V seuil HIGH ESP8266 ✓)
 *      3.3V→ 3.3× 10/(39+10) = 0.67V  (~ seuil limite)
 *    • Pour 3.3V en entrée, ajouter un pull-up supplémentaire de 10kΩ
 *      vers 3.3V côté GPIO pour assurer un HIGH fiable.
 *    • Les 1kΩ en série limitent le courant de chaque fil tach Noctua
 *      et isolent les ventilateurs entre eux.
 *
 *
 *  ┌──────────────────────────────────────────────────────────────────────┐
 *  │                  SECTION 3 : CIRCUIT DE SORTIE                       │
 *  │          ESP8266 → Deye TACH Input (transistor NPN)                  │
 *  │                                                                      │
 *  │  L'onduleur Deye applique une tension inconnue sur sa broche         │
 *  │  tach input (3.3V, 5V ou 12V). Le circuit de sortie utilise un      │
 *  │  transistor NPN en collecteur ouvert pour s'adapter à TOUTE         │
 *  │  tension sans modification matérielle.                                │
 *  └──────────────────────────────────────────────────────────────────────┘
 *
 *  Circuit de sortie CANAL 1 (GPIO12 → Deye TACH CH1) :
 *
 *                           +12V (ou +5V/+3.3V Deye)
 *                                │
 *                                │ (tension inconnue Deye)
 *                                │
 *                          ┌──────┴──────┐
 *                          │             │
 *                         10kΩ          │  (pull-up Deye interne)
 *                          │             │
 *                          │             │
 *    ESP8266              │    ┌────────┴────────┐
 *    GPIO12 (D6)         │    │                 │
 *         │              │    │   Deye TACH     │
 *         ├──┤           │    │   Input CH1     │
 *        4.7kΩ          │    │                 │
 *         │              │    └────────┬────────┘
 *         │              │             │
 *         │           ┌──┴──┐          │
 *         │           │     │          │
 *         │      Base │  NPN│  Émetteur│
 *         │           │     │   │      │
 *         │           │     │   │      │
 *         │           └──┬──┘   │      │
 *         │              │      │      │
 *                        │     GND     │
 *                        │      │      │
 *                        │     GND ────┘
 *                        │
 *                  TACH OUT CH1
 *                  (vers Deye)
 *
 *  Circuit de sortie CANAL 2 (GPIO4 → Deye TACH CH2) :
 *
 *                           +12V (ou +5V/+3.3V Deye)
 *                                │
 *                          ┌──────┴──────┐
 *                          │             │
 *                         10kΩ          │
 *                          │             │
 *    ESP8266              │    ┌────────┴────────┐
 *    GPIO4 (D2)           │    │                 │
 *         │              │    │   Deye TACH     │
 *         ├──┤           │    │   Input CH2     │
 *        4.7kΩ          │    │                 │
 *         │              │    └────────┬────────┘
 *         │           ┌──┴──┐          │
 *         │      Base │  NPN│  Émetteur│
 *         │           │     │   │      │
 *         │           └──┬──┘   │      │
 *                        │      │      │
 *                       GND    GND ────┘
 *
 *  Notes de sortie :
 *    • Transistor NPN : 2N2222, BC547, BC548, S9013, S8050, C1815, etc.
 *      Tous fonctionnent. Le 2N2222 ou BC547 sont recommandés.
 *    • Résistance base : 4.7kΩ (limite le courant de base)
 *      Ibase = (3.3V - 0.7V) / 4700 = 0.55 mA
 *      Ic max (saturation) = β × Ibase = 100 × 0.55mA = 55 mA >> 1 mA
 *      nécessaire pour le pull-up. Saturation garantie.
 *    • Résistance collecteur : 10kΩ (si le pull-up Deye est absent ou
 *      trop faible)
 *    • Quand GPIO = HIGH → transistor saturé → collecteur ≈ GND → signal BAS
 *    • Quand GPIO = LOW  → transistor bloqué → collecteur tiré vers +V → signal HAUT
 *    • Le circuit fonctionne avec TOUTE tension Deye (3.3V, 5V, 12V)
 *      car le transistor est en collecteur ouvert.
 *
 *
 *  ┌──────────────────────────────────────────────────────────────────────┐
 *  │              SECTION 4 : CÂBLAGE VENTILATEURS NOCTUA                 │
 *  │                                                                      │
 *  │  Pour CHAQUE ventilateur Noctua (×4 total) :                         │
 *  │                                                                      │
 *  │    Noir (GND)    ────→ masse commune du système                     │
 *  │    Rouge (+12V)  ────→ +12V du connecteur ventilateur Deye          │
 *  │    Jaune (TACH)  ────→ vers circuit d'entrée (voir section 2)       │
 *  │                                                                      │
 *  │  Câblage pour 2× NF-A9-FLX (9 cm) :                                  │
 *  │    Fils TACH jaunes :                                                  │
 *  │      → 1kΩ → point commun entrée                                     │
 *  │      → 1kΩ → point commun entrée                                     │
 *  │                                                                      │
 *  │  Câblage pour 2× NF-A6x25-FLX (6 cm) :                               │
 *  │    Fils TACH jaunes :                                                  │
 *  │      → 1kΩ → point commun entrée                                     │
 *  │      → 1kΩ → point commun entrée                                     │
 *  └──────────────────────────────────────────────────────────────────────┘
 *
 *
 *  ┌──────────────────────────────────────────────────────────────────────┐
 *  │              SECTION 5 : CÂBLAGE VERS L'ONDULEUR DEYE                │
 *  │                                                                      │
 *  │  Connecteurs ventilateur Deye (chacun 3 broches) :                   │
 *  │                                                                      │
 *  │  Connecteur Fan 1 (pour les 2× NF-A9) :                              │
 *  │    Broche 1 (+12V)  ────→ +12V ventilateur                           │
 *  │    Broche 2 (GND)   ────→ GND commune                                │
 *  │    Broche 3 (TACH)  ────→ TACH entrée (vers circuit d'entrée)        │
 *  │                                                                      │
 *  │  Connecteur Fan 2 (pour les 2× NF-A6) :                              │
 *  │    Broche 1 (+12V)  ────→ +12V ventilateur                           │
 *  │    Broche 2 (GND)   ────→ GND commune                                │
 *  │    Broche 3 (TACH)  ────→ TACH entrée (vers circuit d'entrée)        │
 *  │                                                                      │
 *  │  Sorties vers Deye TACH INPUT :                                      │
 *  │    CH1 TACH OUT ────→ Deye TACH Input CH1 (connecteur fan 1, broche │
 *  │    CH2 TACH OUT ────→ Deye TACH Input CH2 (connecteur fan 2, broche │
 *  │                                                                      │
 *  │  NOTE : Les sorties transistor NPN remplacent les signaux tach       │
 *  │  des ventilateurs Noctua. Elles sont connectées AUX MÊMES broches    │
 *  │  que les ventilateurs sur l'onduleur (le signal Deye ne change pas).  │
 *  │                                                                      │
 *  │  Le circuit fonctionne PARALLÈLEMENT aux ventilateurs :              │
 *  │  les ventilateurs Noctua sont branchés sur les connecteurs           │
 *  │  ventilateur Deye, et le circuit de sortie est branché              │
 *  │  PARALLÈLEMENT sur les mêmes broches TACH de l'onduleur.             │
 *  │                                                                      │
 *  │  Le Deye continue d'alimenter les Noctua en 12V.                     │
 *  │  Le Wemos D1 Mini alimente SON CIRCUIT depuis les +12V via           │
 *  │  le buck converter.                                                   │
 *  └──────────────────────────────────────────────────────────────────────┘
 *
 *
 *  ╔══════════════════════════════════════════════════════════════════════╗
 *  ║                    LISTE DES COMPOSANTS                             ║
 *  ╚══════════════════════════════════════════════════════════════════════╝
 *
 *  Quantité  │ Référence       │ Désignation              │ Emplacement
 *  ──────────┼─────────────────┼──────────────────────────┼─────────────────
 *  1         │ Wemos D1 Mini   │ Module ESP8266          │ CPU principal
 *            │ V2.3.0          │ (LOLIN WEMOS D1 mini)   │
 *  ──────────┼─────────────────┼──────────────────────────┼─────────────────
 *  2         │ 1N5819        │ Diode Schottky 1A/40V  │ Alimentation
 *            │                 │ (diode-OR)              │ (D1, D2)
 *  ──────────┼─────────────────┼──────────────────────────┼─────────────────
 *  1         │ XL4015/E5        │ Buck Converter          │ Alimentation
 *            │ 12V→3.3V        │ 12V → 3.3V / 2A        │
 *  ──────────┼─────────────────┼──────────────────────────┼─────────────────
 *  2         │ 4.7kΩ 1/4W    │ Résistance            │ Sortie (base)
 *            │ E24             │ (Rbase)                 │ CH1, CH2
 *  ──────────┼─────────────────┼──────────────────────────┼─────────────────
 *  2         │ 10kΩ 1/4W     │ Résistance            │ Sortie (collector)
 *            │ E24             │ (Rcollector)            │ CH1, CH2
 *  ──────────┼─────────────────┼──────────────────────────┼─────────────────
 *  1         │ 47kΩ 1/4W     │ Résistance            │ Entrée (pull-up)
 *            │ E24             │ (Rpullup)              │ TACH
 *  ──────────┼─────────────────┼──────────────────────────┼─────────────────
 *  1         │ 39kΩ 1/4W     │ Résistance            │ Entrée (diviseur)
 *            │ E24             │ (Rdiviseur haut)       │ TACH
 *  ──────────┼─────────────────┼──────────────────────────┼─────────────────
 *  1         │ 10kΩ 1/4W     │ Résistance            │ Entrée (diviseur)
 *            │ E24             │ (Rdiviseur bas)        │ TACH
 *  ──────────┼─────────────────┼──────────────────────────┼─────────────────
 *  4         │ 1kΩ 1/4W      │ Résistance            │ Entrée (limitation)
 *            │ E24             │ (Rseries)              │ TACH Noctua
 *  ──────────┼─────────────────┼──────────────────────────┼─────────────────
 *  2         │ 2N2222        │ Transistor NPN        │ Sortie
 *            │ ou BC547        │ (Q1, Q2)               │ CH1, CH2
 *            │ ou S9013        │                         │
 *  ──────────┼─────────────────┼──────────────────────────┼─────────────────
 *  1         │ EC12-3.3V     │ Buck converter        │ Alimentation
 *            │ (ajustable)     │ 12V → 3.3V / 2A       │
 *            │                 │ (module LM2596/MT3608)  │
 *  ──────────┴─────────────────┴──────────────────────────┴─────────────────
 *
 *  NOTE : Les 10kΩ en sortie (collector) sont optionnels si l'onduleur
 *  Deye fournit déjà un pull-up interne sur ses broches tach. On les
 *  garde par sécurité en cas de pull-up trop faible ou inexistant.
 *
 *
 *  ╔══════════════════════════════════════════════════════════════════════╗
 *  ║                    PINOUT WEMOS D1 MINI                              ║
 *  ╚══════════════════════════════════════════════════════════════════════╝
 *
 *  GPIO    │ D-Pin    │ Utilisé par
 *  ────────┼──────────┼─────────────────────────────────────────────
 *  GPIO0   │ D3       │ Non utilisé (pull-up requis au boot)
 *  GPIO1   │ TX       │ Non utilisé (UART)
 *  GPIO2   │ D4       │ Non utilisé (LED onboard active LOW)
 *  GPIO3   │ RX       │ Non utilisé (UART)
 *  GPIO4   │ D2       │ ✓ Sortie TACH CH2 (NF-A6 × 2)
 *  GPIO5   │ D1       │ Non utilisé
 *  GPIO12  │ D6       │ ✓ Sortie TACH CH1 (NF-A9 × 2)
 *  GPIO13  │ D7       │ ✓ Entrée TACH Noctua (GPIO interrupt)
 *  GPIO14  │ D5       │ Non utilisé (SCL I2C)
 *  GPIO15  │ D8       │ Non utilisé (boot : doit être HIGH)
 *  GPIO16  │ D0       │ ✓ LED statut (intégrée, active LOW)
 *
 *
 *  ╔══════════════════════════════════════════════════════════════════════╗
 *  ║                    NOTES DE CONCEPTION                             ║
 *  ╚══════════════════════════════════════════════════════════════════════╝
 *
 *  1. ISOLATION LOGICIELLE
 *     Les ISRs (tachInputISR et tachOutputISR) s'exécutent en dehors de
 *     la boucle principale et du WiFi. Elles accèdent directement aux
 *     registres GPIO hardware pour une latence minimale (< 5 µs).
 *
 *  2. ALIMENTATION
 *     Le buck converter alimente le Wemos D1 Mini depuis les +12V des
 *     connecteurs ventilateur Deye. La diode-OR permet à un seul
 *     connecteur d'alimenter le circuit même si l'autre est éteint.
 *
 *  3. COMPATIBILITÉ TENSION ENTRÉE
 *     Le diviseur 39k + 10k assure que la tension sur GPIO13 ne dépasse
 *     jamais 3.6V, même si le signal tach atteint 12V.
 *
 *  4. COMPATIBILITÉ TENSION SORTIE
 *     Le transistor NPN en collecteur ouvert s'adapte à TOUTE tension
 *     Deye (3.3V, 5V, 12V) car le pull-up est fourni par l'onduleur
 *     lui-même sur sa broche tach.
 *
 *  5. Fréquence de Sortie
 *     La fréquence de sortie est déterminée par :
 *       freq_sortie = freq_entrée × ratio
 *     avec freq_entrée = RPM_ventilateur × 2 / 60
 *     La plage couvre typiquement 10 Hz à 2 kHz.
 *
 *  6. LED DE STATUT
 *     La LED intégrée au Wemos D1 Mini (GPIO16) s'allume quand la
 *     simulation est active (sortie PWM en cours). Éteinte quand
 *     aucun signal tach n'est détecté.
 *
 * ============================================================================
 *  FIN DU DOCUMENT DE CIRCUIT
 * ============================================================================
 */
