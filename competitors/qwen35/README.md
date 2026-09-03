# 🌬️ Deye Fan Controller - Noctua Edition

## 📋 Description du Projet

Ce firmware permet de simuler les RPM de ventilateurs Noctua NF-A9-flx (9cm) et NF-A6x25 flx (6cm) pour un onduleur Deye SUN-8K-SG05LP1-EU-AM2-P.

### 🎯 Objectifs

- ✅ Lecture des signaux tachymètre de ventilateurs Noctua
- ✅ Application de ratios personnalisables par canal
- ✅ Génération d'un signal RPM simulé compatible avec l'onduleur Deye
- ✅ Support universel 3.3V/5V/12V
- ✅ Interface web pour configuration temps réel
- ✅ Lissage des données pour éviter les approximations brutales
- ✅ Protection contre les interférences WiFi

---

## 📦 Composants Requis

### Hardware

| Composant | Quantité | Notes |
|-----------|----------|-------|
| Weemos D1 Mini (ESP-12S) V2.3.0 | 1 | Carte principale |
| Ventilateur Noctua NF-A9-flx | 1 | Canal 9cm |
| Ventilateur Noctua NF-A6x25 flx | 1 | Canal 6cm |
| Onduleur Deye SUN-8K-SG05LP1-EU-AM2-P | 1 | Chargeur |

### Transistors (déjà disponibles)

| Type | Quantité | Utilisation |
|------|----------|-------------|
| 2N3904 ou 2N2222 | 2 | Entrées tachymètre (9cm + 6cm) |
| SS8550 | 1 | Sortie vers onduleur Deye |
| BC547 | 1 | LED d'état |

### Resistances

| Valeur | Quantité | Utilisation |
|--------|----------|-------------|
| 10kΩ | 3 | Pull-up pour entrées et LED |

---

## 🔌 Schéma de Connexion

Voir [`schematic.md`](./schematic.md) pour le schéma détaillé.

### Résumé des connexions

```
Onduleur Deye (Pin TACH/RPM)
    │
    ├──┬── 10kΩ Pull-up ───┬── 2N3904/2N2222 (Entrée 9cm)
    │   │                   │
    │   │                   ├── Sortie vers onduleur
    │   │                   │
    │   │                   └── SS8550 ───┬── LED BC547
    │   │                                 │
    │   └─────────────────────────────────┴── GND
    │
    ├──┬── 10kΩ Pull-up ───┬── 2N3904/2N2222 (Entrée 6cm)
    │   │                   │
    │   │                   ├── Sortie vers onduleur
    │   │                   │
    │   │                   └── SS8550 ───┬── LED BC547
    │   │                                 │
    │   └─────────────────────────────────┴── GND
```

---

## 💻 Installation

### 1. Préparer l'environnement Arduino IDE

1. **Installer les bibliothèques nécessaires:**
   - ESP8266 Boards (Firmata)
   - ESP8266WebServer
   - Ticker

2. **Copier le firmware:**
   ```bash
   cp firmware_main.cpp /chemin/vers/votre/projet/
   ```

### 2. Configurer les pins dans Arduino IDE

Dans `Tools > Board`, sélectionner:
- **ESP8266 NodeMCU 1.0 (ESP-12E Module)** ou équivalent Weemos D1 Mini

### 3. Compiler et téléverser

```bash
arduino.exe
# Ou utiliser l'interface graphique
```

### 4. Optimisations activées dans le firmware

Le code QWEN35 inclut maintenant :

- ✅ **Anti-rebond** : Ignore les fronts < 1 ms entre deux interruptions
- ✅ **Optimisation GPIO** : GPOS/GPOC pour toggle en 1 cycle CPU
- ✅ **Overclock CPU** : Passage à 160 MHz pour hautes fréquences
- ✅ **ISR en IRAM** : Code critique dans la mémoire IRAM
- ✅ **Protection WiFi** : Détection des transmissions WiFi

---

## 🚀 Optimisations du Firmware (v2.0)

### Anti-rebond

```cpp
#define DEBOUNCE_MIN_US  1000UL     // Ignore les fronts < 1 ms
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

**Bénéfice :** Évite les faux comptages dus aux rebonds de contact.

### Optimisation GPIO (GPOS/GPOC)

```cpp
// Au lieu de : digitalWrite(PIN_LED, ledState);
// Utiliser : GPOS(PIN_LED, ledState);  // 1 cycle CPU !

void toggleLED() {
    if (dataValid9 || dataValid6) {
        ledState = !ledState;
        GPOS(PIN_LED, ledState);  // Optimisation GPIO
    }
}
```

**Bénéfice :** Toggle GPIO en 1 cycle CPU au lieu de ~10 cycles avec digitalWrite.

### Overclock CPU

```cpp
void setup() {
    system_update_cpu_freq(SYS_CPU_160MHZ);  // Passage à 160 MHz
    Serial.printf("[CPU] Overclocking à %u MHz\n", system_get_cpu_freq());
}
```

**Bénéfice :** Fréquence de traitement doublée (80 → 160 MHz).

### ISR en IRAM

```cpp
void IRAM_ATTR I_9cm() {
    // Code critique dans la mémoire IRAM
    // Accès rapide sans cache miss
}
```

**Bénéfice :** Évite les pauses pendant l'exécution de l'ISR.

---

---

## 🌐 Interface Web

### Accès à l'interface

1. **Connectez-vous au point d'accès WiFi:**
   - SSID: `Deye-Fan-Config`
   - Mot de passe: `12345678`

2. **Ouvrez le navigateur** sur l'adresse IP affichée dans la série

### Fonctionnalités de l'interface

#### 📊 Affichage temps réel

- RPM réel des deux ventilateurs
- RPM simulé par canal
- RPM simulé total (pour l'onduleur)
- Statut WiFi
- Indicateurs de validité des données

#### ⚙️ Configuration

**Ratios:**
- Canal 9cm: `0.0 - 4.67` (défaut: `2.0`)
- Canal 6cm: `0.0 - 4.67` (défaut: `2.5`)

**WiFi:**
- Configuration du réseau STA (optionnel)
- Vérification de la connexion

#### 🔄 Mise à jour automatique

- Fréquence: **100ms** (10 fois/seconde)
- Lissage: **250ms** (4 échantillons)
- Protection contre les refresh interférents

---

## 🔧 Paramètres du Firmware

### Ratios par défaut

| Canal | Ratio | Exemple |
|-------|-------|---------|
| 9cm | 2.0 | 1500 RPM → 3000 simulé |
| 6cm | 2.5 | 1200 RPM → 3000 simulé |

### Plage de ratios

- **Minimum:** `0.0`
- **Maximum:** `4.67` (7000/1500)

### Fréquences d'échantillonnage

| Fonction | Fréquence | Délai |
|----------|-----------|-------|
| Lissage RPM | 4 Hz | 250ms |
| LED clignotement | 4 Hz | 250ms |
| Update WiFi | ~10 Hz | 100ms |
| Update interface | 10 Hz | 100ms |

---

## 📈 Comportement du Lissage

### Algorithme de lissage

```
RPM_lissé = (RPM_échantillon[0] + RPM_échantillon[1] 
             + RPM_échantillon[2] + RPM_échantillon[3]) / 4
```

### Gestion des transitions

- **Données valides:** Lissage activé
- **Pas de données:** RPM = 0
- **Retour des données:** Valeur brute utilisée jusqu'au prochain échantillon

### Pourquoi ce lissage?

1. Évite les sauts brusques dans l'affichage
2. Filtre le bruit des signaux tachymètre
3. Maintient une valeur stable pour l'onduleur Deye
4. Permet de voir les variations progressives

---

## 🛡️ Protection contre les Interférences WiFi

### Mécanisme de protection

```cpp
bool wifiBusy = false;
uint32_t lastWifiUpdate = 0;

// Vérifié toutes les 100ms
if (WiFi.transmitting()) {
    wifiBusy = true;
    lastWifiUpdate = millis();
} else if (millis() - lastWifiUpdate > 10) {
    wifiBusy = false;
}
```

### Impact sur l'interface

- **Serveur web:** Pause de 50ms si WiFi occupé
- **Mise à jour des données:** Continue normalement
- **LED:** Fonctionne indépendamment du WiFi

---

## 🎨 LED d'État

### Comportement

| État | Action |
|------|--------|
| Démarrage | Éteinte (fixe) |
| Données valides | Clignote à 4Hz (250ms) |
| Pas de données | Éteinte |

### Signification

- **Allumé/Éteint:** Cycle complet = 500ms
- **Période:** 250ms ON / 250ms OFF
- **Fréquence:** 4 Hz

---

## 📊 Exemples d'utilisation

### Cas 1: Ventilateurs à pleine vitesse

```
Ventilateur 9cm: 1500 RPM
Ventilateur 6cm: 1200 RPM

RPM simulé 9cm: 1500 × 2.0 = 3000
RPM simulé 6cm: 1200 × 2.5 = 3000
RPM total: (3000 + 3000) / 2 = 3000
```

### Cas 2: Ventilateurs à vitesse réduite

```
Ventilateur 9cm: 1000 RPM
Ventilateur 6cm: 800 RPM

RPM simulé 9cm: 1000 × 2.0 = 2000
RPM simulé 6cm: 800 × 2.5 = 2000
RPM total: (2000 + 2000) / 2 = 2000
```

### Cas 3: Un ventilateur arrêté

```
Ventilateur 9cm: 0 RPM
Ventilateur 6cm: 1200 RPM

RPM simulé 9cm: 0 × 2.0 = 0
RPM simulé 6cm: 1200 × 2.5 = 3000
RPM total: (0 + 3000) / 2 = 1500
```

---

## 🔍 Dépannage

### Problème: LED ne clignote pas

**Cause:** Pas de données valides détectées

**Solution:**
- Vérifier les connexions des ventilateurs
- S'assurer que les ventilateurs tournent
- Vérifier les interrupteurs matériels

### Problème: RPM affichés = 0

**Cause:** Ventilateurs ne tournent pas ou signaux non détectés

**Solution:**
- Vérifier l'alimentation des ventilateurs
- Vérifier les connexions tachymètre
- Augmenter légèrement la vitesse des ventilateurs

### Problème: RPM trop élevés/bas

**Cause:** Ratio incorrect

**Solution:**
- Ajuster le ratio dans l'interface web
- Utiliser la formule: `RPM_simulé = RPM_réel × ratio`

### Problème: Interface lente

**Cause:** Charge WiFi élevée

**Solution:**
- Attendre que le WiFi soit libre
- Vérifier la connexion WiFi
- Réduire les autres appareils WiFi

---

## 📝 Notes Importantes

### Tension universelle

Le circuit utilise des transistors NPN pour:
- **Entrées:** Accepter 3.3V, 5V ou 12V
- **Sorties:** Générer un signal compatible Deye

### Calibration

Pour calibrer les ratios:

1. Mesurer les RPM réels avec un tachymètre
2. Ajuster le ratio pour obtenir le RPM simulé désiré
3. Utiliser l'interface web pour appliquer les changements

### Sécurité

⚠️ **ATTENTION:**
- Travaillez sous tension seulement si qualifié
- L'onduleur contient des condensateurs haute tension
- Débranchez toujours avant de modifier le câblage
- Respectez les distances de sécurité

---

## 📄 Licence

Ce projet est fourni sous licence MIT.

---

## 🤝 Contribution

Les contributions sont les bienvenues!

---

## 📞 Support

Pour toute question ou problème:
- Consultez la documentation
- Vérifiez le dépannage
- Contactez le développeur

---

## 📋 Version

**Version:** 2.0.0  
**Date:** 2026-06-18  
**Compatible:** Weemos D1 Mini V2.3.0 + ESP-12S  
**Optimisations:** Anti-rebond, GPIO GPOS/GPOC, CPU 160 MHz, ISR IRAM
