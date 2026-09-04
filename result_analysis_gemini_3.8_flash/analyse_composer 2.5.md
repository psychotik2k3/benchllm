# Analyse — composer 2.5

**Sources analysées** : `competitors/composer 2.5/deye_fan/deye_fan.ino`, `competitors/composer 2.5/deye_fan/schema_electronique.md`

---

## ✅ Points techniques positifs

### Temps-réel / ISR
- Timer1 matériel configuré en interruption périodique rapide (`TIM_LOOP`) à 10 µs (`TIMER_TICK_US = 10`), offrant une résolution fine pour la décrémentation des demi-périodes.
- Interruptions GPIO sur front descendant (`FALLING`) pour mesurer les impulsions des ventilateurs Noctua.
- Variables de transfert partagées déclarées `volatile`.

### Démarrage / Boot readiness
- Initialisation des sorties au repos (`digitalWrite(..., LOW)`) dans `setup()` avant le démarrage du timer.
- Lancement de `setupTimer1()` avant l'initialisation du WiFi pour isoler la génération dès le départ.

### Génération sortie / Timer1
- Décompte de ticks à 100 kHz (10 µs par tick), limitant la gigue de quantification à 10 µs.
- En l'absence de signal, la ligne est relâchée au repos par l'extinction du transistor.

### Fidélité au protocole tachymétrique / Fail-safe
- Règle des 2 impulsions par tour appliquée (`PULSES_PER_REV = 2`).
- Timeout d'entrée de 3 secondes (`INPUT_TIMEOUT_US = 3000000UL`) surveillé dans `loop()`, ramenant la simulation à l'arrêt.
- Bornage des ratios entre 1.0 et 6.0.

### Filtres / Signal
- Rejet des périodes hors limites d'entrée : `period < MIN_PERIOD_US` (500 µs) ou `> MAX_PERIOD_US` (600 000 µs).

### WiFi / Réseau
- Mode `WIFI_AP_STA` simultané avec désactivation de la veille modem (`WiFi.setSleepMode(WIFI_NONE)`).

### Persistance / Stockage
- Sauvegarde EEPROM avec contrôle d'intégrité par CRC16 (`calcCrc`).

### Documentation hardware/code
- Schéma ASCII structuré décrivant le montage OR-ing avec diodes Schottky 1N5819 et l'étage de sortie à collecteur ouvert.

### Qualité de code générale (positif)
- **Structure / organisation** : Fonctions bien découpées par blocs de responsabilités.
- **Lisibilité / nommage** : Nommage clair des structures et des broches.
- **Commentaires / documentation inline** : Commentaires d'en-tête décrivant l'affectation des broches.
- **Gestion d'erreurs / robustesse** : Validation des ratios reçus par les formulaires web.

---

## ❌ Points techniques négatifs

### Temps-réel / ISR (critique)
- [critique] : Calculs en virgule flottante effectués directement dans l'interruption GPIO `handleTachEdge()` :
  ```cpp
  float ratio = c->ratio;
  uint32_t outPeriodUs = (uint32_t)(period / ratio);
  ```
  Sur ESP8266 (architecture Xtensa sans FPU matérielle), les opérations flottantes logicielles dans une ISR corrompent le contexte des registres et provoquent des ralentissements majeurs ou des paniques du noyau — impact : risque élevé de plantage système sous interruption.
- [critique] : Manipulation des broches GPIO dans l'ISR Timer1 (`gpioOutHigh`/`gpioOutLow`) via une lecture-modification-écriture non atomique :
  ```cpp
  GPIO_REG_WRITE(GPIO_OUT_ADDRESS, GPIO_REG_READ(GPIO_OUT_ADDRESS) | (1 << pin));
  ```
  Contrairement à l'utilisation des registres dédiés `GPOS` et `GPOC`, cette opération non atomique peut écraser un changement d'état provoqué simultanément par une autre partie du firmware — impact : corruption intermittente des états GPIO.

### Documentation hardware/code (critique)
- [critique] : Le document `schema_electronique.md` préconise de raccorder directement la sortie du OU-diode (12V) sur la broche **5V** du Wemos D1 Mini en affirmant que le régulateur AMS1117 supportera la dissipation thermique (~1 W). Or, un ESP8266 en émission WiFi crête (300 mA) sous 12V impose une dissipation de \((12 - 3,3) \times 0,3 \approx 2,6\,\text{W}\) : l'AMS1117 entre en coupure thermique ou brûle en quelques minutes — impact : destruction matérielle de la carte Wemos sans convertisseur buck externe.

### Gestion mémoire / HEAP (majeur)
- [majeur] : La génération de la page HTML dans `htmlPage()` utilise des dizaines de concaténations dynamiques d'objets `String` sur le tas (heap) à chaque requête HTTP — impact : fragmentation rapide de la mémoire RAM pouvant entraîner un crash au bout de quelques heures de rafraîchissement continu (`<meta http-equiv='refresh' content='2'>`).

### Filtres / Signal (majeur)
- [majeur] : Absence de filtre de lissage (moyenne glissante ou filtre EMA) : chaque impulsion brute d'entrée recalcule immédiatement la période cible, transférant toute la gigue mécanique du ventilateur sur la sortie.

### Interface Web (mineur)
- [mineur] : Rafraîchissement de l'interface par balise `<meta http-equiv='refresh' content='2'>` provoquant le rechargement brutal de toute la page web, rendant la saisie des formulaires difficile.

### Qualité de code générale (négatif)
- [majeur] : Violation des conventions temps-réel embarquées (arithmétique flottante en ISR, absence de registres GPOS/GPOC atomiques) — impact : instabilité fondamentale du runtime.

---

## ⭐ Note globale : 3.8/10 — Défaillances critiques en interruption et conception électrique risquée
Malgré une fréquence d'échantillonnage de timer séduisante (10 µs), ce firmware effectue des divisions flottantes en ISR, utilise des registres non atomiques et préconise un câblage d'alimentation dangereux pour le microcontrôleur.

## ⭐ Note qualité de code : 4.5/10 — Problèmes structurels temps-réel
Présentation soignée en apparence, mais méconnaissance des contraintes bas niveau du processeur ESP8266 (FPU absente, registre GPIO atomique ignoré, allocations dynamiques excessives).
