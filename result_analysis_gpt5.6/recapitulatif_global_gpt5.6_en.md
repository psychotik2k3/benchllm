# Global Summary — GPT-5.6 Full Reanalysis from Scratch

This summary was rebuilt directly from the current source files of all 25 competitors. Previous reports were not used as evidence. Model and reasoning-level variants remain separate competitors.

## 1. Inventory

- Competitor folders: **25**
- Individual reports regenerated: **25**
- Competitors without a report: **none**

## 2. Major Methodological Correction

The fresh source review invalidates several earlier conclusions. The clearest case is **Gemini_4_Argon_medium**: its NMI timeout clears neither the stale target half-period nor `hasSignal`. The first edge after a stall refreshes the age and immediately re-enables the old frequency before a new period exists. It therefore does not implement safe two-edge restart. Its overall score is reduced to **5.7/10**.

The same checks were applied to every submission: static compilability, Timer1 units, first-edge behavior, stall/restart state, transitive IRAM safety, electrical inputs/outputs, power supply, persistence, and Web security.

## 3. Summary Table

| Rank | Version | Overall | Code quality | Main strengths | Decisive weaknesses |
|---:|---|---:|---:|---|---|
| 1 | **claude opus 5 medium** | **8.8/10** | **9.0/10** | True Timer1 NMI, absolute CCOUNT deadlines, seqlock publication, correct two-edge restart, and viable hardware. | No boot guard tach, private core APIs, no HTTP authentication, and no delivered tests. |
| 2 | **Claude_Opus_5.5_xhigh** | **8.7/10** | **8.8/10** | Feature-complete NMI chain, advanced filtering, 500 ms NMI fail-safe, rich networking, and excellent hardware documents. | Open administration by default, private APIs, no reproducible tests, and an incorrect low-input MP1584 claim. |
| 3 | **GPT-5.6_Sol_xhigh** | **8.0/10** | **8.4/10** | Core waveform engine, strict persistence, escaped JSON, extracted tach math, native tests, and careful hardware. | No boot signal, two-second timeout, target core API not build-validated, no HTTP authentication, and narrow test coverage. |
| 4 | **gpt-5.6 sol medium** | **7.8/10** | **8.3/10** | Two-channel one-shot scheduler, absolute deadlines, correct first-edge/restart behavior, strict CRC configuration, and prudent power design. | Maskable Timer1 IRQ rather than explicit NMI, no boot guard, no authentication, and no automated tests. |
| 5 | **muse_wonder_1.0_medium** | **6.6/10** | **7.7/10** | Timestamp rings, one-shot NMI output, absolute deadlines, fixed status buffers, polished UI, and solid hardware. | CRC includes the CRC field because of padding, ring overwrite, loop-rate-dependent slew, and loop-dependent stall handling. |
| 6 | **kimi_k3_max** | **6.1/10** | **7.1/10** | Two-deadline Timer1 engine, direct GPIO, channel factorization, CRC16 configuration, clear UI, and sound hardware. | The first boot/restart edge becomes a huge period, stall is handled in loop, standard IRQ is overclaimed as NMI, and there are no tests. |
| 7 | **kimi k3** | **5.9/10** | **6.8/10** | Readable two-channel one-shot architecture, correct 2 PPR math, EMA, CRC16, AP+STA, and viable hardware. | False first sample, poor post-stall priming, non-NMI Timer1, missing output transistor pull-downs, and no authentication. |
| 8 | **grok 4.6 medium** | **5.8/10** | **7.0/10** | Compact NMI engine, two independent outputs, direct GPIO, correct 2 PPR math, CRC32, and generally viable hardware. | Stale setpoint survives a stall, one recovery edge restarts it, rejected glitches refresh timestamps, and NMI load is high. |
| 9 | **Gemini_4_Argon_medium** | **5.7/10** | **7.8/10** | Sophisticated one-shot NMI, CCOUNT/Q16 timing, 500 ms watchdog, bounded buffers, CRC32, and rich documentation. | Stale target survives timeout and restarts on one edge; possible 64-bit ISR helper; undocumented clock register; unsafe 33-ohm/discrete supply. |
| 10 | **composer 2.5 prompt from claude** | **5.2/10** | **6.3/10** | Two channels, correct ratio direction, async server, PROGMEM UI, LittleFS defaults, and a usable buck/open-collector schematic. | micros/digitalWrite in the timer ISR, cumulative scheduling drift, invalid edges refresh freshness, stale restart, and loop-dependent filters. |
| 11 | **deepseek4 flash** | **4.8/10** | **6.5/10** | Ambitious NMI event calendar, Q16 timing, adaptive timeout, fixed buffers, and detailed buck/open-collector hardware. | First edge accepted, invalid edges refresh state, CRC covers one CRC byte, NMI synchronization is flawed, and secrets are exposed. |
| 12 | **claude-sonnet5-free** | **4.8/10** | **6.2/10** | Readable fixed-tick two-channel engine, direct GPIO, correct 2 PPR math, fixed JSON, checksum storage, and documented outputs. | First/restart edge accepted as a period, maskable ISR, three-second timeout, and Noctua input lacks the required pull-up. |
| 13 | **gpt-free** | **4.6/10** | **6.2/10** | Short CCOUNT capture ISRs, core waveform outputs, correct first-edge handling, factorized channels, and a complete Web UI. | Input divider has no tach pull-up, no standalone schematic, implicit 160 MHz thresholds, weak persistence, and clear-text secrets. |
| 14 | **Grok_4.7_medium** | **4.4/10** | **8.2/10** | Excellent NMI/seqlock engine, correct two-edge recovery, absolute scheduling, plausible filtering, bounded JSON, and CRC32. | Collector-base rail OR is reverse-biased and does not power the board; discrete regulator is unstable; permanent high-rate NMI; open administration. |
| 15 | **glm5.3** | **2.8/10** | **6.5/10** | Buffered capture, multi-edge EMA, absolute scheduling, captive portal, asynchronous scan, correct CRC coverage, and strong documentation. | Invalid hexadecimal literal prevents compilation; two fan tach outputs are paralleled and frequency is incorrectly divided by fan count. |
| 16 | **gemini-free** | **2.5/10** | **4.5/10** | Compact two-channel concept, async server, PROGMEM UI, LittleFS JSON, and conceptually correct 2 PPR formulas. | Timer reload uses 80 instead of 5 ticks per microsecond, making output about 16 times too slow; first edge is accepted; no hardware design. |
| 17 | **gemini 3.1 pro** | **2.3/10** | **3.8/10** | Two channels, Timer1 before WiFi, direct GPIO, understood 2 PPR math, and nonblocking AP+STA. | Invalid hexadecimal literal, unbounded strcpy, RPM tied to assumed loop duration, costly fixed timer tick, and weak hardware. |
| 18 | **glm_5.2_high** | **2.0/10** | **4.0/10** | Factorized channel handling, intended independent waveforms, four-period average, LittleFS, escaped HTML, and detailed hardware. | Three syntax errors, 32-bit period conversion overflow, rejected CCOUNT wrap, and two asynchronous fan tachs connected in parallel. |
| 19 | **composer 2.5** | **1.2/10** | **4.0/10** | Channel structure, intended Timer1 architecture, direct GPIO, CRC16, timeout, and conceptually correct 2 PPR direction. | Invalid hexadecimal literal, timer about five times too fast, floating-point ISR work, false first edge, and 12 V wired to the Wemos 5 V pin. |
| 20 | **qwen3.6_prompt_from_claude** | **1.0/10** | **3.5/10** | Intended dual capture, moving/EMA filters, async Web, OTA, LittleFS, correct ratio direction, and open-collector outputs. | Multiple compile/API errors, local Ticker destroyed after setup, active-low outputs asserted at boot, stale low after stall, and boost converter used as a buck. |
| 21 | **Swift_1.5_Qwen3.8-27B_medium-IQ3S** | **0.6/10** | **2.5/10** | Readable two-channel intent, correct input 2 PPR formula, EMA/bounds, AP+STA, and recognition of open-collector/rail-OR needs. | Incompatible APIs, invalid and non-rearmed timer, incorrect output frequency, broken stall detection, and an AP2112 exposed to 12 V. |
| 22 | **qwen3.6** | **0.5/10** | **2.5/10** | Attempts two captures, two deadlines, persistent ratios, a two-channel UI, and knows the display-side 2 PPR formula. | ESP32 APIs and syntax errors on ESP8266, reversed period math, long pre-timer WiFi block, no reliable restart, boot strap risk, and direct 12 V supply. |
| 23 | **qwen3.6-35b-fp8-freetoken** | **0.3/10** | **2.0/10** | Attempts AP+STA, captive portal, EEPROM validation, and open-collector signaling. | Syntax and ESP32 API errors, LOW-level interrupt storm, one timer/pin for two outputs, reversed frequency math, paralleled 12 V tachs, and invalid 3.3 V supply. |
| 24 | **gemma4** | **0.2/10** | **1.2/10** | Small source, protected AP, compact polling page, and separate HTTP handlers. | Does not compile, has no waveform generator, only one output, no timed RPM window, flash-pin GPIO choices, no persistence, and no usable schematic. |
| 25 | **qwen35** | **0.1/10** | **1.0/10** | Responsive UI documentation, a calibration guide, moving-average intent, and a protected AP. | Syntax errors, invented APIs, duplicate loop, flash GPIOs, no Timer1, one fixed 1 Hz output, cumulative counters labeled RPM, and incoherent hardware. |

## 4. Overall Ranking

1. **claude opus 5 medium** — **8.8/10** — Best overall submission; validate against the pinned target toolchain.
2. **Claude_Opus_5.5_xhigh** — **8.7/10** — Most complete reference, narrowly penalized for security and validation.
3. **GPT-5.6_Sol_xhigh** — **8.0/10** — Excellent official foundation with incomplete full-system validation.
4. **gpt-5.6 sol medium** — **7.8/10** — Strong and careful solution, without NMI or tests.
5. **muse_wonder_1.0_medium** — **6.6/10** — Strong real-time design with broken persistence and loop-dependent dynamics.
6. **kimi_k3_max** — **6.1/10** — Solid steady-state architecture with incorrect post-stall restart.
7. **kimi k3** — **5.9/10** — Potentially workable foundation with a false first sample and non-NMI timer.
8. **grok 4.6 medium** — **5.8/10** — Good NMI engine whose stale restart and measurement logic need correction.
9. **Gemini_4_Argon_medium** — **5.7/10** — Sophisticated engine undermined by stale restart and critical power defects.
10. **composer 2.5 prompt from claude** — **5.2/10** — Good skeleton whose fail-safe and restart must be corrected.
11. **deepseek4 flash** — **4.8/10** — Ambitious architecture with fragile persistence and low-level assumptions.
12. **claude-sonnet5-free** — **4.8/10** — Plausible firmware with inadequate restart and input hardware.
13. **gpt-free** — **4.6/10** — Reasonable firmware with incomplete input hardware and exposed credentials.
14. **Grok_4.7_medium** — **4.4/10** — Excellent firmware paired with a nonfunctional power supply.
15. **glm5.3** — **2.8/10** — Very ambitious design blocked by compilation and parallel-tach assumptions.
16. **gemini-free** — **2.5/10** — Blocking timer-unit error and missing schematic.
17. **gemini 3.1 pro** — **2.3/10** — Does not compile and measures RPM too coarsely.
18. **glm_5.2_high** — **2.0/10** — Non-compiling submission with a broken period measurement.
19. **composer 2.5** — **1.2/10** — Non-compiling and electrically dangerous.
20. **qwen3.6_prompt_from_claude** — **1.0/10** — Non-compiling design with a destroyed scheduler and wrong converter.
21. **Swift_1.5_Qwen3.8-27B_medium-IQ3S** — **0.6/10** — ESP8266-incompatible and electrically dangerous.
22. **qwen3.6** — **0.5/10** — Wrong platform, broken syntax, and inverted tach mathematics.
23. **qwen3.6-35b-fp8-freetoken** — **0.3/10** — Non-compiling architecture incapable of two independent tach outputs.
24. **gemma4** — **0.2/10** — Non-compiling draft with no tach simulator.
25. **qwen35** — **0.1/10** — No compilable firmware and no actual tach simulator.

## 5. Code Quality Ranking

1. **claude opus 5 medium** — **9.0/10** — Exceptionally strong real-time invariants.
2. **Claude_Opus_5.5_xhigh** — **8.8/10** — Highly rigorous but very large.
3. **GPT-5.6_Sol_xhigh** — **8.4/10** — Best modularity and testability.
4. **gpt-5.6 sol medium** — **8.3/10** — Clean, defensive, and maintainable.
5. **Grok_4.7_medium** — **8.2/10** — One of the strongest real-time engines.
6. **Gemini_4_Argon_medium** — **7.8/10** — Highly structured, with a decisive state-machine defect.
7. **muse_wonder_1.0_medium** — **7.7/10** — Polished, with several missed invariants.
8. **kimi_k3_max** — **7.1/10** — Well structured, but with an incomplete fail-safe.
9. **grok 4.6 medium** — **7.0/10** — Compact and readable, with an incomplete state machine.
10. **kimi k3** — **6.8/10** — Readable but overconfident.
11. **deepseek4 flash** — **6.5/10** — Well commented but too assertive.
12. **glm5.3** — **6.5/10** — Careful architecture that was not validated.
13. **composer 2.5 prompt from claude** — **6.3/10** — Structured but insufficiently deterministic.
14. **claude-sonnet5-free** — **6.2/10** — Readable, with several unverified assumptions.
15. **gpt-free** — **6.2/10** — Clear but oversized.
16. **gemini-free** — **4.5/10** — Compact but unverified.
17. **glm_5.2_high** — **4.0/10** — Well presented but never tested.
18. **composer 2.5** — **4.0/10** — Reasonably presented but technically unvalidated.
19. **gemini 3.1 pro** — **3.8/10** — Readable intent with weak robustness.
20. **qwen3.6_prompt_from_claude** — **3.5/10** — Ambitious but not integrated.
21. **Swift_1.5_Qwen3.8-27B_medium-IQ3S** — **2.5/10** — Apparent structure on incorrect foundations.
22. **qwen3.6** — **2.5/10** — Verbose and explanatory, but technically nonviable.
23. **qwen3.6-35b-fp8-freetoken** — **2.0/10** — Very large submission around nonviable logic.
24. **gemma4** — **1.2/10** — Incomplete and inconsistent code.
25. **qwen35** — **1.0/10** — Incoherent generated artifact.

## 6. Method and Architecture Comparison

### Best real-time engines

- **claude opus 5 medium** has the strongest input state machine and NMI/CCOUNT consistency. Invalid periods re-prime the filter, restart truly requires two edges, and NMI snapshots use a seqlock.
- **Claude_Opus_5.5_xhigh** provides the most feature-complete NMI chain, with advanced filtering, latency diagnostics, and a fast NMI fail-safe.
- **Grok_4.7_medium** has excellent real-time firmware and correct restart behavior, but its collector-base power OR is reverse-biased in the proposed direction and cannot power the board.
- **GPT-5.6_Sol_xhigh** uses the core waveform engine and offers the best modularity and testability, though target-core validation is still required.

### Recurring failure patterns

- Treating the first edge as a period (`kimi k3`, `kimi_k3_max`, `composer 2.5`, `deepseek4 flash`).
- Stopping only the output while retaining a stale period/setpoint (`Gemini_4_Argon_medium`, `grok 4.6 medium`).
- Applying filters or slew on every loop iteration instead of every new measurement (`muse_wonder_1.0_medium`, `composer 2.5 prompt from claude`).
- Paralleling asynchronous tach outputs and assuming their frequencies add (`glm5.3`, `glm_5.2_high`, `qwen3.6-35b-fp8-freetoken`).
- Using ESP32 APIs on the ESP8266 target (`qwen3.6`, `qwen3.6-35b-fp8-freetoken`, `Swift_1.5_Qwen3.8-27B_medium-IQ3S`).
- Designing a power supply without headroom or with a non-conducting topology (`Gemini_4_Argon_medium`, `Grok_4.7_medium`, several Qwen submissions).

## 7. Merger Recommendation

### Recommended foundation

**Use `claude opus 5 medium` as the real-time foundation and `Claude_Opus_5.5_xhigh` as the feature/hardware reference.** The former handles priming, invalid periods, and post-stall restart best; the latter provides the strongest networking, diagnostics, and electrical package.

### Components to borrow

1. **Engine and input state** — from `claude opus 5 medium`: one-shot NMI, absolute CCOUNT deadlines, seqlock, explicit priming, invalid-period reset, and two-edge restart.
2. **Loop-independent fail-safe** — from `Claude_Opus_5.5_xhigh`: a short NMI timeout, while clearing period, target, filters, and validation state.
3. **Math and tests** — from `GPT-5.6_Sol_xhigh`: keep `tach_math.h`, then add boot, wrap, noise, stall, restart, and edge-calendar tests.
4. **Hardware** — qualified Schottky diodes, TVS/PTCs, a true buck-boost if fan rails approach 5 V, 3.3 V Noctua tach pull-ups, and BC337/2N2222 open-collector outputs with base-emitter pull-downs.
5. **Web security** — mandatory authentication, CSRF/origin protection, no secret readback, and rate-limited flash writes.
6. **Persistence** — magic + version + size + CRC using `offsetof(Config, crc)`, dual-slot or transactional writes, and save/load round-trip tests.

### Mandatory fixes before deployment

- Define and test the boot policy on a real Deye: zero RPM, a bounded provisional period, or an explicit grace period.
- Monitor both fans in each pair through a deliberate strategy; never parallel their tach outputs.
- Pin the ESP8266 core, compile in CI, inspect the IRAM map, and measure jitter/timeouts under WiFi and flash writes.
- Test each power rail separately, both rails together, low input voltage, rail switching, and WiFi current peaks.

## 8. Conclusion

The final ranking is led by the two high-end Claude submissions. **claude opus 5 medium (8.8/10)** has a slight advantage because its post-stall restart is technically stronger and its hardware is viable. **Claude_Opus_5.5_xhigh (8.7/10)** remains the most complete delivery. **GPT-5.6_Sol_xhigh (8.0/10)** is the best modular and testing foundation. No candidate should be installed without a target build and electrical/oscilloscope validation.
