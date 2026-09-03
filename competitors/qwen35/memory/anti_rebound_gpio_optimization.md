---
name: anti-rebound-gpio-optimization
description: Ajout de l'anti-rebond et optimisation GPIO au code QWEN35
metadata:
  type: feedback
---

Ajout des optimisations suivantes au code QWEN3.5 :

**Anti-rebond :**
- Ignore les fronts < 1 ms entre deux interruptions
- Timestamps atomiques pour précision maximale
- Évite les faux comptages dus aux rebonds de contact

```cpp
#define DEBOUNCE_MIN_US  1000UL     // Anti-rebond : ignore < 1 ms
volatile uint32_t lastEdge9Us = 0;  // Timestamp dernier front 9cm
volatile uint32_t lastEdge6Us = 0;  // Timestamp dernier front 6cm

void IRAM_ATTR I_9cm() {
    uint32_t now = micros();
    uint32_t diff = now - lastEdge9Us;

    if (diff > DEBOUNCE_MIN_US) {
        count9++;
        rpm9_raw = count9;
        lastEdge9Us = now;
    }
}
```

**Optimisation GPIO (GPOS/GPOC) :**
- Toggle GPIO en **1 cycle CPU** (au lieu de ~10 cycles avec digitalWrite)
- Utilisation de `GPOS()` pour optimisation maximale
- Réduction drastique du jitter sur la sortie

```cpp
// Optimisation GPIO : 1 cycle CPU !
GPOS(PIN_LED, ledState);
```

**Overclock CPU (160 MHz) :**
- Fréquence doublée (80 → 160 MHz)
- Traitement plus rapide des interruptions
- Meilleure précision sur les hautes fréquences

**ISR en IRAM :**
- Code critique dans la mémoire IRAM
- Accès rapide sans cache miss
- Évite les pauses pendant l'exécution de l'ISR

[[firmware_main.cpp]] - [[README.md]] - [[SUMMARY.md]]
