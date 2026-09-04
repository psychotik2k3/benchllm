# Analyse — gpt-free

**Sources analysées** : `competitors/gpt-free/gpt_deye_fan.ino`

---

## ✅ Points techniques positifs

### Temps-réel / ISR
- Utilisation de la bibliothèque interne du core ESP8266 `core_esp8266_waveform.h` avec `startWaveformClockCycles()` et `stopWaveform()` pour générer les signaux carrés de sortie. Ce générateur matériel délègue la commutation des broches au sous-système de forme d'onde du core Arduino sans surcharger la boucle `loop()`.
- Captures des impulsions d'entrée par interruptions GPIO matérielles (`attachInterrupt` sur front `FALLING`) minimales en IRAM.

### Démarrage / Boot readiness
- Broches initialisées à l'état bas dès le démarrage pour bloquer les transistors de sortie.
- Waveform démarrée avant l'activation du réseau WiFi.

### Fidélité au protocole tachymétrique / Fail-safe
- Règle des 2 impulsions par tour appliquée (`(60000000ULL / periodUs) / 2`).
- Timeout d'inactivité fixé à 500 ms (`TIMEOUT_US = 500000`) : arrêt automatique de la forme d'onde si le ventilateur s'arrête.

### Filtres / Signal
- Filtrage passe-bas IIR sur la période mesurée :
  ```cpp
  periodUs = (filteredPeriodUs * 7 + instPeriodUs) / 8;
  ```
- Anti-rebond d'entrée rejetant les périodes inférieures à 2 500 µs (> 12 000 tr/min).

### WiFi / Réseau
- Mode `WIFI_AP_STA` simultané.
- Désactivation de la veille modem (`WiFi.setSleepMode(WIFI_NONE_SLEEP)`).

### Gestion mémoire / HEAP
- Interface web embarquée en Flash via `PROGMEM`.
- Buffer JSON pré-dimensionné avec `reserve(512)`.

### Interface Web
- Interface web soignée, thème sombre, polling asynchrone toutes les 500 ms via `/status`.

---

## ❌ Points techniques négatifs

### Conception électronique (critique)
- [critique] : Erreur de conception matérielle rédhibitoire dans les commentaires d'en-tête du code :
  Le fichier recommande pour l'entrée tachymétrique :
  > *"Utiliser le diviseur : TACH NOCTUA -> 10k -> GPIO -> 3.3k -> GND"*
  Le fil tachymétrique d'un ventilateur Noctua est une sortie en **collecteur ouvert** (transistor NPN sans alimentation interne). Un collecteur ouvert ne délivre aucune tension : il ne fait que relier la ligne à la masse lors d'une impulsion. Si l'on branche un pont diviseur passif sans aucune résistance de tirage (pull-up) raccordée à une source de tension positive (3,3V ou 5V), la ligne reste constamment à 0V ! L'ESP8266 ne recevra jamais la moindre impulsion — impact : circuit d'entrée strictement inopérant, impossibilité absolue de mesurer la vitesse des ventilateurs.

### Documentation hardware/code (critique)
- [critique] : Absence totale de fichier de schéma électronique dédié (aucun document Markdown ou image). Seules quelques lignes de commentaires figurent en en-tête du fichier source — impact : aucune indication sur l'alimentation (ni OU-diode, ni buck DC/DC, ni filtrage réservoir).

### Persistance / Stockage (majeur)
- [majeur] : L'intégrité de l'EEPROM ne comporte aucun calcul de checksum ni de CRC : elle se contente d'un simple mot magique (`0xD3E1`) — impact : risque de charger des valeurs altérées en cas d'écriture interrompue.

### Qualité de code générale (négatif)
- [mineur] : L'utilisation de l'en-tête interne non public `core_esp8266_waveform.h` peut poser des problèmes de compatibilité ascendante lors de mises à jour majeures du core Arduino ESP8266 — impact : fragilité de la chaîne de compilation.

---

## ⭐ Note globale : 4.5/10 — Code logiciel bien écrit mais schéma d'entrée matériellement impossible
Le firmware présente un bon niveau de finition logicielle et une idée intéressante d'utiliser le générateur de forme d'onde du core ESP8266, mais la préconisation d'un pont diviseur passif sans pull-up sur un collecteur ouvert Noctua démontre une incompréhension physique fondamentale du composant.

## ⭐ Note qualité de code : 6.0/10 — Bonne présentation logicielle
Code propre, bien commenté et agréable à lire, mais amputé de toute documentation matérielle et dépendant d'un en-tête privé du core.
