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
| 2   | Red    | 12V      | VIN        | Power up to Nucleo VIN | CN7-24 | lower → Nucleo |
| 3   | White  | PWM_B    | PB13       | TIM1_CH1N         | CN10-30  | Nucleo → lower |
| 4   | Yellow | GND      | GND        | Logic ground      | CN7-20   | —              |
| 5   | Orange | BKIN     | PB12       | TIM1_BKIN         | CN10-16  | lower → Nucleo |
| 6   | Green  | FREQ_FB  | PA0        | TIM2_CH1 (capture)| CN7-28   | lower → Nucleo |
| 7   | Blue   | 3V3      | 3V3        | Si8621 Vdd1 (logic side) | CN7-16 | Nucleo → lower |
| 8   | Purple | GND      | GND        | Logic ground      | CN10-9   | —              |

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

- **12V (Conn 1, Red/pin 2):** feeds the Nucleo VIN from the lower board's
  12V wallwart rail. It's DC power, not a switching signal — no noise concern
  sharing the connector with the PWM lines.
- **Grounds:** Conn 1 keeps a logic ground on Yellow (pin 4, next to PWM_B) and
  Purple (pin 8). Conn 2 has one analog ground on Green (pin 6).
- **3.3V (Conn 1, Blue/pin 7):** from the Nucleo's onboard regulator, travels
  DOWN to power the Si8621 logic side (Vdd1) only. The lower board's 5V rail
  powers everything else (Si8621 Vdd2, 74HC14, relay coil).
- **5V (Conn 2, Blue/pin 7):** optional — only wire if the Nucleo perf needs
  5V. Normally the Nucleo runs from the 12V on VIN, so this may stay unused.
- **Fail-safe:** BKIN is active-LOW with a pull-up on the Nucleo. If a JST is
  unplugged, no PWM reaches the gate drivers anyway (10kΩ pulldowns on the
  IXDN604 inputs hold the gates OFF), so a disconnected board = gates off.
- **Assembly:** JST housings hot-glued to the perf after final seating to resist
  contactor vibration while staying removable with heat.
