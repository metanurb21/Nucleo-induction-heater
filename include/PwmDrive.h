// ============================================================
//  PwmDrive — TIM1 complementary PWM with hardware dead-time
//  and break input (BKIN) for the IGBT gate drive.
//
//  TIM1_CH1  (PA8)  = PWM_A
//  TIM1_CH1N (PB13) = PWM_B (complementary)
//  TIM1_BKIN (PB12) = hardware fault, active LOW, kills outputs
//
//  Dead-time is generated in hardware (DTG bits in BDTR). The
//  break input forces both outputs to their inactive (off) state
//  in ~1 timer clock with zero software involvement.
// ============================================================
#pragma once

#include <Arduino.h>

namespace PwmDrive
{
    // Initialize TIM1, GPIO alternate functions, dead-time, and break.
    // Outputs start DISABLED (safe). Call enableOutputs() to start driving.
    void init();

    // Set PWM frequency in Hz (clamped to config min/max).
    // Recomputes ARR and keeps 50% duty. Safe to call while running.
    void setFrequency(uint32_t freqHz);

    // Get the currently commanded frequency (Hz).
    uint32_t getFrequency();

    // Set dead-time in nanoseconds (clamped to config min/max).
    // Converts ns to the TIM1 DTG register value.
    void setDeadTimeNs(uint16_t ns);

    // Get current dead-time (ns).
    uint16_t getDeadTimeNs();

    // Enable the complementary outputs (MOE bit). Gates start switching.
    void enableOutputs();

    // Disable outputs immediately (clear MOE). Gates go off. Software path.
    void disableOutputs();

    // True if outputs are currently enabled.
    bool outputsEnabled();

    // True if the hardware break (BKIN) has tripped since last clear.
    // BIF is a latch: it stays set until clearBreak(), and it is set by
    // any BKIN event even while MOE=0. Clear it immediately before
    // enabling outputs, never earlier in a startup sequence.
    bool breakTripped();

    // Re-arm after a break event (only call once fault condition is gone).
    void clearBreak();

    // Live level of the PB12 fault line, read as a GPIO (independent of
    // the latched BIF flag). True = OCP board armed and healthy.
    // Use this to confirm the fault is actually gone before clearing BIF
    // and arming the gates.
    bool faultLineHealthy();
}
