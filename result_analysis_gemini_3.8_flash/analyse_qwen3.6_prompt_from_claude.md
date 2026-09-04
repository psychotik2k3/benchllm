# Analyse — qwen3.6_prompt_from_claude

**Sources analysées** : `competitors/qwen3.6_prompt_from_claude/deye_fan.ino`, `competitors/qwen3.6_prompt_from_claude/SCHEMATIC.md`

---

## ✅ Points techniques positifs

### Temps-réel / ISR
- Utilisation de la bibliothèque `Ticker` pour cadencer un ordonnanceur à 20 kHz (`SCHEDULER_HZ = 20000`).
- Commutation directe des broches de sortie via les registres matériels rapides `GPOS` et `GPOC`.
- Captures des impulsions d'entrée par interruptions GPIO matérielles (`attachInterrupt` sur front `FALLING`) concises en IRAM.
- Overclocking CPU à 160 MHz (`system_update_cpu_freq(160)`) pour garantir une réserve de puissance de calcul.

### Démarrage / Boot readiness
- Broches initialisées à l'état bas dès le démarrage pour maintenir les transistors NPN bloqués.

### Fidélité au protocole tachymétrique / Fail-safe
- Règle des 2 impulsions par tour respectée (`PPR = 2`).
- Timeout d'inactivité fixé à 2 secondes (`STALL_TIMEOUT_MS = 2000`) : désactivation du canal et maintien de la ligne au repos en cas de décrochage du ventilateur.
- Limiteur de pente (`MAX_SLEW_PCT = 15`) évitant les sauts de fréquence discontinus.

### Filtres / Signal
- Moyenne glissante sur 4 échantillons et filtre passe-bas sur les périodes mesurées.
- Anti-rebond d'entrée par élimination des fronts trop rapprochés (< 10 000 µs / 3 000 tr/min).

### WiFi / Réseau
- Utilisation de `ESPAsyncWebServer` et `ArduinoOTA` pour des mises à jour sans fil à distance.

### Gestion mémoire / HEAP
- Sérialisation JSON directe dans les handlers HTTP sans instanciation d'objets `String` volumineux.

### Persistance / Stockage
- Sauvegarde de la configuration dans le système de fichiers `LittleFS` avec `ArduinoJson`.

### Documentation hardware/code
- Document `SCHEMATIC.md` très soigné : OU-diode avec diodes Schottky BAT54/SS14, condensateur réservoir de 1 000 µF, convertisseur buck externe MT3608 (12V vers 5V), et filtre d'entrée RC (4,7 kΩ / 1 nF).

---

## ❌ Points techniques négatifs

### Temps-réel / Stabilité logicielle (critique)
- [critique] : Erreur de portée de variable fatale dans la fonction `setup()` (ligne 943) :
  L'ordonnanceur `Ticker` est instancié en tant que variable **locale** à l'intérieur de `setup()` :
  ```cpp
  void setup() {
      ...
      Ticker scheduler;
      scheduler.attach(1.0f / (float)SCHEDULER_HZ, schedulerTick);
      ...
  } // <--- Destruction immédiate de l'objet scheduler !
  ```
  Sur ESP8266, lorsqu'un objet `Ticker` déclaré localement sort de sa portée (*out of scope*), son destructeur est appelé et détache immédiatement l'interruption logicielle du timer ! En conséquence, la fonction `schedulerTick()` s'arrête net dès que le microcontrôleur termine son initialisation : plus aucune impulsion n'est générée en sortie après le démarrage — impact : le simulateur s'arrête définitivement quelques millisecondes après le boot.

### Filtres / Signal (majeur)
- [majeur] : Le filtre anti-rebond d'entrée impose une période minimale de 10 000 µs (`MIN_PERIOD_US = 10000`). À 2 impulsions par tour, cela correspond à une vitesse maximale de seulement :
  \[
  \text{RPM}_{\max} = \frac{60 \times 10^6}{2 \times 10\,000} = 3\,000\,\text{tr/min}
  \]
  Or, les petits ventilateurs Noctua de 6 cm (NF-A6x25-FLX) mentionnés dans le cahier des charges atteignent précisément 3 000 tr/min en régime nominal, et peuvent dépasser cette valeur de 5 à 10 % en pointe. Toute vitesse supérieure à 3 000 tr/min sera immédiatement rejetée comme parasite par le filtre — impact : blocage de la lecture du ventilateur 6 cm dès qu'il approche de son régime maximal.

### Qualité de code générale (négatif)
- [mineur] : Présence de multiples bibliothèques additionnelles lourdes (`ArduinoOTA`, `ESPAsyncWebServer`, `AsyncTCP`) augmentant l'empreinte mémoire flash sans bénéfice déterminant pour un simulateur temps-réel bas niveau.

---

## ⭐ Note globale : 5.2/10 — Projet prometteur ruiné par une variable locale dans setup()
Le firmware et le schéma présentent d'excellentes qualités de finition, mais la déclaration locale de l'instance `Ticker scheduler` détruit le timer à la sortie du `setup()`, coupant définitivement la simulation au boot.

## ⭐ Note qualité de code : 6.5/10 — Code élégant mais non testé sur banc réel
Belle architecture logicielle et bon usage des registres atomiques, trahie par une faute d'inattention fatale sur la durée de vie d'un objet C++.
