/*
 * PROJECT: Deye Fan Simulation & Management - Universal Tachometer Adapter
 * HARDWARE: Weemos D1 Mini (ESP-12S) V2.3.0
 * ONDULEUR: Deye SUN-8K-SG05LP1-EU-AM2-P
 *
 * DESCRIPTION:
 *   Ce firmware lit les signaux de tachymètre de ventilateurs Noctua NF-A9-flx et NF-A6x25 flx,
 *   applique des ratios personnalisables, et génère un signal simulé compatible avec l'onduleur Deye.
 *
 * CARACTERISTIQUES:
 *   - Support universel 3.3V/5V/12V (transistors NPN pour adaptation)
 *   - Lissage sur fenêtre glissante 1 seconde (4 échantillons/seconde)
 *   - Interface web pour configuration temps réel
 *   - Protection contre les interférences WiFi
 *   - LED d'état intelligente
 *   - ANTI-REBOUND : Ignore les fronts < 1 ms (comme CLAUDE)
 *   - OPTIMISATION GPIO : GPOS/GPOC pour toggle en 1 cycle CPU
 *   - OVERCLOCK CPU : Configurable (80/160 MHz)
 *
 * CIRCUIT:
 *   - Entrées: 2N3904/2N2222 (NPN) pour conversion universelle
 *   - Sortie: SS8550 (NPN) pour pilotage Deye
 *   - LED: BC547 avec résistance de protection
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Ticker.h>

// ============================================================================
// CONFIGURATION DES PINES (Weemos D1 Mini V2.3.0)
// ============================================================================
#define PIN_TACH_9CM    10   // Physical Pin D1 - Ventilateur 9cm Noctua NF-A9-flx
#define PIN_TACH_6CM    11   // Physical Pin D2 - Ventilateur 6cm Noctua NF-A6x25 flx
#define PIN_OUTPUT      14   // Physical Pin D5 - Sortie vers onduleur Deye
#define PIN_LED         12   // Physical Pin D6 - LED d'état

// ============================================================================
// CONFIGURATION CPU ET RATIOS (MODIFIABLES)
// ============================================================================
// Configuration de l'overclock CPU : choisissez entre 80 MHz ou 160 MHz
// - SYS_CPU_80MHZ : Fréquence standard (consommation réduite)
// - SYS_CPU_160MHZ : Fréquence élevée (meilleures performances)
#define CPU_FREQ_MODE    SYS_CPU_160MHZ

// Ratios par défaut: RPM_simulé = RPM_lu × ratio
// 9cm: 2.0 (ex: 1500 RPM → 3000 simulé)
// 6cm: 2.5 (ex: 1200 RPM → 3000 simulé)
#define RATIO_9CM_DEFAULT  2.0f
#define RATIO_6CM_DEFAULT  2.5f

// Plage de ratios : RPM_max / RPM_min = 7000 / 1400 = 5.0 (max) à 0 (min)
const float RATIO_MAX = 5.0f;
const float RATIO_MIN = 0.0f;

// ============================================================================
// VARIABLES DE COMPTAGE (volatile pour accès depuis ISR et main loop)
// ============================================================================
volatile uint32_t count9 = 0;    // Compteur tachymètre 9cm
volatile uint32_t count6 = 0;    // Compteur tachymètre 6cm

// Ratios actuels (non volatils - modifiés via web)
float ratio9 = RATIO_9CM_DEFAULT;
float ratio6 = RATIO_6CM_DEFAULT;

// RPM bruts (sans lissage)
volatile uint32_t rpm9_raw = 0;
volatile uint32_t rpm6_raw = 0;

// RPM lissés (pour affichage stable)
uint32_t rpm9_smoothed = 0;
uint32_t rpm6_smoothed = 0;

// RPM simulé final
uint32_t simRPM = 0;

// Timestamp pour le toggle de sortie
uint32_t lastPulseMs = 0;

// ============================================================================
// VARIABLES D'ETAT ET DE LISSAGE
// ============================================================================
Ticker calculationTicker;        // Pour le lissage (4 fois/seconde)
Ticker ledPulseTicker;           // Pour la LED (4 fois/seconde)
Ticker wifiSafeTicker;           // Pour synchronisation avec WiFi

bool ledState = false;           // État de la LED
bool outState = false;           // État de la sortie vers onduleur
bool dataValid9 = false;         // Indicateur de validité des données 9cm
bool dataValid6 = false;         // Indicateur de validité des données 6cm

// Variables pour le lissage (fenêtre glissante)
uint32_t samples9[4] = {0, 0, 0, 0};  // Derniers 4 échantillons RPM 9cm
uint32_t samples6[4] = {0, 0, 0, 0};  // Derniers 4 échantillons RPM 6cm
uint32_t sampleIndex9 = 0;            // Index d'échantillon actuel 9cm
uint32_t sampleIndex6 = 0;            // Index d'échantillon actuel 6cm

// ============================================================================
// ANTI-REBOUND OPTIMISATION (comme CLAUDE)
// ============================================================================
#define DEBOUNCE_MIN_US  1000UL     // Anti-rebond : ignore < 1 ms entre 2 fronts
volatile uint32_t lastEdge9Us = 0;  // Timestamp dernier front 9cm
volatile uint32_t lastEdge6Us = 0;  // Timestamp dernier front 6cm

// Variables pour éviter les interférences WiFi
bool wifiBusy = false;              // Indicateur de charge WiFi
uint32_t lastWifiUpdate = 0;        // Dernier update WiFi (ms)

// ============================================================================
// FONCTIONS D'INTERRUPTION (ISR) - CODE MINIMAL SANS BLOCAGE
// Avec anti-rebond et optimisation GPIO
// ============================================================================
void IRAM_ATTR I_9cm() {
    uint32_t now = micros();
    uint32_t diff = now - lastEdge9Us;

    // Anti-rebond : ignorer les fronts trop proches (< 1 ms)
    if (diff > DEBOUNCE_MIN_US) {
        count9++;
        rpm9_raw = count9;
        lastEdge9Us = now;
    }
}

void IRAM_ATTR I_6m() {
    uint32_t now = micros();
    uint32_t diff = now - lastEdge6Us;

    // Anti-rebond : ignorer les fronts trop proches (< 1 ms)
    if (diff > DEBOUNCE_MIN_US) {
        count6++;
        rpm6_raw = count6;
        lastEdge6Us = now;
    }
}

// ============================================================================
// FONCTION DE CALCUL DU LISSAGE (fenêtre glissante 1 seconde)
// ============================================================================
void updateSmoothedRPM() {
    // Décalage circulaire des échantillons
    samples9[`] = rpm9_raw;
    samples6[sampleIndex6] = rpm6_raw;

    sampleIndex9 = (sampleIndex9 + 1) % 4;
    sampleIndex6 = (sampleIndex6 + 1) % 4;

    // Calcul de la moyenne des 4 derniers échantillons
    uint32_t sum9 = samples9[0] + samples9[1] + samples9[2] + samples9[3];
    uint32_t sum6 = samples6[0] + samples6[1] + samples6[2] + samples6[3];

    // Éviter la division par zéro
    if (sum9 > 0) {
        rpm9_smoothed = sum9 / 4;
    } else {
        rpm9_smoothed = 0;
    }

    if (sum6 > 0) {
        rpm6_smoothed = sum6 / 4;
    } else {
        rpm6_smoothed = 0;
    }

    // Calcul du RPM simulé
    simRPM = (uint32_t)((rpm9_smoothed * ratio9 + rpm6_smoothed * ratio6) / 2.0f);

    // Toggle de la sortie vers l'onduleur avec optimisation GPIO
    if (millis() - lastPulseMs > 500) {
        GPOS(PIN_OUTPUT, !outState);
        outState = !outState;
        lastPulseMs = millis();
    }
}

// ============================================================================
// GESTION DE LA LED - Optimisation GPIO (comme CLAUDE)
// ============================================================================
void toggleLED() {
    // Clignote seulement si des données valides sont présentes
    if (dataValid9 || dataValid6) {
        ledState = !ledState;
        // Utilisation de GPOS/GPOC pour optimisation GPIO (1 cycle CPU)
        GPOS(PIN_LED, ledState);
    }
}

// ============================================================================
// GESTION DE LA SORTIE SIMULÉE - Optimisation GPIO
// ============================================================================
void toggleOutput() {
    // Toggle de la sortie vers l'onduleur Deye
    // Utilisation de GPOS/GPOC pour optimisation GPIO (1 cycle CPU)
    GPOS(PIN_OUTPUT, !outState);
    outState = !outState;
}

// ============================================================================
// GESTION DE LA SYNCHRONISATION WIFI
// ============================================================================
void checkWifiStatus() {
    // Vérifier si le WiFi est actif et en cours de transmission
    WiFiStatus_t status = WiFi.status();

    if (status & WL_CONNECTED) {
        // Si transmission en cours, marquer comme occupé
        if (WiFi.transmitting()) {
            wifiBusy = true;
            lastWifiUpdate = millis();
        } else {
            // Rétablir l'état après 10ms si pas de transmission
            if (millis() - lastWifiUpdate > 10) {
                wifiBusy = false;
            }
        }
    } else {
        wifiBusy = false;
    }
}

// ============================================================================
// SERVEUR WEB
// ============================================================================
ESP8266WebServer server(80);

// Page HTML principale avec interface de configuration
const char* html_page = R"=====(
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset='UTF-8'>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Deye Fan Controller - Noctua</title>
    <style>
        :root {
            --primary: #2c3e50;
            --secondary: #3498db;
            --success: #27ae60;
            --warning: #f39c12;
            --danger: #e74c3c;
            --light: #ecf0f1;
            --dark: #2c3e50;
        }

        * { margin: 0; padding: 0; box-sizing: border-box; }

        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }

        .container {
            background: white;
            border-radius: 20px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            padding: 40px;
            max-width: 600px;
            width: 100%;
        }

        h1 {
            color: var(--primary);
            text-align: center;
            margin-bottom: 10px;
            font-size: 1.8em;
        }

        .subtitle {
            text-align: center;
            color: #7f8c8d;
            margin-bottom: 30px;
            font-size: 0.9em;
        }

        .card {
            background: var(--light);
            border-radius: 15px;
            padding: 20px;
            margin-bottom: 20px;
            border-left: 4px solid var(--secondary);
        }

        .card-title {
            font-weight: bold;
            color: var(--primary);
            margin-bottom: 15px;
            display: block;
        }

        .data-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 10px 0;
            border-bottom: 1px solid #bdc3c7;
        }

        .data-row:last-child {
            border-bottom: none;
        }

        .label {
            color: #7f8c8d;
            font-size: 0.9em;
        }

        .value {
            font-weight: bold;
            color: var(--primary);
            font-size: 1.2em;
        }

        .value.active {
            color: var(--success);
        }

        input[type="number"] {
            padding: 10px;
            border: 2px solid #bdc3c7;
            border-radius: 8px;
            width: 100px;
            font-size: 1.1em;
            text-align: center;
            transition: border-color 0.3s;
        }

        input[type="number"]:focus {
            outline: none;
            border-color: var(--secondary);
        }

        .wifi-section {
            background: #f8f9fa;
            border-radius: 12px;
            padding: 15px;
            margin-bottom: 20px;
        }

        .wifi-status {
            display: inline-block;
            width: 12px;
            height: 12px;
            border-radius: 50%;
            background: var(--danger);
            margin-right: 8px;
            vertical-align: middle;
            transition: background 0.3s;
        }

        .wifi-status.connected {
            background: var(--success);
        }

        button {
            background: var(--secondary);
            color: white;
            border: none;
            padding: 12px 24px;
            border-radius: 8px;
            font-size: 1em;
            cursor: pointer;
            width: 100%;
            margin-top: 10px;
            transition: background 0.3s, transform 0.2s;
        }

        button:hover {
            background: #2980b9;
            transform: translateY(-2px);
        }

        button:active {
            transform: translateY(0);
        }

        .status-message {
            text-align: center;
            padding: 10px;
            margin-top: 15px;
            border-radius: 8px;
            display: none;
        }

        .status-message.success {
            background: #d4edda;
            color: #155724;
            display: block;
        }

        .status-message.error {
            background: #f8d7da;
            color: #721c24;
            display: block;
        }

        .footer {
            text-align: center;
            margin-top: 20px;
            color: #95a5a6;
            font-size: 0.8em;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🌬️ Gestion Ventilateurs Noctua</h1>
        <p class="subtitle">Deye SUN-8K - Simulation RPM pour NF-A9-flx & NF-A6x25 flx</p>

        <!-- Section WiFi -->
        <div class="wifi-section">
            <span class="wifi-status" id="wifiStatus"></span>
            <strong>WiFi:</strong> <span id="wifiText">Configurant...</span><br>
            <small style="color: #7f8c8d;">SSID: <code id="apSsid">Deye-Fan-Config</code></small>
        </div>

        <!-- Section Ventilateur 9cm -->
        <div class="card">
            <span class="card-title">🔹 Ventilateur 9cm (NF-A9-flx)</span>
            <div class="data-row">
                <span class="label">RPM Réel:</span>
                <span class="value" id="r9">0</span>
            </div>
            <div class="data-row">
                <span class="label">Ratio:</span>
                <input type="number" step="0.1" min="0" max="4.67" id="v9" value="2.0">
            </div>
            <div class="data-row">
                <span class="label">RPM Simulé:</span>
                <span class="value active" id="rs9">0</span>
            </div>
        </div>

        <!-- Section Ventilateur 6cm -->
        <div class="card">
            <span class="card-title">🔸 Ventilateur 6cm (NF-A6x25 flx)</span>
            <div class="data-row">
                <span class="label">RPM Réel:</span>
                <span class="value" id="r6">0</span>
            </div>
            <div class="data-row">
                <span class="label">Ratio:</span>
                <input type="number" step="0.1" min="0" max="4.67" id="v6" value="2.5">
            </div>
            <div class="data-row">
                <span class="label">RPM Simulé:</span>
                <span class="value active" id="rs6">0</span>
            </div>
        </div>

        <!-- Section RPM Total -->
        <div class="card" style="border-left-color: var(--success);">
            <span class="card-title">📊 RPM Simulé Total (pour Deye)</span>
            <div class="data-row">
                <span class="label">RPM Simulé:</span>
                <span class="value active" style="font-size: 1.5em;" id="rs">0</span>
            </div>
        </div>

        <!-- Bouton Refresh -->
        <button onclick="forceRefresh()">🔄 Actualiser manuellement</button>

        <!-- Message de statut -->
        <div class="status-message" id="statusMessage"></div>

        <div class="footer">
            <p>Interface mise à jour automatiquement (100ms)</p>
            <p>Protection contre les interférences WiFi activée</p>
        </div>
    </div>

    <script>
        // Variables globales pour la synchronisation
        let lastR9 = 0;
        let lastR6 = 0;
        let lastRS = 0;
        let updateCount = 0;

        // Fonction pour afficher le message de statut
        function showStatus(message, type) {
            const msg = document.getElementById('statusMessage');
            msg.textContent = message;
            msg.className = 'status-message ' + type;

            // Masquer après 3 secondes
            setTimeout(() => {
                msg.style.display = 'none';
            }, 3000);
        }

        // Fonction pour actualiser manuellement
        function forceRefresh() {
            fetch('/data?force=true')
                .then(r => r.json())
                .then(d => {
                    lastR9 = d.r9;
                    lastR6 = d.r6;
                    lastRS = d.rs;
                    updateDisplay();
                    showStatus('Données actualisées', 'success');
                })
                .catch(e => {
                    showStatus('Erreur de connexion', 'error');
                });
        }

        // Fonction pour mettre à jour l'affichage
        function updateDisplay() {
            document.getElementById('r9').textContent = lastR9;
            document.getElementById('r6').textContent = lastR6;
            document.getElementById('rs9').textContent = lastR9 * 2.0;
            document.getElementById('rs6').textContent = lastR6 * 2.5;
            document.getElementById('rs').textContent = lastRS;

            // Ajouter une classe active si les données sont valides
            const r9El = document.getElementById('r9');
            const r6El = document.getElementById('r6');

            if (lastR9 > 0) {
                r9El.classList.add('active');
            } else {
                r9El.classList.remove('active');
            }

            if (lastR6 > 0) {
                r6El.classList.add('active');
            } else {
                r6El.classList.remove('active');
            }
        }

        // Mise à jour automatique toutes les 100ms
        setInterval(function() {
            fetch('/data')
                .then(r => r.json())
                .then(d => {
                    lastR9 = d.r9;
                    lastR6 = d.r6;
                    lastRS = d.rs;
                    updateDisplay();
                })
                .catch(e => {
                    // Ignorer les erreurs de connexion
                });
        }, 100);

        // Vérifier l'état du WiFi au chargement
        function checkWifiStatus() {
            const statusEl = document.getElementById('wifiStatus');
            const textEl = document.getElementById('wifiText');

            if (navigator.onLine) {
                statusEl.classList.add('connected');
                textEl.textContent = 'Connecté';
            } else {
                statusEl.classList.remove('connected');
                textEl.textContent = 'Déconnecté';
            }
        }

        // Vérifier le statut du WiFi
        setInterval(checkWifiStatus, 2000);
        checkWifiStatus();

        // Écouteur de changement des inputs pour validation
        document.getElementById('v9').addEventListener('change', function() {
            const value = parseFloat(this.value);
            if (value < 0) this.value = 0;
            if (value > 4.67) this.value = 4.67;

            // Envoyer la nouvelle valeur au serveur sans recharger
            fetch('/config?channel=9&ratio=' + this.value);
        });

        document.getElementById('v6').addEventListener('change', function() {
            const value = parseFloat(this.value);
            if (value < 0) this.value = 0;
            if (value > 4.67) this.value = 4.67;

            // Envoyer la nouvelle valeur au serveur sans recharger
            fetch('/config?channel=6&ratio=' + this.value);
        });
    </script>
</body>
</html>
)======";

// Gestionnaire de la racine
void handleRoot() {
    // Vérifier si le WiFi est occupé avant d'envoyer la réponse
    checkWifiStatus();

    if (!wifiBusy || (millis() - lastWifiUpdate > 50)) {
        server.send(200, "text/html", html_page);
    } else {
        // Attendre que le WiFi soit libre
        server.send(503, "text/plain", "Service temporairement indisponible (WiFi occupé)");
    }
}

// Gestionnaire des données JSON
void handleData() {
    checkWifiStatus();

    String json = "{\"r9\":" + String(rpm9_smoothed) +
                  ",\"r6\":" + String(rpm6_smoothed) +
                  ",\"rs\":" + String(simRPM) + "}";

    server.send(200, "application/json", json);
}

// Gestionnaire de configuration
void handleConfig() {
    // Récupérer les paramètres depuis la requête
    String channel = server.arg("channel");
    String ratioValue = server.arg("ratio");

    if (channel == "9" && ratioValue != "") {
        ratio9 = atof(ratioValue.c_str());
        // Limiter la plage
        if (ratio9 < RATIO_MIN) ratio9 = RATIO_MIN;
        if (ratio9 > RATIO_MAX) ratio9 = RATIO_MAX;
    } else if (channel == "6" && ratioValue != "") {
        ratio6 = atof(ratioValue.c_str());
        // Limiter la plage
        if (ratio6 < RATIO_MIN) ratio6 = RATIO_MIN;
        if (ratio6 > RATIO_MAX) ratio6 = RATIO_MAX;
    }

    // Répondre avec les nouvelles valeurs
    String response = "{\"r9\":" + String(ratio9) +
                      ",\"r6\":" + String(ratio6) + "}";
    server.send(200, "application/json", response);
}

// Gestionnaire de l'état WiFi
void handleWifiStatus() {
    WiFiStatus_t status = WiFi.status();

    if (status & WL_CONNECTED) {
        server.send(200, "text/plain", "connected");
    } else {
        server.send(200, "text/plain", "disconnected");
    }
}

// ============================================================================
// INITIALISATION
// ============================================================================
void setup() {
    // Initialisation de la série
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("========================================");
    Serial.println("  Deye Fan Controller - Noctua Edition");
    Serial.println("========================================");

    // ========================================================================
    // OPTIMISATION CPU (comme CLAUDE) - Passage à 160 MHz
    // ========================================================================
    system_update_cpu_freq(SYS_CPU_160MHZ);
    Serial.printf("[CPU] Overclocking à %u MHz\n", system_get_cpu_freq());

    // Configuration des pins
    pinMode(PIN_TACH_9CM, INPUT_PULLUP);
    pinMode(PIN_TACH_6CM, INPUT_PULLUP);
    pinMode(PIN_OUTPUT, OUTPUT);
    pinMode(PIN_LED, OUTPUT);

    // Initialisation des interrupteurs matériels avec IRAM_ATTR
    attachInterrupt(digitalPinToInterrupt(PIN_TACH_9CM), I_9cm, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_TACH_6CM), I_6m, FALLING);

    Serial.printf("Interrupts configurés (IRAM):\n");
    Serial.printf("  9cm: Pin %d\n", PIN_TACH_9CM);
    Serial.printf("  6cm: Pin %d\n", PIN_TACH_6CM);

    // Initialisation des variables anti-rebond
    lastEdge9Us = micros();
    lastEdge6Us = micros();

    // Configuration du Ticker pour le lissage (4 fois/seconde = 250ms)
    calculationTicker.attach(250, updateSmoothedRPM);

    // Configuration du Ticker pour la LED (4 fois/seconde = 250ms)
    ledPulseTicker.attach(250, toggleLED);

    // Configuration du Ticker pour la synchronisation WiFi
    wifiSafeTicker.attach(100, checkWifiStatus);

    // Initialisation WiFi en mode AP+STA
    Serial.println("\nConfiguration WiFi...");
    WiFi.mode(WIFI_AP_STA);

    // Créer un point d'accès pour la configuration
    WiFi.softAP("Deye-Fan-Config", "12345678");

    // Tenter de se connecter au réseau existant (optionnel)
    const char* ssid = "";
    const char* password = "";

    if (strlen(ssid) > 0) {
        WiFi.begin(ssid, password);
        Serial.printf("Connexion au réseau: %s\n", ssid);

        // Attendre la connexion
        int timeout = 0;
        while (WiFi.status() != WL_CONNECTED && timeout < 30) {
            delay(500);
            timeout++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("Connecté à l'IP: %s\n", WiFi.localIP().toString().c_str());
        } else {
            Serial.println("Échec de connexion au réseau, mode AP activé");
        }
    } else {
        Serial.println("Mode AP uniquement (aucun réseau configuré)");
    }

    // Configuration du serveur web
    server.on("/", handleRoot);
    server.on("/data", handleData);
    server.on("/config", handleConfig);
    server.on("/wifi", handleWifiStatus);

    server.begin();
    Serial.println("Serveur web démarré sur le port 80");

    // Afficher l'adresse IP du point d'accès
    Serial.printf("\nAdresse AP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.println("Ouvrez cette adresse dans un navigateur pour configurer");

    Serial.println("\n========================================");
    Serial.println("Initialisation terminée");
    Serial.println("========================================\n");
}

// ============================================================================
// AUTOMATISATION HOME ASSISTANT - BATTERY SWITCH
// ============================================================================
// Cette section contient l'automatisation pour basculer vers la batterie
// quand la production PV est inférieure à la charge pendant >10 minutes
// Conditions:
//   - Production PV < Charge pendant plus de 10 minutes
//   - Priorité: Solar/Utility/Battery
//   - SOC batterie > 95%
// Actions:
//   1. Changer priorité vers Solar/Battery/Utility
//   2. Attendre 5 secondes
//   3. Mettre back_to_battery_capacity au niveau du SOC actuel
//   4. Attendre 5 secondes
//   5. Forcer le SOC à 100%
// ============================================================================

const char* ha_automation_yaml = R"=====(
# Automatisation : Basculement vers la batterie en cas de faible production solaire
# Déclenche quand la production PV est inférieure à la charge pendant >10 minutes
# Conditions:
#   - Production PV < Charge pendant plus de 10 minutes
#   - Priorité: Solar/Utility/Battery
#   - SOC batterie > 95%
# Actions:
#   1. Changer priorité vers Solar/Battery/Utility
#   2. Attendre 5 secondes
#   3. Mettre back_to_battery_capacity au niveau du SOC actuel
#   4. Attendre 5 secondes
#   5. Forcer le SOC à 100%

- id: battery_switch_auto
  alias: "Basculement vers la batterie - Deye Fan Controller"
  description: "Automatisation de basculement vers la batterie en cas de faible production solaire"
  trigger:
    - platform: numeric_state
      entity_id: sensor.growatt_spf_pv_power_1
      below: "{{ states('sensor.growatt_spf_load_power') }}"
      for:
        minutes: 10
  condition:
    - condition: state
      entity_id: select.growatt_spf_output_source_priority
      state: "Solar/Utility/Battery"
    - condition: numeric_state
      entity_id: sensor.growatt_spf_battery_state_of_charge
      above: 95
  action:
    - service: select.select_option
      data:
        option: "Solar/Battery/Utility"
      target:
        entity_id: select.growatt_spf_output_source_priority
    - delay:
        seconds: 5
    - service: number.set_value
      data:
        value: "{{ states('sensor.growatt_spf_battery_state_of_charge') }}"
      target:
        entity_id: sensor.growatt_spf_battery_state_of_charge
    - delay:
        seconds: 5
    - service: number.set_value
      data:
        value: "100"
      target:
        entity_id: sensor.growatt_spf_battery_state_of_charge
)=====";

// Fonction pour exporter l'automatisation vers un fichier YAML
void exportHAAutomation() {
    File haFile = SPIFFS.open("/ha_automation.yaml", "w");
    if (haFile) {
        haFile.print(ha_automation_yaml);
        haFile.close();
        Serial.println("[HA] Automatisation exportée vers /ha_automation.yaml");
    } else {
        Serial.println("[HA] Échec d'exportation de l'automatisation");
    }
}

// Fonction pour afficher les informations de l'automatisation
void displayHAInfo() {
    Serial.println("========================================");
    Serial.println("  HOME ASSISTANT AUTOMATION INFO");
    Serial.println("========================================");
    Serial.printf("ID: battery_switch_auto\n");
    Serial.println("Alias: Basculement vers la batterie - Deye Fan Controller");
    Serial.println("Description: Automatisation de basculement vers la batterie en cas de faible production solaire");
    Serial.println();
    Serial.println("CONDITIONS DE DÉCLENCHEMENT:");
    Serial.println("  - Production PV < Charge pendant >10 minutes");
    Serial.println("  - Priorité actuelle = Solar/Utility/Battery");
    Serial.println("  - SOC batterie > 95%%");
    Serial.println();
    Serial.println("ACTIONS EXÉCUTÉES:");
    Serial.println("  1. Changer priorité vers Solar/Battery/Utility");
    Serial.println("  2. Attendre 5 secondes");
    Serial.println("  3. Mettre back_to_battery_capacity au niveau du SOC actuel");
    Serial.println("  4. Attendre 5 secondes");
    Serial.println("  5. Forcer le SOC à 100%%");
    Serial.println();
    Serial.println("========================================");
}

void loop() {
    // Gestion du serveur web
    server.handleClient();

    // Vérification périodique de la charge WiFi (déjà fait par le ticker)
    static uint32_t lastCheck = 0;
    if (millis() - lastCheck > 1000) {
        // Vérifier si une transmission WiFi est en cours
        if (WiFi.transmitting()) {
            wifiBusy = true;
            lastWifiUpdate = millis();
        } else {
            wifiBusy = false;
        }
        lastCheck = millis();
    }

    // Céder le CPU au stack WiFi (comme CLAUDE)
    yield();
}
