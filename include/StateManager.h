// ============================================================
//  StateManager — the system brain.
//
//  Owns the SystemState machine and the safe startup/shutdown
//  sequences. Coordinates all modules:
//    - Reads Encoder for user input (start/stop, adjust settings)
//    - Runs Protection checks and executes shutdown on fault
//    - Drives PllControl during RUNNING
//    - Commands MainsControl and PwmDrive in the right order
//
//  Startup order (safety):
//    verify temp OK -> verify mains -> energize contactor ->
//    charge bus -> enable PWM -> begin PLL tracking
//
//  Shutdown order (safety, fastest first):
//    disable PWM -> drop contactor -> set fault state
// ============================================================
#pragma once

#include <Arduino.h>
#include "SystemState.h"
#include "config.h"

namespace StateManager
{
    void init();

    // Call every loop. Handles timing-based polling internally.
    void update();

    // Current system state (for display, etc.).
    SystemState state();

    // Current encoder adjustment mode.
    EncoderMode encoderMode();
}
