# Firmware Architecture & Bring-Up Plan

Modular firmware for the STM32 Nucleo F446RE induction heater PLL controller.

> **STATUS: Phases 1–2 verified on hardware.** Boots clean at 180MHz, TIM1
> complementary PWM + hardware dead-time confirmed on the AD3 and calibrated.
> TFT, encoder, NTC sensing, and the state machine all verified working.
> Remaining: PLL loop closure (Phase 3), gate drive (Phase 4), full power
> (Phase 5). Do NOT jump straight to driving IGBTs.

---

## Module Overview

| Module | Files | Responsibility |
|--------|-------|----------------|
| **config** | `config.h` | Single source of truth: pins, thresholds, tunables |
| **PwmDrive** | `PwmDrive.{h,cpp}` | TIM1 complementary PWM, hardware dead-time, BKIN break |
| **PllControl** | `PllControl.{h,cpp}` | TIM2 input capture, resonance tracking, detune |
| **Protection** | `Protection.{h,cpp}` | OCP + temperature fault detection |
| **Sensing** | `Sensing.{h,cpp}` | ADC acquisition, NTC conversion, EMA smoothing |
| **MainsControl** | `MainsControl.{h,cpp}` | Contactor relay + zero-cross switching |
| **Encoder** | `Encoder.{h,cpp}` | TIM3 quadrature + button |
| **Display** | `Display.{h,cpp}` | ST7735 TFT UI (idle / running / fault) |
| **StateManager** | `StateManager.{h,cpp}` | State machine + startup/shutdown sequencing |
| **SystemState** | `SystemState.h` | Shared state enum (avoids circular deps) |
| **main** | `main.cpp` | Thin orchestrator: init modules, loop → StateManager |

### Dependency direction

```
main
 └── StateManager
      ├── PwmDrive      (TIM1)
      ├── PllControl    (TIM2)   ──uses──> PwmDrive
      ├── Protection             ──uses──> Sensing, PwmDrive
      ├── Sensing       (ADC)
      ├── MainsControl           ──uses──> Sensing
      ├── Encoder       (TIM3)
      └── Display                ──reads──> all sensing/state modules
```

Modules never call StateManager (no upward calls). Display only reads.

---

## Timer / Peripheral Allocation

| Peripheral | Used by | Purpose |
|-----------|---------|---------|
| TIM1 | PwmDrive | Complementary PWM (PA8/PB13), dead-time, BKIN (PB12) |
| TIM2 | PllControl | Input capture (PA0) for frequency measurement |
| TIM3 | Encoder | Quadrature decode (PB4/PB5) |
| ADC1 | Sensing | OCP (PA1), Vbus (PA4), NTC (PC0), AC sense (PC1) |
| SPI (soft) | Display | ST7735 TFT — PA5 SCK, PA7 MOSI, PB6 CS, PC7 DC, PA9 RST, **PB2 BLK** |
| GPIO | MainsControl | Contactor (PB14) |
| GPIO | StateManager | Status LED (PB0), Fault LED (PB1) |
| USART2 | Serial | ST-Link VCP console (PA2/PA3) |

---

## Safety Model (layered)

1. **TIM1 BKIN** — hardware break. OCP comparator drives PB12; a fault
   forces PWM outputs off in ~6ns with zero software involvement.
   `AOE=0` so it latches until firmware re-arms (`clearBreak` + re-enable).
2. **Software OCP** — `Protection::checkFast()` polls raw ADC every 1ms,
   shuts down if over threshold.
3. **Over-temperature** — NTC on IGBT heatsink, 500ms poll, 80°C shutdown.
4. **Mains-loss** — detected during run, drops contactor.
5. **Boot-safe** — contactor OFF and PWM disabled before anything else in
   `setup()`.

**Shutdown order (fastest first):** disable PWM → drop contactor → fault state.
**Startup order (guarded):** temp OK → mains present → energize contactor →
charge bus → enable PWM → begin PLL tracking.

---

## Bring-Up Phases

Work through these IN ORDER. Do not connect the power stage until Phase 4.

### Phase 1 — Board alive ✅ (done)
Blink + serial + button. Verified toolchain, upload, serial.
Reference: `docs/phase1_blink_reference.cpp`.

### Phase 2 — PWM generation ✅ (done, AD3-verified)
Use the dedicated test env: `pio run -e pwm_test -t upload`. It bypasses the
state machine and drives PWM immediately, with live serial commands
(`f<hz>`, `d<ns>`, `e`, `x`, `?`) for tuning while scoping.

Results on the AD3 (probing PA8 / PB13):
- ✅ Complementary square waves confirmed
- ✅ **Clock calibrated:** actual TIM1 clock measured at **184 MHz** (not the
  assumed 180). `TIM1_CLOCK_HZ` updated. Post-calibration: commanded 80 kHz
  reads 80.023 kHz (0.03% err), 50 kHz reads 49.947 kHz (0.11% err).
- ✅ **Hardware dead-time confirmed and adjustable** (300 ns cmd → ~278 ns
  pre-calibration, tracked linearly; 500 ns → ~470 ns). Accurate post-cal.
- ✅ Clean 3.3V logic edges (~53 ns rise/fall, low overshoot)

### Phase 2b — UI + sensing ✅ (done)
- ✅ TFT renders splash / idle / running / fault (note: **BLK backlight pin
  PB2 must be wired** or the screen appears dead)
- ✅ Encoder button: start / stop / reset-fault all working
- ✅ Encoder rotation adjusts frequency live
- ✅ NTC reads accurate room temp, responds to finger warmth / breath
- ✅ State machine transitions verified end-to-end

### Phase 3 — Feedback + UI (NO power stage)
- Feed a signal generator (AD3) square wave into the FREQ_FB path (PA0,
  through the 74HC14/isolator or directly for bench test).
- Confirm `PllControl::getMeasuredFreqHz()` reads the injected frequency.
- Confirm the PLL steers `PwmDrive` frequency toward the measured value.
- Verify encoder adjusts values, button starts/stops, TFT shows all screens.
- Test BKIN: pull PB12 low, confirm PWM outputs die and state goes fault.

### Phase 4 — Gate drive (isolated, low voltage)
- Connect Si8621 → IXDN604 → GDT, but power the H-bridge from a LOW voltage
  bench supply (e.g., 12–24V through the variac at minimum).
- Probe the GDT secondaries / IGBT gates on the AD3.
- Tune dead-time to the edge while watching for shoot-through current.
- Verify the CT → 74HC14 → PA0 feedback closes the PLL loop on the real tank.

### Phase 5 — Full power (variac, gradual)
- Bring up on the variac slowly (as always).
- Tune detune offset for maximum coupling ("power mode").
- Verify OCP trips at a safe threshold before anything melts that shouldn't.
- Log frequency/temp/current over a melt run for analysis.

---

## Resolved during bring-up

- ✅ **TIM2 IRQ collision** — the Arduino STM32 core already defines
  `TIM2_IRQHandler`, which caused a multiple-definition link error against our
  hand-rolled ISR. Fixed by using the core's `HardwareTimer` input-capture
  callback API in `PllControl` instead of a raw ISR.
- ✅ **TIM1 register setup + AF numbers validated** on hardware (PWM and
  dead-time both confirmed on the AD3). TIM3 encoder mode also working.
- ✅ **Clock constant calibrated** — `TIM1_CLOCK_HZ` corrected 180 → 184 MHz
  from scope measurements.

## Known TODOs / Iteration Notes

- **Encoder mode cycling** — currently `s_encMode` is fixed to FREQUENCY.
  Add long-press or a second button to cycle FREQ/DETUNE/DEADTIME/OCP.
- **Bus voltage scaling** — `Sensing::readBusRaw()` returns raw ADC; add the
  divider math once the resistor values are finalized.
- **CT scaling to real amps** — convert OCP ADC counts to actual current for
  display (needs CT ratio + burden calibration).
- **PLL loop tuning** — `PLL_MAX_STEP_HZ` slew limit and update rate may need
  tuning for stable lock without hunting. Consider a proper PI loop on the
  phase error rather than pure frequency matching.
- **Startup sweep** — the ESP32 PLL version swept the VCO up through
  resonance to acquire lock. The digital version may need an equivalent
  frequency sweep on start before handing over to tracking, if the tank
  won't self-start at `PWM_FREQ_START_HZ`.
- **Watchdog** — enable the STM32 IWDG so a firmware hang forces a reset
  (outputs go safe). Not yet wired up.
- **Mains-present robustness** — `Sensing::mainsPresent()` currently returns
  true if ANY sample exceeds threshold, so a *floating* ADC pin can falsely
  read "MAINS: OK". Better: require the signal to actually vary (a rectified
  sine wiggles; a floating/stuck pin doesn't) so the safe default is "no mains".

---

## Build & Upload

Two environments:

```
# Normal firmware (state machine, boots safe to IDLE)
pio run -e nucleo_f446re -t upload

# PWM bench test mode (drives PWM immediately, serial f/d/e/x/? commands)
pio run -e pwm_test -t upload

pio device monitor      # serial console @ 115200
```

`upload_protocol = mbed` copies `firmware.bin` to `/Volumes/NOD_F446RE`.
Note the normal firmware does NOT drive PWM until started; `pwm_test` does NOT
init the TFT. Flash whichever matches what you're testing.

The "Disk not ejected properly" macOS warning on upload is normal for the
mbed/mass-storage flashing method — the board resets after receiving firmware.
