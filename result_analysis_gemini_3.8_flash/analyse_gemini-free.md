# Analyse — gemini-free

**Sources analysées** : `competitors/gemini-free/gemini-code-1787264142602.cpp`

---

## ✅ Points techniques positifs

### Temps-réel / ISR
- Capture des impulsions des ventilateurs d'entrée par interruptions matérielles GPIO avec mesure de l'intervalle temporel en microsecondes (`micros()`).
- Utilisation de `ESPAsyncWebServer` et `ArduinoJson` pour le serveur web.

### Démarrage / Boot readiness
- Initialisation des sorties à l'état bas dans le `setup()`.

### Filtres / Signal
- Présence d'un filtre anti-rebond simple éliminant les intervalles inférieurs à 2 000 µs (`MIN_PULSE_INTERVAL_US`).

### Persistance / Stockage
- Utilisation du système de fichiers `LittleFS` avec un fichier JSON pour sauvegarder la configuration.

---

## ❌ Points techniques négatifs

### Documentation hardware/code (critique)
- [critique] : Aucun document technique, aucun schéma électronique, ni aucun fichier d'explication d'installation ou de câblage n'est fourni dans le dossier du compétiteur (un seul fichier `.cpp` brut sans en-tête explicatif) — impact : impossibilité complète de câbler ou déployer le projet sans rétro-ingénierie totale.

### Temps-réel / ISR (critique)
- [critique] : Erreur fatale dans le calcul de rechargement du Timer1 matériel :
  Le timer est initialisé avec un diviseur 16 :
  ```cpp
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
  ```
  Sur ESP8266 à 80 MHz, l'horloge du Timer1 après division par 16 tourne à **5 MHz** (soit **5 ticks par microseconde**). Or, dans l'interruption `onTimerISR()`, la durée est rechargée en multipliant par 80 :
  ```cpp
  timer1_write(80 * timerStepUs); // Commentaire faux : "80 ticks/µs à 80MHz"
  ```
  Avec `timerStepUs = 50`, le compteur est chargé avec 4 000 ticks. À 5 ticks/µs, 4 000 ticks correspondent à **800 µs** au lieu des 50 µs souhaitées ! Le timer s'exécute ainsi 16 fois trop lentement (1 250 Hz au lieu de 20 000 Hz) — impact : distorsion temporelle massive de la fréquence de simulation, sortie tachymétrique totalement erronée.
- [critique] : Appel de `digitalWrite()` directement à l'intérieur de l'ISR du Timer1 :
  ```cpp
  digitalWrite(PIN_TACH_OUT_1, outState1);
  digitalWrite(PIN_TACH_OUT_2, outState2);
  ```
  Au lieu de registres atomiques `GPOS`/`GPOC`, ces fonctions lentes dégradent lourdement le déterminisme temporel.

### Stabilité / Runtime (majeur)
- [majeur] : Présence d'un appel bloquant `delay(50)` au beau milieu de la fonction `loop()` :
  ```cpp
  void loop() {
      ...
      delay(50);
  }
  ```
  Cela gèle la boucle principale pendant 50 ms à chaque itération, dégradant la réactivité globale et le traitement des requêtes réseau.

### Persistance / Stockage (majeur)
- [majeur] : Absence de vérification du code retour de `LittleFS.begin()` dans `setup()` : en cas d'échec de montage du système de fichiers, le firmware continue son exécution dans un état indéfini sans journaliser d'erreur.

### Qualité de code générale (négatif)
- [majeur] : Nommage aléatoire du fichier source (`gemini-code-1787264142602.cpp`), absence de structure modulaire, mélange des préoccupations.

---

## ⭐ Note globale : 4.0/10 — Erreur mathématique sur le timer matériel et absence de documentation
Le firmware souffre d'un bug majeur dans la configuration de l'horloge du Timer1 (multiplication par 80 au lieu de 5, ralentissant l'ISR d'un facteur 16), contient un `delay()` bloquant et ne fournit strictement aucun schéma électrique.

## ⭐ Note qualité de code : 5.0/10 — Code inachevé et non vérifié
Le code donne l'impression d'un premier jet non testé, avec des constantes contradictoires et une absence totale de livrables matériels.
