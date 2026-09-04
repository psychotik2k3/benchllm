# Analyse — grok 4.6 medium

**Sources analysées** : `competitors/grok 4.6 medium/deye_fan/deye_fan.ino`, `competitors/grok 4.6 medium/SCHEMA_ELECTRONIQUE.md`

---

## ✅ Points techniques positifs

### Temps-réel / ISR
- Dérivation du Timer1 matériel vers le vecteur NMI (`ETS_FRC_TIMER1_NMI_INTR_ATTACH(nmiTachIsr)`) avec fonction d'attachement `NmiTimSetFunc()`.
- ISR NMI exécutée en RAM (`IRAM_ATTR`), immunisée contre les coupures d'interruption du WiFi et du système de fichiers.
- Commutation des sorties par écriture directe dans les registres matériels `GPOS` et `GPOC`.
- Absence complète de calculs en virgule flottante sous interruption NMI.

### Démarrage / Boot readiness
- Broches initialisées à l'état bas (`digitalWrite(..., LOW)`) avant la configuration en sortie.
- Moteur NMI activé dès `setup()` avant la connexion WiFi.

### Génération sortie / Timer1
- Timer1 cadencé en mode boucle automatique (`TIM_LOOP`) à 50 kHz (`NMI_PERIOD_TICKS = 100` à 5 MHz avec diviseur 16), soit un tick de 20 µs.
- Décompte déterministe d'intervalles par demi-période entière (`halfPeriodTicks`).

### Fidélité au protocole tachymétrique / Fail-safe
- Règle des 2 impulsions par tour appliquée (`PULSES_PER_REV = 2`).
- Timeout d'inactivité fixé à 400 ms (`STALL_TIMEOUT_US = 400000UL`) : extinction automatique de la simulation et relâchement de la ligne au repos en cas d'arrêt du ventilateur.
- Bornage des ratios entre 1,0 et 5,0.

### WiFi / Réseau
- Mode `WIFI_AP_STA` simultané.
- Désactivation de la persistance flash du WiFi (`WiFi.persistent(false)`) préservant la mémoire flash des cycles d'écriture répétés.

### Gestion mémoire / HEAP
- Interface web embarquée en Flash via `PROGMEM`.
- Endpoints JSON sérialisés dans des tampons de taille fixe.

### Interface Web
- Interface graphique soignée avec thème sombre, actualisation AJAX toutes les 500 ms via `/api/status`, et badges d'état clairs.

### Persistance / Stockage
- Persistance en EEPROM protégée par un mot magique (`0xDEADF00D`) et un calcul d'intégrité par CRC32 complet (`calculateCRC32`).

### Documentation hardware/code
- Document `SCHEMA_ELECTRONIQUE.md` très satisfaisant :
  - Recommandation claire d'instrumenter un seul ventilateur par paire (alerte explicite contre la mise en parallèle de deux lignes tachymétriques).
  - Synoptique d'alimentation avec OU-diode Schottky (1N5819/SS14), condensateur réservoir de 1 000 µF, et convertisseur buck externe MP1584/Mini360 réglé à 5,0 V.
  - Résistances de pull-up d'entrée de 4,7 kΩ vers le rail 3,3 V de l'ESP8266 et étages de sortie en collecteur ouvert avec transistors NPN 2N2222/2N3904.

### Qualité de code générale (positif)
- **Structure / organisation** : Code structuré, lisible et bien segmenté.
- **Lisibilité / nommage** : Identifiants explicites et constants bien hiérarchisées.
- **Commentaires / documentation inline** : Explications détaillées des contraintes de l'ESP8266.
- **Gestion d'erreurs / robustesse** : Validation des entrées HTTP avec `constrain()`.

---

## ❌ Points techniques négatifs

### Génération sortie / Timer1 (majeur)
- [majeur] : L'ISR NMI est cadencée à une fréquence fixe de 50 kHz (période de 20 µs). Contrairement à un ordonnanceur one-shot à haute résolution, chaque demi-période générée est un multiple discret de 20 µs. Pour un signal simulant 6 000 tr/min (200 Hz, demi-période de 2 500 µs), l'erreur de discrétisation reste faible, mais pour des régimes plus élevés, la quantification introduit une gigue de phase systématique de 20 µs — impact : gigue temporelle perceptible sur le signal simulé.

### Filtres / Signal (majeur)
- [majeur] : Absence de filtre de lissage (moyenne mobile ou EMA) sur les périodes mesurées : le calcul utilise la période brute mesurée entre les deux derniers fronts valides, transmettant instantanément les variations d'échantillonnage à la consigne de sortie.

### Temps-réel / Traitement d'entrée (majeur)
- [majeur] : Bug logique dans la capture tachymétrique `tachCapture()` :
  La variable d'horodatage `ch->lastEdgeUs = micros()` est mise à jour **avant** le contrôle de plausibilité du front (anti-rebond `dt < MIN_EDGE_CYCLES`). En conséquence, si un train de parasites haute fréquence frappe la broche d'entrée, `lastEdgeUs` est continuellement rafraîchi même si tous les fronts sont rejetés par le filtre, empêchant le timeout de décrochage (`STALL_TIMEOUT_US`) de s'activer — impact : risque de maintenir artificiellement la simulation active alors que le ventilateur s'est arrêté sous parasitage.

---

## ⭐ Note globale : 7.2/10 — Très bon projet avec NMI et solide documentation
Une solution de très bonne facture technique dotée d'une minuterie NMI robuste et d'un schéma électronique réaliste, diminuée par une quantification à 20 µs, l'absence de lissage de période et un défaut dans le rafraîchissement du timeout sous parasitage.

## ⭐ Note qualité de code : 7.4/10 — Code rigoureux et bien présenté
Code propre, bien encapsulé et doté d'un contrôle CRC32 irréprochable.
