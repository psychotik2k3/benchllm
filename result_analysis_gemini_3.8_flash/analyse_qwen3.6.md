# Analyse — qwen3.6

**Sources analysées** : `competitors/qwen3.6/deye_fan.ino`, `competitors/qwen3.6/SCHÉMA_CIRCUIT.md`

---

## ✅ Points techniques positifs

### Démarrage / Boot readiness
- Broches de sortie configurées à l'état bas au démarrage.

### WiFi / Réseau
- Présence d'un serveur web standard avec endpoints `/` et `/set`.

---

## ❌ Points techniques négatifs

### Compilation / API incompatible (critique)
- [critique] : Le fichier `deye_fan.ino` utilise les fonctions de gestion de timer de l'architecture **ESP32**, totalement inexistantes dans le core Arduino de l'**ESP8266** :
  ```cpp
  hw_timer_t *tachTimer = NULL;
  tachTimer = timerBegin(1, TIMER_PRESCALER, true);
  timerAttachInterrupt(tachTimer, &tachOutputAllISR, true);
  timerAlarmWrite(tachTimer, ...);
  ```
  Sur ESP8266, l'API matérielle est `timer1_attachInterrupt()`, `timer1_enable()` ou `Ticker`. Les types `hw_timer_t` et `timerBegin()` provoquent une erreur de compilation immédiate `fatal error: unknown type name 'hw_timer_t'` — impact : le code ne compile absolument pas pour la cible Wemos D1 Mini / ESP8266.
- [critique] : Erreur de syntaxe grossière à la ligne 218 de `deye_fan.ino` :
  ```cpp
  EEPROM.end();
  ```
  Cette instruction est placée en dehors de toute fonction, directement dans l'espace global du fichier source, empêchant toute compilation — impact : échec de compilation immédiat.

### Logique de simulation / Calcul mathématique (critique)
- [critique] : Inversion mathématique de la formule de période simulée :
  Dans l'interruption `fan1TachISR()` (ligne 321), le code calcule la période cible en **multipliant** la période mesurée par le ratio :
  ```cpp
  uint32_t simPeriodU32 = (uint32_t)((uint64_t)period * fan1RatioInt10 / 10);
  ```
  Or, la période est inversement proportionnelle à la fréquence (\(T = 1 / f\)) ! Pour simuler un ventilateur tournant plus vite (par exemple avec un ratio de 2,5), il faut **diviser** la période par 2,5, et non la multiplier ! En multipliant la période, la fréquence résultante est divisée : le ventilateur simulé tourne 2,5 fois **plus lentement** que le ventilateur réel — impact : contresens physique absolu menant tout droit au déclenchement de l'alarme de sous-vitesse par l'onduleur Deye.

### Documentation hardware/code (critique)
- [critique] : Le schéma `SCHÉMA_CIRCUIT.md` (§2.1) préconise de relier directement la sortie du OU-diode 12V à la broche **VIN** du module Wemos D1 Mini. Le régulateur embarqué sur les Wemos D1 Mini (RT9013 ou ME6211) possède une tension d'entrée maximale absolue de 5,5 V ou 6,0 V. Y appliquer du 12V le détruit instantanément en fumée — impact : destruction matérielle de la carte de développement dès la première mise sous tension.

### Persistance / Stockage (majeur)
- [majeur] : Absence totale de mécanisme de somme de contrôle ou CRC sur les données EEPROM.

---

## ⭐ Note globale : 2.0/10 — Code incompilable, formules inversées et schéma destructeur
Le projet ne compile pas (mélange d'API ESP32 sur ESP8266 et syntaxe C++ invalide), inverse mathématiquement la vitesse simulée et préconise de griller le régulateur de la carte en lui injectant du 12V direct.

## ⭐ Note qualité de code : 2.5/10 — Qualité médiocre et code non testé
Code manifestement généré sans aucune vérification ni compilation de contrôle.
