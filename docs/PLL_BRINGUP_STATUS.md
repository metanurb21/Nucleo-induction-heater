# PLL Bring-up — Status and Next Steps

## Established

- **The power stage works.** Resonance at **57 kHz**, confirmed three
  independent ways: DC bus current peaks, the green LED across the work
  coil is brightest, and crucible contents heat.
- Burden output, measured: **1.24 V pk @ 30 V-in**, **5.12 V pk @ 150 V-in**
  (57 kHz, non-clipping).
- OCP tripping is disabled — `BKIN_ENABLED false` in config.h. The board
  stays wired and its ARMED LED still reports, but TIM1 never watches
  PB12. Protection posture is variac ramp + operator judgement + NTC
  over-temp, which is what every successful run on this rig has used.
- Display moved to **hardware SPI**. Software SPI was taking 548 ms per
  render, which held the main loop to ~1.8 Hz — `PLL_UPDATE_MS` is 5 ms,
  so the PLL could never have worked regardless of anything else.
- `PLL_CLOSED_LOOP false` — the PLL measures and reports only. The
  encoder retains frequency control.

## Why the PLL has never locked — two stacked causes

### 1. The feedback conditioner was built for a 150 V signal

A 74HC14 on a 5 V rail needs roughly **2.9 V** to trigger. Scaling the
measured burden output:

| Input | Burden peak | vs HC14 threshold |
|---|---|---|
| 30 V | 1.24 V | far below — no triggering |
| 80 V | ~2.7 V | marginal — erratic triggering |
| 150 V | 5.12 V | clean |

Observed at 80 V: phase scattered, and the per-edge spread `sp` flipping
between 0 (only one edge captured in the interval) and 100 (edges
everywhere). That is a signal too small to square reliably, not a
firmware fault.

**Fix: replace the 74HC14 with an LM393 comparator** (spares on hand),
inputs biased mid-rail, with hysteresis. That squares a few hundred
millivolts reliably and makes the feedback work at 30 V as well as 150 V,
which also means PLL development no longer requires running at high power.

### 2. Software phase measurement can't work at 57 kHz

Phase was being sampled by reading `TIM1->CNT` inside the TIM2 capture
interrupt. At 57 kHz the period is **17.5 µs**, while interrupt latency
through the Arduino `HardwareTimer` path is a few microseconds *and
varies*. That jitter is a large fraction of the period, so the reading
scatters even with a perfect input signal. The 57 kHz interrupt rate is
also a significant CPU load — `BOARD_V2_DIRECT.md` already recorded the
capture ISR starving the rest of the system when edges flooded in.

**Fix: measure phase in hardware.** Slave TIM2 to TIM1 so its counter
resets at the top of every PWM cycle. The capture register then *is* the
phase, in timer ticks, with zero software latency and no interrupt needed
at all.

```
TIM1->CR2:  MMS = 010      // update event drives TRGO
TIM2->SMCR: TS  = 000      // ITR0 = TIM1_TRGO  (RM0390 trigger table)
            SMS = 100      // reset mode: TRGO resets TIM2->CNT
TIM2 CH1:   input capture on PA0 (unchanged)

phase_ticks   = TIM2->CCR1
ticks_per_pwm = TIM2_CLOCK_HZ / f_pwm          (TIM2CLK = 90 MHz)
phase_pct     = 100 * phase_ticks / ticks_per_pwm
```

Read `CCR1` from the main loop at whatever rate suits — no ISR.

Two things to handle:

- **Phase wrap.** A crossing near the reset boundary reads near 0 or near
  full period. Unwrap relative to the target rather than treating it as a
  raw percentage.
- **Frequency measurement is lost.** With TIM2 reset every PWM period you
  can no longer measure the current's period from it. That's acceptable —
  phase is the error signal and frequency carried no information anyway
  (the tank current is forced at the drive frequency, so measuring it
  just returns what was commanded). Keep an edge counter if a sanity
  check on edge rate is still wanted.

## Order of work

1. **LM393 comparator stage** in place of the 74HC14. Bench-verify with
   the AD3 driving it — a few hundred mV in, clean square out.
2. **TIM2 hardware sync.** Verify at low voltage: `phase_pct` should be
   steady, and should move smoothly as the encoder sweeps frequency.
   Steadiness is the pass criterion, not any particular value.
3. **Characterise.** Sweep 53–61 kHz and record phase at each point. The
   value at 57 kHz becomes `PLL_PHASE_TARGET_PCT`. Which way phase moves
   as frequency rises gives the sign of `PLL_PHASE_GAIN_HZ`.
4. **Close the loop.** Set `PLL_CLOSED_LOOP true` with those two
   constants. Start with low gain.
5. OCP board noise investigation — see the open item in
   `OCP_BOARD_Final.md`. Independent of all the above.

Steps 1 and 2 are both testable with a signal generator and no HV.
