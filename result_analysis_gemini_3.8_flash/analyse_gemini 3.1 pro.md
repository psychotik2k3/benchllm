# Analyse — gemini 3.1 pro

**Sources analysées** : `competitors/gemini 3.1 pro/deye_fan.ino`, `competitors/gemini 3.1 pro/schema_et_explications.md`

---

## ✅ Points techniques positifs

### Temps-réel / ISR
- Timer1 matériel utilisé en mode périodique (`TIM_DIV16`, `TIM_EDGE`, `TIM_LOOP`) cadencé à 50 µs (`TICKS_PER_50US = 250`).
- Basculement des sorties GPIO dans l'ISR via les registres matériels atomiques `GPOS` et `GPOC`.
- Comptage d'impulsions d'entrée par interruptions matérielles GPIO (`RISING`) minimales.

### Démarrage / Boot readiness
- Broches initialisées à l'état bas au démarrage.
- Timer1 démarré dès le `setup()`.

### Génération sortie / Timer1
- Décompte de demi-période par décrémentation de pas de 50 µs dans `onTimerISR()`.

### Fidélité au protocole tachymétrique / Fail-safe
- Règle de calcul 2 impulsions par tour appliquée.
- En cas de perte de signal (0 RPM mesuré), le flag de sortie est désactivé et la broche ramenée à l'état bas (transistor bloqué, niveau haut côté onduleur).

### WiFi / Réseau
- Mode `WIFI_AP_STA` simultané.
- Endpoints de statut JSON et de configuration par formulaire POST.

---

## ❌ Points techniques négatifs

### Temps-réel / Régulation (critique)
- [critique] : La vitesse d'entrée (RPM) n'est pas mesurée par période inter-fronts (en microsecondes), mais par comptage d'impulsions rafraîchi **une seule fois par seconde** dans `loop()` :
  ```cpp
  if (now - lastRpmCalc >= 1000) {
      uint32_t p9 = pulses9;
      pulses9 = 0;
      rpm9 = p9 * 30;
      ...
  }
  ```
  Ce choix technique introduit une latence inacceptable de 1 000 ms sur la détection de toute variation de régime du ventilateur. Lors d'une montée rapide en charge de l'onduleur, la simulation reste figée pendant 1 seconde, augmentant considérablement le risque que l'onduleur Deye détecte une anomalie de ventilation et se mette en défaut — impact : asservissement tachymétrique incapable de suivre les accélérations dynamiques des ventilateurs.

### Documentation hardware/code (critique)
- [critique] : Dans `schema_et_explications.md` (§3.1), l'auteur suggère d'utiliser des diodes petits signaux **1N4148** pour réaliser le OU-diode de l'alimentation 12V. La diode 1N4148 a un courant direct continu maximal de 150 à 200 mA. Lors des pointes d'émission du module WiFi de l'ESP8266 (courant crête pouvant dépasser 350 à 400 mA), les diodes 1N4148 seront détruites par surintensité thermique — impact : risque immédiat de panne d'alimentation et de destruction de composant.

### Persistance / Stockage (majeur)
- [majeur] : La structure de configuration stockée en EEPROM ne comporte aucun mécanisme de contrôle d'intégrité (aucun CRC, ni somme de contrôle). Elle ne vérifie qu'un mot magique d'initialisation (`0xA5`) — impact : risque d'appliquer des ratios corrompus ou invalides suite à une écriture partielle.

### Gestion mémoire / HEAP (majeur)
- [majeur] : Le gestionnaire de page web `handleRoot()` procède à une copie complète de la chaîne `INDEX_HTML` (stockée en Flash) dans un objet dynamique `String` et effectue plusieurs appels `.replace()` :
  ```cpp
  String html = FPSTR(INDEX_HTML);
  html.replace("%RATIO9%", String(config.ratio9, 2));
  ```
  Cette opération alloue et réalloue continuellement de volumineux blocs de mémoire sur le tas, provoquant une fragmentation rapide de la RAM sur ESP8266 — impact : instabilité mémoire et plantage potentiel après plusieurs consultations de la page.

### Filtres / Signal (majeur)
- [majeur] : Aucun filtre anti-rebond matériel ou logiciel n'est implémenté sur les entrées d'interruption : le moindre parasite haute fréquence est comptabilisé comme une impulsion de rotation complète — impact : surestimation erratique de la vitesse du ventilateur en environnement parasité.

---

## ⭐ Note globale : 4.8/10 — Temps-réel trop lent et documentation électronique dangereuse
L'utilisation de registres atomiques et du Timer1 matériel est positive, mais mesurer les RPM à une fréquence d'une seule fois par seconde détruit toute réactivité dynamique, et préconiser des diodes 1N4148 pour alimenter un ESP8266 est une erreur d'ingénierie majeure.

## ⭐ Note qualité de code : 5.5/10 — Code standard mais simpliste
Code compréhensible mais reposant sur des allocations dynamiques `String` non maîtrisées et une gestion rudimentaire de l'EEPROM.
