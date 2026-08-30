# Inter-Board JST Connector Mapping

Two identical 8-pin JST connectors carry signals between the **Nucleo perf
(top)** and the **driver/IXDN perf (bottom)**. Split by domain: fast/critical
signals on Connector 1, slow/analog signals on Connector 2.

**Wire color = pin number** (read left-to-right at a glance, Black first):

| Pin | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|-----|---|---|---|---|---|---|---|---|
| Color | Black | Red | White | Yellow | Orange | Green | Blue | Purple |

See `NUCLEO_PINOUT.md` for full pin functions and morpho locations.

---

## Connector 1 — Fast / Critical Signals

| Pin | Color  | Signal   | Nucleo Pin | Function          | Morpho   | Direction      |
|-----|--------|----------|------------|-------------------|----------|----------------|
| 1   | Black  | PWM_A    | PA8        | TIM1_CH1          | CN10-23  | Nucleo → lower |
| 2   | Red    | GND      | GND        | Ground (to star)  | CN7-20   | —              |
| 3   | White  | PWM_B    | PB13       | TIM1_CH1N         | CN10-30  | Nucleo → lower |
| 4   | Yellow | GND      | GND        | Ground (to star)  | CN10-9   | —              |
| 5   | Orange | BKIN     | PB12       | TIM1_BKIN         | CN10-16  | lower → Nucleo |
| 6   | Green  | FREQ_FB  | PA0        | TIM2_CH1 (capture)| CN7-28   | lower → Nucleo |
| 7   | Blue   | 3V3      | 3V3        | Si8621 logic-side ref (optional) | CN7-16 | Nucleo → lower |
| 8   | Purple | GND      | GND        | Ground (to star)  | CN10-20  | —              |

## Connector 2 — Slow / Analog Signals

| Pin | Color  | Signal    | Nucleo Pin | Function      | Morpho   | Direction      |
|-----|--------|-----------|------------|---------------|----------|----------------|
| 1   | Black  | ADC_OCP   | PA1        | ADC1_IN1      | CN7-30   | lower → Nucleo |
| 2   | Red    | ADC_NTC   | PC0        | ADC1_IN10     | CN7-38   | lower → Nucleo |
| 3   | White  | ADC_AC    | PC1        | ADC1_IN11     | CN7-36   | lower → Nucleo |
| 4   | Yellow | ADC_VBUS  | PA4        | ADC1_IN4      | CN7-32   | lower → Nucleo |
| 5   | Orange | CONTACTOR | PB14       | GPIO output   | CN10-28  | Nucleo → lower |
| 6   | Green  | GND       | GND        | Analog ground | CN7-20   | —              |
| 7   | Blue   | 5V        | —          | (optional feed up) | —   | lower → Nucleo (opt) |
| 8   | Purple | spare     | —          | —             | —        | —              |

---

## Notes

- **Single star ground:** All GND pins (Conn 1: Red/2, Yellow/4, Purple/8;
  Conn 2: Green/6) tie to the one star-ground point on the lower board along
  with the 5V/12V/15V returns. The Nucleo shares this ground. Simple and clean.
- **Grounds by connector:** Conn 1 carries three grounds flanking the fast PWM
  signals for tight return paths. Conn 2 has one ground for the analog returns.
- **3.3V (Conn 1, Blue/pin 7):** Nucleo's 3.3V — optional reference down to the
  isolator logic side. (Si8621 can run both sides from 5V; wire 3.3V only if you
  want the logic side at 3.3V to match Nucleo I/O levels exactly.)
- **5V (Conn 2, Blue/pin 7):** spare / unused.
- **Fail-safe:** BKIN is active-LOW with a pull-up on the Nucleo. If a JST is
  unplugged, no PWM reaches the gate drivers anyway (10kΩ pulldowns on the
  IXDN604 inputs hold the gates OFF), so a disconnected board = gates off.
- **Assembly:** JST housings hot-glued to the perf after final seating to resist
  contactor vibration while staying removable with heat.

## Noise Mitigation (single-ground strategy)

Since logic and power share one ground, noise control comes from:
- **Single star ground** — all returns meet at ONE point, no daisy-chaining.
  Prevents ground loops and shared-path voltage drops. (Planned.)
- **Twisted signal leads** — each JST signal twisted with a ground return to
  minimize loop area / pickup. (Done.)
- **Decoupling caps** — 100nF at every IC power pin + bulk caps (in schematic).
- **Physical separation** — fast PWM/gate lines kept away from analog sense
  lines (Conn 1 vs Conn 2 split already does this).
- If MCU glitching appears under load during testing, revisit (ferrite beads on
  the Nucleo supply + sense lines, or true isolation). Don't pre-solve unseen
  noise — the AD3 can pinpoint the actual source if it happens.
