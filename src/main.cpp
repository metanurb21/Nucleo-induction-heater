// ============================================================
//  Nucleo F446RE — Phase 1: Hello World
//  Verify: toolchain, upload, serial, onboard LED
//  
//  The Nucleo F446RE has:
//    - User LED (green) on PA5 (Arduino pin D13 / LED_BUILTIN)
//    - User button (blue) on PC13 (active LOW, no external pull-up needed)
//    - Serial over ST-Link USB (Serial object, no extra wiring)
//
//  Plug in USB, upload, open serial monitor at 115200.
// ============================================================

#include <Arduino.h>

#define USER_LED LED_BUILTIN  // PA5 — green LED
#define USER_BTN PC13         // Blue button — active LOW

unsigned long lastBlink = 0;
unsigned long lastReport = 0;
bool ledState = false;
int blinkInterval = 500;  // ms

void setup()
{
    Serial.begin(115200);
    delay(1000);  // Give serial monitor time to connect

    pinMode(USER_LED, OUTPUT);
    pinMode(USER_BTN, INPUT);  // Has internal pull-up on Nucleo

    Serial.println("=================================");
    Serial.println(" Nucleo F446RE — Alive!");
    Serial.println(" Induction Heater PLL Controller");
    Serial.println(" Phase 1: Hardware Verification");
    Serial.println("=================================");
    Serial.println();
    Serial.printf("CPU Clock: %lu MHz\n", SystemCoreClock / 1000000UL);
    Serial.printf("LED pin: PA5 (LED_BUILTIN)\n");
    Serial.printf("Button pin: PC13 (USER_BTN)\n");
    Serial.println();
    Serial.println("LED blinking at 1Hz. Press blue button to toggle fast/slow.");
    Serial.println("If you see this, toolchain + upload + serial all working.");
}

void loop()
{
    unsigned long now = millis();

    // Blink the LED
    if (now - lastBlink >= (unsigned long)blinkInterval)
    {
        lastBlink = now;
        ledState = !ledState;
        digitalWrite(USER_LED, ledState);
    }

    // Check button (active LOW, no debounce needed for this test)
    if (digitalRead(USER_BTN) == LOW)
    {
        blinkInterval = 100;  // Fast blink while held
    }
    else
    {
        blinkInterval = 500;  // Normal blink
    }

    // Periodic serial report (every 2 seconds)
    if (now - lastReport >= 2000)
    {
        lastReport = now;
        Serial.printf("[%6lu ms] LED=%s  BTN=%s  Blink=%dms\n",
                      now,
                      ledState ? "ON " : "OFF",
                      digitalRead(USER_BTN) == LOW ? "PRESSED" : "released",
                      blinkInterval);
    }
}
