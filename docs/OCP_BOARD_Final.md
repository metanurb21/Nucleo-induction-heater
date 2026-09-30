# OCP Board — Build Document

Overcurrent protection for the induction heater. Senses tank current via
the existing CT/burden, compares a peak-detected and attenuated sample
against an adjustable threshold, and drops an opto-isolated fail-safe line
into the Nucleo's TIM1 break input (PB12) to kill the gate drive in
hardware.

**Build from the connection list.** The diagrams are for orientation; the
connection list is authoritative.

---

## Scope and limits

End-to-end latency is **30–80 µs**, dominated by the optocoupler's
turn-off. At 27 kHz that is one to two cycles.

This is **overcurrent protection, not short-circuit protection.** It
catches progressive overcurrent, detuning and coupling changes. IGBT
desaturation happens in ~10 µs and will beat this chain — short-circuit
protection requires a desat detector at the gate driver, which is a
separate job.

**There is no latch on this board.** TIM1 provides it: a BKIN event clears
MOE in hardware (~6 ns) and sets BIF, which holds until firmware clears it,
and MOE can only be re-set by firmware. Reset is the encoder button via
`StateManager`. The board itself re-arms automatically as the peak detector
decays.

---

## Bill of materials

| Qty | Part | Value | Role |
|---|---|---|---|
| 1 | IC | LM393 | Comparator |
| 1 | Optocoupler | PC817 | Isolation to PB12 |
| 1 | Diode | UF4007 | Peak-detect rectifier |
| 1 | Pot | 10 kΩ 1-turn (Bourns 3310P) | Threshold |
| 1 | LED | Red 3 mm, **Vf ≤ 2.2 V** | ARMED indicator |
| 1 | Resistor | 220 Ω | Front-end series |
| 1 | Resistor | 270 Ω | LED string (see Stage 3) |
| 1 | Resistor | 1 kΩ | Pot bottom limit |
| 1 | Resistor | 1.5 kΩ | Opto emitter, Nucleo side |
| 1 | Resistor | 7.5 kΩ | Pot top limit |
| 1 | Resistor | 10 kΩ | Attenuator bottom |
| 2 | Resistor | 22 kΩ | Attenuator top, OUT1 pull-up |
| 1 | Resistor | 1 MΩ | Hysteresis |
| 1 | Cap | 1 nF | PB12 filter |
| 1 | Cap | 10 nF film | C_peak |
| 3 | Cap | 100 nF | 2× decoupling, threshold filter |
| 1 | Cap | 100 µF electrolytic | Supply bulk |
| — | Perfboard, 8-pin socket, 4-pin socket, wire, standoffs | | |

Meter every resistor before fitting it. Use sockets for the LM393 and
PC817.

---

## Four rules

**1. One ground.** Everything on the board side shares a single continuous
ground node. Any two ground points must read under 1 Ω to each other. The
supply filter is in the positive leg only; ground passes straight through.

**2. The PC817 is the only isolation barrier.** Nucleo 3.3 V and Nucleo GND
touch *only* the opto output side — PC817 pins 3 and 4, the 1.5 kΩ and the
1 nF. Nothing else on the board goes near Nucleo ground. One convenience
strap defeats the isolation between your tank and the MCU.

**3. Ground the correct burden leg.** The CT/burden also feeds the 74HC14
frequency chain, which references one side of the burden to the driver
board ground. Ground **that same physical leg** here. Ground the opposite
leg and the two grounds short the 100 Ω burden out, killing both the OCP
signal and the PLL feedback. Buzz it out before powering.

**4. No capacitor on PC817 pin 2.** It connects to LM393 pin 1 and nothing
else. A cap there must charge through the LED string when OUT1 releases,
holding the optocoupler on for ~100 µs at 100 nF and wrecking the trip
latency.

---

## Stage 0 — Supply

```
  Main board 5V ─────────┬──────────┬──────────┬──────► VCC rail
                         │          │          │
                      [100µF]+   [100nF]    [100nF]
                         │        at LM393    near the
                         │-       pin 8       270Ω's VCC tap
                         │          │          │
  Main board GND ────────┴──────────┴──────────┴──────► GND rail
```

- **VCC = 5.1 V** on this rig. Every threshold below assumes it.
- No series resistor and no series diode in the positive leg. Use a keyed
  JST connector to prevent a reversed supply.
- Both 100 nF go **VCC to GND**, mounted physically at the pins they serve,
  with short leads. Neither attaches to a PC817 pin.
- Observe the 100 µF polarity: on an aluminium radial can the **stripe
  marks negative**. Negative goes to GND.

---

## Stage 1 — CT front end

```
  Existing CT/burden: 44T CT, 100Ω/100W burden, 0.1µF/2kV cap
  Measured: 1.24 V pk @ 30 V-in,  5.12 V pk @ 150 V-in (57 kHz)

   burden leg 1 │                     burden leg 2 │
                ▼                                  ▼
            [220Ω]                                GND
                │
                ▼
            ──►│── UF4007
                │
                ▼
           NODE "PK" ──┬──[10nF film]── GND
                       │
                    [22kΩ]
                       │
           NODE "SIG" ─┴──┬──[10kΩ]── GND
                          │
                          └────────► LM393 pin 3 (IN1+)
```

- Attenuation NODE PK → NODE SIG = 10k / 32k = **0.3125**
- Discharge τ = 10 nF × 32 kΩ = **320 µs**. 11% droop per cycle at 27 kHz,
  5% at 57 kHz, full reset ~1.5 ms after current stops. The 10 kΩ is what
  gives C_peak its discharge path — without it the node ratchets to the
  rail on the LM393's input bias current alone.
- Attack τ = (220 Ω + 100 Ω burden) × 10 nF = **3.2 µs**, ~2 cycles to full
  peak at 27 kHz.

**Transfer function:** NODE SIG = (V_burden_pk − 0.8 V) × 0.3125

| Burden peak | Approx. input | NODE SIG |
|---|---|---|
| 1.24 V | 30 V | 0.14 V |
| **5.12 V** | **150 V** | **1.36 V** |
| 7.15 V | ~210 V | 2.00 V ← threshold |
| 8.2 V | ~240 V | 2.33 V |
| 10.6 V | ~310 V | 3.08 V (top of pot range) |

---

## Stage 2 — Threshold

```
   VCC ──[7.5kΩ]──┬── pot pin 1
                  │
             [10kΩ pot]   "OCP THRESHOLD"
                  │
                  ├── pin 2 (wiper) ──┬──► NODE "THRESH" ──► LM393 pin 2 (IN1-)
                  │                   │
                  │               [100nF]
                  │                   │
   GND ──[1kΩ]────┴── pot pin 3      GND
```

- Chain totals 18.5 kΩ. Wiper range **0.27 V to 3.1 V**.
- LM393 common-mode ceiling is VCC − 1.5 V = **3.6 V**. The 7.5 kΩ keeps
  the wiper 0.5 V below it. Above that ceiling the comparator can read its
  inputs reversed and **fail to trip** — this bracket is not optional.
- The 1 kΩ stops the threshold reaching 0 V, where it would trip on any
  signal at all.
- Always set the threshold while metering **LM393 pin 2**, not the wiper.
  That is the voltage the comparator actually compares.

Recompute if you substitute: `V_max = VCC × 11k / chain`,
`V_min = VCC × 1k / chain`, ceiling `= VCC − 1.5 V`.

---

## Stage 3 — Comparator and output

```
   ── BOARD SIDE (driver board ground) ─────────┊── NUCLEO SIDE ──
                                                ┊
   VCC ──[22kΩ]───────────────┐                 ┊    Nucleo 3.3V
                              │                 ┊         │
   VCC ──[270Ω]──►│───────►│──┤                 ┊    PC817 pin 4
              LED_ARMED   PC817                 ┊    (Collector)
              (red)       pin 1                 ┊         │
                          (anode)               ┊    PC817 pin 3
                              │                 ┊    (Emitter) ──┬──► PB12
   NODE "SIG" ──► pin 3 ──────┤◄──[1MΩ]         ┊                │
                  (IN1+)      │   hysteresis    ┊           [1.5kΩ]  [1nF]
   NODE "THRESH" ► pin 2      │                 ┊                │      │
                  (IN1-)      │                 ┊        Nucleo GND ────┘
                              │                 ┊
                  pin 1 ──────┴── PC817 pin 2   ┊
                  (OUT1)          (cathode)     ┊

   LM393 pin 8 = VCC (100nF at the pin), pin 4 = GND
   LM393 pins 5, 6 → GND.  Pin 7 → unconnected.
```

Both LEDs face the same direction: current runs VCC → 270 Ω → red LED →
PC817 LED → OUT1.

### Polarity

The LM393 pulls LOW when IN− > IN+ and floats when IN+ > IN−. Signal on
IN+, threshold on IN−:

| Condition | OUT1 | LEDs | PB12 | Result |
|---|---|---|---|---|
| Healthy, signal < threshold | LOW | **lit** | ~3.1 V | gates permitted |
| Overcurrent, signal > threshold | floats | dark | ~0 V | BKIN → gates off |
| Board unpowered | floats | dark | LOW | gates off |
| Cable pulled | — | — | LOW via 1.5 kΩ | gates off |

**Healthy is the actively driven state**, so every failure lands on "gates
off". BKIN is active-LOW (BKP=0), matching firmware — no polarity change
needed.

### Values

- **270 Ω depends on your LED's Vf.** Target 5–6 mA:
  `I = (VCC − Vf_LED − 1.2 V − 0.3 V) / R`

  | LED Vf | R at 5.1 V | Current |
  |---|---|---|
  | 1.8 V | 330 Ω | 5.5 mA |
  | **2.0–2.2 V** | **270 Ω** | **5.6 mA** |

  **Red only.** Red has the lowest forward voltage, and that headroom is
  what remains for the optocoupler. A 3.3 V LED leaves ~0.3 V across the
  resistor — under 1 mA, which starves the PC817 so PB12 never comes up.
  Do not compensate by shrinking the resistor; at that little headroom the
  current swings wildly with LED and rail variation.

- `22 kΩ` pull-up gives OUT1 a defined logic high for the hysteresis
  resistor to work from. OUT1 sinks 5.6 mA LED + 0.22 mA pull-up = 5.8 mA,
  inside the LM393's guaranteed 6 mA at Vol ≤ 400 mV.
- `1 MΩ` OUT1 → IN1+ gives **~35 mV of hysteresis** (NODE SIG Thevenin
  impedance is 6.9 kΩ). Once tripped it pushes IN+ up, making the trip
  stickier — the correct direction. On pin 3, not pin 2.
- `LED_ARMED` in series with the opto LED: lit means powered, below
  threshold, and sinking current. If it fails open the opto goes dark too,
  so failure lands safe. It does not latch — the peak detector decays in
  320 µs, so a trip blinks it dark for about a millisecond. The latched
  state lives in the Nucleo.
- `1.5 kΩ` emitter resistor, sized for turn-off speed. Worst-case PC817
  CTR of 50% gives 2.8 mA against the 2.07 mA needed to hold PB12 at
  3.1 V — 1.35× margin. **Measure it** (checks below).
- `1 nF` at PB12 gives ~1.5 µs of glitch filtering. F446 TIM1 has no BKF
  break filter, so all break-input filtering is external; the 320 µs
  peak-detect hold is what makes that sufficient.

---

## Connection list

### Stage 0

| From | To |
|---|---|
| Main board 5 V | VCC rail — direct, no diode, no resistor |
| Main board GND | GND rail — direct |
| VCC rail | 100 µF (+) |
| 100 µF (−) | GND rail |
| VCC rail | 100 nF → GND, at LM393 pin 8 |
| VCC rail | 100 nF → GND, near the 270 Ω's VCC tap |

### Stage 1

| From | To |
|---|---|
| Burden leg 1 | 220 Ω |
| 220 Ω (other end) | UF4007 anode |
| Burden leg 2 | GND rail — **same leg the 74HC14 chain references** |
| UF4007 cathode | NODE PK |
| NODE PK | 10 nF film → GND |
| NODE PK | 22 kΩ |
| 22 kΩ (other end) | NODE SIG |
| NODE SIG | 10 kΩ → GND |
| NODE SIG | LM393 pin 3 (IN1+) |

### Stage 2

| From | To |
|---|---|
| VCC | 7.5 kΩ |
| 7.5 kΩ (other end) | Pot pin 1 |
| Pot pin 3 | 1 kΩ |
| 1 kΩ (other end) | GND |
| Pot pin 2 (wiper) | NODE THRESH |
| NODE THRESH | 100 nF → GND |
| NODE THRESH | LM393 pin 2 (IN1−) |

### Stage 3

| From | To |
|---|---|
| LM393 pin 1 (OUT1) | 22 kΩ |
| 22 kΩ (other end) | VCC |
| LM393 pin 1 (OUT1) | 1 MΩ |
| 1 MΩ (other end) | NODE SIG |
| VCC | 270 Ω |
| 270 Ω (other end) | LED_ARMED anode (long leg) |
| LED_ARMED cathode | PC817 pin 1 (Anode) |
| PC817 pin 2 (Cathode) | LM393 pin 1 (OUT1) — nothing else, no cap |
| LM393 pin 4 | GND |
| LM393 pin 8 | VCC |
| LM393 pin 5 (IN2+) | GND |
| LM393 pin 6 (IN2−) | GND |
| LM393 pin 7 (OUT2) | unconnected |
| PC817 pin 4 (Collector) | Nucleo 3.3 V |
| PC817 pin 3 (Emitter) | Nucleo PB12 |
| PC817 pin 3 (Emitter) | 1.5 kΩ |
| 1.5 kΩ (other end) | **Nucleo GND** |
| PC817 pin 3 (Emitter) | 1 nF → **Nucleo GND** |

Nucleo pins used: **PB12 only.**

### Capacitor grounds

| Cap | From | To |
|---|---|---|
| 100 µF | VCC rail | Board GND |
| 100 nF | VCC rail | Board GND |
| 100 nF | VCC rail | Board GND |
| 10 nF | NODE PK | Board GND |
| 100 nF | NODE THRESH | Board GND |
| 1 nF | PC817 pin 3 / PB12 | **Nucleo GND** |

Only the 1 nF and the 1.5 kΩ touch Nucleo ground.

---

## Expected voltages

Armed and healthy, pot at 2.0 V, nothing on the burden, VCC 5.1 V:

| Point | Reading |
|---|---|
| VCC rail | 5.1 V |
| NODE PK | < 50 mV |
| NODE SIG / LM393 pin 3 | < 50 mV |
| NODE THRESH / LM393 pin 2 | 2.0 V |
| LM393 pin 8 | 5.1 V |
| LM393 pin 4 | 0 V |
| LM393 pin 1 (OUT1) | **0.3 V** |
| 270 Ω, VCC end | 5.1 V |
| 270 Ω / LED anode | **3.6 V** |
| LED cathode / PC817 pin 1 | **1.5 V** |
| PC817 pin 2 | 0.3 V |
| PB12 | **≥ 2.6 V**, nominally 3.1 V |

Each step down the LED string is a component dropping its share: 1.5 V
across the resistor, 2.1 V across the red LED, 1.2 V across the opto LED.
**A missing drop means no current is flowing at that point** — walk the
chain until the voltage stops matching.

Current draw:

| State | Draw |
|---|---|
| LM393 not fitted | 0.27 mA |
| Armed, LED lit | ~7 mA |
| Tripped, LED dark | ~0.8 mA |

---

## Commissioning

**No HV and no work coil until the trip point check passes.**

**1. Before power.** Ground continuity: any two ground points under 1 Ω.
Short check: VCC rail to GND rail should settle at **18.5 kΩ** with the
LM393 out — that pot chain is the only DC path. Under a few hundred ohms
means a short; do not apply power.

**2. First power-up, current limited to ~50 mA.** Expect **5.1 V** on VCC
and **0.27 mA** draw with the LM393 out. Confirm the polarity sign: black
probe on GND, red on VCC, must read positive.

**3. Threshold range.** LM393 fitted, pins 5 and 6 to GND. Sweep the pot
and meter **pin 2**: expect **0.27 V to 3.1 V**. Two things matter — the
top must stay below 3.6 V, and 2.0 V must be reachable.

**4. Front end quiescent.** Nothing on the burden. NODE PK and NODE SIG
both **under 50 mV and staying there over a minute.** A slow climb means
the 10 kΩ or C_peak's ground return is missing. The LM393 must be fitted
for this test to be meaningful — its input bias current is the thing the
10 kΩ has to sink.

**5. Comparator.** Pot to 1.0 V at pin 2. Jumper **VCC to NODE PK** (not to
NODE SIG — that would exceed the common-mode ceiling):

| Condition | NODE SIG | OUT1 |
|---|---|---|
| No jumper | ~30 mV | **LOW** |
| Jumper on | **1.6 V** | **HIGH**, ~5 V |
| Jumper removed | ~30 mV | back **LOW** in ms |

NODE SIG at 1.6 V with NODE PK at 5.1 V also confirms the 0.3125
attenuation ratio, which the whole transfer table depends on.

**6. Hysteresis.** With the jumper held, sweep the pot through the trip
point both ways. The two flip points should differ by **~35 mV**, and the
trip should be the stickier direction.

**7. Output stage.** ARMED LED lit when healthy, dark on trip, draw
stepping ~7 mA → ~0.8 mA. OUT1's low state rises from 70 mV to ~0.3 V once
the LED string is connected — that is expected, it is now sinking 5.6 mA.

**8. PB12 levels.** Nucleo connected:

- Armed: **PB12 ≥ 2.6 V.** Below that, the PC817 is a low-CTR part — drop
  the 1.5 kΩ to 1 kΩ, or the 270 Ω to 220 Ω, and re-measure.
- Tripped: **PB12 under 0.3 V.**
- Pull the board's 5 V: **PB12 must go LOW, not high.** Confirm this
  explicitly; it is the fail-safe behaviour.

**9. Break path.** `pio run -e pin_check -t upload`, press `b`. Ground PB12
through 1 kΩ. BIF must latch and stay latched after the short is removed.
`c` clears it. Proves the whole BKIN path with the gates never enabled.

**10. Trip point, AD3 injection.** Drive the front-end input where the
220 Ω meets burden leg 1 with a 27 kHz sine, no CT. Pot at 2.0 V. Ramp the
amplitude and confirm the trip at **~7.2 V peak**, per the transfer table.
Check repeatability within a few hundred mV and clean re-arm.

**11. Real CT, no coil, no HV.** Per `OCP_BRINGUP.md` Stage 3 — drive the CT
primary with a controlled test current. Confirm trip and re-arm.

**12. Contactor noise.** Pot at 2.0 V, still no HV. Cycle the contactor
**twenty times.** The ARMED LED must stay lit and the Nucleo report no
fault for all twenty. If it trips: confirm the contactor coil has a flyback
diode (1N4007, cathode to +), shorten the decoupling cap leads, and keep
the CT/burden wiring away from the contactor coil wiring. If it still
trips, add a 4.7–10 Ω between the 5 V feed and the VCC rail — that plus the
100 µF forms a 1 ms low-pass. Mount it away from the electrolytic, and
re-run the Stage 2 divider sums since VCC will drop slightly.

**13. HV via the variac**, per `BOARD_V2_DIRECT.md`.

---

## Threshold setting

Start at **2.0 V measured at LM393 pin 2.** That trips at ~7.2 V burden
peak — about 1.5× the measured 150 V-in operating point.

To retune at a new power level, scope the burden peak during a good run:

```
  NODE THRESH = (V_burden_pk − 0.8) × 0.3125 × 1.5
```

The resistor bracket makes it impossible to exceed 3.1 V, which is the
point.

---

## Open item — spurious BKIN trip under PWM

**Status:** board built and verified through step 8. Threshold range, quiescent
front end, comparator polarity, trip, auto-rearm, LED string current
(6.3 mA), and PB12 levels all pass. Main firmware uploaded and running.

**Symptom:** with no HV **and the work coil disconnected from the burden**,
pressing start runs for a few seconds, then faults `FAULT_OCP_HW` —
`BIF` set, reported as `BKIN Break` on the TFT.

The CT carries zero primary current in this condition, so there is no
legitimate trip. This is noise reaching the break path.

Two details that look like evidence but aren't:

- **ARMED LED stays lit.** Expected, and it rules nothing out. A real trip
  darkens it for only ~1 ms (the 320 µs peak-detect decay plus re-arm),
  which is invisible. It was only visibly dark during step 5 because the
  test jumper was *held*.
- **"ADC 989" on the fault screen.** That field prints the PA1 reading
  unconditionally for either OCP fault type. Software OCP is disabled and
  989 is below the 3000 threshold regardless. PA1 reads as a floating pin.

### Plan, in order

**1. Scope first, change nothing.** Two channels on the AD3: **PB12** and
**LM393 pin 1 (OUT1)**, trigger on PB12 falling below 2 V.

| Result | Meaning | Fix path |
|---|---|---|
| Both move together | The board genuinely tripped — noise is in the front end or the threshold reference | Step 2 or 4 |
| PB12 dips, OUT1 doesn't | Glitch picked up on the PB12 wire itself | Step 3 |

**2. If the board is tripping: change the NODE THRESH 100 nF to 1 µF.**
The threshold is a divider off VCC, so its absolute value tracks rail
noise — and that rail also feeds the IRLB8721 level-shift pull-ups, which
switch at the PWM frequency. The pot's Thevenin impedance at a 2.0 V
setting is ~4.4 kΩ, so 1 µF gives τ = 4.4 ms, corner ~36 Hz. That kills
60 kHz rail noise on the reference far better than a supply filter would,
with nothing in the current path and no settling cost — the threshold is a
static setting.

**Do not re-fit the old 10 Ω supply resistor.** Its power rating was never
the issue (0.5 mW at 7 mA, so 1/4 W vs 1 W is irrelevant), and the reason
it overheated during the early build was never explained. Filtering the
sensitive node directly is the better fix. If a supply filter does turn out
to be needed, use a ferrite bead — no DC drop, nothing to dissipate.

**3. If the glitch is only on PB12:** increase the 1 nF at the pin. 10 nF
gives ~15 µs of filtering instead of ~1.5 µs. This costs real trip latency
against a 30–80 µs budget, so only do it if the scope justifies it.

**4. Check the CT/burden lead routing regardless.** Twist the pair, and
keep it away from the gate drive and level-shift wiring. Free, and it
addresses noise coupling into NODE SIG directly.

**5. Remove a variable: test in `manual_drive`.** With the coil
disconnected the 74HC14 has no input signal, so PA0 receives noise and
`PllControl` steers the PWM frequency around during the run — a moving
noise target. `manual_drive` never arms the PLL and holds the frequency
where you set it, while using the same pre-arm gate and BIF handling.

```
pio run -e manual_drive -t upload
```

Clean there but faulting in normal firmware implicates the PLL rather than
the OCP board.

**6. Then re-run step 12** (twenty contactor cycles) and continue to the
AD3 trip-point check and the real-CT test.
