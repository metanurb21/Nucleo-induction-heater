# OCP Board — Build Guide (v2 — SUPERSEDED, DO NOT BUILD)

> **⚠️ SUPERSEDED BY `OCP_BOARD_Final.md`. Do not build from this document.**
>
> This design has confirmed faults that cause a permanent false trip with
> no HV present:
> 1. `C_peak` has no ground return and no bleed resistor — the peak
>    detector is a one-way ratchet that LM393 input bias current drives to
>    the rail in milliseconds.
> 2. The threshold is specified at 4.5–5 V, outside the LM393's
>    common-mode range (Vcc − 1.5 V = 3.5 V max).
> 3. The burden signal is fed in unattenuated, also outside that range.
> 4. No power-on reset on the latch — it boots in a random state.
> 5. No decoupling, no clock filtering, shared contactor coil rail.
> 6. Fail-permissive output: an unpowered board reads as "no fault".
> 7. The PC2 tank divider corrupts adjacent ADC channels.
>
> See `OCP_BOARD_Final.md` for the current design.

Final design. Build exactly as specified below. Supply: 5V, from the
existing dedicated 5V/5A rail on the main driver board.

---

## Bill of Materials

| Qty | Part | Value |
|---|---|---|
| 1 | Diode | UF4007 |
| 1 | Capacitor | 2.2nF MKT film, 63V |
| 1 | IC | LM393 (dual comparator) |
| 1 | IC | 74HCT74 (dual D flip-flop) |
| 1 | Potentiometer | 10kΩ, 1-turn |
| 1 | Resistor | 10kΩ |
| 1 | Resistor | 10kΩ |
| 1 | Resistor | 680Ω |
| 1 | Optocoupler | PC817 |
| 1 | Resistor | 3kΩ |
| 1 | Momentary switch module | Digital push-button, 3-pin (VCC/GND/OUT), active-LOW |
| 1 | Resistor | 10MΩ |
| 1 | Resistor | 2MΩ |
| 1 | Capacitor | 100nF MKT film, 63V |
| — | Perfboard, wire, standoffs | as needed |

---

## Section 1 — Current-Sense Overcurrent Trip

```
CT/Burden board (existing) — 2 output legs
   leg 1 │                    leg 2 │
         ▼                          ▼
      UF4007 ──► 2.2nF cap ──► NODE A     GND (this board)
                             │
                             ▼
                     ┌───────────────┐
     NODE B ────────►│ IN1(-)   OUT1 │──┬──► NODE C
   (pot wiper)        │ IN1(+)  LM393 │  │
     NODE A ────────► │  #1           │  R = 10kΩ to 5V
                       └───────────────┘  │
                                          5V
```

**Pot (sets threshold, feeds NODE B):**
```
5V ──[10kΩ pot]── GND        wiper → NODE B
```

**Latch (74HCT74, flip-flop #1):**
```
NODE C ──────────────────────► pin 3 (1CLK)
5V ───────────────────────────► pin 2 (1D)
5V ───────────────────────────► pin 4 (1PR)
5V ──[10kΩ]──┬─────────────────► pin 1 (1CLR)
             │
        Reset button (OUT pin)
             │
            GND
pin 7 → GND
pin 14 → 5V
pin 5 (1Q) ──────────────────► NODE D (latch output)
pin 6 (1Q̄) — unconnected
```

**Reset button module:**
```
Button VCC → 5V
Button GND → GND
Button OUT → pin 1 (1CLR), in parallel with the 10kΩ pull-up above
```

**Isolation + output to Nucleo:**
```
NODE D ──[680Ω]──► PC817 pin 1 (Anode) ... PC817 pin 2 (Cathode) ──► GND
PC817 pin 3 (Emitter) ── GND
PC817 pin 4 (Collector) ──[3kΩ]── 3.3V
PC817 pin 4 (Collector) ─────────► Nucleo PB12
```

**LM393 unused comparator (#2):** tie IN2(+) and IN2(-) to GND.
**74HCT74 unused flip-flop (#2):** tie 2CLR (pin 13) and 2PR (pin 10) to
VCC, 2D (pin 8) and 2CLK (pin 9) to GND. Outputs 2Q (pin 11) and 2Q̄
(pin 12), plus 1Q̄ (pin 6), are outputs — safe to leave unconnected.

### Section 1 — Connection List

| From | To |
|---|---|
| CT/Burden board output, leg 1 | UF4007 anode |
| CT/Burden board output, leg 2 | GND (this board's ground — completes the burden's return path) |
| UF4007 cathode | 2.2nF cap (+) |
| 2.2nF cap (+) | NODE A |
| NODE A | LM393 pin 3 (IN1+) |
| Pot pin 1 | 5V |
| Pot pin 3 | GND |
| Pot pin 2 (wiper, marked on part) | NODE B → LM393 pin 2 (IN1-) |
| LM393 pin 1 (OUT1) | NODE C |
| NODE C | 10kΩ resistor |
| 10kΩ resistor (other end) | 5V |
| NODE C | 74HCT74 pin 3 (1CLK) |
| 74HCT74 pin 2 (1D) | 5V |
| 74HCT74 pin 4 (1PR) | 5V |
| 74HCT74 pin 1 (1CLR) | 10kΩ resistor |
| 10kΩ resistor (other end) | 5V |
| 74HCT74 pin 1 (1CLR) | Reset button OUT pin |
| Reset button VCC | 5V |
| Reset button GND | GND |
| 74HCT74 pin 7 | GND |
| 74HCT74 pin 14 | 5V |
| 74HCT74 pin 5 (1Q) | NODE D |
| 74HCT74 pin 6 (1Q̄) | (unconnected) |
| NODE D | 680Ω resistor |
| 680Ω resistor (other end) | PC817 pin 1 (Anode) |
| PC817 pin 2 (Cathode) | GND |
| PC817 pin 3 (Emitter) | GND |
| PC817 pin 4 (Collector) | 3kΩ resistor |
| 3kΩ resistor (other end) | Nucleo 3.3V |
| PC817 pin 4 (Collector) | Nucleo PB12 |
| LM393 pin 4 (GND) | GND |
| LM393 pin 8 (VCC) | 5V |
| LM393 pin 5 (IN2+) | GND |
| LM393 pin 6 (IN2-) | GND |
| LM393 pin 7 (OUT2) | (unconnected — output pin, safe to float) |
| 74HCT74 pin 8 (2D) | GND |
| 74HCT74 pin 9 (2CLK) | GND |
| 74HCT74 pin 10 (2PR) | 5V |
| 74HCT74 pin 13 (2CLR) | 5V |
| 74HCT74 pin 6 (1Q̄) | (unconnected — output pin, safe to float) |
| 74HCT74 pin 11 (2Q) | (unconnected — output pin, safe to float) |
| 74HCT74 pin 12 (2Q̄) | (unconnected — output pin, safe to float) |

---

## Section 2 — Tank Voltage Sense (display only)

```
Tank Leg A ──[10MΩ]──┬──[2MΩ]── Tank Leg B
                      │
                  NODE E (midpoint)
                      │
                  [100nF cap] ── GND
                      │
                      ▼
                Nucleo PC2
```

---

### Section 2 — Connection List

| From | To |
|---|---|
| Tank Leg A | 10MΩ resistor |
| 10MΩ resistor (other end) | NODE E (midpoint) |
| NODE E | 2MΩ resistor |
| 2MΩ resistor (other end) | Tank Leg B |
| NODE E | 100nF cap (+) |
| 100nF cap (-) | GND |
| NODE E | Nucleo PC2 |

---

## Nucleo Connections

| Nucleo Pin | Connects to |
|---|---|
| PB12 | PC817 phototransistor collector (via 3kΩ pull-up to 3.3V) |
| PC2 | Tank voltage divider midpoint (NODE E) |

No other Nucleo pins are used by this board. The reset button wires
directly into the OCP board (74HCT74 pin 1) — it does not connect to
the Nucleo.

---

## Threshold Setting

Set the pot so NODE B reads approximately **4.5-5V**. Fine-tune on the
bench once running, using real load conditions.
