# Simplified OCP Board — Plain-Text Schematic (v2 — SUPERSEDED)

> **⚠️ SUPERSEDED BY `OCP_BOARD_Final.md`. Do not build from this document.**
>
> Retained because it holds the real bench measurements that the current
> design is calibrated against — 1.24 V burden peak at 30 V-in, 5.12 V at
> 150 V-in, 57 kHz non-clipping — plus the CT/burden build details.
>
> The circuit itself has confirmed faults; see the banner in
> `OCP_BUILD_GUIDE.md`. Note in
> particular that the Stage 1b discussion concludes the threshold can go
> to "4.5-5 V" because the pot can't exceed VCC. The real ceiling is the
> LM393's common-mode limit of Vcc − 1.5 V = 3.5 V, which makes the
> "option (a) accept reduced margin" decision unworkable rather than
> merely tight. v4 solves it with a 0.3125x attenuator.

Plain text/ASCII layout — renders correctly in any editor or terminal, no
diagram extension required. Real measured values throughout (see
`BOARD_V2_DIRECT.md` bring-up log for the underlying bench data).

**Revision note:** this version replaces an earlier draft that labeled
comparator 2 as "SET/RESET" without a real working latch topology, and
implicitly assumed pull-ups without stating values or connections. Every
pull-up, every pin, and every polarity is made explicit below. LM393
outputs are OPEN-COLLECTOR — they can only pull LOW, never drive HIGH on
their own. Every open-collector output in this design has its own
explicit pull-up resistor with a stated value and connection point.

---

## LM393 pinout reference (only comparator 1 used in this design)

8-pin DIP/SOP package:

| Pin | Name | Notes |
|---|---|---|
| 1 | OUT1 | Comparator 1 output — open-collector — **used** |
| 2 | IN1(-) | Comparator 1 inverting input — **used** |
| 3 | IN1(+) | Comparator 1 non-inverting input — **used** |
| 4 | GND | Shared — **used** |
| 5 | IN2(+) | Comparator 2 non-inverting input — **unused, tie off per BOM notes** |
| 6 | IN2(-) | Comparator 2 inverting input — **unused, tie off per BOM notes** |
| 7 | OUT2 | Comparator 2 output — open-collector — **unused, leave unconnected (open-collector, safe to float)** |
| 8 | VCC | Shared — **used** |

**LM393 output polarity rule:**
Output pulls LOW when V(IN-) > V(IN+).
Output floats HIGH (pulled up by its external pull-up resistor) when
V(IN+) > V(IN-).

**Comparator 2 is unused in this design** (the latch function moved to
the 74HCT74, see Stage 1c). Per the BOM, its unused inputs (IN2+, IN2-)
should be tied to a defined level (commonly GND or VCC, either is fine
since nothing depends on its output) rather than left floating — floating
comparator inputs can oscillate or draw excess current even when their
output is never used.

---

## 1. Current-Sense OCP Trip (real protection path → PB12)

### Stage 1a — CT/burden signal conditioning (unchanged from earlier drafts)

```
 [Existing CT/Burden board — unchanged]
 43T CT + 100Ω/100W burden + existing 330nF/2kV cap
                │
                │  burden output, measured:
                │  1.24V peak @ 30V-in
                │  5.12V peak @ 150V-in (57kHz, non-clipping)
                ▼
        ┌───────────────┐
        │  UF4007 diode │  rectifies burden signal
        └───────┬───────┘
                ▼
        ┌───────────────┐
        │  C_peak        │  2.2nF MKT film, 63V (confirmed on hand)
        │  peak-detect   │  holds rectified peak voltage between cycles
        └───────┬───────┘
                │
                ▼
         NODE "SIGNAL" ── this is the comparator 1 input signal,
                           carries the peak-detected burden voltage
```

### Stage 1b — Comparator 1 (the trip detector)

```
         NODE "SIGNAL" ──────────────────────► pin 3, IN1(+)
                                                      │
         NODE "THRESH" ──────────────────────► pin 2, IN1(-)
                                                      │
                                              ┌───────┴────────┐
                                              │  LM393          │
                                              │  Comparator 1   │
                                              └───────┬─────────┘
                                                      │
                                                pin 1, OUT1
                                                (open-collector)
                                                      │
                                    ┌─────────────────┴──────────────────┐
                                    │                                    │
                              R_pu1 = 10kΩ                          NODE "TRIP1"
                              pin 1 → VCC (5V rail)                 (goes to comparator 2,
                              (pull-up — REQUIRED,                   see Stage 1c below)
                               OUT1 cannot drive HIGH
                               without this)
```

**NODE "THRESH" (the pot):**

```
   VCC (5V) ──────┐
                  ▼
            ┌───────────┐
            │  10kΩ pot │  "OCP Threshold" — 1-turn, confirmed on hand
            │  (wiper)  │  wiper → NODE "THRESH" → pin 2, IN1(-)
            └───────────┘
                  │
                 GND
```

Pot wired as a simple voltage divider between VCC and GND, wiper feeds
IN1(-). Turning the pot sets the threshold voltage comparator 1 compares
the signal against.

**✅ RESOLVED — VCC = 5V, from the existing dedicated 5V/5A rail already
on the main driver board.** Confirmed in-spec for both parts: LM393
supports 2V-36V single-supply (5V is a common, well-characterized
operating point per its own datasheet); 74HCT74 is a standard 5V-family
logic part by design ("HCT" = TTL-compatible thresholds at 5V). No
dedicated regulator needed for this board — it taps the same 5V/5A rail
already feeding other logic on the driver board. Nucleo's 3.3V rail is
used only where explicitly noted (PC817 output-side pull-up, Stage 1d).

With VCC = 5V confirmed, the pot's threshold starting point should be set
so NODE "THRESH" corresponds to roughly 1.5-2x the measured 5.12V burden
peak — i.e., start the wiper somewhere in the 7-10V range... **but note:
NODE "THRESH" cannot exceed VCC = 5V**, since the pot divides between VCC
and GND. This means the originally-discussed "8-10V equivalent" starting
point is not achievable as a direct wiper voltage on a 5V rail.

**DECIDED: option (a) — accept the reduced margin, no attenuation stage
for now.** Keep this build as simple as possible at current test power
levels; set the pot threshold near the top of the achievable 0-5V range
(e.g. 4.5-5V) and tune from there on the bench. Adding a rescaling
attenuation stage is explicitly deferred — if/when this build moves to
higher power (e.g. the planned 240V variac work) and the burden signal
starts approaching or clipping at the 5V rail, that is the trigger to
design a second-generation OCP board with proper headroom, rather than
retrofitting this one. This is a deliberate simplicity-first choice, not
an oversight.

**Behavior of comparator 1:**
- Normal operation (signal below threshold): V(IN1+) < V(IN1-) →
  OUT1 pulls LOW.
- Overcurrent (signal exceeds threshold): V(IN1+) > V(IN1-) →
  OUT1 floats HIGH (via R_pu1).

**So NODE "TRIP1" is normally LOW, and goes HIGH on an overcurrent
event.** This is the signal that needs to set the latch.

### Stage 1c — 74HCT74 dual D flip-flop used as the latch

**Revision note:** an earlier draft of this section tried to build the
latch from a second LM393 comparator channel, cross-coupled with a
feedback resistor. Walking through it rigorously surfaced a real polarity
bug in the reset path AND an incomplete bias reference on IN2(-) — no
clean fix without a proper mid-rail bias network. Rather than carry that
risk into a physical build, this stage now uses a **74HCT74 dual D
flip-flop** (confirmed on hand from the previous build) instead. A
flip-flop's Q output holds its state by internal design — no bias
network to get wrong, no derivation risk. This is a strictly better
component choice for this job than a second comparator channel.

**74HCT74 pinout (flip-flop 1 only is used — same convention as the
original DeCosta board's use of this same part):**

| Pin | Name | Function |
|---|---|---|
| 1 | 1CLR | Clear, active-LOW |
| 2 | 1D | Data input |
| 3 | 1CLK | Clock, rising-edge triggered |
| 4 | 1PR | Preset, active-LOW |
| 5 | 1Q | Output |
| 6 | 1Q̄ | Inverted output |
| 7 | GND | Ground |
| 14 | VCC | Supply |

```
   NODE "TRIP1" (comparator 1's OUT1, via R_pu1 = 10kΩ pull-up,
                 as established in Stage 1b above)
        │
        └──────────────────────────────────────► pin 3, 1CLK
                                                   (rising edge LOW→HIGH
                                                    clocks in a "1")

   VCC ─────────────────────────────────────────► pin 2, 1D
                                                   (tied HIGH — always
                                                    ready to latch "1")

   VCC ─────────────────────────────────────────► pin 4, 1PR
                                                   (tied HIGH — preset
                                                    disabled, unused)

   VCC ──[R_clr_pu = 10kΩ]──┬───────────────────► pin 1, 1CLR
                            │
                    (Reset button module,
                     confirmed active-LOW output
                     per continuity test)
                            │
                           GND (when button pressed)

                                              ┌───────────────┐
                                              │   74HCT74      │
                                              │  Flip-flop 1   │
                                              └───────┬────────┘
                                                       │
                                                pin 5, 1Q
                                                (this IS the latch
                                                 output — internal
                                                 flip-flop design
                                                 holds this state,
                                                 no external feedback
                                                 network required)
                                                       │
                                                       ▼
                                          NODE "LATCHED_OUT"
                                          (feeds PC817 stage, Stage 1d)

   pin 7 = GND, pin 14 = VCC (shared supply pins, standard)
   pin 6 (1Q̄) — unused in this design, leave unconnected
```

**Verified behavior (this is a real, internally-guaranteed flip-flop
truth table — not something we're deriving from scratch):**

*Idle, no trip, button not pressed:*
- 1CLR = HIGH (via R_clr_pu, button not pressed) — clear is inactive.
- 1CLK has not seen a rising edge (comparator 1's OUT1 stays LOW while
  under threshold) — flip-flop holds its last state. At power-up, most
  74HCT74 implementations reset to Q=LOW on the very first CLR pulse or
  power cycle; if this needs to be guaranteed at boot, a brief LOW pulse
  on 1CLR at power-up (or simply pressing the reset button once before
  first use) puts it in a known state. **Practical note: verify this in
  practice — pulse CLR once during initial bench power-up before relying
  on the idle state.**
- 1Q = LOW. NODE "LATCHED_OUT" = LOW.

*Overcurrent trip occurs:*
- Comparator 1's OUT1 transitions LOW→HIGH (signal exceeded threshold).
- This rising edge on 1CLK, with 1D held HIGH, clocks a "1" into the
  flip-flop.
- 1Q goes HIGH — **and stays HIGH**, by the flip-flop's own internal
  design, regardless of what 1CLK does afterward (no more rising edges
  needed, no feedback network required — this is the actual point of
  using a real flip-flop here).
- NODE "LATCHED_OUT" = HIGH. Feeds the PC817 stage, trips PB12.

*Clearing the trip (button pressed):*
- Button press pulls pin 1, 1CLR, LOW — directly and unambiguously
  activates the flip-flop's asynchronous clear function.
- 1Q is forced back to LOW immediately, independent of 1CLK or 1D.
- NODE "LATCHED_OUT" = LOW. Trip cleared.
- Releasing the button lets R_clr_pu pull 1CLR back HIGH, re-arming the
  flip-flop to catch the next trip.

**This behavior is correct and requires no further derivation** — every
transition follows directly from the 74HCT74's documented, standard
flip-flop truth table, not a custom bias network we have to verify by
hand.

---

### Stage 1d — PC817 isolation into PB12 (unchanged from earlier design)

```
   NODE "LATCHED_OUT" (74HCT74 pin 5, 1Q)
        │
        ▼
   ┌───────────────┐
   │  680Ω         │  LED-side series resistor (confirmed on hand)
   └───────┬───────┘
           ▼
   ┌───────────────────────┐
   │       PC817            │  optocoupler — TRUE GALVANIC ISOLATION
   │  CTR ≥ 50% @ 5mA        │  between OCP board's analog/logic side
   └───────────┬─────────────┘  and the Nucleo
               │ isolated output (phototransistor)
               ▼
   ┌───────────────┐
   │   3kΩ          │  pull-up to 3.3V (Nucleo rail) — confirmed on hand
   └───────┬───────┘
           ▼
   ═══════════════════════════════════════
   PB12 (TIM1_BKIN) — Nucleo CN10 pin 16
   ═══════════════════════════════════════
   Idle  = HIGH (3.3V) = no fault
   Trip  = LOW (PC817 pulls down) = fault
   Matches existing active-LOW BKP=0 firmware config.
   REPLACES the bare 10kΩ pull-up placeholder currently there.
```

---

## 2. Tank Voltage Sense (display only — NOT a trip path) — unchanged, no errors found here

```
   Tank Leg A                                    Tank Leg B
   (either coil leg,                             (reference return,
    arbitrary)                                    other coil leg)
        │                                              │
        ▼                                              │
   ┌─────────┐                                         │
   │  10MΩ   │  top leg                                │
   └────┬────┘                                         │
        │                                              │
        ●──────────── MIDPOINT ─────┐                  │
        │             (tap point)   │                  │
        ▼                           │                  │
   ┌─────────┐                      │                  │
   │  2MΩ    │  bottom leg          │                  │
   └────┬────┘                      │                  │
        │                           │                  │
        └───────────────────────────┴──────────────────┘
                    (bottom leg returns to Leg B)

   Ratio: 2MΩ / 12MΩ ≈ 1:6
   Measured: ~9-13Vpp real tank V @ 150V-in →
             ~1.5-2.2Vpp at midpoint (two independent bench
             readings, see BOARD_V2_DIRECT.md for both data points)

        MIDPOINT
            │
            ▼
      ┌───────────┐
      │ 100nF MKT │  smooths for ADC sampling (confirmed on hand, 63V)
      │ film cap  │
      └─────┬─────┘
            ▼
   ═══════════════════════════════════════
   PC2 (ADC1_IN12) — Nucleo CN7 pin 35
   ═══════════════════════════════════════
   NEW pin assignment (currently unused).
   Firmware scales reading by known ÷6 ratio + peak-detect
   factor to estimate real tank voltage for TFT display only.
   NEVER connects to PB12 or any protection logic — purely
   cosmetic "Tank: ~XX V" readout.
```

---

## New Nucleo pin/firmware summary

| Pin | Function | Status | Notes |
|---|---|---|---|
| PB12 (CN10-16) | TIM1_BKIN, OCP trip in | Existing pin | Bare 10kΩ pull-up placeholder replaced by the PC817 stage. No firmware change needed — same active-LOW polarity already expected. |
| PC2 (CN7-35) | ADC1_IN12, Tank-V sense | **New** | Add `#define PIN_ADC_TANKV PC2` to `config.h`. Add read + display field. No open issues. |
| PC3 (CN7-37) | Reset button in | **New — but wired directly into the OCP board's 1CLR network, NOT read by the Nucleo.** | The reset button module connects directly to the 74HCT74's 1CLR pin (Stage 1c) on the OCP board itself — it does not need to run to a Nucleo GPIO at all. **PC3 assignment is no longer needed for this purpose.** |
| PB15 (CN10-26) | Latch reset out | **Not needed** | The 74HCT74's own asynchronous CLR pin handles reset directly via the button, with no Nucleo GPIO involvement. This simplifies the original plan — one less pin, one less firmware responsibility. |

**Net result: this redesign needs ZERO new Nucleo GPIOs for the reset
function** — the button wires directly into the OCP board's flip-flop.
PC2 (Tank-V ADC) is still needed as originally planned. PB12 is unchanged
(existing pin, new source driving it).

---

## Design principles vs. the DeCosta board (still valid, unaffected by the latch issue above)

- One pot, one job — threshold only, never tangled with reset timing or tank-V sensing (DeCosta's single pot did all three).
- Tank-V is informational only — fully decoupled from the trip/latch path, cannot cause a false trip.
- True opto-isolation (PC817) instead of a bare resistor divider into PB12.
- Dedicated reset button — no state-dependent overload of the existing encoder button.
- Every stage independently bench-testable (signal generator → comparator → latch → opto → PB12), unlike DeCosta's single pot serving three interdependent roles that made isolated testing impossible.

---

## Bill of Materials

Grouped by section. All items below marked "Confirmed on hand" per
conversation.

### 1. Current-Sense OCP Trip

| Qty | Part | Value/Type | Status | Notes |
|---|---|---|---|---|
| 1 | Diode | UF4007 (ultrafast rectifier) | Confirmed on hand | Rectifies burden signal |
| 1 | Capacitor | 2.2nF MKT film, 63V | Confirmed on hand | Peak-detect hold cap |
| 1 | IC | LM393 (dual comparator) | Confirmed on hand (BOJACK kit) | Only comparator 1 (pins 1/2/3) used — trip detector. Comparator 2 unused, leave IN2(+)/IN2(-) tied to GND or VCC per datasheet guidance to avoid an undefined floating input. |
| 1 | IC | 74HCT74 (dual D flip-flop) | Confirmed on hand (from previous build) | Only flip-flop 1 (pins 1-7) used — the latch. Flip-flop 2 unused, tie its CLR/PR to VCC and D to GND or VCC per standard practice for an unused section. |
| 1 | Potentiometer | 10kΩ, 1-turn | Confirmed on hand | "OCP Threshold" |
| 1 | Resistor | R_pu1 = 10kΩ | Provisional value, non-critical | Comparator 1 OUT1 pull-up — REQUIRED, LM393 output is open-collector |
| 1 | Resistor | R_clr_pu = 10kΩ | Provisional value, non-critical | 74HCT74 1CLR pull-up — holds clear inactive until button press |
| 1 | Resistor | 680Ω | Confirmed on hand | PC817 LED-side series resistor |
| 1 | Optocoupler | PC817 | Confirmed on hand | True isolation into PB12 |
| 1 | Resistor | 3kΩ | Confirmed on hand | PC817 output-side pull-up to 3.3V |
| — | Misc | Perfboard or small PCB, wire, standoffs | — | Physical build |

### 2. Tank Voltage Sense (display only) — no open issues

| Qty | Part | Value/Type | Status | Notes |
|---|---|---|---|---|
| 1 | Resistor | 10MΩ | Confirmed on hand | Top leg |
| 1 | Resistor | 2MΩ | Confirmed on hand | Bottom leg |
| 1 | Capacitor | 100nF MKT film, 63V | Confirmed on hand | Smooths divider midpoint |
| — | Wire | HV-rated | Already run | Tank Leg A/B taps |

### 3. Reset / Nucleo Interface

| Qty | Part | Value/Type | Status | Notes |
|---|---|---|---|---|
| 1 | Momentary switch module | Digital push-button, 3-pin (VCC/GND/OUT), active-LOW | Confirmed on hand | Power from 3.3V rail (module side). Output wires directly to 74HCT74 pin 1 (1CLR) on the OCP board — no Nucleo GPIO involved. |

### Not needed for this build

- 555 timer, ramp pot, R20/R17/LED1 — no soft-start section in this design
- UCC27425 — not used, PB12 interfaces directly via the PC817 stage
- DeCosta board itself — fully retired, kept as spare/reference only

---

## Open items — status

- [x] `C_peak` — 2.2nF MKT film, 63V, confirmed on hand
- [x] PC817 series resistor — 680Ω confirmed on hand
- [x] PC817 pull-up resistor — 3kΩ confirmed on hand
- [x] Tank-V smoothing cap — 100nF MKT film, 63V, confirmed on hand
- [x] OCP Threshold pot — 10kΩ, 1-turn, confirmed on hand
- [x] **RESOLVED — latch stage now uses the 74HCT74 dual flip-flop**
      (confirmed on hand from the previous build) instead of a second
      LM393 comparator channel. Removes the bias-network derivation risk
      entirely — the flip-flop's Q output holds state by internal design,
      verified against its standard, documented truth table.
- [x] Reset mechanism finalized: button wires directly to 74HCT74 1CLR,
      no Nucleo GPIO needed. PC3/PB15 assignments from the original plan
      are no longer required.
- [ ] Bench-verify the 74HCT74's power-up state — pulse 1CLR once during
      initial bring-up to guarantee Q starts LOW, per the note in
      Stage 1c above.
- [ ] Confirm unused LM393 comparator-2 inputs and unused 74HCT74
      flip-flop-2 inputs are tied off per datasheet guidance (added to
      BOM notes above) — floating CMOS/TTL inputs on unused sections can
      cause erratic power draw or crosstalk even if that section is
      otherwise unused.
- [x] VCC confirmed = 5V, from the existing dedicated 5V/5A rail on the
      main driver board. Confirmed in-spec for both LM393 and 74HCT74.
- [x] **DECIDED — threshold margin is tighter than planned, but
      accepted as-is for simplicity.** VCC=5V and the measured burden
      signal already peaks at 5.12V at full power, leaving little room
      in the pot's 0-5V range. No attenuation stage added — deliberate
      choice to keep this build simple at current power levels. Set the
      pot near 4.5-5V and tune on the bench. **Trigger for a future,
      properly-scaled second-generation board: if/when burden signal
      approaches or clips at 5V as power scales up (e.g. the 240V
      variac work).** Not a blocker for building and testing now.
