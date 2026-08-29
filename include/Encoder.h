// ============================================================
//  Encoder — rotary encoder (TIM3 hardware quadrature) + button.
//
//  TIM3 in encoder mode decodes the quadrature signal in
//  hardware (zero CPU, no missed steps, no bounce). We read the
//  counter delta each poll to get rotation direction/amount.
//
//  The button is polled with debounce. What the encoder adjusts
//  depends on the current EncoderMode (frequency, detune,
//  dead-time, OCP threshold) — the StateManager decides.
// ============================================================
#pragma once

#include <Arduino.h>

namespace Encoder
{
    void init();

    // Signed rotation delta since last call (+ = CW, - = CCW).
    // Reads and clears the accumulated TIM3 counter movement.
    int32_t readDelta();

    // True once per physical button press (debounced, edge-detected).
    bool buttonPressed();
}
