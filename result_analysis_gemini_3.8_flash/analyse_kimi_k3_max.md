# Analyse — kimi_k3_max

**Sources analysées** : `competitors/kimi_k3_max/deye_fan.ino`, `competitors/kimi_k3_max/schema_electronique.md`

---

## ✅ Points techniques positifs

### Temps-réel / ISR
- Utilisation du Timer1 matériel FRC1 en mode one-shot (`TIM_DIV16`, `TIM_EDGE`, `TIM_SINGLE`), avec programmation dynamique sur échéances absolues en cycles CPU (`ESP.getCycleCount()`, résolution 12,5 ns).
- L'ordonnanceur calcule dynamiquement l'échéance la plus proche (`nearest`) parmi les canaux actifs et programme le rechargement matériel avec `timer1_write(ticks)`.
- Commutation instantanée des broches de sortie par accès direct aux registres matériels atomiques `GPOS` (mise à 1) et `GPOC` (mise à 0).
- Accumulation d'échéance absolue (`nextEdgeCycle += half`) interdisant toute dérive temporelle cumulative, couplée à un mécanisme de resynchronisation propre en cas de retard exceptionnel.
- Toutes les fonctions d'interruption sont placées en mémoire RAM d'instructions via `IRAM_ATTR`.
- Zéro calcul en virgule flottante sous interruption : l'ISR d'entrée ne fait que mesurer le delta de cycles CPU `now - lastEdgeCycle` et stocker la valeur.
- Section critique ultra-courte (< 1 µs) sous `noInterrupts()` / `interrupts()` lors du transfert des consignes depuis la boucle `loop()`.

### Démarrage / Boot readiness
- Sorties initialisées explicitement à l'état bas (`digitalWrite(..., LOW)`) avant la configuration en `pinMode(..., OUTPUT)` afin de garantir le blocage des transistors NPN dès le reset.
- Schéma préconisant des résistances physiques de 10 kΩ entre base et émetteur (R4) pour maintenir les transistors bloqués pendant les états de haute impédance du bootloader ESP8266.
- Timer1 armé uniquement dès qu'une voie devient active : aucun glitch ou train d'impulsions parasite émis vers l'onduleur au boot.

### Génération sortie / Timer1
- Découpage temporel à 5 MHz (ticks de 0,2 µs) offrant une finesse de cadencement remarquable.
- Plafond (`MAX_TIMER_TICKS = 8388607`, limite du compteur 23 bits) et plancher de garde-fou (`MIN_TIMER_TICKS = 100`, soit 20 µs) protégeant le matériel contre les saturations et les réarmements trop rapprochés.
- Relâchement immédiat de la ligne au repos (`GPOC`, transistor bloqué) dès qu'un canal devient inactif.

### Fidélité au protocole tachymétrique / Fail-safe
- Règle stricte des 2 impulsions par tour respectée (`rpmIn = 30 * F_CPU / period`).
- Timeout d'inactivité fixé à 1,5 s (`STALL_CYCLES`) : en l'absence de signal physique (ventilateur arrêté ou déconnecté), la sortie est coupée (`setOutput(i, 0)`), l'onduleur mesurant fidèlement 0 tr/min.
- Bornage défensif des ratios multiplicateurs entre 0,5 et 8,0 avec piège anti-NaN (`clampRatio`).

### Filtres / Signal
- Anti-rebond et filtre passe-bas d'entrée éliminant les impulsions parasites inférieures à 400 µs (`MIN_PULSE_CYCLES`), autorisant les régimes moteurs jusqu'à 75 000 tr/min à 2 PPR.
- Filtrage passe-bas exponentiel sur les périodes mesurées (\(\alpha = 0,25\) : `0.75f * smoothPeriod + 0.25f * period`) garantissant une consigne de sortie stable et progressive face à l'onduleur.

### WiFi / Réseau
- Mode `WIFI_AP_STA` simultané sans coupure du point d'accès de secours (`DeyeFanSim`).
- `WiFi.persistent(false)` pour éviter les écritures flash parasites du SDK Espressif.
- `WiFi.setSleepMode(WIFI_NONE_SLEEP)` désactivant la mise en veille du modem WiFi afin de supprimer les latences de réveil.
- Reconnexion automatique en mode Station toutes les 15 s sans blocage de la boucle applicative.
- Prise en compte immédiate des modifications de ratios et de configuration réseau **sans nécessiter de redémarrage**.

### Gestion mémoire / HEAP
- Interface web embarquée en mémoire Flash (`INDEX_HTML[] PROGMEM`).
- Zéro allocation dynamique lors de la génération de l'API de statut `/api/status` : emploi d'un buffer statique `char buf[360]` et de `snprintf`.
- Astuce de formatage des nombres à virgule : conversion préalable en entiers (`r10 / 10`, `r10 % 10`) évitant le recours au format `%f` de `snprintf` sur ESP8266.

### Interface Web
- Interface claire et responsive avec thème sombre, affichant en temps réel les régimes mesurés, simulés et les statuts par voie.
- Polling AJAX asynchrone toutes les secondes via `/api/status`.
- Mise à jour en ligne des ratios et du WiFi avec retour direct.

### Persistance / Stockage
- Persistance en EEPROM (512 octets) sécurisée par un mot magique de 4 octets (`0x44455945`, "DEYE").
- Contrôle d'intégrité par **CRC16-CCITT** complet (`0x1021`) sur l'ensemble de la structure.
- Restauration automatique de valeurs par défaut saines en cas d'EEPROM vierge ou corrompue.

### Documentation hardware/code
- Document `schema_electronique.md` exemplaire, concis et directement exploitable :
  - Avertissement explicite : instrumenter un seul ventilateur par paire (interdiction absolue du montage parallèle des collecteurs ouverts Noctua).
  - Étage d'alimentation : OU-diode Schottky 1N5819 (1 A) et convertisseur buck MP1584 réglé à 5,0 V alimentant la broche 5V du Wemos (bypasse la surchauffe de l'AMS1117).
  - Étage d'entrée : pull-up externe de 10 kΩ vers le 3,3 V de l'ESP8266 (tension adaptée au collecteur ouvert Noctua) + résistance série de 1 kΩ pour l'immunité aux transitoires ESD.
  - Étage de sortie : transistors NPN 2N2222 en collecteur ouvert avec résistances de base de 2,2 kΩ et pull-down de 10 kΩ base-émetteur.
  - Nomenclature (BOM) et table de câblage claires.

### Qualité de code générale (positif)
- **Structure / organisation** : Code structuré, modulaire et remarquablement équilibré entre bas niveau et services applicatifs.
- **Lisibilité / nommage** : Identifiants explicites, mise en page homogène et soignée.
- **Commentaires / documentation inline** : Explications détaillées sur le budget de gigue, l'isolation temps-réel et les calculs de conversion.
- **Gestion d'erreurs / robustesse** : Bornage systématique, résistance totale aux divisions par zéro et aux chaînes non terminées.
- **Respect des conventions C++/Arduino** : Respect rigoureux des contraintes de la plateforme Xtensa ESP8266.

---

## ❌ Points techniques négatifs

### Temps-réel / ISR (mineur)
- [mineur] : L'ordonnanceur Timer1 one-shot utilise le vecteur d'interruption standard du processeur (priorité 1). Bien que la gigue reste minime (< 10 µs en cas de burst WiFi du SDK) et imperceptible pour l'onduleur Deye, elle n'offre pas l'immunité absolue d'un vecteur NMI de niveau 3 (tel qu'implémenté par Claude Opus 5 Medium) — impact : micro-gigue résiduelle théorique lors de trafic WiFi intense.

### Régulation / Réactivité (mineur)
- [mineur] : La mise à jour de la demi-période simulée dans `updateChannel()` est cadencée toutes les 100 ms dans `loop()`. Lors d'une accélération brutale du ventilateur, la répercussion sur la sortie présente un temps de réponse pouvant atteindre 100 ms (ce qui reste toutefois largement inférieur au temps d'intégration de ~1 s de l'onduleur Deye) — impact : temps de réaction discret de 100 ms.

### Gestion mémoire (mineur)
- [mineur] : Dans `handleStatus()`, la variable locale `String staIp` effectue une petite allocation dynamique temporaire sur le tas lors de la lecture de `WiFi.localIP().toString()` — impact : allocation mémoire transitoire négligeable.

---

## ⭐ Note globale : 8.4/10 — Remarquable compétiteur, directement sur le podium
Une solution d'une très haute élégance technique associant un ordonnanceur FRC1 one-shot calé sur les cycles CPU, une persistance EEPROM avec CRC16, une interface web sans redémarrage et un schéma électronique irréprochable. Se place immédiatement en 2ᵉ position globale, juste derrière GPT-5.6 Sol Medium.

## ⭐ Note qualité de code : 8.6/10 — Excellente facture d'ingénierie
Code extrêmement propre, lisible, concis et parfaitement optimisé pour l'architecture Xtensa de l'ESP8266 sans complexité inutile.
