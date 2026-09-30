// ============================================================
//  Induction Heater PLL Controller — Configuration
//  STM32 Nucleo F446RE
//
//  Single source of truth for pin assignments, thresholds,
//  and tunable constants. See docs/NUCLEO_PINOUT.md for the
//  full board pin map and docs/SHIELD_LAYOUT.md for wiring.
// ============================================================
#pragma once

#include <Arduino.h>

// ============================================================
//  PIN ASSIGNMENTS  (see docs/NUCLEO_PINOUT.md)
// ============================================================

// ---- PWM Outputs (TIM1 advanced timer) --------------------
//   PA8  = TIM1_CH1  (PWM_A)  -> Si8621 #1 A1 -> IXDN604 #1
//   PB13 = TIM1_CH1N (PWM_B)  -> Si8621 #1 A2 -> IXDN604 #2
//   Complementary pair with hardware dead-time.
#define PIN_PWM_A       PA8   // TIM1_CH1
#define PIN_PWM_B       PB13  // TIM1_CH1N

// ---- Hardware Break Input (TIM1 BKIN) ---------------------
//   Active-LOW fault kills PWM in hardware (~6ns).
//
//   Source: OCP board v4, PC817 phototransistor in EMITTER-FOLLOWER
//   (collector -> 3.3V, emitter -> PB12 + 1.5k to Nucleo GND).
//   The opto LED is driven straight off the LM393's open-collector
//   output — no external latch, no reset button on the board.
//     opto LED lit   = armed, under threshold = PB12 ~3.1V = healthy
//     opto LED dark  = over threshold, board unpowered, or wire pulled
//                    = PB12 pulled low by the 1.5k = gates off
//   This is genuinely fail-safe (v2 was fail-permissive: an
//   unpowered board read as "no fault"). See docs/OCP_BOARD_Final.md.
//
//   TIM1 IS THE LATCH. v4 has no external flip-flop: a BKIN event
//   clears MOE in hardware (~6ns) and sets BIF, which stays set
//   until clearBreak(), and MOE can only be re-set by firmware.
//   So the trip latches and the gates stay off with no external
//   latch involved. Reset is the encoder button, via StateManager.
//
//   Consequence: PB12 may be LOW briefly at power-up while the OCP
//   board's rail comes up, so TIM1 can latch BIF at boot. That is
//   expected. StateManager clears BIF immediately before enabling
//   outputs and pre-checks the line as a GPIO — see startup().
//
//   NOTE: F446 TIM1 has no BKF break filter (RM0390 BDTR has no
//   filter field, unlike F3/F7/G4), so all break-input glitch
//   filtering is external. In v4 that is the 1nF at the pin plus
//   the 320us peak-detect hold in the OCP front end, which is what
//   makes a single unfiltered comparator output acceptable here.
#define PIN_FAULT_BKIN  PB12  // TIM1_BKIN

// ---- PLL Frequency Feedback (TIM2 input capture) ----------
//   Tank CT -> burden -> 74HC14 -> Si8621 #2 -> here.
//   Square wave at tank resonant frequency.
#define PIN_FREQ_FB     PA0   // TIM2_CH1 (input capture)

// ---- Analog Sensing (ADC1) --------------------------------
#define PIN_ADC_OCP     PA1   // ADC1_IN1  — tank current/voltage (fast OCP)
#define PIN_ADC_VBUS    PA4   // ADC1_IN4  — DC bus voltage monitor
#define PIN_ADC_NTC     PC0   // ADC1_IN10 — IGBT heatsink thermistor
#define PIN_ADC_AC      PC1   // ADC1_IN11 — mains AC sense (zero-cross)
//  PC2 (ADC1_IN12) was the tank voltage sense. REMOVED in OCP board v3
//  and the pin is now free. The 10M/2M divider was a 1.67MΩ source into
//  an unbuffered ADC input: the F446 sample-and-hold can't settle on it,
//  and residual charge on the sample cap corrupted the next channel
//  sampled — including the PA1 OCP read. (It also caused the measured
//  [loop] STALL that led to it being commented out in Display.cpp.)
//  Beyond that it was non-isolated tank-to-MCU copper referenced to Tank
//  Leg B rather than ground. A correct version needs a linear isolation
//  amplifier (HCNR201 / ISO224 class). Deferred. See docs/OCP_BOARD_Final.md.

// ---- Rotary Encoder (TIM3 encoder mode) -------------------
#define PIN_ENC_A       PB4   // TIM3_CH1
#define PIN_ENC_B       PB5   // TIM3_CH2
#define PIN_ENC_BTN     PC13  // Encoder push button (active LOW)

// ---- TFT Display (SPI1, ST7735 128x160) -------------------
#define PIN_TFT_SCK     PA5   // SPI1_SCK
#define PIN_TFT_MOSI    PA7   // SPI1_MOSI
#define PIN_TFT_CS      PB6
#define PIN_TFT_DC      PC7
#define PIN_TFT_RST     PA9
#define PIN_TFT_BLK     PB2   // Backlight enable (HIGH = on)

// ---- Contactor / Mains Control ----------------------------
//   GPIO -> optocoupler -> 5V relay -> 120V AC -> contactor coil.
#define PIN_CONTACTOR   PB14  // HIGH = energize contactor

// ---- Status LEDs ------------------------------------------
#define PIN_LED_STATUS  PB0   // Green — run/idle heartbeat
#define PIN_LED_FAULT   PB1   // Red — fault indicator

// ============================================================
//  PWM / TIMER CONFIG
// ============================================================
//  TIM1 clock on F446RE APB2 timer domain = 180 MHz.
//  PWM frequency range for induction heater: 50-100 kHz typical.
//  ARR (auto-reload) sets frequency: f_pwm = TIM1CLK / (PSC+1) / (ARR+1)
//  With PSC=0 and TIM1CLK=180MHz:
//    50 kHz  -> ARR = 3599
//    100 kHz -> ARR = 1799
// TIM1 timer clock — CALIBRATED against measured PWM on the AD3.
// Assumed 180 MHz gave 81.8kHz@80k cmd and 51.1kHz@50k cmd; both back-
// calculate to ~184.0 MHz actual (the Arduino STM32 core's SYSCLK config).
// Using the measured value makes commanded freq/dead-time match reality.
#define TIM1_CLOCK_HZ       184000000UL
#define PWM_FREQ_MIN_HZ     20000     // Lower bound of operating range (lowered
                                       // from 50000 for MANUAL_DRIVE_MODE bring-up
                                       // — target resonance ~27kHz on this coil)
#define PWM_FREQ_MAX_HZ     100000    // Upper bound of operating range
#define PWM_FREQ_START_HZ   60000     // Default startup frequency (coil dependent)
#define PWM_DUTY_PCT        50        // Fixed 50% for full-bridge square drive

// ---- Dead-time --------------------------------------------
//  Hardware dead-time between complementary outputs (nanoseconds).
//  Tune with Analog Discovery on the actual gate signals.
//  Start conservative (safe from shoot-through), reduce for power.
//  DTG register resolution ~5.6ns/step at 180MHz.
#define DEADTIME_NS_DEFAULT 300       // Starting dead-time (ns)
#define DEADTIME_NS_MIN     100       // Minimum allowed (safety floor)
#define DEADTIME_NS_MAX     1000      // Maximum allowed

// ============================================================
//  PLL / FREQUENCY TRACKING
// ============================================================
//  The PLL measures actual tank resonant frequency via input
//  capture, then steers PWM frequency to track it.
//  ---- PLL loop closure ------------------------------------
//  false = OPEN LOOP. The PLL measures and reports phase and
//  frequency but never steers PwmDrive — the encoder stays in
//  charge. This is the characterisation mode: sweep manually,
//  record phase against frequency, and find the phase value that
//  corresponds to your known resonance. That measured value
//  becomes PLL_PHASE_TARGET_PCT below.
//
//  Why phase and not frequency: in a forced oscillation the tank
//  current is at the DRIVE frequency, so measuring current
//  frequency just returns what you already commanded — it carries
//  no information about where resonance is. Phase does: below
//  resonance the tank is capacitive and current leads, above it is
//  inductive and lags, and at resonance the current zero-crossing
//  sits at a fixed point in the drive cycle.
//
//  Set true only after PLL_PHASE_TARGET_PCT is measured.
#define PLL_CLOSED_LOOP     false

//  Phase (as % of the PWM period) at which the current zero-cross
//  occurs when the tank is at resonance. MEASURE THIS in open loop
//  at your known resonant frequency before closing the loop.
#define PLL_PHASE_TARGET_PCT  50.0f

//  Loop gain: Hz of frequency correction per 1% of phase error.
//  Start low. Negate it if the loop runs the wrong way (the sign
//  depends on which edge the 74HC14 gives you).
#define PLL_PHASE_GAIN_HZ     20.0f

#define PLL_UPDATE_MS       5         // How often to run the PLL loop (ms)
#define PLL_LOCK_TOLERANCE_HZ 500     // Consider "locked" within this error
#define PLL_MAX_STEP_HZ     200       // Max freq change per update (slew limit)

//  Power-mode detune: intentionally offset from resonance to
//  maximize current/coupling into the workpiece (the old
//  "tune for maximum smoke" behavior, but controlled).
//  0 = track resonance exactly. Positive = above resonance.
#define DETUNE_DEFAULT_HZ   0
#define DETUNE_MIN_HZ       -5000
#define DETUNE_MAX_HZ       5000
#define DETUNE_STEP_HZ      100

// ============================================================
//  PROTECTION / OCP
// ============================================================
// ============================================================
//  DIAGNOSTICS
// ============================================================
//  Loop-timing instrumentation: the "[loop] STALL" and "[timing]"
//  serial messages. Invaluable for finding a stall (it caught the
//  548ms software-SPI display render), but it floods the monitor
//  once a persistent stall exists, which makes everything else
//  unreadable. Off by default; turn on to investigate timing.
#define LOOP_DIAG_ENABLED   false
#define LOOP_DIAG_MS        50        // report a block slower than this

// ============================================================
//  PROTECTION / OCP
// ============================================================
#define ADC_SAMPLES         8         // Averaging samples per read

//  ---- Hardware OCP (BKIN / PB12) -- DISABLED --------------
//  false = TIM1's break input is never enabled, so the OCP board
//  cannot stop a run. The board stays wired and powered, and its
//  ARMED LED still shows if it trips, so you keep the information
//  without the interruption. PB12's live level is still reported
//  in the [pll] status line and by the 'b' serial command.
//
//  This is the protection posture every successful manual run on
//  this rig has used: variac ramp + operator judgement + NTC
//  over-temp. It is not a regression from a working state.
//
//  Set true to re-arm hardware overcurrent protection.
#define BKIN_ENABLED        false

//  ---- Software OCP (PA1 ADC path) -- DISABLED --------------
//  PA1 has no current sensor connected. OCP board v3 does not wire it
//  either (PB12 is its only Nucleo connection). Polling a floating ADC
//  input against a 3000-count threshold every 1ms is a guaranteed false
//  trip once anything inductive switches nearby — this is why normal
//  firmware tripped on contactor close while MANUAL_DRIVE_MODE (which
//  never calls Protection::checkFast()) ran fine.
//
//  Set true ONLY after a real sensor is wired to PA1 and calibrated.
//  The hardware OCP path (BKIN/PB12) is unaffected and stays active.
#define SOFT_OCP_ENABLED    false

#define OCP_THRESHOLD       3000      // ADC counts — tune after calibration
#define OCP_THRESH_MIN      500
#define OCP_THRESH_MAX      4000
#define OCP_THRESH_STEP     50
#define OCP_CHECK_MS        1         // Fast OCP poll interval when running

// ============================================================
//  NTC THERMISTOR (IGBT heatsink)
//  NTCLE100E3103: 10k @ 25C, B25/85 = 3435K
//  Divider: 3.3V -> 10k fixed -> junction(ADC) -> NTC -> GND
// ============================================================
#define NTC_ENABLED         true
#define NTC_SERIES_R        10000.0f  // Fixed resistor (ohms)
#define NTC_NOMINAL_R       10000.0f  // NTC resistance at 25C
#define NTC_NOMINAL_T       298.15f   // 25C in Kelvin
#define NTC_BETA            3435.0f   // B25/85 coefficient
#define TEMP_SHUTDOWN_C     80.0f     // Auto-shutdown threshold
#define TEMP_WARNING_C      65.0f     // Warning on display
#define TEMP_CHECK_MS       500       // Temperature poll interval

// ============================================================
//  MAINS / AC SENSE
// ============================================================
//  120V -> isolation TX -> rectifier -> divider -> ADC.
//  Omit smoothing cap for zero-cross detection (rectified sine).
#define AC_PRESENT_THRESHOLD 200      // ADC counts above = mains present
#define ZEROCROSS_THRESHOLD  100      // ADC counts below = near zero-cross
#define USE_ZEROCROSS_SWITCH true     // Switch contactor at zero-cross
#define ZEROCROSS_TIMEOUT_MS 30       // Max wait for a zero-cross (safety)

// ============================================================
//  TIMING / SEQUENCING
// ============================================================
#define BUS_CHARGE_MS       500       // Delay after contactor before PWM
#define SPLASH_MS           2500      // Welcome screen duration
#define DISPLAY_MS          250       // TFT refresh interval
#define BTN_DEBOUNCE_MS     250       // Encoder button debounce

// ============================================================
//  SMOOTHING
// ============================================================
#define EMA_ALPHA           0.15f     // 0.05 = smooth, 0.3 = responsive

// ============================================================
//  ENCODER MODES
//  What the encoder adjusts depends on system state / mode.
// ============================================================
enum EncoderMode
{
    ENC_MODE_FREQUENCY,   // Adjust base/start frequency (manual/setup)
    ENC_MODE_DETUNE,      // Adjust power-mode detune offset
    ENC_MODE_DEADTIME,    // Adjust dead-time
    ENC_MODE_OCP,         // Adjust OCP threshold
    ENC_MODE_COUNT        // Number of modes (for cycling)
};
