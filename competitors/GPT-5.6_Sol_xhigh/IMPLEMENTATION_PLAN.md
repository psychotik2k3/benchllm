# Deye Fan Tach Simulator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: execute each task in order and
> preserve the test-first checks described below.

**Goal:** Livrer un simulateur tachymétrique double canal pour Wemos D1 Mini,
avec configuration web persistante et dossier de câblage complet.

**Architecture:** Les entrées collecteur ouvert sont mesurées par interruptions
IRAM. Le moteur d'ondes Timer1 du coeur ESP8266 génère les sorties, qui sont
converties en collecteur ouvert par deux BC337. Le serveur HTTP ne partage avec
le chemin temps réel que des instantanés atomiques et des paramètres validés.

**Tech Stack:** Arduino ESP8266 core 3.1.x, ESP8266WiFi, ESP8266WebServer,
EEPROM émulée, C++11.

## Contraintes globales

- Carte cible : `LOLIN(WEMOS) D1 R2 & mini`.
- Un seul tachymètre est surveillé dans chaque paire de ventilateurs.
- Ratios initiaux : 2,50× pour 6 cm et 2,00× pour 9 cm.
- Ratios configurables : 0,10× à 10,00×.
- AP et STA sont actifs simultanément.
- La sortie électrique Deye doit accepter un pull-up externe de 3,3 à 12 V.
- La limite temps réel propre à l'ESP8266 décrite dans `DESIGN.md` reste
  explicitement applicable.

---

### Tâche 1 — Mathématiques tachymétriques testables

**Fichiers :**
- Créer : `test_tach_math.cpp`
- Créer : `tach_math.h`

**Interfaces :**
- `TachMath::rpmFromPeriodUs(float) -> float`
- `TachMath::outputHalfPeriodUs(float, float) -> uint32_t`
- `TachMath::ratioIsValid(float) -> bool`

- [ ] Écrire des assertions pour 3 000 tr/min, 1 600 tr/min, les deux ratios
  initiaux et les bornes de validation.
- [ ] Compiler avant la création de `tach_math.h` et constater l'échec attendu.
- [ ] Écrire les trois fonctions pures sans dépendance Arduino.
- [ ] Compiler et exécuter les assertions ; résultat attendu : code retour 0.

### Tâche 2 — Firmware

**Fichier :**
- Créer : `deye_fan.ino`

**Interfaces :**
- Interruptions `onTach6Edge()` et `onTach9Edge()`.
- Traitement périodique `updateTachChannels()`.
- Routes HTTP `/`, `/api/status` et `/save`.

- [ ] Définir le brochage et initialiser les sorties à transistor bloqué.
- [ ] Accumuler les périodes valides dans les interruptions IRAM.
- [ ] Extraire atomiquement les mesures et appliquer le filtrage.
- [ ] Démarrer, actualiser ou arrêter chaque onde Timer1.
- [ ] Ajouter le stockage EEPROM avec signature, version et CRC32.
- [ ] Démarrer AP+STA avec sommeil Wi-Fi désactivé.
- [ ] Ajouter l'interface web et valider toute valeur reçue.
- [ ] Vérifier que le sketch n'effectue ni allocation, ni réseau, ni écriture
  flash dans une interruption.

### Tâche 3 — Dossier électrique et utilisation

**Fichier :**
- Créer : `SCHEMA_ET_MONTAGE.md`

- [ ] Fournir la nomenclature complète.
- [ ] Décrire les quatre blocs : alimentation, deux entrées, deux sorties.
- [ ] Montrer le raccordement des quatre ventilateurs, avec un seul tach par
  paire.
- [ ] Documenter les contrôles hors tension et à l'oscilloscope.
- [ ] Signaler la vérification obligatoire des masses et de la capacité en
  courant des connecteurs.

### Tâche 4 — Vérification finale

- [ ] Exécuter le test natif de `tach_math.h`.
- [ ] Compiler le sketch avec Arduino CLI si l'outil et le coeur ESP8266 sont
  disponibles localement.
- [ ] Contrôler l'absence d'erreur statique dans les fichiers créés.
- [ ] Rapporter exactement les vérifications exécutées et les limites qui
  requièrent le matériel réel.
