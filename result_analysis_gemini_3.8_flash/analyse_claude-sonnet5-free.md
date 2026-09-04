# Analyse — claude-sonnet5-free

**Sources analysées** : `competitors/claude-sonnet5-free/deye_fan.ino`, `competitors/claude-sonnet5-free/schema_electronique.md`

---

## ✅ Points techniques positifs

### Temps-réel / ISR
- Utilisation du Timer1 matériel en mode réarmement automatique (`TIM_DIV16`, `TIM_EDGE`, `TIM_LOOP`) cadencé à 10 kHz (tick de 100 µs).
- ISR Timer1 minimale (`onTimer1ISR`) : décompte de compteurs entiers et basculement direct des broches par registres `GPOS` et `GPOC`, sans appel à `digitalWrite()`.
- Captures tach d'entrée par interruptions GPIO matérielles (`attachInterrupt` sur front `RISING`) minimales, horodatées avec `micros()`.
- Variables partagées marquées `volatile` et protégées par des sections critiques `noInterrupts()` / `interrupts()` lors des lectures/écritures composites.

### Démarrage / Boot readiness
- Sorties initialisées à l'état bas (`digitalWrite(..., LOW)`) avant le démarrage des timers, évitant l'activation parasite du transistor NPN au démarrage.
- Timer1 démarré avant l'initialisation du serveur web.

### Génération sortie / Timer1
- Génération périodique stable avec compte à rebours par pas de 100 µs indépendant de l'état de la boucle `loop()`.
- Transition propre : mise à zéro de la broche et forçage de l'état bloqué quand le canal devient inactif.

### Fidélité au protocole tachymétrique / Fail-safe
- Règle des 2 impulsions par tour respectée (`PULSES_PER_REV = 2`).
- Timeout de 3 000 ms (`TACH_TIMEOUT_MS`) : si aucun front n'est détecté, les RPM retombent à 0 et la sortie simulée est relâchée au repos (niveau haut côté onduleur).
- Bornage de sécurité sur la demi-période minimale (`MIN_OUT_HALF_PERIOD_100US = 10`, soit 1 ms demi-période / 500 Hz max).

### Filtres / Signal
- Anti-rebond d'entrée par rejet des périodes inférieures à 2 000 µs (`TACH_MIN_PERIOD_US`), ignorant le bruit électrique haute fréquence au-delà de 15 000 tr/min.

### WiFi / Réseau
- Configuration simultanée en mode `WIFI_AP_STA`.
- Désactivation de la mise en veille du modem WiFi (`WiFi.setSleepMode(WIFI_NONE_SLEEP)`) pour réduire les latences d'interruption.
- Overclock CPU à 160 MHz (`system_update_cpu_freq(SYS_CPU_160MHZ)`) pour maximiser la marge de calcul.

### Gestion mémoire / HEAP
- Page web stockée en mémoire flash dans `PAGE_MAIN[] PROGMEM`.
- Endpoints JSON `/status` et `/config` utilisant `snprintf` sur des buffers de taille fixe sur la pile.

### Interface Web
- Interface web claire, thème sombre, affichant en direct les RPM mesurés, simulés et l'état des canaux par des badges dynamiques.
- Formulaire pour ajuster les ratios de 0,1 à 10 et reconfigurer les réseaux STA et AP.

### Persistance / Stockage
- Sauvegarde en EEPROM (taille de la structure `Config`) avec mot magique `0xDEE5FA20UL`.

### Documentation hardware/code
- Schéma descriptif avec OU-diode Schottky, module buck 12V vers 5V, et filtrage réservoir.

### Qualité de code générale (positif)
- **Structure / organisation** : Découpage clair des blocs (ISR, régulation, serveur web, setup/loop).
- **Lisibilité / nommage** : Variables et constantes explicites, indentations régulières.
- **Commentaires / documentation inline** : Excellente introduction expliquant la stratégie d'isolation et ses limites physiques.
- **Gestion d'erreurs / robustesse** : Validation des bornes sur les ratios lors de la réception des requêtes HTTP.
- **Duplication / DRY** : Fonctions d'ISR d'entrée factorisées avec `handleTachEdge(uint8_t ch)`.
- **Respect des conventions C++/Arduino** : Utilisation rigoureuse de `volatile`, typage adapté (`uint32_t`, `int32_t`).
- **Complexité / maintenabilité** : Code simple à appréhender et maintenir.
- **Sécurité basique** : Contrôle de longueur de chaînes avec `strncpy`.
- **Testabilité** : Découplage clair de la logique de calcul `updateChannel()`.

---

## ❌ Points techniques négatifs

### Documentation hardware/code (critique)
- [critique] : Erreur conceptuelle majeure dans `schema_electronique.md` (§4) : le document affirme que le ventilateur Noctua possède une pull-up interne tirant vers le +12V et propose d'intercaler un transistor NPN inverseur piloté par la base via une résistance de 10 kΩ connectée au fil tach Noctua. Or, le tach Noctua est un pur collecteur ouvert (sans aucune tension interne) : sans pull-up vers le +12V ou +5V, ce transistor d'entrée ne conduira jamais et l'ESP8266 ne mesurera aucun front — impact : étage d'entrée matériel non fonctionnel tel que documenté.

### Génération sortie / Timer1 (majeur)
- [majeur] : La résolution du Timer1 est fixée à 100 µs (`TIMER1_TICKS_PER_ISR = 500` à 5 MHz) : à haute fréquence (ex: 350 Hz, demi-période de 1 428 µs), la quantification introduit une erreur discrète pouvant atteindre 7 % sur la fréquence de sortie — impact : gigue de quantification importante par rapport à un ordonnanceur one-shot à résolution fine.

### Filtres / Signal (majeur)
- [majeur] : Absence totale de filtrage de lissage (ni moyenne glissante, ni EMA) : la moindre variation instantanée mesurée sur une impulsion d'entrée est répercutée directement sur la consigne de sortie — impact : instabilité de la fréquence simulée perçue par l'onduleur Deye lors des micro-variations mécaniques du ventilateur.

### Persistance / Stockage (mineur)
- [mineur] : L'intégrité de l'EEPROM repose sur une simple somme pondérée `calcChecksum()` au lieu d'un algorithme robuste de type CRC32 — impact : détection moins fiable des corruptions partielles de mémoire flash.

### Qualité de code générale (négatif)
- [mineur] : La sauvegarde de configuration (`handleSave`) déclenche un redémarrage complet de l'ESP8266 (`ESP.restart()`) même pour une simple modification de ratio — impact : interruption inutile de la simulation tachymétrique vers l'onduleur.

---

## ⭐ Note globale : 5.8/10 — Conception logicielle correcte mais schéma matériel erroné
Firmware Arduino propre et bien articulé autour d'une ISR Timer1 cadencée à 10 kHz, mais lourdement pénalisé par une documentation matérielle erronée sur la nature du collecteur ouvert Noctua et l'absence de lissage du signal.

## ⭐ Note qualité de code : 7.5/10 — Code soigné et lisible
Code très bien structuré, clair et pédagogique, respectant les règles d'isolation des interruptions matérielles sur ESP8266.
