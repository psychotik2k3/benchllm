# Analyse — kimi k3

**Sources analysées** : `competitors/kimi k3/deye_fan.ino`, `competitors/kimi k3/schema_electronique.md`  
*(Modèle Kimi K3 avec Thinking réglé sur High)*

---

## ✅ Points techniques positifs

### Temps-réel / ISR
- Capture d'entrée par interruptions GPIO matérielles sur front descendant (`FALLING`) minimales et placées en RAM d'instructions (`IRAM_ATTR`).
- Génération de sortie confiée au Timer1 matériel (FRC1) en mode one-shot réarmé à la demande (`TIM_DIV16`, `TIM_EDGE`, `TIM_SINGLE`), avec cadencement à 5 MHz (5 ticks/µs).
- Commutation directe des broches de sortie par accès aux registres matériels instantanés `GPOS` et `GPOC`.
- Suivi d'échéance absolue (`nextToggleUs += half`) évitant l'accumulation systématique de retard temporel.
- Aucune allocation dynamique, aucune chaîne `String` et aucun calcul flottant complexe au sein des interruptions.

### Démarrage / Boot readiness
- Sorties initialisées explicitement à `LOW` dès le début de `setup()`, avant l'initialisation de l'EEPROM et du WiFi, maintenant les transistors NPN bloqués.
- Timer1 et interruptions configurés avant le démarrage des services réseau.

### Génération sortie / Timer1
- Ordonnanceur one-shot à plus proche échéance gérant simultanément les deux canaux de manière asynchrone sur un unique timer matériel.
- Plancher anti-famine (`MIN_TIMER_TICKS = 30 µs`) et plafond de rechargement borné à 1 000 000 µs (5 000 000 ticks < \(2^{23}\)).

### Fidélité au protocole tachymétrique / Fail-safe
- Règle standard des 2 impulsions par tour appliquée (`rpm = 60000000.0f / (ema * 2)`).
- Canaux 9 cm et 6 cm traités de façon strictement indépendante avec des ratios dédiés.
- Timeout d'inactivité de 1,5 s (`TIMEOUT_US`) coupant la génération (`halfPeriodUs = 0`) et forçant le transistor au blocage (`GPOC`), l'onduleur détectant fidèlement l'arrêt.
- Bornage des ratios entre 0,5 et 10,0.

### Filtres / Signal
- Anti-rebond d'entrée éliminant les impulsions parasites inférieures à 300 µs (`MIN_PULSE_US`).
- Filtrage passe-bas exponentiel entier calculé dans l'ISR (`((ema * 3u) + dt) >> 2`, équivalent \(\alpha = 0,25\)) évitant toute arithmétique flottante sous interruption.

### WiFi / Réseau
- Mode `WIFI_AP_STA` simultané non bloquant avec publication mDNS (`http://deyefan.local`).
- `WiFi.persistent(false)` pour préserver la mémoire Flash des réécritures automatiques du SDK.

### Gestion mémoire / HEAP
- Page web stockée en Flash via `PROGMEM` (`PAGE_HTML[] PROGMEM`).
- Sérialisation JSON de statut via `snprintf` sur un tampon statique de taille fixe (`char buf[320]`).

### Interface Web
- Interface utilisateur responsive en thème sombre avec rafraîchissement asynchrone régulier par polling `/api/status`.
- Routes POST dédiées pour la mise à jour des ratios et de la configuration WiFi.

### Persistance / Stockage
- Sauvegarde en EEPROM (512 octets) sécurisée par un mot magique (`0xDEFA0123u`) et un contrôle d'intégrité par CRC16-CCITT.
- Validation des chaînes avec terminaison NUL forcée.

### Documentation hardware/code
- Document `schema_electronique.md` complet : OU-diode avec Schottky 1N5819 (1 A), module buck MP1584 réglé à 5,0 V alimentant la broche 5V du Wemos, condensateurs de filtrage (470 µF et 100 nF), et pull-up externe de 10 kΩ vers 3,3 V pour les tachymètres Noctua.

### Qualité de code générale (positif)
- **Structure / organisation** : Code lisible, concis et bien segmenté.
- **Lisibilité / nommage** : Utilisation de `constexpr`, structures de canaux claires.
- **Commentaires / documentation inline** : Invariants matériels et choix d'architecture bien explicités.
- **Gestion d'erreurs / robustesse** : Présence d'un timeout de silence et de vérification CRC.

---

## ❌ Points techniques négatifs

### Temps-réel / ISR & Démarrage (majeur)
- [majeur] : Au premier démarrage ou après une période d'inactivité, `next = now + 1000000u` arme le timer pour un sommeil passif d'une seconde. Lorsqu'un ventilateur commence à tourner, la boucle `loop()` met à jour `halfPeriodUs` mais ne réveille pas immédiatement le Timer1 : le premier front simulé peut ainsi subir une latence de démarrage atteignant près de 1 000 ms — impact : risque de détection d'anomalie de rotation par l'onduleur Deye lors des démarrages de ventilateur.
- [majeur] : Horodatage d'entrée avec `dt = now - ch[i].lastPulseUs` : au tout premier front reçu, `lastPulseUs` vaut 0. La différence `now - 0` est donc acceptée sans plafond supérieur et injectée dans l'EMA, faussant la période filtrée sur les premiers tours de rotation — impact : première estimation de RPM erratique au boot.

### Génération sortie / Timer1 (mineur)
- [mineur] : Horodatage basé sur `micros()` (résolution 1 µs) plutôt que sur le registre natif de cycles CPU (`ESP.getCycleCount()`, 12,5 ns). Bien que suffisant pour un tachymètre, la précision est moindre que celle de Kimi K3 Max ou Claude Opus 5 Medium.

### Interface Web (mineur)
- [mineur] : Dans le script JavaScript, la fonction périodique `tick()` réassigne inconditionnellement `i1.value = s.c1.ratio` et `i2.value = s.c2.ratio` chaque seconde. Si l'utilisateur est en train de saisir une nouvelle consigne au clavier, sa frappe est écrasée par le rafraîchissement AJAX — impact : ergonomie de saisie perturbée. *(Défaut corrigé dans Kimi K3 Max avec `if (document.activeElement !== f)`)*.

### Documentation hardware/code (mineur)
- [mineur] : Absence de résistance de pull-down base-émetteur (10 kΩ) sur les transistors de sortie NPN dans la nomenclature BOM. Pendant la phase de bootloader où les broches ESP8266 sont en haute impédance (Hi-Z), la base peut flotter légèrement — impact : risque de micro-glitch transitoire à l'allumage. *(Défaut corrigé dans Kimi K3 Max)*.

---

## 🔬 Comparaison : Kimi K3 (Thinking High) vs Kimi K3 Max

L'analyse comparative directe entre **Kimi K3** et **Kimi K3 Max** démontre une progression spectaculaire de rigueur d'ingénierie :
1. **Résolution temporelle** : Kimi K3 utilisait `micros()` (1 µs) ; Kimi K3 Max passe à `ESP.getCycleCount()` à 12,5 ns avec typage 100% entiers en cycles CPU.
2. **Latence de démarrage** : Kimi K3 laissait dormir Timer1 pendant 1 s lors des phases inactives ; Kimi K3 Max réarme instantanément le timer dès qu'une voie redevient active (`scheduleNext`).
3. **Sécurité matérielle** : Kimi K3 Max ajoute les résistances de pull-down 10 kΩ base-émetteur (R4) pour neutraliser tout état flottant au boot.
4. **Ergonomie Web** : Kimi K3 Max protège les champs de saisie active (`document.activeElement`) et supprime le format `%f` de `snprintf`.

---

## ⭐ Note globale : 7.6/10 — Très solide compétiteur, précurseur de Kimi K3 Max
Une architecture élégante, un Timer1 one-shot efficace et une excellente documentation électronique. Les quelques défauts de jeunesse sur la première mesure, la latence de réveil et l'écrasement de saisie web le séparent du score d'excellence de sa déclinaison Max.

## ⭐ Note qualité de code : 8.0/10 — Code propre, concis et structuré
Un code C++ très bien écrit, factorisé, attentif à la mémoire et exempt de dépendances inutiles.
