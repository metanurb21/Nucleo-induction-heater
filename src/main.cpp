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

// ============================================================
//  PWM_TEST_MODE — bench verification of the gate signals.
//  Enable by building with -DPWM_TEST_MODE (see platformio.ini).
//
//  Bypasses the state machine, contactor, and all protections.
//  Just generates complementary PWM on PA8/PB13 so you can probe
//  frequency, complementary behavior, and dead-time on a scope.
//
//  SAFE ONLY WITH NO POWER STAGE CONNECTED. This drives the gate
//  signal pins directly with no interlocks.
//
//  Serial commands (115200):
//    f<hz>   set frequency, e.g. "f75000"
//    d<ns>   set dead-time, e.g. "d200"
//    e       enable outputs
//    x       disable outputs
//    ?       print current state
// ============================================================
#ifdef PWM_TEST_MODE

static void testPrintState()
{
    Serial.printf("PWM: freq=%lu Hz  deadtime=%u ns  outputs=%s\n",
                  (unsigned long)PwmDrive::getFrequency(),
                  PwmDrive::getDeadTimeNs(),
                  PwmDrive::outputsEnabled() ? "ENABLED" : "disabled");
}

void setup()
{
    Serial.begin(115200);
    delay(300);
    Serial.println("=====================================");
    Serial.println(" PWM TEST MODE — F446RE");
    Serial.printf( " Core: %lu MHz\n", SystemCoreClock / 1000000UL);
    Serial.println(" Probe PA8 (CH1) and PB13 (CH1N) on the AD3");
    Serial.println(" NO POWER STAGE — signal generation only");
    Serial.println("=====================================");
    Serial.println(" Commands: f<hz>  d<ns>  e(nable)  x(disable)  ?(status)");

    PwmDrive::init();                       // TIM1 up, outputs still off
    PwmDrive::setFrequency(PWM_FREQ_START_HZ);
    PwmDrive::setDeadTimeNs(DEADTIME_NS_DEFAULT);
    PwmDrive::enableOutputs();              // start driving immediately

    Serial.println("Outputs ENABLED at startup for probing.");
    testPrintState();
}

void loop()
{
    // Simple serial command parser for live tuning while scoping.
    static char buf[16];
    static uint8_t idx = 0;

    while (Serial.available())
    {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r')
        {
            buf[idx] = '\0';
            if (idx > 0)
            {
                switch (buf[0])
                {
                case 'f':
                    PwmDrive::setFrequency((uint32_t)atol(buf + 1));
                    testPrintState();
                    break;
                case 'd':
                    PwmDrive::setDeadTimeNs((uint16_t)atoi(buf + 1));
                    testPrintState();
                    break;
                case 'e':
                    PwmDrive::enableOutputs();
                    testPrintState();
                    break;
                case 'x':
                    PwmDrive::disableOutputs();
                    testPrintState();
                    break;
                case '?':
                    testPrintState();
                    break;
                default:
                    Serial.println("? unknown cmd");
                    break;
                }
            }
            idx = 0;
        }
        else if (idx < sizeof(buf) - 1)
        {
            buf[idx++] = c;
        }
    }
}

#else  // ===================== NORMAL FIRMWARE =====================

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

#endif // PWM_TEST_MODE
