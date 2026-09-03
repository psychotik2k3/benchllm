# 📊 Récapitulatif Global — Classement & Recommandations de Fusion

**Date d'analyse :** 2026-09-04  
**Projet cible :** Simulateur tachy­mètre ventilateurs Noctua pour onduleur Deye SUN-8K-SG05LP1-EU-AM2-P  
**Matériel cible :** LOLIN (WEMOS) D1 mini (ESP-12S / ESP8266)

---

## 1. Classement des 9 compétiteurs

| Rang | Compétiteur | Score Global | Score Code | Bugs critiques | Fichiers source analysés |
|:---:|-------------|:---:|:---:|:---:|---|
| 🥇 **1** | **deepseek4 flash** | **8,8** | 8,2 | ~1 | `deye_fan.ino` + `README.md` + `SCHEMA_ELECTRONIQUE.md` |
| 🥈 **2** | **claude-sonnet5-free** | **8,5** | 8,2 | ~2 | `deye_fan.ino` + `schema_electronique.md` |
| 🥉 **3** | **qwen3.6_prompt_from_claude** | **8,2** | 7,5 | ~1 | `deye_fan.ino` + `SCHEMATIC.md` |
| 4 | glm5.3 | 8,0 | 7,8 | 0 fonctionnels | `deye_fan/deye_fan.ino` + `README.md` + `schema_electronique.md` |
| 5 | gpt-free | 7,5 | 6,5 | 1 | `gpt_deye_fan.ino` (unique fichier) |
| 6 | qwen3.6 | 7,2 | 6,8 | 2 | `deye_fan.ino` + `SCHÉMA_CIRCUIT.md` |
| 7 | gemini-free | 5,3 | 5,0 | 2 | `gemini-code-1787264142602.cpp` (unique fichier) |
| 8 | qwen35 | 4,8 | 3,2 | 3 | `firmware_main.cpp` + `README.md` + docs internes |
| 9 | gemma4 | 3,2 | 3,5 | 6+ | `firmware_main.cpp` + `README.md` |

### Notes par rang

- **🥇 deepseek4 flash (8,8/10)** — NMI FRC1 avec registres bare-metal → immunité WiFi absolue. Buffer JSON sur pile (`snprintf_P`), CRC8 intégral EEPROM, reconnexion STA non-bloquante. Le seul point faible : passwords exposés en clair dans l'API JSON.
- **🥈 claude-sonnet5-free (8,5/10)** — Timer1 TIM_LOOP 10kHz, GPOS/GPOC directs, checksum EEPROM pondéré par indice, architecture explicitement documentée dans l'entête du fichier. Pénalisé par absence de filtrage temporel et reconnexion WiFi inexistante.
- **🥉 qwen3.6_prompt_from_claude (8,2/10)** — Version améliorée de qwen3.6 : correction `digitalWrite()` → GPOS/GPOC, triple filtrage EMA, OTA + mMDNS inclus, JSON zero-allocation GET. Ticker @20kHz en software au lieu de hardware timer direct.
- **4 glm5.3 (8,0/10)** — Timer FRC1 + CCOUNT, buffer circulaire 128 entrées, EEPROM+CRC32, mDNS activé, aucun bug fonctionnel, zéro bibliothèque externe. Points faibles : pas d'IRAM_ATTR explicite sur la sortie ISR, mesure sans `noInterrupts()` pour copie tampon 512 octets.
- **5 gpt-free (7,5/10)** — Meilleure structure de code propre (`TachChannel` réutilisable), filtre IIR correct sur périodes cycliques, CCOUNT en assembly inline, mot de passe AP valide WPA2. Dépendance externe bloquante (`core_esp8266_waveform.h`), String concatenation JSON à 2 req/s.
- **6 qwen3.6 (7,2/10)** — Triple-barrière documentée (ISR entrée → Timer1 sortie → loop web), Timer1 matériel dédié, EEPROM validation magique, design dark theme. Bugs : `digitalWrite()` dans ISR Timer1 + dangling `EEPROM.end()`.
- **7 gemini-free (5,3/10)** — StaticJsonDocument anti-fragmentation, LittleFS JSON persistant, 3 ISRs IRAM_ATTR. Bugs : `digitalWrite()` dans ISR Timer1, résolution timer 50 µs → erreur ±2,5% haute RPM, **aucune documentation hardware**.
- **8 qwen35 (4,8/10)** — IRAM_ATTR, anti-rebound 1 ms, Web UI fonctionnelle, documentation exceptionnelle. Bugs : `GPOS()` non défini → compilation bloquante, sortie à 1 Hz fixe indépendamment du RPM, `WiFi.transmitting()` n'existe pas.
- **9 gemma4 (3,2/10)** — Squelette web + ISR correct mais la fonctionnalité principale (génération signal sur D5) **n'est pas implémentée du tout**. Ratios Web UI inopérants, aucune persistance, String concatenation → fragmentation HEAP.

---

## 2. Meilleures pratiques par catégorie

| Catégorie | Meilleur compétiteur | Ce qu'il faut retenir |
|-----------|---------------------|----------------------|
| **1. ISR / Temps-réel** | **deepseek4 flash** | NMI FRC1 + registres bare-metal (W1TS/W1TC, event-calendar) — immunité absolue aux sections critiques WiFi |
| **2. Démarrage / Boot** | **glm5.3** | Fallback AP intelligent + mailbox ISR↔loop sans rupture de phase + EEPROM CRC32 double validation |
| **3. Génération sortie** | **deepseek4 flash** | NMI avec event-calendar + XOR différé W1TS/W1TC — zéro jitter, zéro dépendance noyau |
| **4. Protocole / Fail-safe** | **glm5.3** | Mode cache-panne par canal (RPM fixe configurable en cas de perte signal) + stall detection adaptatif |
| **5. Filtres / Signal** | **qwen3.6_prompt_from_claude** | Triple lissage : moyenne glissante 4 échantillons + double EMA (α=0,3 + α=0,5) sur périodes cycliques |
| **6. WiFi / Réseau** | **deepseek4 flash** | Reconnexion périodique non-bloquante (20s dans loop) + mDNS activé + hostname configuré |
| **7. Gestion mémoire / HEAP** | **deepseek4 flash** | Buffer JSON sur pile (`char[]` + `snprintf_P` avec PSTR en flash) — ZÉRO allocation heap par requête |
| **8. Interface Web** | **qwen3.6_prompt_from_claude** | Sliders AJAX temps-réel + masquage password dans API JSON + reboot différé non-bloquant 2s |
| **9. Persistance / Stockage** | **deepseek4 flash** | CRC8 sur intégralité structure EEPROM — détection de toute corruption partielle, bien supérieur au magic number seul |
| **10. Documentation hardware** | **claude-sonnet5-free / qwen3.6_pFC** | Schémas bloc ASCII + BOM complète 15+ composants + calculs de dimensionnement par sous-circuit |
| **11. Qualité code générale** | **gpt-free** | Structure `TachChannel` réutilisable, IIR correct sur périodes cycliques, constantes `constexpr`, commentaires inline mathématiques |

---

## 3. Architecture idéale par fusion

Si on compile le meilleur firmware à partir des 9 compétiteurs, voici les choix optimaux :

### Cœur temps-réel (ISR + génération sortie)

| Choix technique | Source recommandée | Raison |
|----------------|-------------------|--------|
| **NMI FRC1** pour ISR de sortie | deepseek4 flash | Un NMI ne peut pas être masqué par WiFi — seule approche offrant immunité absolue |
| Registres **bare-metal W1TS/W1TC** | deepseek4 flash | Adresses datasheet directes (`0x60000308UL`, `0x60000304UL`) — zéro dépendance noyau Arduino |
| **EMA α≈0,2** (IIR) sur périodes mesurées | glm5.3 / qwen3.6_pFC | Lissage réaliste : ni trop lent (α=1/8 = 8 mesures), ni trop agressif (α=0,5). Constante de temps ~5 fronts |
| **Buffer circulaire** timestamps entrée | glm5.3 | Mesure insensible à la charge réseau — 128 entrées protègent même WiFi intense |
| Calcul fixe-point Q16 pour ratio × RPM | deepseek4 flash | Zéro flottant dans les canaux temps-réel — `ratioToQ16()` → `(perUs * q16_ratio) >> 16` |
| Désactivation modem sleep WiFi | claude-sonnet5-free | `WIFI_NONE_SLEEP` — élimine les rafales RF périodiques qui créent du jitter |

### Persistance & Configuration

| Choix technique | Source recommandée | Raison |
|----------------|-------------------|--------|
| **EEPROM + CRC8** sur structure complète | deepseek4 flash | Le CRC8 couvre tous les bytes excepté le checksum lui-même — détecte toute corruption partielle |
| Structure typée avec tailles protégées | deepseek4 flash / glm5.3 | `char staSsid[33]`, `char staPw[65]` — respect des limites WiFi, pas de débordement possible |
| Cooldown 5s entre écritures Flash | qwen3.6_pFC | Protection usure EEPROM (100 000 cycles max) + anti-abus client |
| Fallback automatique aux defaults | deepseek4 flash / claude-sonnet5-free | Si EEPROM corrompue → réécriture auto d'une config saine au boot |

### WiFi, Réseau & Mise à jour

| Choix technique | Source recommandée | Raison |
|----------------|-------------------|--------|
| **Reconnexion périodique non-bloquante** 20s | deepseek4 flash | Signal simulé ne gèle PAS (contrairement à qwen3.6 qui bloque ~10s) |
| **mDNS activé + addService()** | glm5.3 / deepseek4 flash | `MDNS.begin("deye-fan")` + publication service HTTP — découverte Zeroconf réelle |
| **OTA intégré** (ArduinoOTA) | qwen3.6_pFC | Port 8266, mot de passe — mises à jour firmware sans câblage USB (essentiel en hauteur) |
| Overclock CPU 160 MHz | claude-sonnet5-free / qwen3.6_pFC | Marge de calcul WiFi/loop ×2, coût +15% conso acceptable pour appareil branché en continu |

### Interface Web & API REST

| Choix technique | Source recommandée | Raison |
|----------------|-------------------|--------|
| **Sliders AJAX temps-réel** + `fetch` POST | qwen3.6_pFC | Changement ratio immédiat sans recharger la page — UX fluide |
| **Masquage password** dans réponses API | qwen3.6_pFC | `"••••••••"` au lieu du plaintext — sécurité réelle si capture réseau |
| Reboot différé non-bloquant 2s | claude-sonnet5-free / qwen3.6_pFC | Flag `needRestart` traité dans loop() → garantie réponse TCP avant reboot |
| HTML en PROGMEM + JSON sur pile | deepseek4 flash + claude-sonnet5-free | ~5 KB SRAM économisée + zéro fragmentation HEAP |
| Validation bornée côté client ET serveur | deepseek4 flash | `clampU32()` côté serveur renforce les bornes HTML — defense in depth |

### Sécurité & Robustesse (à ajouter par fusion)

| Élément | Statut chez compétiteurs | Effort estimé |
|---------|------------------------|---------------|
| **Watchdog software** (`wdt_enable()`) | ❌ Absent de TOUS | ~3 lignes : setup + feed périodique dans loop |
| **Authentification web HTTP Basic** | ❌ Absent de TOUS | ~20 lignes : header `Authorization` sur `/save` et `/wifisave` |
| **Test d'interférences radio simultanées** | ❌ Non testé par aucun | Point de validation critique, non-codable dans le firmware |
| **LED heartbeat / statut système** | ⚠️ Incomplet chez plusieurs | Afficher RSSI STA et mode AP/STA visible dans HTML |

---

## 4. Tableau comparatif final (vue synthèse)

| Aspect | gemma4<br>3,2 | qwen35<br>4,8 | qwen3.6<br>7,2 | gemini-free<br>5,3 | gpt-free<br>7,5 | qwen3.6_pFC<br>8,2 | glm5.3<br>8,0 | deepseek4 flash<br>**8,8** | claude-sonnet5-free<br>**8,5** |
|--------|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **ISR entrée IRAM_ATTR** | ❌ | ✅ | ✅ | ✅ | ✅ | ✅ | ⚠️ | ✅ | ✅ |
| **Écriture sortie ISR** | N/A (jamais) | GPOS undef | `digitalWrite()` CRITIQUE | `digitalWrite()` CRITIQUE | External header | GPOS/GPOC SDK | Noeau-attaché | **Registres bruts dsheet** | GPOS/GPOC SDK |
| **Immunité WiFi ISR** | N/A | Timer normal | HW timer isolé | Waveform matériel | External hdr | Software Ticker | FRC1 non-NMI | **NMI — immunité absolue** | Timer1 TIM_LOOP |
| **Signal proportionnel** | ❌ | ⚠️ 1 Hz fixe | ✅ Proportionnel | ✅ Proportionnel | ✅ Proportionnel | ✅ Proportionnel | ✅ Proportionnel | ✅ Proportionnel | ✅ Proportionnel |
| **Filtrage temporel** | ❌ | ⚠️ faux | Anti-rebound | ❌ | IIR 7/8+1/8 | Triple EMA réel | EMA α=0,25 | EMA α=0,125 | **❌ Aucun** |
| **Mémoire HEAP (JSON)** | String concat | String 10 Hz | OK | StaticJsonDocument | String concat | Mixte DynamicJson | Core pur (zéro lib) | **Pile + snprintf_P** | Pile + snprintf |
| **Persistance** | ❌ | SPIFFS cassé | EEPROM magic | LittleFS JSON | EEPROM magic | LittleFS JSON | EEPROM + CRC32 | **EEPROM + CRC8 intégral** | EEPROM + checksum pondéré |
| **mDNS** | ❌ | ❌ | ❌ | ❌ | ❌ | Inactif | ✅ Actif | ✅ Actif | ❌ Absent |
| **OTA** | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ Intégré | ❌ 0 libs | ❌ Absent | ❌ Absent |
| **Docs hardware** | Correcte | Exceptionnelle | Exceptionnelle | Aucune | Limitée | Exceptionnelle | Exceptionnelle | Exceptionnelle | Exceptionnelle |
| **Reconnexion WiFi** | Jamais | Bloquante 10s | Bloquante 10s | Jamais | Jamais | Non-bloquante 30s | Périodique 20s | Périodique 20s | ❌ Jamais |
| **Mot de passe AP** | `12345678` | `12345678` | Ouvert (`""`) | `12345678` | `DeyeFan123` ✅ | `deyetach1` ✅ | N/A | `deye-fan` | `changeme123` |
| **Passwords API clair** | ❌ | N/A | N/A | N/A | N/A | Masqué ✅ | N/A | **En clair** ❌ | N/A |

---

## 5. Verdict final

### Vainqueur : deepseek4 flash (8,8 / 10)

deepseek4 flash est le seul compétiteur à avoir implémenté un **NMI FRC1 avec accès directs aux registres bare-metal** du ESP8266, garantissant une immunité totale aux interférences WiFi sur la génération de signal. Son approche zéro-allocation heap (pile + `snprintf_P`), son CRC8 intégral EEPROM, et sa reconnexion STA non-bloquante en font un firmware temps-réel de qualité professionnelle. La documentation hardware complète (schémas, BOM, calculs) est également au plus haut niveau.

### Points communs à corriger chez TOUS les compétiteurs

| Manquement | Impact | Effort |
|-----------|--------|--------|
| Aucun watchdog SW/HW | Pas de reboot auto en cas de crash WiFi ou boucle infinie | ~3 lignes de code |
| Aucune authentification web | N'importe qui sur le réseau peut modifier ratios et WiFi | ~20 lignes (HTTP Basic Auth) |
| Jamais testé en conditions réelles avec interférences radio simultanées | Aucun ne garantit son comportement sous charge WiFi maximale | Validation externe required |

### Prochaine étape recommandée : compilation du firmware fusion

Prendre les meilleurs composants identifiés ci-dessus et les combiner dans un firmware unique avec :
1. NMI FRC1 (deepseek4 flash) pour la sortie
2. Triple EMA IIR (qwen3.6_pFC) sur périodes cycliques
3. EEPROM + CRC8 intégral (deepseek4 flash)
4. Reconnexion STA 20s non-bloquante (deepseek4 flash) + mDNS actif (glm5.3) + OTA (qwen3.6_pFC)
5. Buffer JSON pile + `snprintf_P` (deepseek4 flash)
6. Sliders AJAX + password masqué (qwen3.6_pFC)
7. **Watchdog software** — à ajouter de zéro
8. **HTTP Basic Auth** — à ajouter de zéro

---

*Document généré le 2026-09-04 à partir de l'analyse complète des 9 compétiteurs dans `competitors/`.*  
*Analyses individuelles sauvegardées dans `result_analysis/analyse_<competitor>.md`.*
