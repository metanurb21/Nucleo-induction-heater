// ============================================================
//  PHASE 1 REFERENCE — Hello World blink/serial test.
//  Kept for reference. This is the code that verified the
//  toolchain, upload, serial, LED, and button on the bare Nucleo.
//  NOT compiled (lives in docs/, outside src/).
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
    delay(1000);
    pinMode(USER_LED, OUTPUT);
    pinMode(USER_BTN, INPUT);
    Serial.println("Nucleo F446RE — Phase 1 alive");
    Serial.printf("CPU Clock: %lu MHz\n", SystemCoreClock / 1000000UL);
}

void loop()
{
    unsigned long now = millis();
    if (now - lastBlink >= (unsigned long)blinkInterval)
    {
        lastBlink = now;
        ledState = !ledState;
        digitalWrite(USER_LED, ledState);
    }
    blinkInterval = (digitalRead(USER_BTN) == LOW) ? 100 : 500;
    if (now - lastReport >= 2000)
    {
        lastReport = now;
        Serial.printf("[%6lu ms] LED=%s BTN=%s\n", now,
                      ledState ? "ON" : "OFF",
                      digitalRead(USER_BTN) == LOW ? "PRESSED" : "released");
    }
}
