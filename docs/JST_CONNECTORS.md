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
| 2   | Red    | LOGIC GND| GND        | Logic ground (isolator Vdd1 return) | CN7-20 | — |
| 3   | White  | PWM_B    | PB13       | TIM1_CH1N         | CN10-30  | Nucleo → lower |
| 4   | Yellow | LOGIC GND| GND        | Logic ground      | CN10-9   | —              |
| 5   | Orange | BKIN     | PB12       | TIM1_BKIN         | CN10-16  | lower → Nucleo |
| 6   | Green  | FREQ_FB  | PA0        | TIM2_CH1 (capture)| CN7-28   | lower → Nucleo |
| 7   | Blue   | 3V3      | 3V3        | Si8621 Vdd1 (logic side) | CN7-16 | Nucleo → lower |
| 8   | Purple | LOGIC GND| GND        | Logic ground      | CN10-20  | —              |

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

- **TRUE ISOLATION:** The Nucleo is powered INDEPENDENTLY (USB or its own
  isolated 12V brick), NOT from the lower board. So Connector 1 carries NO power
  up to the Nucleo — it only carries the Nucleo's LOGIC 3.3V + logic ground DOWN
  to the isolator logic side, plus the isolated signals.
- **Logic ground crosses down (Conn 1: Red/2, Yellow/4, Purple/8):** three logic
  grounds carry the Nucleo's ground reference down to the Si8621 Vdd1/GND1 side.
  This logic ground connects ONLY to Si8621 pin 4 (GND1) on the lower board — it
  must NEVER touch the lower board's POWER ground. That separation is the whole
  point of the isolators.
- **3.3V (Conn 1, Blue/pin 7):** Nucleo's regulator output, powers Si8621 Vdd1
  (logic side) only. Referenced to the logic grounds above.
- **Conn 2 grounds:** Green (pin 6) is a logic ground for the analog/ADC returns
  going back to the Nucleo. The sense signals (OCP/NTC/AC/VBUS) reference this.
- **5V (Conn 2, Blue/pin 7):** spare / unused (Nucleo no longer needs 5V up).
- **Fail-safe:** BKIN is active-LOW with a pull-up on the Nucleo. If a JST is
  unplugged, no PWM reaches the gate drivers anyway (10kΩ pulldowns on the
  IXDN604 inputs hold the gates OFF), so a disconnected board = gates off.
- **Assembly:** JST housings hot-glued to the perf after final seating to resist
  contactor vibration while staying removable with heat.

> ⚠️ ADC ground caution: the analog sense signals (Conn 2) originate on the
> POWER side but must be read against the Nucleo's LOGIC ground. If you did NOT
> isolate the ADC path, the sense grounds would bridge the two domains and
> defeat isolation. Options: (a) isolated ADC front-end, or (b) accept that the
> sense lines form a deliberate single-point ground link. Decide this before
> full-power runs — flagged for Phase 4/5. For low-voltage bring-up it's fine.
