# Deye Fan Tachometer Simulator — Challenge Summary and Engineering Pitfalls

## 1. Problem Statement

The target system is a Deye SUN-8K-SG05LP1-EU-AM2-P solar inverter whose original 12 V NMB fans are being replaced with quieter, slower Noctua fans:

- Two Noctua NF-A6x25-FLX fans replace the original 60 mm fan.
- Two Noctua NF-A9-FLX fans replace the original 90 mm fan.
- Only one tachometer signal from each fan pair must be measured.

The inverter monitors fan tachometer signals and may raise an alarm or change its behavior when the reported speed is lower than expected. The requested device must therefore measure the actual Noctua fan speeds, multiply each speed by a configurable ratio, and generate two synthetic tachometer signals that represent higher RPM values.

This is not merely a Web-connected RPM counter. It is a real-time signal-acquisition and signal-generation system operating next to power electronics, with safety, timing, electrical compatibility, startup, and failure-mode constraints.

## 2. Target Platform and Available Hardware

- MCU: LOLIN/Wemos D1 mini V2.3.0, ESP-12S / ESP8266.
- Fan interface: three wires per fan — GND, +12 V, and open-collector tach.
- Available discrete components: resistors and common NPN transistors such as 2N2222, 2N3904, BC547, BC337, S8050, SS8050, and similar devices.
- Firmware environment: Arduino IDE for the LOLIN/Wemos D1 mini.

The final design must include both:

1. Complete, compilable Arduino firmware (`deye_fan.ino`).
2. A complete electrical design covering power, both tach inputs, both synthetic tach outputs, component values, and wiring.

## 3. Required Functional Behavior

The implementation must:

- Measure one 60 mm Noctua tach signal and one 90 mm Noctua tach signal independently.
- Apply an independently configurable multiplication ratio to each channel.
- Generate one synthetic tach signal per Deye fan input.
- Preserve the standard PC-fan convention of two tach pulses per revolution unless verified otherwise.
- Provide an AP+STA Web interface.
- Allow ratio and WiFi configuration without recompilation.
- Persist configuration across reboots.
- Display measured and simulated RPM for both channels.
- Use a status LED to distinguish active simulation from waiting for an input signal.
- Keep tach acquisition and generation stable during heavy WiFi or HTTP activity.

## 4. Core Tachometer Mathematics

For a fan producing two pulses per revolution:

```text
RPM = frequency_hz × 60 / 2
RPM = frequency_hz × 30
RPM = 30,000,000 / period_us
```

If the required output RPM is the measured RPM multiplied by ratio `R`:

```text
output_frequency = input_frequency × R
output_period = input_period / R
output_half_period = input_period / (2 × R)
```

Common mistakes include multiplying the period by the ratio, confusing period with half-period, applying the pulse-per-revolution factor twice, or calculating RPM from a cumulative pulse counter without a valid time window.

## 5. Real-Time Architecture Pitfalls

### Hardware Timer vs Software Timing

The output should use the ESP8266 hardware Timer1/FRC1 or another demonstrably deterministic hardware mechanism. `Ticker`, `millis()`, polling in `loop()`, `delayMicroseconds()`, `tone()`, and software PWM are generally unsuitable for precise dual-channel tach synthesis under WiFi load.

Timer1 is shared by both output channels. A robust design normally schedules the nearest pending edge, updates absolute deadlines, and rearms the one-shot timer. Independent phases must be preserved.

### Drift and Jitter

Scheduling the next edge as `now + halfPeriod` after every interrupt accumulates interrupt latency into the waveform. Prefer an absolute phase such as:

```cpp
nextEdge += halfPeriod;
```

If the ISR is late by more than one interval, resynchronize without emitting a burst of artificial edges.

### ISR and IRAM Safety

Marking an ISR `IRAM_ATTR` is not enough. Every transitively called helper, constant, and data access must also be safe while the flash cache is unavailable. Particular attention is required for:

- `micros()` and framework helpers;
- 32-bit or 64-bit division and multiplication helpers;
- `timer1_write()` and private SDK APIs;
- constants accidentally stored in flash;
- EEPROM or flash writes occurring concurrently.

The ESP8266 core version should be pinned, the linker map inspected, and the firmware compiled in CI.

### Shared-State Consistency

Atomic 32-bit writes do not make a multi-field update transactional. A period, timestamp, state flag, and output setpoint can be observed from different updates. Use a short critical section where valid, or a versioned snapshot/seqlock for NMI-to-loop publication.

`noInterrupts()` does not necessarily protect against an NMI. The previous interrupt state should also be restored instead of blindly calling `interrupts()`.

## 6. Input Measurement and Filtering Traps

- The first edge is only a reference; it is not a valid period.
- After a stall, the first returning edge is also only a reference.
- A rejected glitch must not refresh the signal-validity timeout.
- Period calculations must support timer wraparound through unsigned subtraction.
- Intermediate arithmetic must be wide enough to avoid 32-bit overflow.
- Input periods must be bounded to a realistic fan-speed range.
- A single valid-looking interval should not immediately enable a trusted output.

A robust channel state machine distinguishes at least:

- no edge received;
- first edge received;
- validating;
- running;
- stale/stalled;
- fault.

Useful filtering can combine:

- minimum and maximum period validation;
- relative outlier rejection;
- confirmation over several coherent periods;
- a short median filter;
- an EMA/IIR reset after signal loss.

Filtering must advance only when a new measurement arrives. Reapplying the same sample on every `loop()` iteration creates a load-dependent filter or slew rate.

## 7. Fail-Safe Semantics

The design must explicitly define what happens when a real fan stalls, disconnects, or produces invalid pulses.

Maintaining the last valid synthetic RPM indefinitely defeats genuine fan-failure detection. A safer default is to release the Deye tach line after a short, justified timeout. The timeout should be related to the expected fan period rather than chosen arbitrarily.

When a stall is detected:

- disable and electrically release the output;
- invalidate the old period and output setpoint;
- reset filters and validation counters;
- require multiple coherent periods before restarting.

Otherwise, one isolated edge after a stall can reactivate a stale synthetic frequency and falsely report a running fan.

Only one fan tach from each physical pair should be measured. Connecting two unsynchronized open-collector tach outputs in parallel does not add their frequencies: low pulses overlap, edges disappear, and a failed fan can be masked by the other fan.

## 8. Boot Readiness

“Timer initialized before WiFi” does not mean “valid tach available at boot.”

The design must account for:

- MCU startup time;
- storage initialization or filesystem formatting;
- fan spin-up;
- the first reference edge;
- enough subsequent edges to validate a period;
- the delay until the first generated output edge.

Blocking WiFi association before Timer1 initialization is unacceptable. However, even a non-blocking design can expose a zero-RPM window long enough for the inverter to raise an alarm.

Possible strategies include a measured grace period, an explicit boot state, or a strictly bounded provisional RPM. A provisional signal has safety implications and must not hide a fan that never starts.

There is also a physical circular-dependency risk: if the Wemos is powered only from fan rails that the inverter enables after validating tach, the simulator cannot start. This behavior must be tested on the actual inverter.

## 9. Electrical Interface Pitfalls

### Noctua Tach Inputs

The Noctua tach output is normally open collector. It requires a pull-up to the ESP8266's 3.3 V rail. Never pull an ESP8266 GPIO up to 5 V or 12 V.

The input stage should include appropriate current limiting and, where needed, transistor isolation, Schmitt behavior, or filtering. The schematic must distinguish the fan's +12 V wire from its tach wire.

### Deye Tach Outputs

The Deye tach pull-up voltage is unknown and may be 3.3 V, 5 V, or 12 V. The ESP8266 GPIO must never be connected directly to it.

Use an NPN open-collector output:

- emitter to common ground;
- collector to the Deye tach input;
- GPIO to base through a calculated resistor;
- base-emitter pull-down so the transistor remains off during reset.

Check transistor `VCEO`, pinout, required collector current, forced gain, base current, and low-level saturation voltage. “Any NPN works” is not an acceptable component qualification.

### Dual-Rail Power OR-ing

The two fan connectors may be powered at different times or voltages. They must not backfeed one another.

Use correctly oriented Schottky diodes or a qualified ideal-diode/MOSFET circuit. A transistor junction used as a diode must be checked for polarity and current capability; `VCBO` or `VEBO` is a maximum blocking rating, not evidence that the chosen direction conducts.

Never join the rails before the OR-ing devices.

### 12 V to 5 V Conversion

Never connect 12 V directly to the Wemos 5 V pin.

The converter must support:

- the actual minimum and maximum fan-rail voltage;
- ESP8266 WiFi current peaks;
- rail PWM or interruptions, if present;
- thermal dissipation;
- startup and brownout behavior;
- reverse current and transients.

A buck converter cannot maintain 5 V from an input near or below 5 V. A resistive or discrete linear preregulator can overheat or collapse during WiFi peaks. Prefer a qualified buck-boost solution when the rail can fall near 5 V.

Add suitable bulk and local decoupling, reverse protection, PTC/fuse protection, and TVS protection where justified. Verify that both Deye grounds are actually equipotential before tying them together.

## 10. Web, Memory, and Persistence Pitfalls

The Web interface controls a signal used by the inverter and should not be treated as harmless telemetry.

Required protections include:

- authentication;
- CSRF/origin protection;
- strict server-side numeric validation;
- bounded string copies and NUL termination;
- HTML and JSON escaping;
- no password readback through APIs;
- explicit HTTP errors instead of silent clamping;
- rate limiting for configuration writes.

Store static HTML in `PROGMEM` and prefer bounded `snprintf` buffers for status JSON. Building a large page with repeated `String` concatenation can fragment the ESP8266 heap.

Persistent configuration should include:

- magic value;
- version and structure size;
- CRC/checksum;
- finite-number and range validation;
- migration or safe fallback;
- ideally dual slots or another atomic-update strategy.

CRC coverage should use `offsetof(Config, crc)`, not assumptions such as `sizeof(Config) - sizeof(crc)`, because compiler padding can accidentally include the CRC field itself. Avoid writing unchanged values repeatedly, and check the result of the commit operation.

## 11. Verification Expected Before Delivery

A credible submission should include or enable:

- a reproducible `arduino-cli` or PlatformIO build with pinned versions;
- compilation warnings treated seriously;
- unit tests for RPM, period, half-period, ratios, saturation, and wraparound;
- tests for first edge, signal loss, recovery, outliers, and full capture buffers;
- a save/reload round-trip test for persistent configuration;
- oscilloscope measurements of frequency, duty cycle, jitter, and startup;
- measurements under HTTP traffic, WiFi reconnects, and flash writes;
- power tests with each fan rail alone, both rails together, low voltage, rail switching, and WiFi current peaks;
- thermal checks of the converter and output transistors;
- verification of Deye tach pull-up voltage and accepted signal levels.

Claims such as “zero jitter,” “fully isolated from WiFi,” or “works at any tach voltage” require measured evidence and a reproducible test procedure.

## 12. What a Strong Submission Looks Like

A strong solution combines:

- a compilable, version-pinned firmware;
- event-driven hardware Timer1 scheduling with absolute deadlines;
- minimal and transitively IRAM-safe ISRs;
- coherent cross-context state publication;
- correct 2 PPR mathematics;
- explicit startup, validation, stall, and recovery states;
- realistic filtering tied to new measurements;
- fail-safe output release after signal loss;
- electrically correct 3.3 V inputs and open-collector outputs;
- safe dual-rail OR-ing and a qualified power converter;
- bounded heap usage and robust persistence;
- a secured Web configuration surface;
- complete calculations, BOM, schematic, and measured validation results.

The central challenge is to satisfy all of these constraints simultaneously. A polished Web UI or sophisticated timer code cannot compensate for a non-compiling sketch, an invalid tach formula, unsafe power circuitry, or failure behavior that hides a stopped fan.
