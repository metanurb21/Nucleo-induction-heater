// ============================================================
//  Display — ST7735 128x160 color TFT UI.
//
//  Renders three screen types based on system state:
//   - Idle:    ready-to-start, settings, temp, mains status
//   - Running: live frequency, lock status, temp, current bar
//   - Fault:   fault type, relevant values, reset prompt
//
//  Only redraws the full screen on state change to avoid flicker;
//  live values update in place.
// ============================================================
#pragma once

#include <Arduino.h>
#include "SystemState.h"
#include "config.h"

namespace Display
{
    void init();
    void splash();

    // Render the current state. Pass the live data needed on screen.
    void render(SystemState state, EncoderMode encMode);

    // Force a full redraw on next render (e.g., after state change).
    void forceRedraw();
}
