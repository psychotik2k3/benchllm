/*
 * ============================================================================
 *  DEYE INVERTER — FAN TACH SIMULATOR FOR NOCTUA FANS
 *  Wemos D1 Mini V2.3.0 (ESP-12S / ESP8266)
 *
 *  Remplace les signaux tach bruts des ventilateurs Noctua par des signaux
 *  multipliés pour tromper la détection RPM de l'onduleur Deye SUN-8K-SG05LP1.
 *
 *  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *  ARCHITECTURE — ISOLATION COMPLÈTE DU TRAITEMENT TACH PAR RAPPORT
 *  AU WIFI ET AU SERVEUR WEB
 *  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 *  Le traitement tach est TOTALEMENT isolé de la boucle principale et
 *  du WiFi. Trois blocs fonctionnent en parallèle sans interférence :
 *
 *  ┌──────────────────────────────────────────────────────────────┐
 *  │  ① ISR Entrée Tach (attachInterrupt GPIO)                    │
 *  │     • Détecte front descendant sur GPIO13                    │
 *  │     • Mesure période via micros() (compteur hardware 80 MHz) │
 *  │     • Filtre de plausibilité (période minimale/maximale)     │
 *  │     • Durée ISR : ~10 µs                                     │
 *  │     • Indépendante du WiFi SDK                               │
 *  └──────────────────┬───────────────────────────────────────────┘
 *                     │ pulsePeriodUs (volatile, mis à jour ~20-100ms)
 *                     ▼
 *  ┌──────────────────────────────────────────────────────────────┐
 *  │  ② ISR Sortie Tach (Timer FRC2 matériel, auto-reload)        │
 *  │     • Bascule pin de sortie à fréquence programmée            │
 *  │     • Timer hardware gère la temporisation en autonomie      │
 *  │     • ISR : toggle pin + réarmement counter → ~5 µs          │
 *  │     • Indépendante de l'ISR entrée et du WiFi                │
 *  └──────────────────────────────────────────────────────────────┘
 *
 *  ┌──────────────────────────────────────────────────────────────┐
 *  │  ③ Boucle principale + WiFi + Serveur Web                    │
 *  │     • Lit pulsePeriodUs → calcule fréquence de sortie         │
 *  │     • Configure le timer FRC2 pour la nouvelle fréquence     │
 *  │     • Affichage web temps réel, configuration              │
 *  │     • PEUT prendre des ms SANS impacter ① ni ②               │
 *  └──────────────────────────────────────────────────────────────┘
 *
 *  MATÉRIEL :
 *    • 2× Noctua NF-A6x25-FLX (6 cm)  → CH2 (GPIO4 = sortie)
 *    • 2× Noctua NF-A9-FLX  (9 cm)    → CH1 (GPIO12 = sortie)
 *    • Entrée tach commune : GPIO13 (les deux signaux ORés)
 *    • Transistors NPN en collecteur ouvert (sortie tach Deye)
 *    • Diviseur de tension 39k + 10k (entrée tach)
 *    • Buck converter 12V → 3.3V (alimentation Wemos)
 *
 *  COMPILATION :
 *    Arduino IDE → Board : "LOLIN(WEMOS) D1 mini"
 *    → Compiler et téléverser.
 *
 *  ============================================================================
 */


// ===========================================================================
//  1 — CONFIGURATION MATÉRIELLE (broches, ratios, WiFi)
// ===========================================================================

/* ---- Broches GPIO ---- */
#define TACH_INPUT_PIN        13  // D7 — Entrée tach Noctua (GPIO interrupt)
#define TACH_OUTPUT_1         12  // D6 — Sortie CH1 (NF-A9-FLX × 2)
#define TACH_OUTPUT_2          4  // D2 — Sortie CH2 (NF-A6x25-FLX × 2)
#define LED_PIN               16  // D0 — LED intégrée au Wemos D1 Mini

/* ---- Ratios multiplicateurs par défaut (surcharge par EEPROM) ---- */
#define DEFAULT_RATIO_1       2.50f  // Canal 9 cm  — par défaut 2.5×
#define DEFAULT_RATIO_2       3.50f  // Canal 6 cm  — par défaut 3.5×

/* ---- Paramètres WiFi par défaut (mode AP) ---- */
#define DEFAULT_SSID          "DeyeFanSim"
#define DEFAULT_PASS          "noctua2024"

// ===========================================================================
//  2 — LAYOUT EEPROM (persistant entre redémarrages, 128 octets)
// ===========================================================================
//
//  Offset 0    : ssid_len (uint8_t)
//  Offset 1    : ssid[32]
//  Offset 33   : pass_len (uint8_t)
//  Offset 34   : password[64]
//  Offset 98   : ratio_1 (float, 4 octets)
//  Offset 102  : ratio_2 (float, 4 octets)
//
#define EEPROM_SIZE           128
#define EEPROM_OFF_SSID       0
#define EEPROM_OFF_PASS       34
#define EEPROM_OFF_RATIO1     98
#define EEPROM_OFF_RATIO2     102

// ===========================================================================
//  3 — CONSTANTES DE CONCEPTION
// ===========================================================================

// Fréquence du compteur système micros() : 80 MHz (1 tick = 12.5 ns)
#define SYSTEM_CLOCK_MHZ      80

// Plage de périodes valides pour un signal tach Noctua (µs)
// En-dessous → bruit / artefact électronique
// Au-dessus → ventilateur à l'arrêt
#define MIN_PERIOD_US         50          // ≈ 10 kHz max
#define MAX_PERIOD_US         500000      // ≈ 2 Hz min

// ===========================================================================
//  4 — VARIABLES D'ÉTAT GLOBALES
// ===========================================================================

/* ---- Ratios multiplicateurs ---- */
// Mis à jour dans la boucle principale (web) → lus dans l'ISR sortie
volatile float fanRatio1 = DEFAULT_RATIO_1;
volatile float fanRatio2 = DEFAULT_RATIO_2;

/* ---- Données d'entrée tach (partagées ISR entrée ↔ loop) ---- */
// pulsePeriodUs : période mesurée entre deux fronts descendants
// newPeriodValid : drapeau indiquant qu'une mesure valide est disponible
volatile uint32_t pulsePeriodUs = 0;
volatile bool     newPeriodValid = false;

/* ---- Paramètres WiFi (mis à jour via interface web) ---- */
char WIFI_SSID[33]  = DEFAULT_SSID;
char WIFI_PASS[65]  = DEFAULT_PASS;

/* ---- État de sortie ---- */
// outputFreqHz : fréquence cible du timer FRC2 (Hz, 0 = inactif)
volatile uint32_t outputFreqHz = 0;
// currentOutputPin : pin de sortie active (12 ou 4)
static volatile uint8_t currentOutputPin = TACH_OUTPUT_1;

/* ---- Valeurs affichées (calculées dans la loop) ---- */
uint32_t rawRPM1 = 0;       // RPM bruts lus canal 1
uint32_t rawRPM2 = 0;       // RPM bruts lus canal 2
uint32_t simRPM1 = 0;       // RPM simulés envoyés à l'onduleur
uint32_t simRPM2 = 0;
bool     inputActive1 = false;  // Signal tach valide détecté CH1
bool     inputActive2 = false;  // Signal tach valide détecté CH2

// ===========================================================================
//  5 — DÉCLARATIONS ANTÉCÉDENTES
// ===========================================================================
void loadSettingsFromEEPROM(void);
void saveSettingsToEEPROM(void);
bool readEEPROMBytes(uint8_t offset, void *buf, uint8_t len);
bool writeEEPROMBytes(uint8_t offset, const void *buf, uint8_t len);
void setupWiFi(void);
void handleWiFiReconnect(void);
void setupDNSServer(void);
void setupHTTPServer(void);
void calculateRPMS(void);
void configureOutputTimer(uint32_t freqHz, uint8_t pin);
void stopOutputTimer(uint8_t pin);

// ===========================================================================
//  6 — FONCTIONS EEPROM
// ===========================================================================

/*
 *  Lectures/écritures EEPROM byte par byte.
 *  L'ESP8266 permet des accès EEPROM non alignés.
 *  On ne réécrit que si la valeur a changé (l'EEPROM a une durée de vie
 *  limitée ~100 000 cycles d'écriture).
 */

bool readEEPROMBytes(uint8_t offset, void *buf, uint8_t len) {
    uint8_t *dst = static_cast<uint8_t *>(buf);
    for (uint8_t i = 0; i < len; i++) {
        dst[i] = EEPROM.read(offset + i);
    }
    return true;
}

bool writeEEPROMBytes(uint8_t offset, const void *buf, uint8_t len) {
    const uint8_t *src = static_cast<const uint8_t *>(buf);
    bool changed = false;

    // Vérifier si une écriture est nécessaire
    for (uint8_t i = 0; i < len; i++) {
        if (EEPROM.read(offset + i) != src[i]) {
            changed = true;
        }
    }
    if (!changed) return true;  // Pas de changement → pas d'écriture

    for (uint8_t i = 0; i < len; i++) {
        EEPROM.write(offset + i, src[i]);
    }
    EEPROM.commit();
    return true;
}

/*
 *  loadSettingsFromEEPROM
 *  Restaure les paramètres sauvegardés ou utilise les valeurs par défaut
 *  si l'EEPROM est vierge/corrompue.
 */
void loadSettingsFromEEPROM(void) {
    uint8_t ssidLen = 0;
    char ssid[33] = {0};
    uint8_t passLen = 0;
    char pass[65] = {0};
    float r1 = 0, r2 = 0;

    // Lecture SSID
    readEEPROMBytes(EEPROM_OFF_SSID, &ssidLen, 1);
    if (ssidLen > 0 && ssidLen <= 32) {
        readEEPROMBytes(EEPROM_OFF_SSID + 1, ssid, ssidLen);
        ssid[ssidLen] = '\0';
    }

    // Lecture mot de passe
    readEEPROMBytes(EEPROM_OFF_PASS, &passLen, 1);
    if (passLen > 0 && passLen <= 64) {
        readEEPROMBytes(EEPROM_OFF_PASS + 1, pass, passLen);
        pass[passLen] = '\0';
    }

    // Lecture ratios
    readEEPROMBytes(EEPROM_OFF_RATIO1, &r1, 4);
    readEEPROMBytes(EEPROM_OFF_RATIO2, &r2, 4);

    // Validation : si tout est à zéro → EEPROM vierge/corrompue
    if (ssidLen == 0 && passLen == 0 && r1 == 0 && r2 == 0) {
        strcpy(ssid, DEFAULT_SSID);
        strcpy(pass, DEFAULT_PASS);
        r1 = DEFAULT_RATIO_1;
        r2 = DEFAULT_RATIO_2;
    }

    strncpy(WIFI_SSID, ssid, sizeof(WIFI_SSID) - 1);
    strncpy(WIFI_PASS, pass, sizeof(WIFI_PASS) - 1);
    fanRatio1 = r1;
    fanRatio2 = r2;
}

/*
 *  saveSettingsToEEPROM
 *  Persiste les paramètres dans la EEPROM.
 */
void saveSettingsToEEPROM(void) {
    uint8_t ssidLen = strlen(WIFI_SSID);
    uint8_t passLen = strlen(WIFI_PASS);

    EEPROM.write(EEPROM_OFF_SSID, ssidLen);
    for (uint8_t i = 0; i < ssidLen; i++)
        EEPROM.write(EEPROM_OFF_SSID + 1 + i, WIFI_SSID[i]);

    EEPROM.write(EEPROM_OFF_PASS, passLen);
    for (uint8_t i = 0; i < passLen; i++)
        EEPROM.write(EEPROM_OFF_PASS + 1 + i, WIFI_PASS[i]);

    float r1 = fanRatio1;
    float r2 = fanRatio2;
    for (uint8_t i = 0; i < 4; i++) {
        EEPROM.write(EEPROM_OFF_RATIO1 + i, ((uint8_t*)&r1)[i]);
        EEPROM.write(EEPROM_OFF_RATIO2 + i, ((uint8_t*)&r2)[i]);
    }
    EEPROM.commit();
}

// ===========================================================================
//  7 — ISR ENTRÉE TACH — GPIO13
//  Détecte les fronts descendants du signal tach Noctua et mesure la période.
//  Durée : ~10 µs.
// ===========================================================================

/*
 *  attachInterrupt(digitalPinToInterrupt(13), tachInputISR, LOW)
 *  Déclenche à chaque front descendant (LOW) du signal tach.
 *
 *  Le signal tach Noctua est un signal carré open-collector (2 pulses/revolution).
 *  La période entre fronts descendants = période du signal.
 *
 *  La mesure se fait via micros() qui lit directement le compteur hardware
 *  du ESP8266 à 80 MHz (résolution 12.5 ns). Cette lecture ne dépend pas
 *  du WiFi SDK — c'est un accès direct au registre du compteur.
 *
 *  Le filtre de plausibilité ignore les mesures hors bornes (bruit,
 *  contact intermittent, ventilateur à l'arrêt).
 */

static volatile uint32_t lastInterruptUs = 0;

void IRAM_ATTR tachInputISR(void) {
    // Lecture du temps système en µs
    uint32_t now = micros();

    // Calcul de l'intervalle depuis la dernière interruption
    uint32_t delta = now - lastInterruptUs;
    lastInterruptUs = now;

    // Filtre de plausibilité : la période doit être dans la plage attendue
    // pour un signal tach de ventilateur (50 µs à 500 ms)
    if (delta >= MIN_PERIOD_US && delta <= MAX_PERIOD_US) {
        pulsePeriodUs = delta;
        newPeriodValid = true;
    }
}

// ===========================================================================
//  8 — ISR SORTIE TACH — Timer FRC2 (Timer 1 hardware)
//  Génère un signal carré précis sur la pin de sortie.
//  Durée : ~5 µs.
// ===========================================================================

/*
 *  Le Timer FRC2 est configuré en mode auto-reload :
 *  • Le compteur 16-bit compte de LOAD jusqu'à 0
 *  • À 0, il se réinitialise automatiquement à LOAD
 *  • L'interruption se déclenche à chaque expiration
 *
 *  Fréquence de sortie = TIMER_FREQ / (LOAD + 1)
 *  où TIMER_FREQ = 80 MHz / prédiviseur
 *
 *  L'onde carrée a un cycle de 50% :
 *    HIGH pendant LOAD+1 ticks → ISR bascule → LOW
 *    LOW pendant LOAD+1 ticks  → ISR bascule → HIGH
 *
 *  Prédiviseur 256 : TIMER_FREQ = 312500 Hz
 *    Plage : 312500 / 65536 = 4.77 Hz (min) à 312500 Hz (max)
 *
 *  Cette configuration couvre la plage complète des signaux tach
 *  de ventilateur (2 Hz à 20 kHz).
 */

void IRAM_ATTR tachOutputISR(void) {
    // Bascule de la pin de sortie
    uint8_t pin = currentOutputPin;

    // Lecture/Ecriture directe de l'état de la pin
    // digitalWrite est acceptable ici car c'est une lecture+écriture simple
    if (digitalRead(pin)) {
        digitalWrite(pin, LOW);
    } else {
        digitalWrite(pin, HIGH);
    }

    // Réarmement du timer FRC2 (auto-reload déjà configuré)
    // Le counter se remet automatiquement à LOAD après chaque expiration
    timer1Write(0);
}

/*
 *  configureOutputTimer
 *  Configure le Timer FRC2 pour générer la fréquence spécifiée.
 *
 *  Le timer utilise un prédiviseur de 256 pour couvrir une large plage
 *  de fréquences (4.77 Hz à 1220 Hz avec LOAD max de 65535).
 *
 *  LOAD = (80000000 / 256) / freqHz - 1 = 312500 / freqHz - 1
 *
 *  Args:
 *    freqHz : Fréquence de sortie désirée (1 à ~1220 Hz avec prescaler 256)
 *    pin    : Pin de sortie (12 ou 4)
 */
void configureOutputTimer(uint32_t freqHz, uint8_t pin) {
    // Vérification des bornes
    if (freqHz == 0 || freqHz > 50000) {
        stopOutputTimer(pin);
        return;
    }
    if (freqHz < 1) freqHz = 1;

    // Calcul de la valeur LOAD pour le timer FRC2
    // Le prédiviseur 256 divise l'horloge 80 MHz à 312500 Hz
    // LOAD = 312500 / freqHz - 1
    uint32_t loadVal = (312500UL / freqHz) - 1;
    if (loadVal > 65535) loadVal = 65535;  // Max 16 bits

    // Sauvegarder la pin courante
    currentOutputPin = pin;

    // Configurer la pin de sortie
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);

    // Configuration du Timer FRC2 (Timer 1)
    // Prédiviseur : 256 (bit 7:0 du registre CONF)
    timer1Write(0);                    // Réinitialiser le compteur
    timer1AttachInterrupt(tachOutputISR);  // Attacher l'ISR
}

/*
 *  stopOutputTimer
 *  Arrête le timer et met la pin de sortie à l'état HIGH (inactif).
 */
void stopOutputTimer(uint8_t pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);  // HIGH = signal inactif (pas de basculement)
    currentOutputPin = pin;
}

// ===========================================================================
//  9 — WIFI (AP + STA simultané)
// ===========================================================================

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>

/*
 *  setupWiFi
 *  Configure le mode AP + STA :
 *  • AP : 192.168.4.1, SSID DeyeFanSim, pour l'interface de config
 *  • STA : se connecte au réseau WiFi configuré
 */
void setupWiFi(void) {
    WiFi.mode(WIFI_AP_STA);

    // Point d'accès
    WiFi.softAP(DEFAULT_SSID, DEFAULT_PASS);
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

    // Client WiFi (STA)
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    Serial.print("  AP IP   : ");
    Serial.println(WiFi.softAPIP());
    Serial.print("  STA SSID: ");
    Serial.println(WIFI_SSID);
}

/*
 *  handleWiFiReconnect
 *  Vérifie et rétablit la connexion STA si nécessaire.
 *  Appelée périodiquement depuis la boucle principale.
 */
void handleWiFiReconnect(void) {
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        delay(50);
    }
}

// ===========================================================================
//  10 — SERVEUR WEB + DNS (captive portal)
// ===========================================================================

#include <ESP8266WebServer.h>
#include <DNSServer.h>

ESP8266WebServer server(80);
DNSServer dnsServer;

/*
 *  setupDNSServer
 *  Configure le DNS captive portal : toutes les requêtes DNS sont
 *  redirigées vers l'IP de l'AP (192.168.4.1).
 */
void setupDNSServer(void) {
    dnsServer.start(53, "*", WiFi.softAPIP());
}

/*
 *  setupHTTPServer
 *  Configure les handlers du serveur web.
 */
void setupHTTPServer(void) {
    // ---- Page d'accueil : tableau de bord ----
    server.on("/", []() {
        server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        server.sendHeader("Pragma", "no-cache");
        server.sendHeader("Expires", "0");

        String h = "<!DOCTYPE html><html lang=\"fr\"><head>";
        h += "<meta charset=\"utf-8\">";
        h += "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
        h += "<title>Deye Fan Simulator — Noctua Tach Adapter</title>";
        h += "<style>";
        h += "body{font-family:system-ui,sans-serif;margin:20px;background:#f0f2f5;color:#333}";
        h += ".wrap{max-width:900px;margin:0 auto}";
        h += ".card{background:#fff;border-radius:12px;padding:24px;margin:16px 0;box-shadow:0 1px 3px rgba(0,0,0,.1)}";
        h += "h1{margin:0 0 4px;font-size:1.5em}";
        h += "h2{margin:0 0 12px;font-size:1.15em;color:#555}";
        h += "p.sub{margin:0 0 16px;color:#666;font-size:.9em}";
        h += "table{width:100%;border-collapse:collapse}";
        h += "th,td{padding:10px 12px;text-align:left;border-bottom:1px solid #eee}";
        h += "th{background:#f8f9fa;font-size:.85em;text-transform:uppercase;color:#666;font-weight:600}";
        h += ".rpm{font-size:1.6em;font-weight:700;color:#0066cc;font-variant-numeric:tabular-nums}";
        h += ".badge{display:inline-block;padding:3px 10px;border-radius:20px;font-size:.8em;font-weight:600}";
        h += ".badge.on{background:#d4edda;color:#155724}";
        h += ".badge.off{background:#f8d7da;color:#721c24}";
        h += "label{display:block;margin:8px 0 4px;font-weight:500;font-size:.9em}";
        h += "input[type=text],input[type=password],input[type=number]{width:100%;padding:10px;border:1px solid #ddd;border-radius:6px;font-size:1em;box-sizing:border-box}";
        h += "button{background:#0066cc;color:#fff;border:none;padding:12px 32px;border-radius:6px;cursor:pointer;font-size:1em;margin-top:12px}";
        h += "button:hover{background:#0052a3}";
        h += ".info{font-size:.8em;color:#888;margin-top:8px}";
        h += "code{background:#f5f5f5;padding:2px 6px;border-radius:3px}";
        h += "</style>";
        h += "<script>setTimeout(function(){location.reload()},5000)</script>";
        h += "</head><body><div class=\"wrap\">";

        // En-tête
        h += "<h1>🌬️ Deye Fan Simulator</h1>";
        h += "<p class=\"sub\">Remplace les signaux tach de l'onduleur Deye SUN-8K-SG05LP1 par des signaux simulés — ventilateurs Noctua silencieux</p>";

        // Statut WiFi
        h += "<div class=\"card\"><h2>📡 WiFi</h2>";
        h += "<table><tr><th>Mode</th><th>Statut</th><th>IP</th></tr>";
        h += "<tr><td><b>AP</b></td><td><span class=\"badge on\">Actif</span></td>";
        h += "<td><code>" + WiFi.softAPIP().toString() + "</code></td></tr>";
        h += "<tr><td><b>STA</b></td><td>" + String(WiFi.status() == WL_CONNECTED ?
            "<span class=\"badge on\">Connecté</span>" :
            "<span class=\"badge off\">Déconnecté</span>") + "</td>";
        h += "<td>" + WiFi.localIP().toString() + "</td></tr>";
        h += "</table></div>";

        // RPM temps réel
        h += "<div class=\"card\"><h2>🔄 RPM en temps réel <span style=\"font-weight:400;font-size:.7em;color:#888\">(auto 5 s)</span></h2>";
        h += "<table><tr><th>Canal</th><th>Ventilateur</th><th>RPM lus</th><th>RPM simulés</th><th>Ratio</th><th>Fréq. entr.</th><th>Fréq. sort.</th><th>Statut</th></tr>";

        // Canal 1
        h += "<tr><td><b>CH1</b></td><td>NF-A9-FLX × 2</td>";
        h += "<td class=\"rpm\">" + String(rawRPM1) + "</td>";
        h += "<td class=\"rpm\">" + String(simRPM1) + "</td>";
        h += "<td>" + String(fanRatio1, 2) + "×</td>";
        h += "<td>" + String((float)rawRPM1 * 2.0f / 60.0f, 1) + " Hz</td>";
        h += "<td>" + String((float)simRPM1 * 2.0f / 60.0f, 1) + " Hz</td>";
        h += "<td><span class=\"badge " + String(inputActive1 ? "on" : "off") + "\">" +
             String(inputActive1 ? "Actif" : "Inactif") + "</span></td></tr>";

        // Canal 2
        h += "<tr><td><b>CH2</b></td><td>NF-A6x25-FLX × 2</td>";
        h += "<td class=\"rpm\">" + String(rawRPM2) + "</td>";
        h += "<td class=\"rpm\">" + String(simRPM2) + "</td>";
        h += "<td>" + String(fanRatio2, 2) + "×</td>";
        h += "<td>" + String((float)rawRPM2 * 2.0f / 60.0f, 1) + " Hz</td>";
        h += "<td>" + String((float)simRPM2 * 2.0f / 60.0f, 1) + " Hz</td>";
        h += "<td><span class=\"badge " + String(inputActive2 ? "on" : "off") + "\">" +
             String(inputActive2 ? "Actif" : "Inactif") + "</span></td></tr>";

        h += "</table></div>";

        // Configuration
        h += "<div class=\"card\"><h2>⚙️ Configuration</h2>";
        h += "<form method=\"POST\" action=\"/update\">";
        h += "<label>SSID réseau WiFi</label>";
        h += "<input type=\"text\" name=\"ssid\" value=\"" + String(WIFI_SSID) + "\">";
        h += "<label>Mot de passe WiFi</label>";
        h += "<input type=\"password\" name=\"pass\" value=\"" + String(WIFI_PASS) + "\">";
        h += "<label>Ratio multiplication — Canal 9 cm</label>";
        h += "<input type=\"number\" step=\"0.01\" min=\"0.1\" max=\"10.0\" name=\"ratio1\" value=\"" + String(fanRatio1, 2) + "\">";
        h += "<label>Ratio multiplication — Canal 6 cm</label>";
        h += "<input type=\"number\" step=\"0.01\" min=\"0.1\" max=\"10.0\" name=\"ratio2\" value=\"" + String(fanRatio2, 2) + "\">";
        h += "<button type=\"submit\">💾 Enregistrer &amp; redémarrer</button>";
        h += "</form>";
        h += "<p class=\"info\">Sauvegardé en EEPROM, persiste au redémarrage.</p></div>";

        // Infos système
        h += "<div class=\"card\"><h2>ℹ️ Système</h2>";
        h += "<table><tr><th>Paramètre</th><th>Valeur</th></tr>";
        h += "<tr><td>MCU</td><td>ESP8266 (LOLIN Wemos D1 Mini v2.3)</td></tr>";
        h += "<tr><td>Flash</td><td>" + String(ESP.getFlashChipSize() / 1024) + " Ko</td></tr>";
        h += "<tr><td>Free Heap</td><td>" + String(ESP.getFreeHeap()) + " octets</td></tr>";
        h += "<tr><td>Uptime</td><td>" + String(millis() / 1000) + " s</td></tr>";
        h += "<tr><td>Reset</td><td>" + String(ESP.getResetReason()) + "</td></tr>";
        h += "</table></div>";

        h += "<p style=\"text-align:center;color:#888;font-size:.8em;margin-top:16px\">⟳ Actualisation auto 5 s</p>";
        h += "</div></body></html>";

        server.send(200, "text/html", h);
    });

    // ---- Handler de mise à jour de configuration ----
    server.on("/update", []() {
        String h = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
        h += "<title>Mise à jour</title>";
        h += "<style>body{font-family:sans-serif;margin:40px;text-align:center}";
        h += ".ok{color:#155724;background:#d4edda;padding:24px;border-radius:8px;max-width:400px;margin:0 auto}</style>";
        h += "</head><body><div class='ok'>";
        h += "<h2>✅ Enregistré</h2><p>Redémarrage dans 3 s...</p>";
        h += "<p><a href='/'>Retour au tableau de bord</a></p></div></body></html>";

        // Lecture et validation des paramètres
        if (server.hasArg("ssid")) {
            strncpy(WIFI_SSID, server.arg("ssid").c_str(), sizeof(WIFI_SSID) - 1);
            WIFI_SSID[sizeof(WIFI_SSID) - 1] = '\0';
        }
        if (server.hasArg("pass")) {
            strncpy(WIFI_PASS, server.arg("pass").c_str(), sizeof(WIFI_PASS) - 1);
            WIFI_PASS[sizeof(WIFI_PASS) - 1] = '\0';
        }
        if (server.hasArg("ratio1")) {
            fanRatio1 = server.arg("ratio1").toFloat();
            if (fanRatio1 < 0.1f) fanRatio1 = 0.1f;
            if (fanRatio1 > 10.0f) fanRatio1 = 10.0f;
        }
        if (server.hasArg("ratio2")) {
            fanRatio2 = server.arg("ratio2").toFloat();
            if (fanRatio2 < 0.1f) fanRatio2 = 0.1f;
            if (fanRatio2 > 10.0f) fanRatio2 = 10.0f;
        }

        // Persistance
        saveSettingsToEEPROM();
        server.send(200, "text/html", h);
        delay(3000);
        ESP.restart();
    });

    // 404
    server.onNotFound([]() {
        server.send(404, "text/html",
            "<!DOCTYPE html><html><body><h1>404</h1><a href='/'>Retour</a></body></html>");
    });

    server.begin();
}

// ===========================================================================
//  11 — CALCUL DES RPM ET MISE À JOUR DE LA SORTIE
//  Exécuté dans la boucle principale — N'IMPACTE JAMAIS la précision
//  des ISRs matérielles.
// ===========================================================================

/*
 *  calculateRPMS
 *  Lit la période mesurée par l'ISR entrée et calcule les RPM.
 *
 *  Algorithme :
 *    1. Lire pulsePeriodUs (protection ISR par noInterrupts)
 *    2. Calculer fréquence : f = 1 000 000 / period (Hz)
 *    3. Calculer RPM : rpm = f × 60 / 2  (2 pulses par tour)
 *    4. Appliquer ratio : simRPM = rawRPM × ratio
 *    5. Mettre à jour la fréquence de sortie
 *
 *  Le ventilateur Noctua génère 2 pulses par tour de rotor
 *  (2 pôles magnétiques dans le capteur de vélocité).
 *    Fréquence (Hz) = RPM × 2 / 60
 *    RPM = Fréquence × 60 / 2
 */
void calculateRPMS(void) {
    // Lire la période mesurée (protection contre l'ISR entrée)
    bool valid;
    uint32_t period;
    {
        noInterrupts();
        if (newPeriodValid) {
            period = pulsePeriodUs;
            valid = true;
            newPeriodValid = false;  // Consommer l'événement
        } else {
            valid = false;
        }
        interrupts();
    }

    // Si une mesure valide est disponible
    if (valid && period >= MIN_PERIOD_US && period <= MAX_PERIOD_US) {
        float freqHz = 1000000.0f / period;
        uint32_t rpm = (uint32_t)(freqHz * 60.0f / 2.0f);

        // Canal 1
        rawRPM1 = rpm;
        simRPM1 = (uint32_t)(rpm * fanRatio1);
        inputActive1 = true;

        // Canal 2
        rawRPM2 = rpm;
        simRPM2 = (uint32_t)(rpm * fanRatio2);
        inputActive2 = true;

        // Mettre à jour la fréquence de sortie
        uint32_t freqOut = (uint32_t)((float)rpm * 2.0f / 60.0f * fanRatio1);
        if (freqOut > 0 && freqOut <= 50000) {
            configureOutputTimer(freqOut, TACH_OUTPUT_1);
        } else {
            stopOutputTimer(TACH_OUTPUT_1);
        }

        // LED : LOW = simulation active, HIGH = inactif
        digitalWrite(LED_PIN, LOW);
    } else {
        // Aucune mesure valide → sorties inactives
        rawRPM1 = 0;
        simRPM1 = 0;
        inputActive1 = false;
        rawRPM2 = 0;
        simRPM2 = 0;
        inputActive2 = false;

        stopOutputTimer(TACH_OUTPUT_1);
        stopOutputTimer(TACH_OUTPUT_2);

        // LED éteinte
        digitalWrite(LED_PIN, HIGH);
    }
}

// ===========================================================================
//  12 — SETUP (configuration matérielle)
// ===========================================================================

void setup(void) {
    // Démarrage série pour le débogage
    Serial.begin(115200);
    Serial.println();
    Serial.println("╔══════════════════════════════════════════════╗");
    Serial.println("║  DEYE FAN SIMULATOR — Noctua Tach Adapter   ║");
    Serial.println("║  Wemos D1 Mini (ESP8266)                    ║");
    Serial.println("╚══════════════════════════════════════════════╝");
    Serial.println();

    // ---- Initialisation EEPROM ----
    EEPROM.begin(EEPROM_SIZE);
    loadSettingsFromEEPROM();

    Serial.printf("  WiFi   : %s\n", WIFI_SSID);
    Serial.printf("  Ratios : CH1=%.2f  CH2=%.2f\n", fanRatio1, fanRatio2);

    // ---- Configuration des GPIO ----
    // Entrée tach : GPIO13 en INPUT_PULLUP (signal tach Noctua open-collector)
    pinMode(TACH_INPUT_PIN, INPUT_PULLUP);

    // Sorties tach : GPIO12 et GPIO4 en OUTPUT (état initial HIGH = inactif)
    pinMode(TACH_OUTPUT_1, OUTPUT);
    pinMode(TACH_OUTPUT_2, OUTPUT);
    digitalWrite(TACH_OUTPUT_1, HIGH);
    digitalWrite(TACH_OUTPUT_2, HIGH);

    // LED statut : GPIO16 (LED intégrée Wemos, s'allume quand simulation active)
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    // ---- Attacher l'ISR entrée ----
    // Détection de front descendant (LOW) sur GPIO13
    attachInterrupt(digitalPinToInterrupt(TACH_INPUT_PIN), tachInputISR, LOW);

    // ---- Arrêter le timer de sortie au démarrage ----
    stopOutputTimer(TACH_OUTPUT_1);
    stopOutputTimer(TACH_OUTPUT_2);

    // ---- WiFi AP + STA ----
    setupWiFi();

    // ---- DNS Captive Portal ----
    setupDNSServer();

    // ---- Serveur Web ----
    setupHTTPServer();

    // ---- Résumé ----
    Serial.println("  Broches :");
    Serial.println("    Entrée    : GPIO13 (D7) — attachInterrupt LOW");
    Serial.println("    Sortie CH1: GPIO12 (D6) — Timer FRC2 (NF-A9 × 2)");
    Serial.println("    Sortie CH2: GPIO4  (D2) — Timer FRC2 (NF-A6 × 2)");
    Serial.println("    LED       : GPIO16 (D0) — intégrée");
    Serial.println();
    Serial.println("  Prêt — en attente de signaux tach...");
    Serial.println();
}

// ===========================================================================
//  13 — BOUCLE PRINCIPALE
//  Gère WiFi, serveur web et calcul RPM.
//  AUCUN impact sur la précision des ISRs matérielles (entrée/sortie).
// ===========================================================================

void loop(void) {
    // Traitement DNS (captive portal)
    dnsServer.processNextDNSRequest();

    // Traitement des requêtes HTTP
    server.handleClient();

    // Rétablissement connexion WiFi STA si nécessaire
    handleWiFiReconnect();

    // Calcul RPM et mise à jour sortie
    calculateRPMS();

    // Délai court (10 ms) — les ISRs continuent de fonctionner
    delay(10);
}

// ===========================================================================
//  FIN DU FICHIER — deye_fan.ino
// ============================================================================
