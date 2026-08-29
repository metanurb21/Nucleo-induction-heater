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
//   Active-LOW fault kills PWM in hardware (~6ns). From OCP
//   comparator via Si8621 #2. Fail-safe: disconnect = gates off.
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
#define TIM1_CLOCK_HZ       180000000UL
#define PWM_FREQ_MIN_HZ     50000     // Lower bound of operating range
#define PWM_FREQ_MAX_HZ     100000    // Upper bound of operating range
#define PWM_FREQ_START_HZ   80000     // Default startup frequency (coil dependent)
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
#define ADC_SAMPLES         8         // Averaging samples per read
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
