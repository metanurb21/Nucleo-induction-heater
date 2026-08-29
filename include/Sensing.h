// ============================================================
//  Sensing — ADC acquisition and conversion.
//
//  Reads:
//   - OCP current/voltage sense (fast, for overcurrent)
//   - DC bus voltage
//   - NTC heatsink temperature
//   - AC mains sense (for presence + zero-cross)
//
//  Provides raw values, EMA-smoothed values, and engineering
//  units (deg C for temperature).
// ============================================================
#pragma once

#include <Arduino.h>

namespace Sensing
{
    void init();

    // Fast OCP channel — read raw ADC (averaged over ADC_SAMPLES).
    int readOcpRaw();

    // Update the smoothed OCP value from a fresh raw read; returns raw.
    int updateOcp();
    float ocpSmoothed();
    int   ocpRaw();

    // Temperature (deg C) from NTC. Returns -40 if open/no sensor.
    float readTemperatureC();
    void  updateTemperature();
    float temperatureSmoothedC();

    // DC bus voltage — raw ADC and scaled (scaling TBD by divider).
    int   readBusRaw();

    // AC mains sense — raw ADC (for presence + zero-cross detection).
    int   readAcRaw();
    bool  mainsPresent();
    bool  nearZeroCross();
}
