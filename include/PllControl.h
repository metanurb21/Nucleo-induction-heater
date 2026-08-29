// ============================================================
//  PllControl — measures tank resonant frequency via TIM2
//  input capture and steers the PWM frequency to track it.
//
//  Feedback: tank CT -> 74HC14 square wave -> Si8621 -> PA0.
//  TIM2_CH1 captures the period between rising edges; from that
//  we compute the actual resonant frequency of the tank.
//
//  The controller then nudges PwmDrive toward that frequency
//  (optionally offset by a detune value for "power mode").
// ============================================================
#pragma once

#include <Arduino.h>

namespace PllControl
{
    // Configure TIM2 input capture on PA0.
    void init();

    // Latest measured tank frequency (Hz). 0 = no signal / not locked.
    uint32_t getMeasuredFreqHz();

    // Run one PLL iteration: read measured freq, compute target
    // (resonance + detune), slew PwmDrive toward it. Call at
    // PLL_UPDATE_MS cadence. Only acts when 'active' is true.
    void update(bool active);

    // True when measured frequency is within lock tolerance of target.
    bool isLocked();

    // Detune offset (Hz) for power mode. + = above resonance.
    void setDetuneHz(int32_t hz);
    int32_t getDetuneHz();

    // Reset internal state (call before each run start).
    void reset();

    // Called from the TIM2 capture ISR. Not for general use.
    void handleCapture();
}
