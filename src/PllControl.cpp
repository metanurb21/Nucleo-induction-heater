// ============================================================
//  PllControl implementation.
//
//  Frequency measurement via TIM2 input capture:
//   - TIM2 runs at a known clock (APB1 timer = 90 MHz on F446RE).
//   - Each rising edge on PA0 captures the counter value.
//   - Period = (capture_n - capture_n-1); freq = TIM2CLK / period.
//   - We use a 32-bit timer (TIM2) so no overflow handling needed
//     at our frequencies.
//
//  Tracking strategy:
//   - target = measured_resonance + detune
//   - slew PwmDrive frequency toward target, limited by
//     PLL_MAX_STEP_HZ per update to avoid abrupt jumps.
//
//  This is scaffolding — the capture math and register setup
//  need validation on the AD3 during bring-up.
// ============================================================

#include "PllControl.h"
#include "PwmDrive.h"
#include "config.h"
#include <HardwareTimer.h>

// We use the Arduino STM32 core's HardwareTimer for TIM2 input capture.
// This avoids colliding with the core's own TIM2_IRQHandler (which caused
// a "multiple definition" link error when we hand-rolled the ISR). The
// HardwareTimer attaches our callback to the capture event cleanly.

namespace PllControl
{
    // TIM2 timer clock on F446RE: APB1 timer domain = 90 MHz.
    static const uint32_t TIM2_CLOCK_HZ = 90000000UL;

    static HardwareTimer* s_timer = nullptr;
    static const uint32_t CAP_CHANNEL = 1;   // TIM2_CH1 on PA0

    static volatile uint32_t s_lastCapture = 0;
    static volatile uint32_t s_period = 0;
    static volatile bool s_haveCapture = false;
    static volatile uint32_t s_edgeTimeoutCtr = 0;

    static uint32_t s_measuredHz = 0;
    static int32_t s_detuneHz = DETUNE_DEFAULT_HZ;
    static bool s_locked = false;

    // ---- capture callback (called from HardwareTimer ISR) --
    void handleCapture()
    {
        uint32_t now = s_timer->getCaptureCompare(CAP_CHANNEL);
        if (s_haveCapture)
        {
            s_period = now - s_lastCapture;    // 32-bit wrap is fine
        }
        s_lastCapture = now;
        s_haveCapture = true;
        s_edgeTimeoutCtr = 0;
    }

    // ---- init ---------------------------------------------
    void init()
    {
        s_timer = new HardwareTimer(TIM2);

        // Max resolution: no prescale, full 32-bit range.
        s_timer->setPrescaleFactor(1);
        s_timer->setOverflow(0xFFFFFFFF, TICK_FORMAT);

        // Configure CH1 (PA0) for rising-edge input capture.
        s_timer->setMode(CAP_CHANNEL, TIMER_INPUT_CAPTURE_RISING, PIN_FREQ_FB);
        s_timer->attachInterrupt(CAP_CHANNEL, handleCapture);

        s_timer->resume();
    }

    // ---- update loop --------------------------------------
    void update(bool active)
    {
        // Compute measured frequency from captured period.
        if (s_haveCapture && s_period > 0)
        {
            s_measuredHz = TIM2_CLOCK_HZ / s_period;
        }

        // Edge timeout: if no capture for a while, treat as no signal.
        // update() is called every PLL_UPDATE_MS; ~200 calls = ~1s.
        if (++s_edgeTimeoutCtr > 200)
        {
            s_measuredHz = 0;
            s_haveCapture = false;
            s_period = 0;
        }

        if (!active)
        {
            s_locked = false;
            return;
        }

        // No valid feedback yet — hold current frequency (or sweep,
        // handled by StateManager during startup). Nothing to track.
        if (s_measuredHz == 0)
        {
            s_locked = false;
            return;
        }

        // Target = resonance + detune, clamped to operating range.
        int32_t target = (int32_t)s_measuredHz + s_detuneHz;
        if (target < (int32_t)PWM_FREQ_MIN_HZ) target = PWM_FREQ_MIN_HZ;
        if (target > (int32_t)PWM_FREQ_MAX_HZ) target = PWM_FREQ_MAX_HZ;

        // Slew-limited move toward target.
        int32_t current = (int32_t)PwmDrive::getFrequency();
        int32_t err = target - current;

        if (abs(err) <= (int32_t)PLL_LOCK_TOLERANCE_HZ)
        {
            s_locked = true;
        }
        else
        {
            s_locked = false;
        }

        int32_t step = err;
        if (step >  (int32_t)PLL_MAX_STEP_HZ) step =  PLL_MAX_STEP_HZ;
        if (step < -(int32_t)PLL_MAX_STEP_HZ) step = -PLL_MAX_STEP_HZ;

        if (step != 0)
        {
            PwmDrive::setFrequency((uint32_t)(current + step));
        }
    }

    uint32_t getMeasuredFreqHz() { return s_measuredHz; }
    bool isLocked()              { return s_locked; }

    void setDetuneHz(int32_t hz)
    {
        if (hz < DETUNE_MIN_HZ) hz = DETUNE_MIN_HZ;
        if (hz > DETUNE_MAX_HZ) hz = DETUNE_MAX_HZ;
        s_detuneHz = hz;
    }
    int32_t getDetuneHz() { return s_detuneHz; }

    void reset()
    {
        s_haveCapture = false;
        s_period = 0;
        s_measuredHz = 0;
        s_locked = false;
        s_edgeTimeoutCtr = 0;
    }
}
