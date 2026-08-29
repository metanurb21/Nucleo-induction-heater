// ============================================================
//  Protection — fault detection and classification.
//
//  Layers (fastest first):
//   1. TIM1 BKIN hardware break (in PwmDrive) — kills PWM in ~6ns.
//   2. Software OCP — polls the ADC current sense, trips if over.
//   3. Over-temperature — NTC on IGBT heatsink.
//
//  This module only DETECTS and reports faults. The StateManager
//  owns the actual shutdown sequence (drop contactor, etc.).
// ============================================================
#pragma once

#include <Arduino.h>

namespace Protection
{
    enum FaultType
    {
        FAULT_NONE,
        FAULT_OCP_HW,    // Hardware break (TIM1 BKIN) tripped
        FAULT_OCP_SW,    // Software overcurrent
        FAULT_TEMP,      // Over-temperature
        FAULT_NO_MAINS   // Mains lost during run
    };

    void init();

    // Adjustable OCP threshold (ADC counts).
    void setOcpThreshold(int counts);
    int  getOcpThreshold();

    // Poll fast protections (call every OCP_CHECK_MS while running).
    // Returns a fault if one is detected this cycle, else FAULT_NONE.
    FaultType checkFast();

    // Poll slow protections (call every TEMP_CHECK_MS while running).
    FaultType checkSlow();

    // Convert a fault type to a short human string for display/log.
    const char* faultName(FaultType f);
}
