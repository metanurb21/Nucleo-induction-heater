// ============================================================
//  Sensing implementation.
//
//  Uses the Arduino analogRead() API for simplicity and
//  portability. The F446RE ADC is fast enough that averaging
//  8 samples still completes well within our OCP poll window.
//
//  For future optimization, this module is the place to switch
//  to DMA-driven continuous ADC if the loop timing needs it.
// ============================================================

#include "Sensing.h"
#include "config.h"
#include <math.h>

namespace Sensing
{
    static float s_ocpSmooth = 0.0f;
    static int   s_ocpRaw = 0;
    static float s_tempSmooth = 25.0f;

    // ---- init ---------------------------------------------
    void init()
    {
        pinMode(PIN_ADC_OCP, INPUT);
        pinMode(PIN_ADC_VBUS, INPUT);
        pinMode(PIN_ADC_NTC, INPUT);
        pinMode(PIN_ADC_AC, INPUT);

        analogReadResolution(12);   // 0..4095

        // Prime the smoothed values with an initial reading.
        s_ocpRaw = readOcpRaw();
        s_ocpSmooth = s_ocpRaw;
        s_tempSmooth = readTemperatureC();
    }

    // ---- averaged raw read helper -------------------------
    static int adcAverage(uint32_t pin)
    {
        long sum = 0;
        for (int i = 0; i < ADC_SAMPLES; i++)
        {
            sum += analogRead(pin);
        }
        return (int)(sum / ADC_SAMPLES);
    }

    // ---- OCP ----------------------------------------------
    int readOcpRaw()
    {
        return adcAverage(PIN_ADC_OCP);
    }

    int updateOcp()
    {
        s_ocpRaw = readOcpRaw();
        s_ocpSmooth = (EMA_ALPHA * s_ocpRaw) + ((1.0f - EMA_ALPHA) * s_ocpSmooth);
        return s_ocpRaw;
    }

    float ocpSmoothed() { return s_ocpSmooth; }
    int   ocpRaw()      { return s_ocpRaw; }

    // ---- Temperature (NTC, Beta equation) -----------------
    float readTemperatureC()
    {
        int adc = adcAverage(PIN_ADC_NTC);
        if (adc <= 0)   adc = 1;        // avoid div-by-zero
        if (adc >= 4095) return -40.0f; // open circuit / no sensor

        float vOut = adc * 3.3f / 4095.0f;
        float rNtc = NTC_SERIES_R * vOut / (3.3f - vOut);
        float tK = 1.0f / ((1.0f / NTC_NOMINAL_T) +
                           (1.0f / NTC_BETA) * logf(rNtc / NTC_NOMINAL_R));
        return tK - 273.15f;
    }

    void updateTemperature()
    {
        float t = readTemperatureC();
        s_tempSmooth = (EMA_ALPHA * t) + ((1.0f - EMA_ALPHA) * s_tempSmooth);
    }

    float temperatureSmoothedC() { return s_tempSmooth; }

    // ---- Bus voltage --------------------------------------
    int readBusRaw()
    {
        return adcAverage(PIN_ADC_VBUS);
    }

    // ---- AC mains sense -----------------------------------
    // Single fast read (not averaged) so zero-cross timing is crisp.
    int readAcRaw()
    {
        return analogRead(PIN_ADC_AC);
    }

    bool mainsPresent()
    {
        // Sample a few times over a short window; if we ever see a
        // reading above the presence threshold, mains is live (the
        // rectified sine peaks well above threshold each half-cycle).
        for (int i = 0; i < 20; i++)
        {
            if (analogRead(PIN_ADC_AC) > AC_PRESENT_THRESHOLD)
                return true;
            delayMicroseconds(500);
        }
        return false;
    }

    bool nearZeroCross()
    {
        return analogRead(PIN_ADC_AC) < ZEROCROSS_THRESHOLD;
    }
}
