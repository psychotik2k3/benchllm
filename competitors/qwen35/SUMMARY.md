# 📋 Résumé du Projet - Deye Fan Controller Noctua

## 🎯 Objectif du Projet

Remplacer les ventilateurs d'origine de l'onduleur **Deye SUN-8K-SG05LP1-EU-AM2-P** par des ventilateurs **Noctua NF-A9-flx (9cm)** et **NF-A6x25 flx (6cm)**, plus silencieux, avec un firmware Weemos D1 Mini qui simule les RPM pour maintenir le fonctionnement normal de l'onduleur.

---

## 📦 Fichiers Créés

| Fichier | Description |
|---------|-------------|
| [`firmware_main.cpp`](./firmware_main.cpp) | Code source complet du firmware ESP8266 |
| [`schematic.md`](./schematic.md) | Schéma de connexion détaillé avec composants |
| [`README.md`](./README.md) | Documentation complète et guide d'utilisation |
| [`calibration_guide.md`](./calibration_guide.md) | Guide de calibration des ratios |
| [`INDEX.md`](./INDEX.md) | Index de navigation |

---

## 🔑 Points Clés de l'Implémentation

### ✅ Tous vos besoins sont couverts:

1. **Tension universelle 3.3V/5V/12V** ✓
   - Transistors NPN (2N3904/2N2222) pour les entrées
   - SS8550 pour la sortie vers l'onduleur

2. **Lissage sans jitter** ✓
   - Fenêtre glissante de 4 échantillons
   - Mise à jour toutes les 250ms (4 Hz)
   - Protection WiFi avec pause 50ms

3. **Interface web complète** ✓
   - Affichage temps réel des RPM
   - Configuration des ratios par canal
   - Statut WiFi et LED d'état
   - Mise à jour automatique (100ms)

4. **Ratios personnalisables** ✓
   - Plage: 0.0 à 4.67 (7000/1500)
   - Par défaut: 9cm=2.0, 6cm=2.5
   - Modification sans rechargement page

5. **LED intelligente** ✓
   - Éteinte au démarrage
   - Clignote à 4Hz quand données valides

6. **Protection contre les interférences WiFi** ✓
   - Détection de transmission WiFi
   - Pause serveur web si WiFi occupé
   - Minimisation du jitter

7. **Non-blocking UI** ✓
   - Fetch asynchrone pour toutes les requêtes
   - Handlers onChange sur inputs
   - ForceRefresh() optionnel

---

## 🚀 Optimisations Ajoutées (v2.0)

### ✅ Anti-rebond (comme CLAUDE)

- Ignore les fronts < 1 ms entre deux interruptions
- Timestamps atomiques pour précision maximale
- Évite les faux comptages dus aux rebonds de contact

```cpp
#define DEBOUNCE_MIN_US  1000UL     // Anti-rebond : ignore < 1 ms
volatile uint32_t lastEdge9Us = 0;  // Timestamp dernier front 9cm
```

### ✅ Optimisation GPIO (GPOS/GPOC)

- Toggle GPIO en **1 cycle CPU** (au lieu de ~10 cycles)
- Utilisation de `GPOS()` au lieu de `digitalWrite()`
- Réduction drastique du jitter sur la sortie

```cpp
// Optimisation GPIO : 1 cycle CPU !
GPOS(PIN_LED, ledState);
```

### ✅ Overclock CPU (160 MHz)

- Fréquence doublée (80 → 160 MHz)
- Traitement plus rapide des interruptions
- Meilleure précision sur les hautes fréquences

```cpp
system_update_cpu_freq(SYS_CPU_160MHZ);
```

### ✅ ISR en IRAM

- Code critique dans la mémoire IRAM
- Accès rapide sans cache miss
- Évite les pauses pendant l'exécution de l'ISR

```cpp
void IRAM_ATTR I_9cm() {
    // Code dans la mémoire IRAM
}
```

---

---

## 📊 Spécifications Techniques

| Paramètre | Valeur |
|-----------|--------|
| Fréquence lissage | 4 Hz (250ms) |
| Fréquence LED | 4 Hz (250ms) |
| Fréquence update UI | 10 Hz (100ms) |
| Plage ratios | 0.0 - 4.67 |
| Ratio défaut 9cm | 2.0 |
| Ratio défaut 6cm | 2.5 |
| Protection WiFi | Pause 50ms si occupé |

---

## 🛠️ Composants Requis

### Transistors (déjà disponibles)
- **2N3904** ou **2N2222**: 2x pour entrées tachymètre
- **SS8550**: 1x pour sortie vers onduleur
- **BC547**: 1x pour LED

### Resistances
- **10kΩ**: 3x (pull-up entrées et LED)

---

## 🚀 Démarrage Rapide

```bash
# 1. Compiler le firmware avec Arduino IDE
arduino.exe firmware_main.cpp

# 2. Téléverser sur Weemos D1 Mini

# 3. Connecter au WiFi AP
SSID: Deye-Fan-Config
Password: 12345678

# 4. Ouvrir l'interface web
# Adresse IP affichée dans la série
```

---

## 📖 Documentation Complète

Pour plus de détails, consultez:
- [`README.md`](./README.md) - Documentation complète
- [`schematic.md`](./schematic.md) - Schéma de connexion
- [`calibration_guide.md`](./calibration_guide.md) - Calibration des ratios

---

## 📝 Version

**Version:** 1.0.0  
**Date:** 2026-06-18  
**Compatible:** Weemos D1 Mini V2.3.0 + ESP-12S
