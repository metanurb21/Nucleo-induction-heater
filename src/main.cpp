// ============================================================
//  Induction Heater PLL Controller — STM32 Nucleo F446RE
//  Main orchestrator (thin — all logic lives in modules).
//
//  Modules:
//    config.h       — pins, thresholds, tunables (single source)
//    PwmDrive       — TIM1 complementary PWM + dead-time + BKIN
//    PllControl     — TIM2 capture, resonance tracking, detune
//    Protection     — OCP + temperature fault detection
//    Sensing        — ADC acquisition + conversion + smoothing
//    MainsControl   — contactor relay + zero-cross switching
//    Encoder        — TIM3 quadrature + button
//    Display        — ST7735 TFT UI
//    StateManager   — the brain: state machine + sequencing
//
//  See docs/FIRMWARE.md for architecture and the phased
//  bring-up plan. Scaffold — validate on the AD3 before
//  connecting any power stage.
//
//  SAFETY: Boots with contactor OFF and PWM outputs disabled.
//  Nothing switches until StateManager runs the guarded startup.
// ============================================================

#include <Arduino.h>
#include "config.h"
#include "PwmDrive.h"
#include "PllControl.h"
#include "Protection.h"
#include "Sensing.h"
#include "MainsControl.h"
#include "Encoder.h"
#include "Display.h"
#include "StateManager.h"

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println("=====================================");
    Serial.println(" Induction Heater PLL — F446RE");
    Serial.printf( " Core: %lu MHz\n", SystemCoreClock / 1000000UL);
    Serial.println("=====================================");

    // ---- Safety-critical outputs FIRST ----
    // Contactor off, PWM disabled before anything else initializes.
    pinMode(PIN_LED_STATUS, OUTPUT);
    pinMode(PIN_LED_FAULT, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);
    digitalWrite(PIN_LED_FAULT, LOW);

    MainsControl::init();   // contactor forced OFF
    PwmDrive::init();       // TIM1 configured, outputs DISABLED (MOE=0)

    // ---- Sensing + protection ----
    Sensing::init();
    Protection::init();

    // ---- PLL feedback capture ----
    PllControl::init();

    // ---- User interface ----
    Encoder::init();
    Display::init();
    Display::splash();
    delay(SPLASH_MS);

    // ---- State machine ----
    StateManager::init();

    Serial.println("Ready. Idle. Press encoder to START.");
}

void loop()
{
    // Everything is timing-managed inside StateManager::update().
    StateManager::update();
}
