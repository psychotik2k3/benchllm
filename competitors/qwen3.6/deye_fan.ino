/*
 * ============================================================================
 *  Deye Inverter — Fan Tach Simulator
 *  Remplacement ventilateurs NMB → Noctua (NF-A6x25 + NF-A9)
 *  Matériel : Wemos D1 Mini V2.3.0 (ESP-12S / ESP8266)
 *  Onduleur : Deye SUN-8K-SG05LP1-EU-AM2-P
 * ============================================================================
 *
 * ────────────────────────────────────────────────────────────────────────────
 *  ARCHITECTURE LOGICIELLE — ISOLATION TACH / RÉSEAU
 * ────────────────────────────────────────────────────────────────────────────
 *
 *  L'exigence critique est qu'aucune activité WiFi ou serveur web ne puisse
 *  introduire de jitter sur le signal tach simulé envoyé à l'onduleur.
 *  Cette isolation est obtenue par une triple barrière :
 *
 *  1. LECTURE TACH (entrées) : interruptions GPIO matérielles (FALLING).
 *     Sur ESP8266, les interruptions GPIO ont la priorité la plus haute et
 *     ne sont jamais masquées par le WiFi ou les tâches RTOS. Chaque bord
 *     descendant est horodaté avec micros(). L'erreur de microsecondes liée
 *     au WiFi est négligeable devant une période de plusieurs centaines de µs.
 *
 *  2. GÉNÉRATION TACH (sorties) : Timer1 matériel (hw_timer_t).
 *     Un seul ISR Timer1 gère les deux sorties simultanément. Chaque sortie
 *     conserve son prochron nextTick absolu ; l'ISR vérifie à chaque tick
 *     quelles sorties doivent basculer, met à jour le GPIO et programme le
 *     prochain interrupt. La boucle principale (serveur web) est TOTALEMENT
 *     isolée : elle ne lit/écrit jamais les registres des sorties tach.
 *
 *  3. SERVEUR WEB / WiFi : boucle loop() classique.
 *     ESP8266WebServer::handleClient() peut bloquer plusieurs centaines de
 *     ms, mais cela n'a AUCUN impact sur Timer1 ni sur les interruptions GPIO.
 *     Le signal tach simulé continue d'être généré à la fréquence exacte
 *     définie par le dernier ratio mesuré.
 *
 *  ┌─────────────────────────────────────────────────────────────────────┐
 *  │              Boucle principale (loop)                               │
 *  │  ┌──────────┐  ┌───────────┐  ┌──────────┐  ┌──────────────────┐  │
 *  │  │ WiFi     │  │ WebServer │  │ EEPROM   │  │ Affichage LED    │  │
 *  │  │ handle   │→ │ handle    │→ │ update   │→ │ status blink     │  │
 *  │  └──────────┘  └───────────┘  └──────────┘  └──────────────────┘  │
 *  │                                                                       │
 *  │         AUCUNE communication avec les ISR Timer1 ou GPIO              │
 *  └─────────────────────────────────────────────────────────────────────┘
 *     ↑                                                                     ↑
 *  ┌──┴─────────────────────────────────────────────────────────────────┐  │
 *  │                      interruptions matérielles                     │  │
 *  │  ┌──────────────────────┐         ┌───────────────────────┐        │  │
 *  │  │ ISR GPIO FALLING     │         │ ISR Timer1 HW         │        │  │
 *  │  │ fan1TachISR()        │         │ tachOutputAllISR()    │        │  │
 *  │  │ → horodatage bord ↓  │         │ → basculement GPIO    │        │  │
 *  │  │ → calcul période     │         │   des deux sorties    │        │  │
 *  │  └──────────────────────┘         └───────────────────────┘        │  │
 *  └─────────────────────────────────────────────────────────────────┘  ↑
 *     ↑                                                                     ↑
 *  WiFi / RTOS tasks (priorité Inférieure aux HW interrupts)               │
 *                                                                             │
 *  Le signal envoyé à l'onduleur traverse UNIQUEMENT :                       │
 *    GPIO → Timer1 ISR → transistor NPN → connecteur Deye                    │
 *  (aucune dépendance logicielle vis-à-vis du réseau)                         │
 * ============================================================================
 */

// ===== INCLUDES =============================================================

#include <Arduino.h>
#include <stdint.h>       // uint64_t, int16_t pour calcul ISR IRAM
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ===== PIN DEFINITIONS (Wemos D1 Mini V2.3.0) ===============================
//
//  Entrées tach :
//    GPIO13 (D7) → Tach entrée ventilateur 9 cm (NF-A9-FLX)
//    GPIO15 (D8) → Tach entrée ventilateur 6 cm  (NF-A6x25-FLX)
//
//  Sorties tach simulées :
//    GPIO12 (D6) → Sortie tach simulée vent. 9 cm
//    GPIO14 (D5) → Sortie tach simulée vent. 6 cm
//
//  Status LED :
//    GPIO2     → LED intégrée Wemos (active LOW)

#define FAN1_TACH_IN        13    // D7 — Tach entrée ventilateur 9cm
#define FAN2_TACH_IN        15    // D8 — Tach entrée ventilateur 6cm
#define FAN1_SIM_OUT        12    // D6 — Sortie tach simulée vent. 9cm
#define FAN2_SIM_OUT        14    // D5 — Sortie tach simulée vent. 6cm
#define STATUS_LED          2     // GPIO2 — LED intégrée Wemos (active LOW)

// ===== CONSTANTS =============================================================

#define PULSES_PER_REVOLUTION   2    // Standard Noctua : 2 impulsions/tour
#define MIN_TACH_PERIOD_US      80   // Période minimale valide (~375 000 RPM en pulses)
#define MAX_TACH_PERIOD_US      100000 // Période maximale valide (60 RPM en RPM réels)

#define TACH_STALE_MS           2000  // Délai au-delà duquel un ventilateur est « à l'arrêt »
#define TACH_UPDATE_TICK_US     10    // Résolution du Timer1 : chaque tick = 10 µs

// Fréquence CPU ESP8266 et prescaler pour Timer1
#define CPU_FREQ_MHZ            80
#define TIMER_PRESCALER         80   // 80 MHz / 80 = 1 MHz → resolution 1 µs par tick
#define US_PER_TICK             1    // Avec prescaler 80, chaque tick = 1 µs

// Addresses EEPROM
#define EEP_MAGIC              0
#define EEP_FAN1_RATIO         2     // Multiplicateur ventilateur 9 cm (float)
#define EEP_FAN2_RATIO         6     // Multiplicateur ventilateur 6 cm
#define EEP_WIFI_SSID          10    // SSID réseau WiFi STA (max 32 octets)
#define EEP_WIFI_PASS          42    // Mot de passe WiFi STA (max 64 octets)
#define EEPROM_SIZE            128

// WiFi defaults
const char *DEFAULT_AP_SSID = "Deye-TachSim";

// ===== GLOBAL STATE (volatile car partagées entre ISR et loop) =================

// Périodes mesurées (us) — mises à jour par les ISR GPIO FALLING
volatile uint32_t fan1PeriodUs     = MAX_TACH_PERIOD_US; // Max = ventilateur à l'arrêt
volatile uint32_t fan2PeriodUs     = MAX_TACH_PERIOD_US;

// Timestamps des derniers fronts descendants (us) — lus dans loop() pour timeout
volatile uint32_t lastFan1IRQ_us   = 0;
volatile uint32_t lastFan2IRQ_us   = 0;

// Drapeaux d'activité : true quand les ventilateurs tournent réellement
volatile bool     fan1Active       = false;
volatile bool     fan2Active       = false;

// Ratios multiplicateurs (chargés depuis EEPROM, modifiables via le web)
// Version float pour le calcul et l'affichage dans la boucle principale.
float              fan1Ratio       = 2.5f;  // Par défaut : ×2.5 pour compenser Noctua plus lent
float              fan2Ratio       = 2.5f;

// Version int10 (×10) des ratios, lue dans les ISRs IRAM pour éviter les FP.
// 2.5 → 25, 3.0 → 30, 1.7 → 17, etc.
volatile int16_t   fan1RatioInt10  = 25;
volatile int16_t   fan2RatioInt10  = 25;

// Périodes simulées (demi-période) — lues par Timer1 ISR, mises à jour quand une
// nouvelle mesure tach arrive. Valeur initiale MAX = signal quasi-statique (fan stopped).
volatile uint32_t  simPeriodUsFan1 = MAX_TACH_PERIOD_US;
volatile uint32_t  simPeriodUsFan2 = MAX_TACH_PERIOD_US;

// État des sorties GPIO (conserve l'état à travers les appels ISR)
static volatile bool simStateFan1  = true;   // true = HIGH sur GPIO → NPN OFF
static volatile bool simStateFan2  = true;

// nextTick absolus du Timer1 pour chaque sortie (tick units, pas us)
// Ces valeurs sont en « ticks Timer1 » où 1 tick = 1 µs (prescaler=80)
static volatile uint32_t fan1NextTick   = 0;
static volatile uint32_t fan2NextTick   = 0;

// État de connexion WiFi (lu par le serveur web)
volatile bool     wifiConnected      = false;
volatile bool     apModeOnly         = true;

// ===== TIMER & SERVER =========================================================

hw_timer_t *tachTimer = NULL;                // Timer1 pour génération tach
ESP8266WebServer server(80);                 // Serveur web port 80

// ============================================================================
//  SECTION 1 — EEPROM HELPERS
// ============================================================================

void eepromWriteRatios(float r1, float r2) {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(EEP_FAN1_RATIO, r1);
    EEPROM.put(EEP_FAN2_RATIO, r2);
    EEPROM.commit();
    EEPROM.end();
}

void eepromWriteWiFi(const char *ssid, const char *pass) {
    EEPROM.begin(EEPROM_SIZE);
    uint8_t addr = EEP_WIFI_SSID;
    // SSID (max 32 octets + null terminator)
    for (int i = 0; ssid[i] && i < 31; i++) EEPROM.write(addr + i, ssid[i]);
    EEPROM.write(addr + ((ssid[31] ? 32 : strlen(ssid)) + 1), 0); // padding
    addr = EEP_WIFI_PASS;
    for (int i = 0; pass[i] && i < 63; i++) EEPROM.write(addr + i, pass[i]);
    EEPROM.write(addr + ((pass[63] ? 64 : strlen(pass)) + 1), 0); // padding
    EEPROM.commit();
    EEPROM.end();
}

/**
 * Charge les ratios multiplicateurs depuis l'EEPROM.
 * Si la signature magique n'est pas trouvée ou que les valeurs sont
 * hors limites, utilise le défaut de 2,5x pour chaque canal.
 */
static void eepromLoadRatios() {
    bool magicOk = false;
    EEPROM.begin(EEPROM_SIZE);
    uint8_t m1 = EEPROM.read(EEP_MAGIC);
    uint8_t m2 = EEPROM.read(EEP_MAGIC + 1);
    if (m1 == 0xDE && m2 == 0xAD) {
        magicOk = true;
    }
    
    float f1, f2;
    EEPROM.get(EEP_FAN1_RATIO, f1);
    EEPROM.get(EEP_FAN2_RATIO, f2);
    
    if (magicOk && f1 >= 0.5f && f1 <= 10.0f && f2 >= 0.5f && f2 <= 10.0f) {
        fan1Ratio = f1;
        fan2Ratio = f2;
    } else {
        // Valeurs par défaut ou EEPROM corrompue
        fan1Ratio = 2.5f;
        fan2Ratio = 2.5f;
    }
    
    // Mettre à jour les versions int10 pour les ISRs IRAM (sans FP)
    fan1RatioInt10 = (int16_t)(fan1Ratio * 10 + 0.5f);
    fan2RatioInt10 = (int16_t)(fan2Ratio * 10 + 0.5f);
}
    EEPROM.end();
}

void eepromReadWiFi(String &ssid, String &pass) {
    bool magicOk = false;
    EEPROM.begin(EEPROM_SIZE);
    uint8_t m1 = EEPROM.read(EEP_MAGIC);
    uint8_t m2 = EEPROM.read(EEP_MAGIC + 1);
    if (m1 == 0xDE && m2 == 0xAD) magicOk = true;
    
    ssid  = ""; pass = "";
    // Lire SSID jusqu'au null terminator
    for (int i = EEP_WIFI_SSID; i < EEP_WIFI_SSID + 34; i++) {
        char c = EEPROM.read(i);
        if (c == 0) break;
        ssid += c;
    }
    // Lire password jusqu'au null terminator
    for (int i = EEP_WIFI_PASS; i < EEP_WIFI_PASS + 66; i++) {
        char c = EEPROM.read(i);
        if (c == 0) break;
        pass += c;
    }
    
    // Si pas de données valides, utiliser des valeurs par défaut
    if (!magicOk || ssid.length() == 0) {
        ssid  = "";
        pass  = "";
    }
    EEPROM.end();
}

void eepromSaveWiFi(const char *ssid, const char *pass) {
    EEPROM.begin(EEPROM_SIZE);
    
    // Marquer EEPROM comme valide
    EEPROM.write(EEP_MAGIC, 0xDE);
    EEPROM.write(EEP_MAGIC + 1, 0xAD);
    
    // SSID (max 32 caractères + null terminator → 34 octets)
    uint8_t addr = EEP_WIFI_SSID;
    int lenSSID = 0;
    for (int i = 0; ssid[i] && i < 31; i++) {
        EEPROM.write(addr + i, ssid[i]);
        lenSSID++;
    }
    EEPROM.write(addr + lenSSID, 0); // null terminator
    // Padding jusqu'à la fin de la zone (34 octets)
    for (int i = lenSSID + 1; i < 34; i++) {
        EEPROM.write(addr + i, 0);
    }
    
    addr = EEP_WIFI_PASS;
    int lenPass = 0;
    for (int i = 0; pass[i] && i < 63; i++) {
        EEPROM.write(addr + i, pass[i]);
        lenPass++;
    }
    EEPROM.write(addr + lenPass, 0); // null terminator
    // Padding jusqu'à la fin de la zone (66 octets)
    for (int i = lenPass + 1; i < 66; i++) {
        EEPROM.write(addr + i, 0);
    }
    
    EEPROM.commit();
    EEPROM.end();
}

// ============================================================================
//  SECTION 2 — TACH INPUT ISR (GPIO FALLING edges)
// ============================================================================

/**
 * ISR appelé à chaque front descendant sur la ligne tach d'un ventilateur.
 * Mesure la période entre deux fronts descendants consécutifs et met à jour
 * les variables globales. Calcul du ratio multiplicateur dans loop(), pas ici.
 *
 * CRITICAL: Cet ISR doit être le plus court possible.
 */

static void IRAM_ATTR fan1TachISR() {
    uint32_t now = micros();
    
    // Anti-rebond / validation de période minimale
    if (lastFan1IRQ_us == 0) {
        lastFan1IRQ_us = now;
        return;
    }
    
    uint32_t period = now - lastFan1IRQ_us;
    lastFan1IRQ_us = now;
    
    // Vérifier que la période est dans les limites raisonnables
    if (period >= MIN_TACH_PERIOD_US && period <= MAX_TACH_PERIOD_US) {
        fan1PeriodUs = period;
        fan1Active   = true;
        
        // Calculer la période simulée (demi-période pour le Timer1 ISR).
        // simPeriod = period / ratio = period * 10 / ratioInt10
        // Utiliser l'int10 version du ratio pour rester en arithmétique
        // entière (critique pour IRAM_ATTR sur ESP8266 où le FP nécessite flash).
        uint32_t simPeriodU32 = (uint32_t)((uint64_t)period * fan1RatioInt10 / 10);
        
        if (simPeriodU32 >= MIN_TACH_PERIOD_US && simPeriodU32 <= MAX_TACH_PERIOD_US * 2) {
            simPeriodUsFan1 = simPeriodU32;
        }
        // Sinon : conserver la valeur précédente de simPeriodUsFan1.
    }
}

static void IRAM_ATTR fan2TachISR() {
    uint32_t now = micros();
    
    if (lastFan2IRQ_us == 0) {
        lastFan2IRQ_us = now;
        return;
    }
    
    uint32_t period = now - lastFan2IRQ_us;
    lastFan2IRQ_us = now;
    
    if (period >= MIN_TACH_PERIOD_US && period <= MAX_TACH_PERIOD_US) {
        fan2PeriodUs = period;
        fan2Active   = true;
        
        uint32_t simPeriodU32 = (uint32_t)((uint64_t)period * fan2RatioInt10 / 10);
        
        if (simPeriodU32 >= MIN_TACH_PERIOD_US && simPeriodU32 <= MAX_TACH_PERIOD_US * 2) {
            simPeriodUsFan2 = simPeriodU32;
        }
        // Sinon : conserver valeur précédente de simPeriodUsFan2.
    }
}

// ============================================================================
//  SECTION 3 — TACH OUTPUT ISR (Timer1 hardware, gère les deux sorties)
// ============================================================================

/**
 * ISR Timer1 matériel : génère les signaux tach simulés sur GPIO12 et GPIO14.
 * Chaque sortie a son propre nextTick absolu ; à chaque tick, l'ISR vérifie
 * si une ou les deux sorties doivent basculer. Le prochain intervalle est
 * programmé à la plus proche échéance de tous les canaux.
 *
 * Cette approche garantit qu'aucun jitter réseau ne peut impacter le signal.
 * Le Timer1 fonctionne indépendamment du CPU et du WiFi.
 */

static void IRAM_ATTR tachOutputAllISR() {
    uint32_t now = timerRead(tachTimer);
    
    // ─── Gestion Fan 1 (GPIO12) ──────────────────────────────────────────
    if (fan1NextTick <= now) {
        // Si le ventilateur est à l'arrêt, forcer HIGH permanent (NPN OFF).
        // simStateFan1 = true → GPIO LOW dans la logique normale, mais ici
        // on veut GPIO HIGH → inversement : quand inactive, pas de toggle.
        if (!fan1Active) {
            // Fan stopped: keep pin HIGH permanently
            digitalWrite(FAN1_SIM_OUT, HIGH);
            fan1NextTick = UINT32_MAX; // stop further scheduling
        } else {
            simStateFan1 = !simStateFan1;
            
            // GPIO : HIGH → NPN OFF → tach line HIGH (fan stopped state)
            //        LOW  → NPN ON  → tach line LOW  (tach pulse present)
            if (simStateFan1) {
                digitalWrite(FAN1_SIM_OUT, HIGH);
            } else {
                digitalWrite(FAN1_SIM_OUT, LOW);
            }
            
            // Programmer le prochain tick : une demi-période plus tard.
            uint32_t tick;
            uint32_t halfPeriod = simPeriodUsFan1 / US_PER_TICK;
            int32_t delta = (int32_t)halfPeriod;
            if (delta <= 0 || delta > MAX_TACH_PERIOD_US) {
                fan1NextTick = UINT32_MAX;
            } else {
                tick = now + delta;
                fan1NextTick = tick;
            }
        }
    }
    
    // ─── Gestion Fan 2 (GPIO14) ──────────────────────────────────────────
    if (fan2NextTick <= now) {
        if (!fan2Active) {
            digitalWrite(FAN2_SIM_OUT, HIGH);
            fan2NextTick = UINT32_MAX;
        } else {
            simStateFan2 = !simStateFan2;
            
            if (simStateFan2) {
                digitalWrite(FAN2_SIM_OUT, HIGH);
            } else {
                digitalWrite(FAN2_SIM_OUT, LOW);
            }
            
            uint32_t tick2;
            uint32_t halfPeriod2 = simPeriodUsFan2 / US_PER_TICK;
            int32_t delta2 = (int32_t)halfPeriod2;
            if (delta2 <= 0 || delta2 > MAX_TACH_PERIOD_US) {
                fan2NextTick = UINT32_MAX;
            } else {
                tick2 = now + delta2;
                fan2NextTick = tick2;
            }
        }
    }
    
    // ─── Programmer le prochain interrupt Timer1 ──────────────────────────
    // Le prochain tick se produit au plus tôt du nextTick de Fan 1 et Fan 2.
    uint32_t next = fan1NextTick;
    if (fan2NextTick < next) next = fan2NextTick;
    
    timerWrite(tachTimer, next);
}

// ============================================================================
//  SECTION 4 — HELPERS CALCUL RPM
// ============================================================================

/**
 * Convertit une période tach mesurée en RPM réels.
 * Noctua : 2 impulsions par tour → RPM = 60 / (period_s × 2)
 *          = 30 / period_s = 30_000_000 / period_us
 */
static uint32_t calculateRPM(uint32_t periodUs) {
    if (periodUs < MIN_TACH_PERIOD_US) return 99999; // Overflow RPM
    return (uint32_t)(30.0f * 1000000.0f / periodUs);
}

/**
 * Retourne la valeur RPM simulée pour un canal donné.
 */
static uint32_t calculateSimRPM(uint32_t periodUs, float ratio) {
    if (periodUs < MIN_TACH_PERIOD_US) return 99999;
    // RPM_simulé = RPM_réel × ratio = 30_000_000 / period_us × ratio
    return (uint32_t)(30.0f * 1000000.0f * ratio / periodUs);
}

// ============================================================================
//  SECTION 5 — WIFI SETUP
// ============================================================================

/**
 * Configure le WiFi en mode AP + STA simultané.
 * - Tente de se connecter au réseau STA mémorisé dans l'EEPROM.
 * - Si échec, crée un point d'accès AP pour la configuration locale.
 * - Les deux modes peuvent coexister (AP = 192.168.4.1).
 */

static void setupWiFi() {
    String ssid, pass;
    eepromReadWiFi(ssid, pass);
    
    WiFi.persistent(false); // Ne pas persister les connexions automatiques
    WiFi.mode(WIFI_AP_STA);
    
    if (ssid.length() > 0) {
        Serial.printf("Tentative connexion STA SSID=%s\n", ssid.c_str());
        WiFi.begin(ssid.c_str(), pass.c_str());
        
        // Attendre jusqu'à 15 secondes la connexion
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 30) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            wifiConnected = true;
            apModeOnly = false;
            Serial.printf("\nConnecté ! IP STA=%s\n", WiFi.localIP().toString().c_str());
        } else {
            Serial.println("\nConnexion STA échouée, mode AP activé.");
            apModeOnly = true;
        }
    } else {
        Serial.println("Pas de credentials WiFi mémorisés. Mode AP uniquement.");
        apModeOnly = true;
    }
    
    // Configurer le point d'accès
    WiFi.softAP(DEFAULT_AP_SSID, "");
    delay(500); // Laisser l'AP démarrer
    
    Serial.printf("AP IP=%s  |  ", WiFi.softAPIP().toString().c_str());
    if (wifiConnected) {
        Serial.printf("STA IP=%s", WiFi.localIP().toString().c_str());
    } else {
        Serial.print("(mode AP uniquement)");
    }
    Serial.println();
}

// ============================================================================
//  SECTION 6 — WEB SERVER HANDLERS
// ============================================================================

/**
 * HTML de base : page principale avec les contrôles et l'affichage RPM.
 */
static String generateHtml(const char *ip, bool connected) {
    String html = "<!DOCTYPE html>\r\n";
    html += "<html lang=\"fr\">\r\n<head>\r\n";
    html += "<meta charset=\"UTF-8\">\r\n";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\r\n";
    html += "<title>Deye TachSim</title>\r\n";
    html += "<meta http-equiv=\"refresh\" content=\"2\">\r\n"; // Auto-refresh 2s
    html += "<style>\r\n";
    html += "body { font-family: Arial, sans-serif; background: #1a1a2e; color: #eee; margin: 0; padding: 20px; }\r\n";
    html += "h1 { color: #00ff88; text-align: center; border-bottom: 2px solid #00ff88; padding-bottom: 10px; }\r\n";
    html += "h2 { color: #44aaff; margin-top: 30px; }\r\n";
    html += ".card { background: #16213e; border-radius: 12px; padding: 20px; margin: 15px auto; max-width: 600px; }\r\n";
    html += "table { width: 100%; border-collapse: collapse; }\r\n";
    html += "td, th { padding: 8px; text-align: left; border-bottom: 1px solid #333; }\r\n";
    html += ".value { font-weight: bold; color: #00ff88; font-size: 1.2em; text-align: center; }\r\n";
    html += ".label { color: #aaa; font-size: 0.9em; }\r\n";
    html += "input[type=number], input[type=text] { background: #0f3460; border: 1px solid #44aaff;\r\n";
    html += "  color: #fff; padding: 6px; border-radius: 4px; width: 80px; }\r\n";
    html += "input[type=submit] { background: #00ff88; color: #1a1a2e; border: none;\r\n";
    html += "  padding: 10px 24px; border-radius: 6px; cursor: pointer; font-size: 1em; font-weight: bold; }\r\n";
    html += "input[type=submit]:hover { background: #00cc6a; }\r\n";
    html += ".status-ok { color: #00ff88; } .status-warn { color: #ffaa00; } .status-err { color: #ff4444; }\r\n";
    html += "</style>\r\n</head>\r\n<body>\r\n";
    
    // Title
    html += "<h1>🌀 Deye TachSim — Fan RPM Simulator</h1>\r\n";
    
    // Status bar
    String wifiStatus;
    uint8_t level = WiFi.RSSI();
    if (wifiConnected) {
        wifiStatus = "<span class=\"status-ok\">● Connecté STA  |  RSSI: " + String(level) + " dBm</span>";
    } else {
        wifiStatus = "<span class=\"status-warn\">● Mode AP (" + String(ip) + ")</span>";
    }
    
    html += "<div class=\"card\"><table><tr>";
    html += "<td>" + wifiStatus + "</td>";
    html += "<td style=\"text-align:right\">";
    html += "<span class=\"label\">Uptime: </span>";
    uint32_t uptime = millis() / 1000;
    html += String(uptime / 86400) + "j " + ((uptime % 86400) / 3600) + "h ";
    html += String(((uptime % 86400) % 3600) / 60) + "m " + (uptime % 60) + "s";
    html += "</td></tr></table></div>\r\n";
    
    // Fan RPM display
    uint32_t rpm1 = calculateRPM(fan1PeriodUs);
    uint32_t rpm1sim = calculateSimRPM(fan1PeriodUs, fan1Ratio);
    uint32_t rpm2 = calculateRPM(fan2PeriodUs);
    uint32_t rpm2sim = calculateSimRPM(fan2PeriodUs, fan2Ratio);
    
    html += "<div class=\"card\"><h2>📊 Mesure &amp; Simulation des RPM</h2>\r\n";
    html += "<table><tr><th></th><th>Fan 9 cm (NF-A9)</th><th>Fan 6 cm (NF-A6x25)</th></tr>\r\n";
    
    html += "<tr><td class=\"label\">Période mesurée</td>";
    if (fan1Active) {
        html += "<td class=\"value\">" + String(fan1PeriodUs) + " µs</td>";
    } else {
        html += "<td class=\"status-err\">— Arrêt</td>";
    }
    if (fan2Active) {
        html += "<td class=\"value\">" + String(fan2PeriodUs) + " µs</td>";
    } else {
        html += "<td class=\"status-err\">— Arrêt</td>";
    }
    html += "</tr>\r\n";
    
    html += "<tr><td class=\"label\">RPM réels</td>";
    if (fan1Active) {
        html += "<td class=\"value\">" + String(rpm1) + " RPM</td>";
    } else {
        html += "<td class=\"status-err\">—</td>";
    }
    if (fan2Active) {
        html += "<td class=\"value\">" + String(rpm2) + " RPM</td>";
    } else {
        html += "<td class=\"status-err\">—</td>";
    }
    html += "</tr>\r\n";
    
    html += "<tr><td class=\"label\">RPM simulés</td>";
    if (fan1Active) {
        html += "<td class=\"value status-ok\">" + String(rpm1sim) + " RPM</td>";
    } else {
        html += "<td class=\"status-err\">—</td>";
    }
    if (fan2Active) {
        html += "<td class=\"value status-ok\">" + String(rpm2sim) + " RPM</td>";
    } else {
        html += "<td class=\"status-err\">—</td>";
    }
    html += "</tr>\r\n";
    
    // Period to simulated period
    float simPeriod1 = fan1Active ? ((float)fan1PeriodUs / fan1Ratio) : 0;
    float simPeriod2 = fan2Active ? ((float)fan2PeriodUs / fan2Ratio) : 0;
    
    html += "<tr><td class=\"label\">Période simulée</td>";
    if (fan1Active) {
        html += "<td class=\"value\">" + String(simPeriod1, 0) + " µs</td>";
    } else {
        html += "<td class=\"status-err\">—</td>";
    }
    if (fan2Active) {
        html += "<td class=\"value\">" + String(simPeriod2, 0) + " µs</td>";
    } else {
        html += "<td class=\"status-err\">—</td>";
    }
    html += "</tr></table></div>\r\n";
    
    // Fan ratio controls
    html += "<div class=\"card\"><h2>⚙️ Ratios Multiplicateurs</h2>\r\n";
    html += "<form method=\"GET\" action=\"/save\">\r\n";
    html += "<table><tr>";
    html += "<td><label>Fan 9 cm : </label></td>";
    html += "<td><input type=\"number\" name=\"fan1ratio\" step=\"0.1\" min=\"0.5\" max=\"10.0\" value=\"" + String(fan1Ratio, 1) + "\" /></td>";
    html += "<td class=\"label\">(× facteur RPM)</td></tr>\r\n";
    
    html += "<tr><td><label>Fan 6 cm : </label></td>";
    html += "<td><input type=\"number\" name=\"fan2ratio\" step=\"0.1\" min=\"0.5\" max=\"10.0\" value=\"" + String(fan2Ratio, 1) + "\" /></td>";
    html += "<td class=\"label\">(× facteur RPM)</td></tr>\r\n";
    
    html += "</table><br/><input type=\"submit\" value=\"💾 Enregistrer et Redémarrer\" />\r\n";
    html += "<p class=\"label\">Les ratios seront sauvegardés en EEPROM. Le système redémarre pour appliquer.</p>\r\n";
    html += "</form></div>\r\n";
    
    // WiFi configuration
    String savedSSID = "";
    {
        String tempSsid, tempPass;
        eepromReadWiFi(tempSsid, tempPass);
        savedSSID = tempSsid;
    }
    
    html += "<div class=\"card\"><h2>📶 Configuration WiFi (mode STA)</h2>\r\n";
    html += "<form method=\"GET\" action=\"/wifisave\">\r\n";
    html += "<table><tr>";
    html += "<td><label>SSID : </label></td>";
    html += "<td><input type=\"text\" name=\"ssid\" value=\"" + savedSSID + "\" size=\"20\" /></td></tr>\r\n";
    
    html += "<tr><td><label>Mot de passe : </label></td>";
    html += "<td><input type=\"password\" name=\"pass\" size=\"25\" /></td></tr>\r\n";
    
    html += "</table><br/><input type=\"submit\" value=\"💾 Enregistrer et Redémarrer\" />\r\n";
    html += "<p class=\"label\">Après redémarrage, le module tentera de se connecter au réseau indiqué.</p>\r\n";
    html += "</form></div>\r\n";
    
    // Footer
    html += "<div class=\"card\" style=\"text-align:center; margin-top:30px;\">\r\n";
    html += "<span class=\"label\">Deye TachSim v1.0  |  ";
    html += "Wemos D1 Mini (ESP8266)  |  ";
    html += "Onduleur : Deye SUN-8K-SG05LP1</span>\r\n";
    html += "</div>\r\n";
    
    html += "</body>\r\n</html>";
    return html;
}

/**
 * Handle GET / — Page principale du serveur web.
 */
static void handleRoot() {
    IPAddress ip = wifiConnected ? WiFi.localIP() : WiFi.softAPIP();
    String html = generateHtml(ip.toString().c_str(), wifiConnected);
    server.send(200, "text/html", html);
}

/**
 * Handle GET /save — Sauvegarde des ratios multiplicateurs.
 * Paramètres : fan1ratio, fan2ratio (GET)
 */
static void handleSave() {
    String f1Str = server.arg("fan1ratio");
    String f2Str = server.arg("fan2ratio");
    
    if (f1Str.length() > 0) {
        float newRatio1 = f1Str.toFloat();
        if (newRatio1 >= 0.5f && newRatio1 <= 10.0f) {
            fan1Ratio = newRatio1;
            fan1RatioInt10 = (int16_t)(newRatio1 * 10 + 0.5f);
        }
    }
    
    if (f2Str.length() > 0) {
        float newRatio2 = f2Str.toFloat();
        if (newRatio2 >= 0.5f && newRatio2 <= 10.0f) {
            fan2Ratio = newRatio2;
            fan2RatioInt10 = (int16_t)(newRatio2 * 10 + 0.5f);
        }
    }
    
    // Sauvegarder en EEPROM
    eepromWriteRatios(fan1Ratio, fan2Ratio);
    
    Serial.printf("Ratios sauvegardés: Fan1=%.1f  Fan2=%.1f\n", fan1Ratio, fan2Ratio);
    
    server.sendHeader("Location", "/");
    server.send(303); // Redirect after GET
    
    // Redémarrer après un bref délai pour laisser le navigateur recevoir la redirection
    delay(1000);
    ESP.restart();
}

/**
 * Handle GET /wifisave — Sauvegarde des credentials WiFi.
 * Paramètres : ssid, pass (GET)
 */
static void handleWifiSave() {
    String newSsid  = server.arg("ssid");
    String newPass  = server.arg("pass");
    
    if (newSsid.length() > 0) {
        eepromSaveWiFi(newSsid.c_str(), newPass.length() > 0 ? newPass.c_str() : "");
        
        // Mettre à jour la connexion WiFi immédiatement si possible
        WiFi.disconnect(false);
        delay(100);
        WiFi.begin(newSsid.c_str(), newPass.c_str());
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            wifiConnected = true;
            apModeOnly = false;
            Serial.printf("\nConnecté à '%s' ! IP=%s\n", newSsid.c_str(), WiFi.localIP().toString().c_str());
        } else {
            // En mode AP, l'on peut quand même redémarrer normalement
            WiFi.softAP(DEFAULT_AP_SSID, "");
            Serial.println("\nConnexion STA échouée. AP conservé.");
        }
    }
    
    server.sendHeader("Location", "/");
    server.send(303);
    
    delay(1000);
    ESP.restart();
}

/**
 * Handle GET /api/status — Retour JSON pour monitoring asynchrone (facultatif).
 */
static void handleApiStatus() {
    String json = "{";
    json += "\"fan1PeriodUs\":" + String(fan1PeriodUs) + ",";
    json += "\"fan2PeriodUs\":" + String(fan2PeriodUs) + ",";
    json += "\"fan1Active\":" + String(fan1Active ? 1 : 0) + ",";
    json += "\"fan2Active\":" + String(fan2Active ? 1 : 0) + ",";
    json += "\"fan1Ratio\":\"" + String(fan1Ratio, 1) + "\",";
    json += "\"fan2Ratio\":\"" + String(fan2Ratio, 1) + "\",";
    json += "\"fan1RPM\":" + String(calculateRPM(fan1PeriodUs)) + ",";
    json += "\"fan2RPM\":" + String(calculateRPM(fan2PeriodUs)) + ",";
    json += "\"fan1SimRPM\":" + String(calculateSimRPM(fan1PeriodUs, fan1Ratio)) + ",";
    json += "\"fan2SimRPM\":" + String(calculateSimRPM(fan2PeriodUs, fan2Ratio));
    json += "}";
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
}

/**
 * Handle 404 — Page non trouvée.
 */
static void handleNotFound() {
    server.send(404, "text/plain", "404: Not Found\r\nRoutes disponibles : / /save /wifisave /api/status");
}

// ============================================================================
//  SECTION 7 — HARDWARE INIT
// ============================================================================

/**
 * Configure les circuits d'entrée et sortie GPIO.
 */
static void setupHardware() {
    // Sorties tach (pilotent les transistors NPN des circuits de sortie).
    // Initialiser en HIGH : GPIO HIGH → NPN OFF → collecteur flottant,
    // l'onduleur tire la ligne vers son pull-up interne → lit un niveau HAUT.
    // C'est l'état « ventilateur à l'arrêt » requis pour le tach de l'onduleur.
    pinMode(FAN1_SIM_OUT, OUTPUT);
    pinMode(FAN2_SIM_OUT, OUTPUT);
    digitalWrite(FAN1_SIM_OUT, HIGH);  // NPN OFF → stopped
    digitalWrite(FAN2_SIM_OUT, HIGH);  // NPN OFF → stopped
    
    // Entrées tach : mode INPUT standard.
    // Le circuit d'entrée externe (Schottky clamp + resistor) gère la protection et la
    // résistance de tirage. Ne JAMAIS utiliser INPUT_PULLUP ici car le circuit externe
    // fournit un pull-down 10k qui dominerait les pullups internes (~30k).
    pinMode(FAN1_TACH_IN, INPUT);
    pinMode(FAN2_TACH_IN, INPUT);
    
    // LED de statut
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, HIGH); // Éteinte au démarrage
    
    Serial.println("[Hardware] Entrées/sorties configurées.");
}

/**
 * Initialise le Timer1 matériel pour la génération du signal tach.
 */
static void setupTachOutputTimer() {
    // Créer et configurer le Timer1 (numéro=1 dans hw_timer_t)
    tachTimer = timerBegin(1, TIMER_PRESCALER, true);  // Prescaler=80 → tick de 1 µs
    
    // Attacher l'ISR
    timerAttachInterrupt(tachTimer, &tachOutputAllISR);
    
    // Programme le premier interrupt dans le futur (valeur arbitraire, sera réécrite dans l'ISR)
    uint32_t firstTick = micros() + TACH_UPDATE_TICK_US;
    timerWrite(tachTimer, firstTick);
    
    Serial.printf("[Timer1] Génér. tach active (résolution=%d µs)\n", US_PER_TICK);
}

// ============================================================================
//  SECTION 8 — SETUP
// ============================================================================

void setup() {
    // Démarrage série pour le debugging (décommenter si besoin)
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("================================================");
    Serial.println(" Deye TachSim — Démarrage");
    Serial.println("------------------------------------------------");
    
    // Lecture des ratios et WiFi depuis l'EEPROM
    eepromLoadRatios();
    Serial.printf("[EEPROM] Ratios chargés: Fan1=%.1f  Fan2=%.1f\n", fan1Ratio, fan2Ratio);
    
    // Configuration matérielle
    setupHardware();
    
    // Initialisation des états de sortie simulée (fan stopped state).
    // simStateFanX = true est la valeur par défaut (global) → GPIO HIGH.
    // fan1Active / fan2Active restent à false jusqu'à ce que le premier
    // front tach soit détecté → le Timer1 ISR forcera HIGH permanent tant
    // qu'aucun ventilateur ne tourne. La sortie reste inerte (=stopped state)
    // pendant toute la phase de boot avant réception d'un vrai signal.
    simStateFan1 = true;
    simStateFan2 = true;
    
    // Configuration du WiFi
    setupWiFi();
    
    // Initialiser les interruptions GPIO tach (après le WiFi pour éviter conflits)
    attachInterrupt(digitalPinToInterrupt(FAN1_TACH_IN), fan1TachISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(FAN2_TACH_IN), fan2TachISR, FALLING);
    
    // Initialiser la génération tach (Timer1)
    setupTachOutputTimer();
    
    // Configuration du serveur web
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_GET, handleSave);
    server.on("/wifisave", HTTP_GET, handleWifiSave);
    server.on("/api/status", HTTP_GET, handleApiStatus);
    server.onNotFound(handleNotFound);
    server.begin();
    
    Serial.println("[Web] Serveur web démarré sur le port 80.");
    Serial.println("================================================");
    Serial.println();
}

// ============================================================================
//  SECTION 9 — MAIN LOOP
// ============================================================================

void loop() {
    // ─── 1. Gestion du serveur web (peut bloquer, mais n'affecte PAS les ISR) ──
    server.handleClient();
    
    // ─── 2. Mise à jour des drapeaux de timeout ventilateurs ────────────────
    // Si aucun front descendant reçu depuis TACH_STALE_MS, marquer comme à l'arrêt.
    // CRITIQUE : NE PAS appeler digitalWrite() ici ! Le Timer1 ISR gère exclusivement
    // les pins de sortie. On doit juste arrêter la génération en mettant simStateFan
    // à true (HIGH = NPN OFF = ligne tach HIGH) et periodUs à MAX.
    uint32_t nowMs = millis();
    static uint32_t lastCheck1 = 0, lastCheck2 = 0;
    
    if (nowMs - lastCheck1 > 500) {
        lastCheck1 = nowMs;
        
        // Fan 1 timeout check
        if (fan1Active && lastFan1IRQ_us != 0) {
            uint32_t irqAgeUs = micros() - lastFan1IRQ_us;
            if (irqAgeUs > (uint32_t)(TACH_STALE_MS * 1000)) {
                fan1Active       = false;
                fan1PeriodUs     = MAX_TACH_PERIOD_US;
                simPeriodUsFan1  = MAX_TACH_PERIOD_US;
                simStateFan1     = true;   // Réinitialiser → HIGH (fan stopped)
                lastFan1IRQ_us   = 0;
            }
        }
        
        // Fan 2 timeout check
        if (fan2Active && lastFan2IRQ_us != 0) {
            uint32_t irqAgeUs = micros() - lastFan2IRQ_us;
            if (irqAgeUs > (uint32_t)(TACH_STALE_MS * 1000)) {
                fan2Active       = false;
                fan2PeriodUs     = MAX_TACH_PERIOD_US;
                simPeriodUsFan2  = MAX_TACH_PERIOD_US;
                simStateFan2     = true;   // Réinitialiser → HIGH (fan stopped)
                lastFan2IRQ_us   = 0;
            }
        }
    }
    
    // ─── 3. Indication LED de statut ────────────────────────────────────────
    // Séquence de clignotement : ON quand simulation active, BLINK lent en mode AP
    static uint32_t ledBlinkLast = 0;
    bool anySimulating = fan1Active || fan2Active;
    
    if (nowMs - ledBlinkLast > 500) {
        ledBlinkLast = nowMs;
        
        if (anySimulating) {
            // Simulation active → LED ON constante
            digitalWrite(STATUS_LED, LOW);  // Active LOW → ON
        } else if (apModeOnly) {
            // Mode AP uniquement → LED clignote lentement (2s cycle)
            static bool ledState = true;
            ledState = !ledState;
            digitalWrite(STATUS_LED, ledState ? HIGH : LOW);  // HIGH=OFF, LOW=ON
        } else {
            // STA connecté mais aucune simulation → LED OFF clignote court
            static bool ledState = true;
            ledState = !ledState;
            digitalWrite(STATUS_LED, ledState ? HIGH : LOW);
        }
    }
    
    // ─── 4. Réinitialisation des compteurs au premier démarrage ──────────────
    // Au tout premier tour de loop, remettre lastFanXIRQ_us à 0 pour que le
    // premier front descendant ne soit pas compté comme une période très longue.
    // IMPORTANT : NE PAS réinitialiser simPeriodUsFanX ici — il a pu être mis
    // à jour pendant setup() par un front tach anticipé. Le réinitialiser
    // supprimerait cette mesure valide.
    static bool firstLoop = true;
    if (firstLoop) {
        lastFan1IRQ_us = 0;
        lastFan2IRQ_us = 0;
        fan1Active = false;   // Réactiver depuis le tout premier front
        fan2Active = false;
        fan1PeriodUs = MAX_TACH_PERIOD_US;   // Valeur par défaut (pas de mesure)
        fan2PeriodUs = MAX_TACH_PERIOD_US;
        // simPeriodUsFanX conserve leur valeur actuelle, initiale ou mise à jour
        // par un ISR pendant setup()
        firstLoop = false;
    }
}

// ============================================================================
//  NOTES DE BRASSAGE ET COMPILATION
// ============================================================================
/*
 * Compilation avec Arduino IDE :
 *   - Carte : LOLIN(WEMOS) D1 mini R2 & R1 (ou LOLIN D32 pour ESP32, adapter pins)
 *   - Processeur : 80 MHz ou 160 MHz (80 MHz plus stable avec les ISR précises)
 *   - Reset Method: nodemcu / ck
 *   - Flash Mode: qio
 *   - Flash Frequency: 40MHz
 *   - CPU Frequency: 80MHz
 *   - Port: sélectionner le port COM USB
 *   - Upload Speed: 921600
 *
 * Bibliothèques requises (toutes incluses dans le noyau ESP8266 Arduino) :
 *   - Arduino.h          (core)
 *   - EEPROM.h           (core)
 *   - ESP8266WiFi.h      (noyau ESP8266 WiFi)
 *   - ESP8266WebServer.h (noyau ESP8266 web server)
 */
