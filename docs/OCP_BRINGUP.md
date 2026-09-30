# OCP Circuit Bring-Up — Stages 1 through 5

Printable field guide for building and bench-verifying the current-limit
(OCP) protection circuit on Board v2, before any HV or work-coil power is
applied. Ports the proven DeCosta board's analog peak-detect + latch
circuit, reconstructed from the DeCosta schematic image (see
`BOARD_V2_DIRECT.md`, "OPEN — OCP circuit" section, for the reconstruction
notes and confidence caveats).

**Hard rule: do not apply HV or connect the work coil until Stage 4 is
verified end-to-end.** Neither PA1 nor PB12 currently provide real
overcurrent protection until this circuit is built and wired in.

---

## What you already have (built, ready to connect)

- **CT:** 44-turn, 0.8mm enameled wire, ferrite toroid (~28mm OD / ~14mm ID
  / ~12mm width)
- **Burden resistor:** 100Ω, 100W
- **Coupling cap at the burden:** 0.1µF, 2000V, PP (polypropylene) film
- **Output:** 2 wires, one from each side of the burden resistor

This CT/burden assembly already feeds the existing 74HC14 frequency
conditioner (PLL feedback path). We are tapping a **second, parallel**
connection off the same two burden wires for the OCP path — do not tap in
series or load the existing PLL feedback connection.

---

## Full signal chain (target, end to end)

```
Work coil
   │
   ▼
CT (44T, 0.8mm, toroid) ── burden 100Ω/100W ── 0.1µF/2000V (built, yours)
   │                                    │
   │ (existing tap,                    │ (NEW tap, Stage 1 below)
   │  do not disturb)                  │
   ▼                                   ▼
74HC14 → PA0 (TIM2_CH1)          200Ω series R → 330nF coupling cap
  [existing PLL feedback]              │
                                       ▼
                          1N5819 x2 (back-to-back clamp, D2/D3)
                                       │
                                       ▼
                          1N4739A x2 in series (9.1V zener pair, D4/D5)
                                       │
                                       ▼
                    MPSA06 (Q1) comparator/threshold stage
                    R12=51k  R16=15k  R13=10k  R1=10k
                    20kΩ 10-turn pot ("Current Limit")
                                       │
                                       ▼
                    MM74HCT74N (dual D flip-flop) — LATCH
                                       │
                                       ▼
                 [level-shift/polarity check — Stage 2]
                                       │
                                       ▼
                        PB12 (TIM1_BKIN) on the Nucleo
                     active-LOW: LOW = fault, kills PWM in HW (~6ns)
```

---

## Parts list (Stage 1 front end + latch)

| Qty | Part | Value | Ref (DeCosta) |
|---|---|---|---|
| 1 | Resistor | 220Ω (sub for 200Ω — close enough, not threshold-critical, see note below) | series R |
| 1 | Capacitor | 330nF (standard film/ceramic, ~50V — NOT the 2000V burden cap) | coupling |
| 2 | Diode | 1N5819 Schottky | D2, D3 |
| 2 | Diode | 1N4739A (9.1V zener, 1W) | D4, D5 |
| 1 | Transistor | MPSA06 NPN | Q1 |
| 1 | Resistor | 47kΩ (sub for 51k — closest on hand, see note below) | R12 |
| 1 | Resistor | 15kΩ | R16 |
| 2 | Resistor | 10kΩ | R13, R1 |
| 1 | Potentiometer | 20kΩ, 10-turn | "Current Limit" |
| 1 | IC | MM74HCT74N (dual D flip-flop) | latch |

---

## Nucleo pins referenced in this guide

| Function | Pin | Timer/Peripheral | Notes |
|---|---|---|---|
| BKIN (hardware break, fault input) | **PB12** | TIM1_BKIN | Active-LOW. Currently only a bare 10kΩ pull-up to 3.3V placeholder — this is what the OCP latch output will replace/feed. |
| PWM_A | PA8 | TIM1_CH1 | Unaffected by this work — for reference only |
| PWM_B | PB13 | TIM1_CH1N | Unaffected by this work — for reference only |
| PLL frequency feedback | PA0 | TIM2_CH1 | Existing HC14 tap — do not disturb |
| OCP software ADC (future, not this stage) | PA1 | ADC1_IN1 | Currently disconnected/floating — out of scope for Stages 1-5, real current sense for this pin is a separate future task |

**Absolute max on PB12 (and all standard STM32F446RE GPIOs): ~3.6V
(VDD + 0.3V).** This is the single most important number in this whole
build — see Stage 2.

---

**Note on the 220Ω substitution:** this series resistor limits/shapes
impedance into the coupling cap — it is not part of the threshold-setting
network (that's the 51k/15k/10k stage + the 20kΩ pot, tuned empirically in
Stage 3). A 220Ω in place of 200Ω is a negligible difference here and does
not need to be sourced exactly.

**Note on the 47kΩ substitution (R12, was 51kΩ):** this one IS part of
Q1's bias/threshold network, so it does shift the trip point somewhat —
47kΩ (8% off) is a much closer substitute than 22kΩ or 100kΩ (both on
hand as alternatives, but 57%/96% off respectively — either would shift
the bias point enough to risk pushing the usable range outside what the
20kΩ pot can dial back into range). Since Stage 1 empirically sweeps and
finds the trip point with a signal generator rather than calculating it
from component values, this substitution is naturally absorbed by the
bench test — just find the real trip point with 47kΩ in place and set the
pot from there, same process either way.

## Stage 1 — Build the signal-conditioning front end

**Goal:** get a working comparator + latch on the bench, verified with a
signal generator. No CT, no coil, no HV.

1. Wire the front-end chain above (200Ω → 330nF → clamp diodes → zener
   pair → MPSA06 stage w/ pot → MM74HCT74N) onto the driver board or a
   separate small perfboard.
2. Leave the **input** of this chain unconnected to the CT for now —
   you'll drive it from a signal generator instead.
3. Leave the **output** (flip-flop Q or Q̄, whichever is being used)
   unconnected to PB12 for now.

**Test rig:**
- Use the AD3's built-in waveform generator. Set it to a sine wave in the
  60-80kHz range (matching your real switching frequency).
- Connect the AD3 generator output to this circuit's input (where the CT
  would normally connect).
- Probe the MPSA06 collector/output and the flip-flop's Q output with the
  AD3 scope channels while sweeping the generator amplitude from 0V up.

**What you're looking for:**
- A clean, repeatable point where Q1 starts conducting and the flip-flop's
  output flips state as amplitude increases.
- **Latching behavior:** once tripped, does the output stay tripped even
  after you drop the generator amplitude back down? (It should — that's
  the whole point of a latch.)
- A way to reset/clear the latch (check the DeCosta board for how reset
  was wired — likely a manual button or power-cycle; note it here once
  found: ______________________).

**Do not proceed to Stage 2 until the trip + latch behavior is confirmed
clean and repeatable on the bench.**

---

## Stage 2 — Determine output polarity and level, adapt for PB12

**Goal:** confirm the latch's output is directly compatible with PB12, or
add a level-shift/inverter stage if not.

1. **Measure logic swing** with a multimeter or scope on the flip-flop's
   output pin, both in the "OK" and "tripped" states.
   - Record measured LOW: ______ V
   - Record measured HIGH: ______ V
2. **Check the supply rail** the MM74HCT74N is running on (5V typical for
   74HCT-series parts — check the DeCosta board's power rail for this IC).
   - Record VCC: ______ V
3. **Confirm polarity against PB12's requirement:**
   - PB12/BKIN is **active-LOW** (`TIM_BDTR_BKP` cleared in firmware) — a
     **LOW** signal on PB12 must correspond to **FAULT/TRIPPED**, and
     **HIGH** must correspond to **OK/no fault**.
   - Which flip-flop output pin (Q or Q̄) gives you this polarity? Use
     that one. If neither does, you'll need a single inverter stage
     between the latch and PB12.

**⚠️ CRITICAL VOLTAGE CHECK — do this before wiring anything to PB12:**
- STM32F446RE GPIO absolute max is **~3.6V** (VDD + 0.3V).
- If the latch's HIGH level is **5V or higher, do NOT wire it directly to
  PB12.** You will need one of:
  - A resistor divider (simple, e.g. 2:1 or similar ratio, sized so the
    divided HIGH stays safely under 3.6V while the divided LOW still
    reads as a clear LOW at PB12).
  - A small logic-level shifter or an open-collector/open-drain output
    stage with a pull-up to 3.3V instead of 5V.
- **Record the plan here once decided:** ___________________________

**Also check:** does the existing 10kΩ pull-up to 3.3V on PB12 (currently
in place as a placeholder) need to be removed, or can it coexist with the
new latch output? If the latch drives the line directly when tripped
(active pull to GND or similar), the pull-up can likely stay as a
fail-safe (disconnected latch = pulled HIGH = no fault, matching the
existing fail-safe philosophy). Confirm this doesn't create a resistive
divider fight between the pull-up and the latch's drive strength.

---

## Stage 3 — Bench-integrate with the real CT, still no coil

**Goal:** exercise the actual CT/burden hardware (not a simulated signal),
fully current-limited, no coil involved.

1. Connect the OCP front end's input to the **real CT/burden's two
   output wires** (the parallel tap described above — same two wires
   feeding the existing 74HC14, do not disturb that connection).
2. **Drive the CT primary manually with a controlled test current** —
   pass a single wire loop through the toroid's window, carrying a known,
   low AC current from a bench AC source through a current-limiting load
   resistor (or your existing isolation TX from the mains-sense work).
   This is a safe way to inject a *real* primary current through the
   actual 44-turn CT without any coil or HV involved.
3. Sweep the test current up slowly. Using the 44:1 turns ratio, you can
   calculate the equivalent "real" primary current the pot is set to trip
   at:
   ```
   I_primary_trip ≈ I_secondary_at_trip × 44
   ```
   (I_secondary is whatever current your test loop is driving through the
   CT window when the latch trips — measure with a clamp meter or
   calculate from your test source + load resistor.)
4. Set the pot to a **deliberately conservative (too-low) starting
   threshold** — same philosophy as the original DeCosta bring-up. You
   will tune this upward later, in Stage 5, exactly as you did originally.

**Record your Stage 3 starting threshold here:** ______ A (primary,
equivalent) at pot position ______ .

---

## Stage 4 — Wire into PB12, verify with PWM running (still no HV)

**Goal:** prove the complete hardware fault path works, end to end, before
the coil ever sees current.

1. Wire the (now level-verified, Stage 2) latch output into **PB12
   (TIM1_BKIN)**.
2. Return to your existing Phase 4 bench setup: GDT + IXDN630MCI + IGBT
   gates connected, **low-voltage bench supply only, no HV**, per
   `docs/FIRMWARE.md`'s bring-up phases.
3. Power up, let the board boot normally (idle screen).
4. Start a run (PWM active, gates driving, still no HV on the bridge).
5. Trigger the Stage 3 test-current loop through the CT to manually
   induce a trip.
6. **Confirm all of the following:**
   - `PwmDrive::breakTripped()` fires — PWM outputs die immediately.
   - The TFT fault screen shows **"OVERCURRENT (HW)"** with "BKIN break
     (PB12)" underneath (this exact wording was added to `Display.cpp`
     during earlier bring-up — confirms the HW path specifically, not the
     software OCP path on PA1).
   - The latch stays tripped (doesn't self-clear) until you explicitly
     clear it via the encoder button (per `StateManager::handleButton()`,
     any fault state → press button → clears break, returns to IDLE).

**Do not proceed to Stage 5 until this end-to-end hardware path is
confirmed working.** This is the point where you have a real, bench-tested
fault path — not a simulated or partial one.

---

## Stage 5 — Variac ramp-up on the real coil

**Only after Stage 4 passes.** This is the same process you used
originally on the DeCosta board.

1. Connect the work coil and full HV path (mains → EMI filter → contactor
   → rectifier → DC bus → H-bridge → coil), per `BOARD_V2_DIRECT.md`'s HV
   section.
2. Start with the variac at **0V**.
3. Bring it up in small increments. Watch/listen for early signs of
   trouble at every step — this is still your primary protection layer
   during initial tuning, same as it always was.
4. If the OCP trips prematurely (nuisance trip below any real fault),
   back the pot off slightly and retry. If it doesn't trip when it
   should, stop and re-check Stages 1-4 — do not just keep increasing
   power.
5. Gradually tune the pot toward the real working threshold as you build
   confidence, exactly as with the original DeCosta build.

---

## Quick-reference: fault display legend (already in firmware)

| TFT shows | Meaning |
|---|---|
| OVERCURRENT (HW) — "BKIN break (PB12)" | This new OCP latch tripped |
| OVERCURRENT (SW) — "PA1 current sense" | Software ADC path on PA1 (separate, not built yet — out of scope for this guide) |

---

## Findings from physical continuity probing (SCITUBE HD board, VER 3)

Probing the actual populated-but-testable board (74HCT74, pin numbers per
standard 74HC/HCT74 pinout) resolved several open questions from the
schematic-image reconstruction:

| Pin | Name | Found connected to | Meaning |
|---|---|---|---|
| 1 | 1CLR (active-LOW clear) | 555 pin 3 (output), AND R1(10k)→10T20k "Current Limit" pot | Latch clear is driven by the 555's oscillator output, not a simple manual reset |
| 2 | 1D (data) | 5V | Tied high — ready to latch "1" on clock |
| 4 | 1PR (active-LOW preset) | 5V | Preset disabled (correct/expected) |
| 6 | 1Q̄ (inverted output) | UCC27425 pin 1 and pin 8 | **This is the "enable"/trip output** — confirmed |
| 14 | VCC | 5V | **Confirms 5V logic — do NOT wire pin 6 directly to PB12 (abs max ~3.6V). Level-shift required.** |

**555 (soft-start/ramp section) — pin 2 → R13(10k), pins 6/7 → R20(100k):**
this is a standard 555 **astable oscillator** wiring, not a one-shot
power-up pulse. Interpretation: the 555 likely auto-clears/re-arms the
latch periodically during the original board's soft-start ramp (so
transient nuisance trips during ramp-up get retried, while a sustained
real overcurrent holds the trip despite repeated clear attempts). This is
solving a problem Board v2's architecture doesn't have in the same form
(manual variac ramp instead of an automatic soft-start ramp).

**Decision: skip the 555, hard-wire CLR (pin 1) to 5V instead.** This
gives manual-clear-only latch behavior — once tripped, stays tripped until
CLR is deliberately pulled LOW. This matches the intended Stage 4/5
behavior (deliberate manual fault clearing, not auto-retry).

**Pot network fully traced — it's not just a threshold pot:**

| Pot pin | Connects to | Role |
|---|---|---|
| 1 | R1(10k) → 74HCT74 pin 1 (CLR) | One end of pot's resistive element, into the clear line |
| 2 (wiper) | R16(15k) → MPSA06 pin 3 (collector) | **This is the actual threshold-adjust tap** — wiper position biases Q1's collector, shifting the trip point |
| 3 | C8 (1kV/10nF) | Other end of pot's resistive element, into a cap |

This pot is structurally part of BOTH the threshold-setting network (via
the wiper, pin 2) AND an RC-style network on the CLR line (via pins 1/3
through R1 and C8) — not two independent things. Given this integration,
guessing at "just tie CLR to 5V" risks disturbing behavior we don't fully
understand from topology alone.

**Revised plan (lower-risk):** cut ONLY the trace from 555 pin 3 to
74HCT74 pin 1. Leave the entire R1/pot/C8 network on the CLR line
completely intact. Then add a pull-up resistor (~10kΩ to 5V) on the CLR
line at the point where the 555 used to feed in, to guarantee CLR settles
to a valid inactive-HIGH once the 555's output is disconnected. This
removes the 555 dependency while preserving whatever the pot/R1/C8 network
was doing electrically — if that network's contribution to CLR timing
turns out to matter, it'll show up empirically during Stage 1 bench
testing rather than being guessed away now.

**Decided — fault clear is STM32-driven only, no separate physical
switch.** `StateManager::handleButton()` already clears `PwmDrive`'s BKIN
latch on any fault state via the encoder button; extend this so the same
button press also pulses a spare GPIO LOW-then-HIGH into 74HCT74 pin 1
(CLR) on the OCP board, unifying fault-clear into the existing flow. Needs
a spare GPIO pin assigned in `config.h` (not yet allocated — check
`docs/NUCLEO_PINOUT.md` for an open pin) and a small firmware addition in
`StateManager::handleButton()`'s fault-clear branch.

## Open items to fill in as you go

- [x] Latch output net confirmed: pin 6 (1Q̄) → UCC27425 pins 1/8 (enable)
- [x] Measured VCC (latch supply rail): 5V — **level-shift to PB12 required**
- [ ] R1/pot-to-pin-1 connection purpose confirmed before hard-wiring CLR to 5V
- [ ] Latch clear mechanism decided: manual switch vs. STM32 GPIO pulse: ______
- [ ] Level-shift component/design decision (Stage 2): ___________________________
- [ ] Stage 3 starting trip threshold: ______ A (primary equiv.) @ pot ______
- [ ] Stage 4 end-to-end confirmed: date ______ initials ______
- [ ] Stage 5 final tuned threshold: ______ A (primary equiv.) @ pot ______
