# Inter-Board JST Connector Mapping

Two 8-pin JST connectors carry signals between the **Nucleo perf (top)** and
the **driver/IXDN perf (bottom)**. Split by domain: fast/critical signals on
Connector 1, slow/analog signals on Connector 2. Grounds placed adjacent to
fast signals for clean return paths.

See `NUCLEO_PINOUT.md` for full pin functions and morpho locations.

---

## Connector 1 — Fast / Critical Signals

| JST Pin | Signal   | Nucleo Pin | Function        | Morpho     | Direction        |
|---------|----------|------------|-----------------|------------|------------------|
| 1       | PWM_A    | PA8        | TIM1_CH1        | CN10-23    | Nucleo → lower   |
| 2       | GND      | GND        | Logic ground    | CN10-9/20  | —                |
| 3       | PWM_B    | PB13       | TIM1_CH1N       | CN10-30    | Nucleo → lower   |
| 4       | GND      | GND        | Logic ground    | CN7-8/20   | —                |
| 5       | BKIN     | PB12       | TIM1_BKIN       | CN10-16    | lower → Nucleo   |
| 6       | FREQ_FB  | PA0        | TIM2_CH1 (capture)| CN7-28   | lower → Nucleo   |
| 7       | 3V3      | 3V3        | Si8621 Vdd1 (logic side) | CN7-16 | Nucleo → lower |
| 8       | spare    | —          | —               | —          | —                |

## Connector 2 — Slow / Analog Signals

| JST Pin | Signal    | Nucleo Pin | Function       | Morpho   | Direction        |
|---------|-----------|------------|----------------|----------|------------------|
| 1       | ADC_OCP   | PA1        | ADC1_IN1       | CN7-30   | lower → Nucleo   |
| 2       | ADC_NTC   | PC0        | ADC1_IN10      | CN7-38   | lower → Nucleo   |
| 3       | ADC_AC    | PC1        | ADC1_IN11      | CN7-36   | lower → Nucleo   |
| 4       | ADC_VBUS  | PA4        | ADC1_IN4       | CN7-32   | lower → Nucleo   |
| 5       | CONTACTOR | PB14       | GPIO output    | CN10-28  | Nucleo → lower   |
| 6       | GND       | GND        | Analog ground  | CN7-20   | —                |
| 7       | 5V        | —          | (from lower board step-down; optional feed up) | — | lower → Nucleo (opt) |
| 8       | spare     | —          | —              | —        | —                |

---

## Notes

- **Grounds:** Connector 1 carries two grounds flanking the fast PWM signals to
  keep switching-edge return currents tight. Connector 2 has one analog ground.
- **3.3V** originates from the Nucleo's onboard regulator and travels DOWN to
  power the Si8621 logic side (Vdd1) only. The lower board's 5V rail powers
  everything else (Si8621 Vdd2, 74HC14, relay coil).
- **5V on Connector 2 pin 7** is optional — only wire it if the Nucleo perf
  needs 5V for something. Normally the Nucleo runs from 12V on VIN, so this may
  be left as a second spare.
- **Fail-safe:** BKIN is active-LOW with a pull-up on the Nucleo. If a JST is
  unplugged, no PWM reaches the gate drivers anyway (10kΩ pulldowns on the
  IXDN604 inputs hold the gates OFF), so a disconnected board = gates off.
- **Assembly:** JST housings hot-glued to the perf after final seating to resist
  contactor vibration while staying removable with heat.
