# Deye Fan Simulator — Noctua Tach Adapter

Remplace les signaux tach de l'onduleur solaire **Deye SUN-8K-SG05LP1-EU-AM2-P** par des signaux simulés pour les ventilateurs **Noctua** silencieux.

## Contexte

Les ventilateurs d'origine NMB (06025VE-12N-CL et 09225VE-12N-CU) tournent à des RPM élevés que l'onduleur détecte correctement. Les Noctua NF-A6x25-FLX et NF-A9-FLX sont plus silencieux mais tournent moins vite — l'onduleur risquerait de lever une alarme RPM trop bas.

**Ce circuit fait croire à l'onduleur que les ventilateurs tournent plus vite** en multipliant le signal tach par un ratio configurable.

## Fonctionnalités

- ✅ Lecture du signal tach de 4 ventilateurs Noctua (2× 6 cm + 2× 9 cm)
- ✅ Multiplication RPM par ratio configurable (0.1× à 10.0×)
- ✅ Interface web pour configuration WiFi et ratios
- ✅ WiFi mode AP + STA simultané (captive portal)
- ✅ Persistance des paramètres dans la EEPROM
- ✅ LED de statut (active = simulation en cours)
- ✅ Isolement complet du traitement tach par rapport au WiFi (ISRs matérielles)

## Schéma de câblage global

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         ONDULEUR DEYE                                    │
│   ┌──────────────┐    ┌──────────────┐                                  │
│   │ Connecteur   │    │ Connecteur   │                                  │
│   │ Fan 1 (+12V) │    │ Fan 2 (+12V) │                                  │
│   │              │    │              │                                  │
│   │  Noir  ──GND─┼────┼── GND ───────┤  (masse commune)                │
│   │  Rouge ──+12V├────┼── +12V ─────┤  (alimentation ventilateurs)     │
│   │  Jaune ──TACH├────┼── TACH ─────┤  (signal tach → entrée)          │
│   └──────────────┘    └──────────────┘                                  │
│         │                    │                                           │
│         │                    │                                           │
│   ┌─────┴────────────────────┴─────────────────────────────────────┐    │
│   │                                                                 │    │
│   │                    CIRCUIT ADAPTATEUR                           │    │
│   │                                                                 │    │
│   │   ┌───────────────────────────────────────────────────────┐     │    │
│   │   │  DIODE-OR + BUCK CONVERTER                            │     │    │
│   │   │  Deye +12V → D1(1N5819) ─┐                           │     │    │
│   │   │  Deye +12V → D2(1N5819) ─┤── 11.7V → Buck 12V→3.3V  │     │    │
│   │   │                           └──→ 3.3V → Wemos D1 Mini  │     │    │
│   │   └───────────────────────────────────────────────────────┘     │    │
│   │                                                                 │    │
│   │   ┌───────────────────────────────────────────────────────┐     │    │
│   │   │  ENTRÉE TACH (GPIO13)                                 │     │    │
│   │   │  4× Noctua TACH → 1kΩ → 47kΩ pull-up → diviseur      │     │    │
│   │   │               39kΩ + 10kΩ → GPIO13                    │     │    │
│   │   └───────────────────────────────────────────────────────┘     │    │
│   │                                                                 │    │
│   │   ┌───────────────────────────────────────────────────────┐     │    │
│   │   │  SORTIE TACH (GPIO12 + GPIO4)                         │     │    │
│   │   │  GPIO12 → transistor NPN → CH1 TACH vers Deye        │     │    │
│   │   │  GPIO4  → transistor NPN → CH2 TACH vers Deye        │     │    │
│   │   └───────────────────────────────────────────────────────┘     │    │
│   │                                                                 │    │
│   │   ┌───────────────────────────────────────────────────────┐     │    │
│   │   │  WEMOS D1 MINI (ESP8266)                              │     │    │
│   │   │  GPIO13 = entrée tach    │  GPIO12 = sortie CH1       │     │    │
│   │   │  GPIO4  = sortie CH2     │  GPIO16 = LED statut       │     │    │
│   │   └───────────────────────────────────────────────────────┘     │    │
│   │                                                                 │    │
│   └─────────────────────────────────────────────────────────────────┘    │
│         │                    │                                           │
│         └────────────────────┘                                           │
│    (les sorties transistor sont branchées PARALLÈLEMENT                   │
│     sur les broches TACH de l'onduleur — même connecteur)               │
└─────────────────────────────────────────────────────────────────────────┘
```

## Liste des composants

| Qté | Composant | Emplacement |
|-----|-----------|-------------|
| 1 | Wemos D1 Mini V2.3.0 (ESP-12S) | CPU |
| 2 | 1N5819 (ou SS14) | Diode Schottky — alimentation |
| 1 | Buck converter 12V→3.3V (XL4015 / LM2596) | Alimentation |
| 2 | Transistor NPN (2N2222, BC547, S9013, C1815...) | Sortie tach CH1, CH2 |
| 2 | Résistance 4.7 kΩ | Base transistor (sortie) |
| 2 | Résistance 10 kΩ | Collecteur transistor (sortie) |
| 1 | Résistance 47 kΩ | Pull-up entrée tach |
| 1 | Résistance 39 kΩ | Diviseur tension entrée |
| 1 | Résistance 10 kΩ | Diviseur tension entrée |
| 4 | Résistance 1 kΩ | Limitation courant entrée Noctua |

## Installation matérielle

### Étape 1 : Préparation de l'onduleur

1. **Couper l'alimentation** de l'onduleur et attendre 5 minutes que les condensateurs se déchargent.
2. **Identifier les connecteurs ventilateur** sur la carte interne de l'onduleur (souvent des connecteurs JST ou Molex).
3. **Noter le brochage** des connecteurs : +12V, GND, TACH.

### Étape 2 : Câblage des ventilateurs Noctua

Pour **chaque** ventilateur Noctua :

| Fil Noctua | Couleur | Connection |
|------------|---------|------------|
| GND | Noir | Masse commune du système |
| +12V | Rouge | +12V du connecteur Deye |
| TACH | Jaune | Circuit d'entrée (→ 1 kΩ → point commun) |

Les 4 fils TACH (2 pour chaque NF-A9, 2 pour chaque NF-A6) passent chacun par une résistance de 1 kΩ puis se rejoignent au point commun d'entrée.

### Étape 3 : Montage du circuit adaptateur

#### Alimentation
1. Relier les deux +12V des connecteurs Deye aux anodes de D1 et D2 (1N5819).
2. Rejoindre les cathodes ensemble → entrée du buck converter.
3. Sortie du buck converter → 3.3V vers la broche VIN (5V pin) du Wemos D1 Mini.
4. Masse commune : cathodes D1/D2, GND buck, GND Wemos, émetteurs transistors.

#### Entrée tach
1. Les 4 fils TACH Noctua (après leurs 1 kΩ respectifs) se rejoignent.
2. Résistance 47 kΩ entre le point commun et +12V (pull-up).
3. Résistance 39 kΩ entre le point commun et GPIO13.
4. Résistance 10 kΩ entre GPIO13 et GND.
5. Résistance 10 kΩ entre +12V et le point de jonction 39k/10k (pull-up supplémentaire pour fiabiliser le niveau HAUT).

#### Sortie tach
1. **CH1** : Collecteur transistor Q1 → broche TACH CH1 de l'onduleur (parallèle sur le connecteur).
2. **CH2** : Collecteur transistor Q2 → broche TACH CH2 de l'onduleur (parallèle sur le connecteur).
3. Émetteurs Q1 et Q2 → GND.
4. Bases Q1 et Q2 → GPIO12 et GPIO4 respectivement via 4.7 kΩ.

### Étape 4 : Verification

1. **Vérifier les polarités** des diodes et du buck converter.
2. **Mesurer la tension** 3.3V du buck converter (doit être stable à 3.30V ±0.05V).
3. **Vérifier les continuités** des masses.
4. **Ne pas brancher** avant d'avoir téléversé le firmware.

## Installation du firmware

### Prérequis
- [Arduino IDE](https://www.arduino.cc/en/software) (version 1.8.x ou 2.x)
- [Drivers CH340](https://github.com/nicekey/ch341g_drivers) (si nécessaire pour le Wemos)

### Configuration Arduino IDE
1. Installer la carte ESP8266 : **Fichier → Préférences → URL supplémentaire** → `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
2. **Outils → Board → LOLIN(WEMOS) D1 mini**
3. **Outils → CPU Frequency → 80 MHz**
4. **Outils → Flash Size → 4M (1M SPIFFS)**
5. Sélectionner le port COM du Wemos

### Téléversement
1. Brancher le Wemos D1 Mini en USB.
2. Ouvrir le fichier `deye_fan.ino` dans Arduino IDE.
3. Compiler et téléverser.
4. Le Wemos redémarre et crée un réseau WiFi **DeyeFanSim** (mot de passe : `noctua2024`).

## Configuration WiFi

1. Se connecter au réseau WiFi **DeyeFanSim**.
2. Ouvrir un navigateur → `192.168.4.1` (captive portal automatique).
3. Configurer :
   - **SSID** : votre réseau WiFi domestique
   - **Mot de passe** : le mot de passe WiFi
   - **Ratios** : valeurs par défaut (2.5× pour 9 cm, 3.5× pour 6 cm)
4. Cliquer sur **Enregistrer & Redémarrer**.
5. Le Wemos se reconnecte à votre réseau et affiche son adresse IP.

### Trouver l'adresse IP
- Via votre routeur (liste des clients DHCP)
- Via l'AP : `192.168.4.1` (mode AP toujours actif)
- Via un scan réseau (fing, nmap...)

## Réglage des ratios

Les ratios déterminent combien de fois plus vite l'onduleur "pense" que les ventilateurs tournent.

### Méthode de calcul

```
Ratio = RPM_onduleur_cible / RPM_noctua_réels
```

1. **Mesurer** le RPM réel du ventilateur Noctua (compteur tachymètre laser ou application smartphone).
2. **Déterminer** le RPM minimum attendu par l'onduleur (consultation manuel Deye ou mesure avec ventilateur d'origine).
3. **Calculer** le ratio.

### Ratios recommandés (valeurs par défaut)

| Ventilateur | RPM typique (12V) | Ratio par défaut | RPM simulés |
|-------------|-------------------|------------------|-------------|
| NF-A9-FLX × 2 | ~1200 RPM | 2.50× | ~3000 RPM |
| NF-A6x25-FLX × 2 | ~2000 RPM | 3.50× | ~7000 RPM |

> ⚠️ **Ajuster les ratios** selon les spécificités de votre onduleur et les RPM réels de vos ventilateurs. Des ratios trop faibles peuvent provoquer une alarme RPM bas. Des ratios excessivement élevés n'ont pas d'effet négatif (sauf consommation légèrement accrue).

### Changement en temps réel
- Interface web → modifier le ratio → Enregistrer & Redémarrer.
- Les ratios sont sauvegardés en EEPROM et persistent au redémarrage.

## Interface web

### Page d'accueil (`192.168.4.1` ou IP STA)

Affiche en temps réel (actualisation auto 5s) :
- **Statut WiFi** : mode AP/STA, connexion, IP
- **RPM temps réel** : RPM lus, RPM simulés, ratios, fréquences, statut
- **Configuration** : WiFi SSID, mot de passe, ratios
- **Informations système** : MCU, flash, heap, uptime, reset reason

### Page de mise à jour (`/update`)

Confirmation après enregistrement + redémarrage automatique.

## Dépannage

### Le Wemos ne démarre pas
- Vérifier la tension 3.3V du buck converter (doit être stable).
- Vérifier les connexions USB et le câble.
- Appuyer sur le bouton RESET du Wemos.

### Impossible de se connecter au WiFi
- Le réseau AP est toujours actif : `192.168.4.1` (SSID : DeyeFanSim, pass : noctua2024).
- Vérifier le SSID et mot de passe dans l'interface web.
- Réinitialiser l'EEPROM : débrancher 12V pendant 30 secondes.

### Les RPM affichés sont 0
- Vérifier le câblage des fils TACH Noctua (jaune → circuit d'entrée).
- Vérifier l'alimentation des ventilateurs Noctua (+12V stable).
- Vérifier les connexions GPIO13 (entrée) et les résistances du diviseur.
- La LED statut (GPIO16/D0) doit clignoter quand un signal est détecté.

### Les RPM simulés ne correspondent pas
- Ajuster les ratios dans l'interface web.
- Vérifier que les ventilateurs tournent (mesurer le RPM réel).
- Vérifier que l'onduleur détecte bien le signal (pas d'alarme RPM).

### L'onduleur affiche une alarme
- Augmenter les ratios de 0.1× et tester.
- Vérifier que le signal de sortie est bien envoyé (LED allumée).
- Vérifier la connexion des transistors de sortie sur les broches TACH Deye.
- Consulter le manuel Deye pour les plages RPM acceptées.

### Jitter sur les RPM (affichage instable)
- Le code utilise des ISRs matérielles isolées du WiFi → jitter minimal.
- Si le problème persiste, vérifier l'alimentation (bruit sur le +12V).
- Ajouter un condensateur 100 nF entre +12V et GND près du buck converter.

### Le WiFi est instable
- Le Wemos D1 Mini est sensible aux interférences RF des alimentations à découpage.
- Ajouter un condensateur 10 µF entre VIN et GND du Wemos.
- Éloigner le Wemos des sources de bruit RF (transformateurs, câbles haute tension).

## Architecture technique détaillée

### Isolement logiciel

```
┌─────────────────────────────────────────────────────────┐
│                    ESP8266                               │
│                                                         │
│  ┌─────────────────────────────────────────────┐       │
│  │  Boucle principale + WiFi SDK               │       │
│  │  • Serveur HTTP / DNS                       │       │
│  │  • Calcul RPM (tous les ~10 ms)             │       │
│  │  • Connexion WiFi (STA)                     │       │
│  │  → PEUT être bloquant (pas de problème)     │       │
│  └─────────────────────────────────────────────┘       │
│                              ▲                          │
│                              │ pulsePeriodUs            │
│                              │ (variable volatile)      │
│                              │                          │
│  ┌──────────────────────┐    │    ┌──────────────────┐ │
│  │ ISR Entrée (GPIO)    │    │    │ ISR Sortie (Tmr1)│ │
│  │ • attachInterrupt    │    │    │ • Timer FRC2     │ │
│  │ • Mesure période µs  │    │    │ • Toggle pin     │ │
│  │ • < 5 µs d'exécution │    │    │ • < 5 µs         │ │
│  │ • Indépendant WiFi   │    │    │ • Indépendant    │ │
│  └──────────────────────┘    │    └──────────────────┘ │
│                              │                          │
└─────────────────────────────────────────────────────────┘
```

### Caractéristiques du traitement tach
- **Précision mesure** : 1 µs (compteur 80 MHz)
- **Précision génération** : 12.5 ns (compteur timer 80 MHz)
- **Jitter maximal estimé** : < 10 µs (lié aux interruptions GPIO)
- **Plage de fréquence** : 2 Hz à 40 kHz
- **Plage de RPM** : 60 RPM à 120 000 RPM (simulation)

## Licence

Ce projet est fourni tel quel. Utilisez-le à vos risques et périls. Modifiez les ratios selon vos besoins spécifiques.

## Auteur

Projet développé pour remplacer les ventilateurs NMB bruyants par des Noctua silencieux sur un onduleur Deye SUN-8K-SG05LP1-EU-AM2-P.

## Version

1.0 — 2025
