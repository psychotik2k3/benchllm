#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Ticker.h>

/*
 * PROJECT: Deye Fan Simulation & Management
 * HARDWARE: Weemos D1 Mini (ESP-12S)
 *
 * COMPONENTS USED:
 * - Transistors: 2N3904 or 2N2222 (for "Universal" Output Buffer)
 * - Resistors: 10k for input lines, logic resistors for LED/Circuit protection.
 *
 * CIRCUIT LOGIC:
 * - Input 1 (D1): 9cm Fan Tach Signal
 * - Input 2 (D2): 6cm Fan Tach Signal
 * - Output (D5): Pulse generator for "Simulated" RPM.
 *   - The NPN transistor allows the Weemos to drive a line that could be sensed at 3.3V, 5V, or 12V.
 * - LED (D6): Status indicator.
 *   - Solid high at startup.
 *   - Blinks at 2Hz when valid data is processed.
 */

#define PIN_TACH_9cm  10 // Physical Pin D1
#define PIN_TACH_6cm  11 // Physical Pin D2
#define PIN_OUTPUT    14 // Physical Pin D5
#define PIN_LED       12 // Physical Pin D6 (typical on Weemos)

volatile uint32_t count9 = 0;
volatile uint32_t count6 = 0;
float ratio9 = 2.0;
float ratio6 = 2.5;
uint32_t rpm9 = 0;
uint32_t rpm6 = 0;
uint32_t simRPM = 0;

Ticker calculationTicker;
Ticker ledPulseTicker;
bool ledState = true;

// Interrupt handlers (ISR) - No blocking code allowed here.
void I_9cm() { count9++; }
void I_6m() { count6++; }

// Process logic handled by Ticker to ensure it stays consistent even during WiFi spikes.
void processData() {
    rpm9 = (uint32_t)(count9 * ratio9);
    rpm6 = (uint32_t)(count6 * ratio6);
    simRPM = (rpm9 + rpm6) / 2;
}

ESP8266WebServer server(80);

const char* html_page = R"=====(
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset='UTF-8'>
    <title>Deye Fan Controller</title>
    <style>
        body { font-family: 'Segoe UI', sans-serif; background:#f4f7f6; text-align:center; color: #333; }
        .card { background: white; padding: 30px; border-radius: 15px; display: inline-block; margin-top: 80px; box-shadow: 0 8px 20px rgba(0,0,0,0.1); }
        input { padding: 10px; margin: 10px; border: 1px solid #ccc; border-radius: 8px; width: 90px; font-size: 1.1em; }
        h1 { color: #2c3e50; margin-bottom: 10px; }
        h3 { color: #7f8c8d; }
        .status-val { font-weight: bold; color: #2980b9; font-size: 1.4em; }
    </style>
</head>
<body>
    <div class='card'>
        <h1>Gestion Ventilation Deye</h1>
        <div>
            <p>Segment 9cm | Réel: <span id='r9'>0</span> | Ratio: <input type='number' step='0.1' id='v9' value='2.0'></p>
            <p>Segment 6cm | Réel: <span id='r6'>0</span> | Ratio: <input type='number' step='0.1' id='v6' value='2.5'></p>
        </div>
        <hr>
        <h3>Signal de Sortie Simulé</h3>
        <p>RPM Simulé : <span id='rs'>0</span></p>
    </div>

    <script>
        // Update the UI automatically without reloading the page to preserve input focus.
        setInterval(function(){
            fetch('/data').then(r=>r.json()).then(d=>{
                document.getElementById('r9').innerText = d.r9;
                document.getElementById('r6').innerText = d.r6;
                document.getElementById('rs').innerText = d.rs;
            });
        }, 1000);
    </script>
</body>
</html>
)======";

void handleRoot() {
    server.send(200, "text/html", html_page);
}

void handleData() {
    String json = "{\"r9\":" + String(rpm9) + ",\"r6\":" + String(rpm6) + ",\"rs\":" + String(simRPM) + "}";
    server.send(200, "application/json", json);
}

void setup() {
    Serial.begin(115200);

    pinMode(PIN_TACH_9cm, INPUT_PULLUP);
    pinMode(PIN_TACH_6cm, INPUT_PULLUP);
    pinMode(PIN_OUTPUT, OUTPUT);
    pinMode(PIN_LED, OUTPUT);

    // Hardware Interrupts for high precision counting
    attachInterrupt(digitalPinToInterrupt(PIN_TACH_9cm), I_9cm, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_TACH_6m), I_6m, FALLING);

    // Use Ticker to handle logic in the background.
    calculationTicker.attach(0.5, processData);
    ledPulseTicker.attach(0.5, []{
        if(count9 > 0 || count6 > 0) {
            ledState = !ledState;
            digitalWrite(PIN_LED, ledState);
        }
    });

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("Deye-Fan-Config", "12345678");

    server.on("/", handleRoot);
    server.on("/data", handleData);
    server.begin();
}

void loop() {
    server.handleClient();
}
```)