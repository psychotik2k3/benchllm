# Analyse — gpt-5.6 sol medium

**Sources analysées** : `competitors/gpt-5.6 sol medium/deye_fan.ino`, `competitors/gpt-5.6 sol medium/SCHEMA_ET_INSTALLATION.md`

---

## ✅ Points techniques positifs

### Temps-réel / ISR
- Utilisation du Timer1 matériel FRC1 en mode one-shot réarmé dynamiquement (`TIM_DIV16`, `TIM_SINGLE`), offrant une résolution d'horloge de 0,2 µs (5 MHz à 80 MHz).
- Ordonnanceur d'événements à échéance la plus proche (*earliest deadline scheduler*) avec suivi par delta de ticks (`targetTicks`), évitant toute gigue de quantification périodique.
- Manipulation des broches de sortie par accès direct aux registres d'écriture atomique :
  ```cpp
  GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, mask); // Mise à 1
  GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, mask); // Mise à 0
  ```
- Moteur temps-réel entièrement placé en IRAM (`IRAM_ATTR`), sans calculs en virgule flottante sous interruption.
- Capture d'entrée sur interruption matérielle `RISING` (prenant en compte l'inversion de l'étage adaptateur d'entrée).

### Démarrage / Boot readiness
- Broches de sortie initialisées à l'état bas avant la configuration matérielle, empêchant toute impulsion intempestive au reset.
- Moteur matériel Timer1 initialisé et opérationnel avant le démarrage de la pile réseau WiFi.
- Sécurisation matérielle documentée : résistances de pull-down de 47 kΩ entre base et émetteur sur les transistors de sortie pour garantir leur blocage pendant les phases de bootloader haute impédance.

### Génération sortie / Timer1
- Programmation des transitions exclusivement sur les frontières de demi-périodes, interdisant tout glitch ou créneau incomplet.
- Plancher de sécurité anti-écrasement (`MIN_HALF_PERIOD_TICKS = 250` ticks / 50 µs) et plafond de rechargement (`MAX_TIMER_TICKS = 0x7FFFFF`), protégeant le matériel contre les saturations du compteur 23 bits.

### Fidélité au protocole tachymétrique / Fail-safe
- Règle stricte des 2 impulsions par tour appliquée.
- Timeout d'inactivité de 2 000 ms (`SIGNAL_TIMEOUT_US = 2000000UL`) : en l'absence de front, le canal est déclaré inactif et la sortie est immédiatement désactivée (transistor bloqué, niveau haut délivré à l'onduleur).
- Bornage des ratios entre 1,0 et 4,0 avec pas de 0,05.

### Filtres / Signal
- Filtrage passe-bas exponentiel sur les périodes mesurées (\(\alpha = 0,25\) : `filteredPeriodUs * 0.75f + latestPeriod * 0.25f`), assurant une transition douce de consigne.
- Fenêtre de plausibilité stricte sur la période d'entrée : rejet systématique de toute impulsion inférieure à 500 µs (> 60 000 tr/min) ou supérieure à 1 500 000 µs (< 20 tr/min).

### WiFi / Réseau
- Mode `WIFI_AP_STA` simultané garantissant la disponibilité permanente de l'AP de configuration même en cas de panne du réseau local.
- Désactivation du mode d'économie d'énergie du modem WiFi (`WiFi.setSleepMode(WIFI_NONE_SLEEP)`).

### Gestion mémoire / HEAP
- Page web stockée en mémoire Flash via `PROGMEM`.
- Utilisation de fonctions d'échappement dédiées (`htmlEscape`, `jsonEscape`) et gestion rigoureuse des chaînes sans débordement de mémoire.

### Interface Web
- Interface utilisateur complète avec thème sombre professionnel, rafraîchissement asynchrone toutes les secondes via `/api/status`.
- Affichage dynamique de l'état des deux canaux (9 cm et 6 cm), avec formulaire sécurisé de mise à jour des paramètres et redémarrage logiciel contrôlé.

### Persistance / Stockage
- Persistance en EEPROM avec mot magique de 4 octets (`0x44465945` pour "DFYE"), versioning, et contrôle d'intégrité strict par CRC32 complet.
- Écriture déclenchée uniquement en cas de modification effective des paramètres.

### Documentation hardware/code
- Document `SCHEMA_ET_INSTALLATION.md` exceptionnel, sans conteste le plus complet du banc d'essai :
  - Distinction rigoureuse entre les deux ventilateurs de 9 cm et les deux ventilateurs de 6 cm.
  - Alerte explicite interdisant de câbler deux tachymètres en parallèle : consigne claire d'instrumenter un seul ventilateur par paire.
  - Section de puissance complète : OU-diode avec Schottky SS34 (3A / 40V), fusibles réarmables PTC (F1, F2 de 750 mA), diode TVS de protection transitoire SMBJ18A, condensateurs réservoirs de 470 µF, et convertisseur buck-boost avec verrouillage en sous-tension (UVLO).
  - Étage d'entrée adaptateur avec transistor NPN (2N3904/BC547) tolérant une plage d'entrée de 5V à 13,2V sans risque pour l'ESP8266.
  - Étage de sortie en collecteur ouvert avec transistor NPN BC337, résistance de base de 2,2 kΩ et pull-down de 47 kΩ.

### Qualité de code générale (positif)
- **Structure / organisation** : Encapsulation exemplaire sous espace de nommage (`namespace DeyeFan`), constantes déclarées avec `constexpr`, typage moderne.
- **Lisibilité / nommage** : Clarté absolue des identifiants et rigueur de mise en forme.
- **Commentaires / documentation inline** : Commentaires techniques denses détaillant chaque contrainte de registre et chaque choix de dimensionnement.
- **Gestion d'erreurs / robustesse** : Résistance totale aux données corrompues et aux déconnexions intempestives.
- **Respect des conventions C++/Arduino** : Respect scrupuleux des règles de l'art du C++ embarqué.

---

## ❌ Points techniques négatifs

### Interface Web (mineur)
- [mineur] : La composition de la réponse JSON dans `apiStatusHandler()` utilise des objets `String` temporaires au lieu d'un buffer statique brut — impact : allocation mémoire transitoire sur le tas, bien que sans risque d'emballement grâce à la taille maîtrisée de la charge utile.

### Temps-réel / ISR (mineur)
- [mineur] : L'utilisation du Timer1 FRC1 détourne l'horloge système standard utilisée par `tone()` et `Servo` — impact : impossibilité d'ajouter des modules PWM génériques sur la même minuterie, conséquence inhérente au choix d'un timer haute précision.

---

## ⭐ Note globale : 8.5/10 — Meilleure solution globale du banc d'essai
Le projet le plus abouti et le plus équilibré : un ordonnanceur one-shot FRC1 à 0,2 µs de résolution, une protection électronique de niveau industriel (fusibles PTC, TVS, buck-boost, clamp actif d'entrée) et une intégrité logicielle sans faille.

## ⭐ Note qualité de code : 8.5/10 — Excellente qualité d'ingénierie logicielle
Code moderne, propre, modulaire et hautement lisible, respectant parfaitement les contraintes temps-réel de l'ESP8266.
