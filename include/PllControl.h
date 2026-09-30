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
    // Configure TIM2 input capture on PA0. Leaves the capture
    // PAUSED (see startCapture()) — boot-safe, matches the
    // contactor/PWM being off by default. This avoids a floating
    // or self-oscillating feedback input (no real CT signal, e.g.
    // coil not running) flooding the ISR during boot before the
    // rest of setup() can finish.
    void init();

    // Arm/disarm the TIM2 input capture. Call startCapture() only
    // when frequency feedback is actually needed (e.g. StateManager
    // startup, right before enabling PWM); call stopCapture() on
    // shutdown. Safe to call repeatedly.
    void startCapture();
    void stopCapture();

    // Latest measured tank frequency (Hz). 0 = no signal / not locked.
    uint32_t getMeasuredFreqHz();

    // Run one PLL iteration: read measured freq, compute target
    // (resonance + detune), slew PwmDrive toward it. Call at
    // PLL_UPDATE_MS cadence. Only acts when 'active' is true.
    void update(bool active);

    // True when the phase error is inside tolerance (closed loop only).
    bool isLocked();

    // Phase of the tank-current zero-crossing within the drive cycle,
    // as a percentage of the PWM period. -1 = no valid feedback.
    //
    // This is the resonance error signal. Sampled by reading TIM1->CNT
    // inside the TIM2 capture ISR: TIM1 counts 0..ARR over exactly one
    // PWM period, so its count at the instant of a zero-crossing is that
    // crossing's phase. Includes a fixed ISR-latency offset, which is
    // irrelevant because the target is calibrated against a known
    // resonance rather than derived analytically.
    float getPhasePct();

    // Spread (max - min) of the per-edge phase over the last reporting
    // interval, as % of period. -1 = no data.
    //
    // THIS IS THE KEY DIAGNOSTIC. A small spread means the current
    // zero-crossings hold a fixed position in the drive cycle, so phase
    // is a usable error signal. A spread approaching 100 means they are
    // uniformly scattered — either the feedback edges are unclean, or
    // the current is not at the drive frequency and the phase is sliding.
    float getPhaseSpreadPct();

    // Feedback edges per second. Compare against the commanded PWM
    // frequency: they should match 1:1. Double means both zero-crossings
    // are triggering; much higher means noise or Schmitt chatter; much
    // lower means edges are being missed.
    uint32_t getEdgeRateHz();

    // Detune offset (Hz) for power mode. + = above resonance.
    void setDetuneHz(int32_t hz);
    int32_t getDetuneHz();

    // Reset internal state (call before each run start).
    void reset();

    // Called from the TIM2 capture ISR. Not for general use.
    void handleCapture();
}
