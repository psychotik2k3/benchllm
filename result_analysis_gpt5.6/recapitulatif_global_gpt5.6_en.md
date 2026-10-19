# Global Summary — GPT-5.6

## 1. Analysis Inventory

The disk contains **22 folders** under `competitors`, all covered by an analysis. Here, `kimi k3` refers to the **thinking high variant** analyzed from `competitors/kimi k3`; `kimi_k3_max` is the **Max variant**, retained as a separate competitor.

### ✅ Analyses Existing Before This Run (19)

- `claude opus 5 medium` — `analyse_claude opus 5 medium.md`
- `claude-sonnet5-free` — `analyse_claude-sonnet5-free.md`
- `composer 2.5` — `analyse_composer 2.5.md`
- `composer 2.5 prompt from claude` — `analyse_composer 2.5 prompt from claude.md`
- `deepseek4 flash` — `analyse_deepseek4 flash.md`
- `gemini 3.1 pro` — `analyse_gemini 3.1 pro.md`
- `gemini-free` — `analyse_gemini-free.md`
- `gemma4` — `analyse_gemma4.md`
- `glm5.3` — `analyse_glm5.3.md`
- `gpt-5.6 sol medium` — `analyse_gpt-5.6 sol medium.md`
- `gpt-free` — `analyse_gpt-free.md`
- `grok 4.6 medium` — `analyse_grok 4.6 medium.md`
- `Grok_4.7_medium` — `analyse_Grok_4.7_medium.md`
- `kimi k3` (**thinking high**) — `analyse_kimi k3.md`
- `kimi_k3_max` (**Max variant**) — `analyse_kimi_k3_max.md`
- `qwen3.6` — `analyse_qwen3.6.md`
- `qwen3.6-35b-fp8-freetoken` — `analyse_qwen3.6-35b-fp8-freetoken.md`
- `qwen3.6_prompt_from_claude` — `analyse_qwen3.6_prompt_from_claude.md`
- `qwen35` — `analyse_qwen35.md`

### 🆕 New Analyses Produced During This Run (3)

- `Gemini_4_Argon_medium` — `analyse_Gemini_4_Argon_medium.md`
- `glm_5.2_high` — `analyse_glm_5.2_high.md`
- `muse_wonder_1.0_medium` — `analyse_muse_wonder_1.0_medium.md`

### ⚠️ Competitors Without an Available Analysis

- None among the 22 competitors in the run.

## 2. 🔍 Revised Scores and Rationale

**No subsequent revision is necessary.** Direct comparison of the 22 reports does not reveal any substantive inconsistency that would justify changing a previous or new score. The differences remain explained by documented blocking defects:

- `muse_wonder_1.0_medium` deserves **7.7/10** overall and **8.1/10** for quality thanks to its genuinely successful ESP8266 3.1.2 build, event-driven Timer1/NMI, 2 PPR formula, and filtering; it remains below the 8.0–8.4 trio because the CRC calculation includes its own field due to padding and rejects nearly every configuration on reboot.
- `Gemini_4_Argon_medium` remains consistent at **6.0/10** and **7.8/10**: its NMI, absolute deadlines, and bounded buffers are solid, but the first edge after a stall reactivates the old period, while the discrete power supply is undersized at 7 V and overloads its resistors during peaks.
- `glm_5.2_high` remains consistent at **1.8/10** despite **4.8/10** in apparent quality: it does not compile, its 32-bit product reduces all calculated periods below 1,000 µs and causes them to be rejected, and its schematic contradictorily connects the rails before OR-ing while recommending an LM2596 that cannot maintain 5 V from 5 V.

The leading trio therefore remains unchanged: overall balance for `gpt-5.6 sol medium`, maximum structural quality for `claude opus 5 medium`, and immediate Timer1 wake-up in `kimi_k3_max`. No score carries an asterisk.

## 3. Summary Table

Table sorted by descending overall score. Each cell contains no more than eight points faithful to the individual report.

| Version | Overall score | Code quality score | ✅ Strengths (5–8) | ❌ Weaknesses (5–8) |
|---|---:|---:|---|---|
| `gpt-5.6 sol medium` | **8.4/10** | **8.2/10** | • Timer1 one-shot at next deadline<br>• Atomic GPIO operations in ISR<br>• Exact 2 PPR formula<br>• IIR reset on stall<br>• Non-blocking AP+STA<br>• Versioned EEPROM with CRC32<br>• Detailed OR-ing, buck-boost, PTC/TVS | • No provisional tach at boot<br>• 2 s stall timeout<br>• One period is enough to reactivate<br>• IRAM dependency of `micros()` not pinned<br>• HTTP without auth/CSRF/TLS<br>• Large HTML `String` on the heap<br>• Saturation not reflected in the UI |
| `kimi_k3_max` — Max | **8.1/10** | **8.3/10** | • Timer1 one-shot with absolute deadlines<br>• Immediate wake-up from idle state<br>• Two genuinely independent channels<br>• Direct GPIO and lean ISRs<br>• EMA outside ISR, reset appropriately<br>• PROGMEM and bounded JSON<br>• CRC16 and STA reconnection<br>• Documented open-collector hardware | • No provisional tach at boot<br>• First interval calculated from zero<br>• No upper bound on period<br>• One period is enough to activate<br>• 1.5 s stall timeout<br>• HTTP without auth/CSRF<br>• 2 kHz saturation has poor observability<br>• Simple buck vulnerable to low rail |
| `claude opus 5 medium` | **8.0/10** | **8.5/10** | • Highly rigorous NMI/CCOUNT<br>• NMI-to-loop seqlock<br>• Absolute deadlines and bounded catch-up<br>• Configurable input/output PPR<br>• Validation followed by integer EMA<br>• PROGMEM and rich diagnostics<br>• EEPROM CRC32/version<br>• Detailed hardware and tests | • Unpinned private NMI APIs<br>• Potentially aggressive NMI load<br>• No tach before an actual measurement<br>• Only one tach read per pair<br>• `apPass` exposed by the API<br>• Unauthenticated HTTP<br>• Unescaped network strings<br>• Contradictory RC values |
| `muse_wonder_1.0_medium` | **7.7/10** | **8.1/10** | • Verified ESP8266 3.1.2 build<br>• Event-driven NMI placed in IRAM<br>• Direct GPIO and two independent phases<br>• Exact 2 PPR formula<br>• Relative rejection, confirmation, and EMA<br>• Glitches rejected without refreshing timeout<br>• PROGMEM and `snprintf` JSON<br>• Detailed OR-ing, PTC/TVS, and NPN | • CRC includes its own field<br>• Settings rejected on every reboot<br>• ISR ring without saturation detection<br>• No provisional tach at boot<br>• Slew executed on every `loop()` iteration<br>• Only one tach monitored per pair<br>• HTTP without auth/CSRF<br>• Buck vulnerable at low rail/PWM |
| `kimi k3` — thinking high | **7.6/10** | **8.0/10** | • Timer1 one-shot with absolute phase<br>• Two channels and direct GPIO<br>• Correct 2 PPR formula<br>• Integer EMA and timeout<br>• PROGMEM and bounded `snprintf`<br>• AP+STA, mDNS, and CRC16<br>• Correct OR-ing and 5 V buck | • Idle wake-up delayed by up to 1 s<br>• First interval measured from boot<br>• Resumption contaminated by the pause<br>• No maximum period bound<br>• No multi-period validation<br>• 1.5 s timeout<br>• No auth/CSRF<br>• Missing NPN pull-down and protections |
| `glm5.3` | **7.2/10** | **8.0/10** | • Timer1 one-shot with absolute deadlines<br>• Bounded catch-up<br>• Timestamp-based measurement and EMA<br>• AP+STA, DNS portal, and mDNS<br>• PROGMEM, fixed buffers, escaped JSON<br>• EEPROM CRC32/version<br>• Comprehensive Web UI and diagnostics<br>• Detailed hardware and test procedure | • Physically incorrect parallel-tach model<br>• Failure of one fan is masked<br>• Long critical snapshot section<br>• Failure masking with an approximately 5 s gap<br>• EMA not reset after signal loss<br>• Stale tach retained for 2.5 s<br>• HTTP without auth/CSRF<br>• Contradictory documentation |
| `grok 4.6 medium` | **7.0/10** | **7.2/10** | • Timer1 NMI and direct GPIO<br>• Two independent channels<br>• Exact 2 PPR conversion<br>• Short 400 ms timeout<br>• PROGMEM and CRC32<br>• AP+STA with reconnection<br>• One tach per pair without paralleling<br>• Consistent OR-ing and open collectors | • Permanent NMI at 50 kHz<br>• 20 µs quantization<br>• Freshness updated before validation<br>• No digital period filter<br>• Non-transactional multi-field publication<br>• Hard-coded 80 MHz CPU<br>• No boot tach<br>• HTTP without auth/CSRF |
| `composer 2.5 prompt from claude` | **6.2/10** | **6.3/10** | • Hardware Timer1 and two deadlines<br>• Correct 2 PPR formula<br>• Averaging and slew limiting planned<br>• Asynchronous AP+STA server<br>• PROGMEM and static JSON documents<br>• Scalable LittleFS<br>• Documented OR-ing, buck, and NPN | • Fixed 20 kHz ISR with `digitalWrite()`<br>• `now + half` accumulates drift<br>• Measurable quantization<br>• Invalid edge postpones timeout<br>• Filter reuses the same measurement<br>• Boot delayed by LittleFS<br>• No auth/CSRF<br>• Non-atomic persistence |
| `Gemini_4_Argon_medium` | **6.0/10** | **7.8/10** | • Good FRC1 NMI and direct GPIO<br>• Absolute deadlines without bursts<br>• Correct 2 PPR and Q16.16 formulas<br>• Short 500 ms timeout<br>• Average of four periods<br>• PROGMEM and bounded JSON<br>• EEPROM CRC32/version<br>• Non-blocking AP+STA and reconnection | • Resumes with old period on first edge<br>• Non-transactional shared state<br>• 64-bit ISR multiplication not audited<br>• No provisional tach at boot<br>• HTTP API without auth/CSRF<br>• Insufficient power supply at 7 V<br>• Resistors overloaded during peaks<br>• NPN OR-ing close to maximum VEBO |
| `claude-sonnet5-free` | **4.4/10** | **7.0/10** | • Hardware Timer1 and direct GPIO<br>• Two factorized channels<br>• Correct 2 PPR formula<br>• PROGMEM and `char` buffers<br>• EEPROM with integrity check<br>• Non-blocking AP+STA<br>• Well-designed open-collector outputs | • Input based on a nonexistent 12 V pull-up<br>• First edge treated as a period<br>• `N+1` counter<br>• Cumulative drift<br>• 100 µs resolution<br>• 3 s timeout<br>• No period filter<br>• Unauthenticated HTTP |
| `gpt-free` | **4.1/10** | **4.7/10** | • CCOUNT capture in short ISR<br>• Waveform generator outside `loop()`<br>• Correct 2 PPR formula<br>• Integer 7/8–1/8 IIR<br>• AP+STA and complete interface<br>• Static structures<br>• Readable and factorized code | • Unversioned internal waveform API<br>• Constants dependent on 160 MHz<br>• No anticipated tach at boot<br>• Incomplete input/power hardware<br>• Secrets returned in clear text<br>• EEPROM without CRC/version<br>• 9 kB page built on the heap<br>• Rearming phase not guaranteed |
| `Grok_4.7_medium` | **4.0/10** | **8.0/10** | • NMI/CCOUNT and direct GPIO<br>• NMI-to-loop seqlock<br>• Absolute deadlines in cycles<br>• Exact 2 PPR formula<br>• IIR reset on stall<br>• Freshness based on a valid period<br>• PROGMEM, bounded JSON, and CRC32<br>• Two factorized channels | • Q1–Q4 C-B junctions reverse-biased<br>• VRAW receives no useful current<br>• Fragile discrete linear regulator<br>• Permanent NMI at 31.25 kHz<br>• NMI divisions not audited for IRAM safety<br>• No provisional tach at boot<br>• One period is enough to activate<br>• HTTP without auth/CSRF |
| `deepseek4 flash` | **3.3/10** | **3.8/10** | • Ambitious one-shot event calendar<br>• NMI and atomic GPIO<br>• Correct 2 PPR formula<br>• Adaptive timeout<br>• Integer EMA<br>• PROGMEM and fixed JSON buffer<br>• Versioned EEPROM<br>• OR-ing, buck, and NPN | • Copy of `volatile SnapT` probably does not compile<br>• `noInterrupts()` ineffective against NMI<br>• Desynchronized idle wake-up<br>• Race on FRC1 registers<br>• First edge accepted as a period<br>• Noise postpones timeout<br>• Secrets exposed<br>• Private APIs not pinned |
| `gemini 3.1 pro` | **2.8/10** | **2.8/10** | • Hardware Timer1 and direct GPIO<br>• Correct 2 PPR intent<br>• Two independent outputs<br>• AP+STA<br>• HTML in PROGMEM<br>• OR-ing, buck, and NPN described | • `0xDEYE0001` prevents the build<br>• Counting assumes an exact 1 s window<br>• 20 kHz tick and cumulative drift<br>• Ratios without a maximum<br>• Unbounded HTTP `strcpy`<br>• STA password exposed<br>• No real filter<br>• Dangerous electrical advice |
| `gemini-free` | **2.5/10** | **3.5/10** | • Hardware Timer1 targeted<br>• Correct 2 PPR formula<br>• Two independent setpoints<br>• Asynchronous AP+STA<br>• PROGMEM and static JSON<br>• `strlcpy` for credentials<br>• Simple interface | • Timer DIV16 approximately 16 times too slow<br>• `.cpp` without explicit `Arduino.h`<br>• First edge accepted as a period<br>• Invalid ratio retains old output<br>• Inconsistent input range<br>• No robust filter<br>• No complete hardware schematic<br>• API without auth |
| `qwen3.6_prompt_from_claude` | **2.5/10** | **3.5/10** | • Good 2 PPR formula<br>• Direct GPIO planned<br>• Averaging and smoothing envisioned<br>• PROGMEM and bounded buffers<br>• Rich AP+STA, OTA, and interface<br>• Scalable LittleFS<br>• Fairly complete functional schematic | • Multiple compilation errors<br>• Local `Ticker` destroyed after `setup()`<br>• `Ticker` is not Timer1<br>• NPN polarity/fail-safe reversed<br>• Output not released on stall<br>• MT3608 is not a buck<br>• Fixed AP/OTA credentials and open HTTP<br>• Non-atomic LittleFS |
| `composer 2.5` | **2.0/10** | **4.0/10** | • Two-channel Timer1 intent<br>• Correct 2 PPR formula<br>• EEPROM with CRC16<br>• Non-blocking AP+STA<br>• Correct 3.3 V input pull-up<br>• Open-collector outputs planned<br>• Compact and readable code | • `0xDEYE` blocks compilation<br>• Probably invalid WiFi symbol<br>• Timer1 approximately five times too fast<br>• Floating point in ISR<br>• Timeout postponed by noise<br>• STA secrets exposed<br>• 12 V recommended on 5 V pin<br>• No pull-down or tests |
| `glm_5.2_high` | **1.8/10** | **4.8/10** | • Waveform API suited to core 3.1.2<br>• Two channels planned separately<br>• Correct 2 PPR intent<br>• Average of four periods<br>• Non-blocking AP+STA<br>• LittleFS and bounded network copies<br>• Open-collector NPN outputs | • Extra parentheses: broken build<br>• Nonexistent `GPIOxx` symbols<br>• 32-bit overflow rejects every period<br>• No output can start<br>• Two parallel tachs recommended<br>• WiFi secrets displayed and open API<br>• Rails connected before the diodes<br>• LM2596 cannot regulate 5 V from 5 V |
| `qwen3.6` | **1.5/10** | **2.5/10** | • Two-channel intent<br>• Correct 2 PPR display formula<br>• Timeout planned<br>• Bounded ratios<br>• AP+STA targeted<br>• Documentation and BOM present<br>• Output NPN sound in principle | • ESP32 API on ESP8266<br>• Definite syntax error<br>• Ratio multiplied instead of divided<br>• Half-period confused with period<br>• Channel unable to restart<br>• Reversed NPN polarity<br>• Risky GPIO15 strap pin<br>• Open AP and direct 12 V |
| `qwen3.6-35b-fp8-freetoken` | **1.0/10** | **2.0/10** | • Readable structure<br>• Correct 2 PPR display formula<br>• ISRs marked IRAM<br>• Bounded HTTP ratios<br>• AP+STA and captive portal<br>• BOM and schematics provided | • Does not compile: EEPROM/Timer1 API<br>• Timer never configured<br>• Channel 2 never generated<br>• Outputs stopped between edges<br>• Four tachs connected in parallel<br>• Reversed NPN fail-safe<br>• 3.3 V injected into 5 V pin<br>• Input can overvoltage the GPIO |
| `qwen35` | **0.5/10** | **1.0/10** | • Extensive documentation<br>• Responsive interface<br>• Protected AP<br>• Ratios bounded server-side<br>• High-voltage warnings | • Syntax and API errors<br>• GPIO10/11 connected to flash<br>• No compliant Timer1<br>• `Ticker` in seconds<br>• RPM derived from cumulative counters<br>• One averaged output<br>• No useful persistence<br>• Inconsistent schematic |
| `gemma4` | **0.3/10** | **0.8/10** | • Compact interface<br>• Minimally protected AP<br>• NPN open collector mentioned<br>• Small, easy-to-read codebase<br>• Simple observation endpoint | • Does not compile<br>• GPIO10/11 reserved for flash<br>• No tach/Timer1 generation<br>• One output for two channels<br>• Incorrect RPM formula<br>• No fail-safe<br>• No real persistence/STA<br>• No complete OR-ing/buck |

## 4. Separate Rankings

Ties are broken by the other score, then alphabetically.

### Ranking by Overall Score — IoT Candidate

1. `gpt-5.6 sol medium` — **8.4/10** — best balance
2. `kimi_k3_max` (Max) — **8.1/10** — very strong real-time performance
3. `claude opus 5 medium` — **8.0/10** — most sophisticated engine
4. `muse_wonder_1.0_medium` — **7.7/10** — excellent engine, broken persistence
5. `kimi k3` (thinking high) — **7.6/10** — very good foundation
6. `glm5.3` — **7.2/10** — advanced, critical tach assumption
7. `grok 4.6 medium` — **7.0/10** — credible NMI, fragile fail-safe
8. `composer 2.5 prompt from claude` — **6.2/10** — comprehensive but nondeterministic
9. `Gemini_4_Argon_medium` — **6.0/10** — good NMI, fragile resumption and power supply
10. `claude-sonnet5-free` — **4.4/10** — clean firmware, invalid input
11. `gpt-free` — **4.1/10** — promising software, incomplete hardware
12. `Grok_4.7_medium` — **4.0/10** — solid firmware, blocked power supply
13. `deepseek4 flash` — **3.3/10** — broken ambitious architecture
14. `gemini 3.1 pro` — **2.8/10** — does not compile
15. `gemini-free` — **2.5/10** — core timing is wrong
16. `qwen3.6_prompt_from_claude` — **2.5/10** — ambitious but not executable
17. `composer 2.5` — **2.0/10** — dangerous and does not compile
18. `glm_5.2_high` — **1.8/10** — does not compile, no valid period
19. `qwen3.6` — **1.5/10** — redesign required
20. `qwen3.6-35b-fp8-freetoken` — **1.0/10** — nonfunctional
21. `qwen35` — **0.5/10** — unusable
22. `gemma4` — **0.3/10** — functionality absent

### Ranking by Code Quality Score

1. `claude opus 5 medium` — **8.5/10** — structural rigor
2. `kimi_k3_max` (Max) — **8.3/10** — rigorous and factorized
3. `gpt-5.6 sol medium` — **8.2/10** — structured and defensive
4. `muse_wonder_1.0_medium` — **8.1/10** — rigorous, untested CRC
5. `kimi k3` (thinking high) — **8.0/10** — clear and compact
6. `glm5.3` — **8.0/10** — maintainable but incorrect domain logic
7. `Grok_4.7_medium` — **8.0/10** — rigorous, invalidating hardware
8. `Gemini_4_Argon_medium` — **7.8/10** — ambitious, fragile resumption invariants
9. `grok 4.6 medium` — **7.2/10** — clear, fragile low-level code
10. `claude-sonnet5-free` — **7.0/10** — readable, insufficiently validated
11. `composer 2.5 prompt from claude` — **6.3/10** — clear, weak invariants
12. `glm_5.2_high` — **4.8/10** — structured, missing basic verification
13. `gpt-free` — **4.7/10** — readable but fragile
14. `composer 2.5` — **4.0/10** — orderly, fundamental errors
15. `deepseek4 flash` — **3.8/10** — documented, incorrect concurrency
16. `gemini-free` — **3.5/10** — readable, unvalidated
17. `qwen3.6_prompt_from_claude` — **3.5/10** — structured, unreliable
18. `gemini 3.1 pro` — **2.8/10** — clear, undeliverable
19. `qwen3.6` — **2.5/10** — readable, inconsistent
20. `qwen3.6-35b-fp8-freetoken` — **2.0/10** — incomplete engine
21. `qwen35` — **1.0/10** — multiple errors
22. `gemma4` — **0.8/10** — inconsistent prototype

## 5. Merger Recommendation

The recommended foundation remains `gpt-5.6 sol medium`, supplemented by the immediate Timer1 wake-up and lean frontend of `kimi_k3_max`, then by the invariants and diagnostics of `claude opus 5 medium`. The genuinely validated build of `muse_wonder_1.0_medium` confirms that an event-driven FRC1 NMI, `GPOS/GPOC`, and atomic-word sharing are viable on ESP8266 core 3.1.2, but its CRC storage must not be reused. **Only one** scheduler must be selected; multiple competing engines must not be merged.

### IoT Features to Borrow

- **Real-time and generation:** retain the one-shot scheduler from `gpt-5.6 sol medium`, described as “*the armed time is subtracted from both counters, then the nearest minimum deadline is scheduled*.” Integrate the rearming behavior from `kimi_k3_max`, where “*`setOutput()` rearms Timer1 as soon as a channel is activated*,” to avoid the one-second wait observed in `kimi k3`. `muse_wonder_1.0_medium` provides proof of compilation on ESP8266 3.1.2 and 0.2 µs resolution; reuse its event-driven principle, but add a saturation counter to the capture ring.
- **Phase and ISR publication:** borrow from `claude opus 5 medium` the adoption of frequency “*at an edge boundary*,” `nextEdge += halfPeriod`, and the seqlock. `Grok_4.7_medium` confirms the value of an even/odd snapshot and improves on Grok 4.6 by refreshing the fail-safe only after a valid period. `Gemini_4_Argon_medium` confirms the value of absolute deadlines, but its sharing of `lastEdgeCycle`, period, and setpoint is not transactional. Pin the core, inspect the IRAM map, and prohibit any resumption using an old setpoint.
- **Boot readiness:** no proposal completely covers startup. Add the states `BOOT_WAITING`, `VALIDATING`, `RUNNING`, `STALE`, and `FAULT`, a measured grace window, then optionally a strictly bounded nominal tach. The shared finding is explicit: in `kimi_k3_max`, “*no provisional nominal signal is produced before two actual edges*.”
- **Protocol and fail-safe:** retain the calculation from `gpt-5.6 sol medium`, “*`RPM = 30 000 000 / period_us`*,” and the rule demonstrated by `Grok_4.7_medium`: “*`lastGood` is updated only after a period has been validated*.” Require three consistent periods before activation, invalidate the period/setpoint on stall, and use a short adaptive timeout. This invalidation specifically avoids the defect in `Gemini_4_Argon_medium`, where the first resumption edge restarts the old frequency. Never reuse the parallel tachs from `glm5.3` or `glm_5.2_high`.
- **Filters:** combine a short median, relative outlier rejection, and resettable EMA. The relative rejection followed by confirmation in `muse_wonder_1.0_medium` is valuable, but its 2% slew must be triggered by a **new period**, not by each `loop()` iteration. The 75/25 EMA outside the ISR from `kimi_k3_max` remains preferable to the filters in `composer 2.5 prompt from claude`, which “*continuously reinserts the same value*” on every loop iteration.
- **WiFi and Web:** retain non-blocking AP+STA, `WiFi.persistent(false)`, reconnection, mDNS, a `PROGMEM` page, and status produced in a bounded buffer. Add authentication, CSRF protection, a unique AP secret, and rate limiting. Never reproduce `claude opus 5 medium`, where “*`/api/status` returns `apPass` in clear text*.”
- **Persistence:** use the `magic`, version, size, CRC32, and `isfinite()` validation from `gpt-5.6 sol medium`, then add dual slots and a generation counter. Calculate coverage with `offsetof(Config, crc)`, never with `sizeof(Config)-sizeof(crc)`: `muse_wonder_1.0_medium` demonstrates that trailing padding can otherwise include the CRC itself and invalidate every reboot. Write only when a change occurs.
- **Hardware:** start with the leaders' Schottky or MOSFET OR-ing, qualified buck-boost, 3.3 V inputs, and NPN outputs. Never reuse Q1–Q4 from `Grok_4.7_medium`, the resistive preregulator from `Gemini_4_Argon_medium`, or the design from `glm_5.2_high`, which connects both rails before its diodes and assumes that an LM2596 maintains 5 V from approximately 5 V. Also retain the instruction from `gpt-5.6 sol medium` that “*the two tach wires must never be connected together*,” and precisely qualify the PTC, TVS, transistor, and converter.

### Code Quality Practices to Integrate

- **Modular architecture:** borrow the logical separation in `claude opus 5 medium` between “*low-level primitives, engine, shared state, application, storage, Web, and networking*,” then move HTML/CSS/JS out of the main file.
- **Explicit concurrency:** use versioned snapshots or the even/odd seqlock correctly applied by `Grok_4.7_medium`; restore the previous interrupt state and never assume that `noInterrupts()` protects an NMI.
- **Minimal ISRs:** no allocation, strings, floating point, `digitalWrite()`, persistence, or network operations. Transitive calls to `micros()` and `timer1_write()` must be validated on the pinned toolchain.
- **Controlled heap:** retain `PROGMEM`/`send_P` and the bounded `char` buffers from `kimi_k3_max`, whose status is “*produced in a bounded `char` buffer with `snprintf`*.” Expose current heap and historical minimum.
- **Safe boundaries:** strict server-side validation, `isfinite()`, lengths, NUL termination, HTML/JSON escaping, visible HTTP errors, authentication, and CSRF protection; no secret may be read back through an API.
- **Reproducible build:** add `platformio.ini` or an `arduino-cli` command, pin the core and libraries, and compile in CI. The successful build of `muse_wonder_1.0_medium` on `esp8266:esp8266:d1_mini` 3.1.2 demonstrates the expected level of evidence; the same step would have caught the extra parentheses and nonexistent GPIO symbols in `glm_5.2_high`, the `0xDEYE` literals, the ESP32 APIs, and the nonexistent Timer1 calls.
- **Tests:** cover PPR, half-period, saturation, wraparound, 32-bit overflow, structure size/padding, CRC after save/reload, first edge, resumption, glitches, full ring, and stall. A round-trip test would have detected Muse's CRC issue; a nominal-period test would have shown that GLM 5.2 rejects all its samples. Measure frequency and jitter on an oscilloscope under WiFi traffic and flash writes.
- **Verifiable documentation:** document units, NPN polarity, limits, and measured results; prohibit claims of “zero jitter” or “fully isolated” without captures or a reproducible protocol.
