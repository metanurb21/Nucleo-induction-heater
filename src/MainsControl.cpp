// ============================================================
//  MainsControl implementation.
// ============================================================

#include "MainsControl.h"
#include "Sensing.h"
#include "config.h"

namespace MainsControl
{
    static bool s_energized = false;

    void init()
    {
        pinMode(PIN_CONTACTOR, OUTPUT);
        digitalWrite(PIN_CONTACTOR, LOW);   // contactor OFF at boot (safe)
        s_energized = false;
    }

    bool energize()
    {
        // Require mains present before we bother closing the contactor.
        if (!Sensing::mainsPresent())
        {
            return false;
        }

        // Optionally wait for a zero-cross to reduce arcing/inrush.
        if (USE_ZEROCROSS_SWITCH)
        {
            unsigned long start = millis();
            while (!Sensing::nearZeroCross())
            {
                if (millis() - start >= ZEROCROSS_TIMEOUT_MS)
                {
                    break;  // timed out — close anyway (safety over elegance)
                }
            }
        }

        digitalWrite(PIN_CONTACTOR, HIGH);  // opto -> relay -> contactor coil
        s_energized = true;
        return true;
    }

    void deEnergize()
    {
        // No zero-cross wait on shutdown — drop immediately.
        digitalWrite(PIN_CONTACTOR, LOW);
        s_energized = false;
    }

    bool isEnergized()
    {
        return s_energized;
    }
}
