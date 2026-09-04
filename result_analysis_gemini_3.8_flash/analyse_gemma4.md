# Analyse — gemma4

**Sources analysées** : `competitors/gemma4/firmware_main.cpp`, `competitors/gemma4/README.md`

---

## ✅ Points techniques positifs

### Documentation hardware/code
- Présence d'un fichier `README.md` décrivant l'objectif théorique du simulateur.

---

## ❌ Points techniques négatifs

### Compilation / Syntaxe (critique)
- [critique] : Le fichier `firmware_main.cpp` ne compile même pas en raison d'une erreur de syntaxe grossière à la fin du fichier :
  ```cpp
  }
  ````)
  ```
  Ces caractères résiduels provoquent une erreur immédiate du compilateur C++ — impact : code strictement inutilisable sans intervention manuelle.

### Hardware / Affectation des broches (critique)
- [critique] : Affectation des broches GPIO 10 et 11 pour les entrées tachymétriques :
  ```cpp
  #define PIN_TACH_9cm 10
  #define PIN_TACH_6cm 11
  ```
  Sur le microcontrôleur ESP8266, les GPIO 6 à 11 sont raccordées en interne aux lignes de données et d'horloge de la puce mémoire SPI Flash externe. Tenter d'utiliser les GPIO 10 et 11 comme entrées numériques interfère avec les accès bus de la Flash et déclenche un plantage immédiat (*Watchdog reset* ou *Fatal exception 0*) dès le démarrage du composant — impact : impossibilité matérielle absolue d'exécuter ce code sur un ESP8266.

### Temps-réel / Architecture logicielle (critique)
- [critique] : Absence totale de génération de signal sur la sortie simulée !
  Le code définit une unique broche de sortie `PIN_OUTPUT 14`, mais celle-ci n'est **jamais manipulée** : il n'y a aucun appel à `digitalWrite()`, aucune ISR de génération, aucun timer matériel configuré. Le système ne génère rigoureusement aucune impulsion vers l'onduleur Deye — impact : le simulateur ne simule rien.
- [critique] : Le calcul des RPM est complètement faux :
  Dans les interruptions d'entrée `tach9_isr()` et `tach6_isr()`, les compteurs `count9` et `count6` sont incrémentés à chaque impulsion sans jamais être remis à zéro :
  ```cpp
  rpm9 = (count9 * ratio9);
  ```
  La variable `rpm9` s'accumule donc jusqu'à l'infini au lieu de représenter une vitesse instantanée — impact : calcul numérique sans aucun sens physique.

### Architecture matérielle (critique)
- [critique] : Une seule broche de sortie `PIN_OUTPUT 14` est prévue pour simuler simultanément deux ventilateurs ayant des diamètres et des régimes moteurs distincts (9 cm et 6 cm) — impact : impossibilité de satisfaire les deux canaux tachymétriques attendus par l'onduleur Deye.

### Persistance / Stockage (majeur)
- [majeur] : Absence totale de persistance (ni EEPROM, ni SPIFFS/LittleFS). Les ratios modifiés sur l'interface web sont perdus à la moindre coupure d'alimentation.

---

## ⭐ Note globale : 1.0/10 — Code halluciné et non fonctionnel
Le livrable ne compile pas, utilise les broches interdites de la Flash SPI de l'ESP8266 qui font planter le processeur, ne pilote jamais sa broche de sortie et accumule les tours à l'infini.

## ⭐ Note qualité de code : 1.5/10 — Projet factice
Code produit sans aucune compréhension de l'architecture matérielle de l'ESP8266 ni des bases de la programmation microcontrôleur.
