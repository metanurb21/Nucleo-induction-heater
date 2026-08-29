// ============================================================
//  MainsControl — the AC contactor and mains safety interface.
//
//  Chain: GPIO -> optocoupler -> 5V relay -> 120V AC ->
//         FUJI 100A contactor coil -> mains to H-bridge.
//
//  Provides safe energize/de-energize with optional zero-cross
//  switching to reduce contactor arcing and inrush.
// ============================================================
#pragma once

#include <Arduino.h>

namespace MainsControl
{
    void init();

    // Energize the contactor. If USE_ZEROCROSS_SWITCH, waits for a
    // mains zero-cross (up to ZEROCROSS_TIMEOUT_MS) before closing.
    // Returns true if energized, false if mains not present.
    bool energize();

    // Drop the contactor immediately (no zero-cross wait — safety).
    void deEnergize();

    // Is the contactor currently commanded ON?
    bool isEnergized();
}
