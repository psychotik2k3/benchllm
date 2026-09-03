# Project: Deye Fan Speed Simulation & Management

## Overview
This project uses a Weemos D1 Mini to intercept and process RPM signals from two different fan sizes (6cm and 9cm) and output a "corrected" tach signal to the Deye inverter. This ensures the inverter correctly recognizes the speed of the replaced Noctua fans by applying specific multipliers (ratios).

## Hardware Components
- **Controller**: Weemos D1 Mini V2.3.0 (ESP8266)
- **Transistors**: 2N3904 or 2N2222 (used for the "Universal" output buffer)
- **Resistors**: 10k Ohm (for input signals), 1k Ohm / 100 Ohm (for output stage)
- **LED**: Standard LED + 220 Ohm resistor

## Connection Diagram

### Input Section (Fan Signals)
| Weemos Pin | Component | Target Device | Purpose |
|------------|-----------|---------------|---------|
| D1        | 10k Res.  | 9cm Fan Tach  | Read raw RPM from the 9cm circuit |
| D2        | 10k Res.  | 6cm Fan Tach  | Read raw RPM from the 6cm circuit |

### Output Section (Simulated Signal)
| Weemos Pin | Component | Connection | Purpose |
|------------|-----------|------------|---------|
| D5         | NPN Trans | Base via 1kR | Driver for the "Simulated" tach pulse |
| [N/A]      | Output    | To Deye Tach | The output pin is shared by the transistor circuit to handle up to 12V. |

### Visual Feedback
| Weemos Pin | Component | Purpose |
|------------|-----------|---------|
| D6 (LED)   | LED + R   | Solid on boot, blinks 2x/sec when data is valid. |

## Circuit Logic - "The Universal Solution"
To ensure the output works whether the Deye expects 3.3V, 5V, or 12V:
We use an NPN transistor (e.g., 2N3904). The Weemos toggles its pin (D5) to switch the transistor. 
- **Why?** This isolates the Weemos from the ground/voltage of the Deye's tach line while providing a clean square wave pulse for the Deye's controller.

## Software Logic
1. **Interrupts**: `IRQ_D1` and `IRQ_D2` count pulses every time a fan rotates.
2. **Calculation**: 
   - $RPM_{9cm} = \text{Count}_{D1} \times \text{Ratio}_1$
   - $RPM_{6cm} = \text{Count}_{D2} \times \text{Ratio}_2$
3. **Simulated Output**: A timer generates a pulse train whose frequency is proportional to the calculated result.

## Web Interface
- **WiFi Manager**: Automatic setup of AP and Station mode.
- **Dashboard**: Displays raw RPM, calculated RPM, and allows setting ratios via sliders/inputs.
- **Anti-Refresh Lock**: Uses simple CSS focus or Javascript state to ensure that if a user is typing a value when the page refreshes (e.g., due to a watchdog or signal loss), the cursor stays in the box.
