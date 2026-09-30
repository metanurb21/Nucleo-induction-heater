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

    // readTankVoltsEstimate() REMOVED — the PC2 tank divider is gone as
    // of OCP board v3. It was a 1.67MΩ source into an unbuffered ADC
    // input, which corrupted adjacent channels (including the PA1 OCP
    // read) via sample-cap charge sharing and caused the measured
    // [loop] STALL. Use readBusRaw() (PA4) for a display level instead.
    // See docs/OCP_BOARD_Final.md, "Section 2 — removed".
}
