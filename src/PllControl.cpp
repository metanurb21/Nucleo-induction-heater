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
#include "stm32f4xx.h"
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

    // ---- Phase measurement -------------------------------------
    //  TIM1 counts 0..ARR over exactly one PWM period, so TIM1->CNT
    //  sampled at the instant of a current zero-crossing IS the phase
    //  of that crossing within the drive cycle. Normalising by ARR+1
    //  makes it independent of frequency.
    //
    //  This is the error signal a resonance tracker actually needs.
    //  Frequency alone cannot work: the tank current is forced at the
    //  drive frequency, so measuring it returns the commanded value.
    //
    //  A fixed offset from ISR latency is included in the reading. That
    //  does not matter, because the target is calibrated empirically
    //  against a known resonance rather than derived from theory.
    //  Accumulated in the ISR over EVERY edge, not sampled once per
    //  update(). At 57 kHz there are ~285 cycles per 5 ms update, so
    //  reading a single edge aliased badly and made a drifting phase
    //  look like random noise. Averaging every edge gives a real mean,
    //  and the min/max spread tells us whether the phase is genuinely
    //  stable or uniformly scattered.
    static volatile uint32_t s_phSum   = 0;    // sum of per-edge phase, 0..999
    static volatile uint32_t s_phCount = 0;    // edges accumulated
    static volatile uint16_t s_phMin   = 9999;
    static volatile uint16_t s_phMax   = 0;

    static float s_phasePct  = -1.0f;          // -1 = no valid reading
    static float s_phaseSpread = -1.0f;        // (max-min) as % of period
    static uint32_t s_edgeRate = 0;            // edges per second

    // Minimum plausible capture period. Guards against a floating or
    // self-oscillating 74HC14 input (no real CT signal present, e.g.
    // coil not running) flooding PA0 with edges far above any real
    // tank frequency. At TIM2CLK=90MHz, this floor corresponds to
    // ~900kHz — comfortably above PWM_FREQ_MAX_HZ (real signals never
    // get this fast), so genuine tank frequencies are never rejected.
    // Discovered during bench bring-up: with no coil connected, the
    // HC14 self-oscillates and floods the ISR badly enough to starve
    // the rest of setup() before it can complete (looked like a hang).
    static const uint32_t MIN_PLAUSIBLE_PERIOD = 100; // TIM2 ticks

    // ---- capture callback (called from HardwareTimer ISR) --
    void handleCapture()
    {
        // Sample TIM1's counter FIRST, before any other work, so the
        // phase reading carries the least possible latency.
        uint32_t t1cnt = TIM1->CNT;
        uint32_t t1arr = TIM1->ARR;

        uint32_t now = s_timer->getCaptureCompare(CAP_CHANNEL);
        if (s_haveCapture)
        {
            uint32_t period = now - s_lastCapture;   // 32-bit wrap is fine
            if (period < MIN_PLAUSIBLE_PERIOD)
            {
                // Implausibly fast edge (noise / self-oscillating input
                // with no real signal) — ignore this edge's period, but
                // still update s_lastCapture so we're ready for the next
                // one. Do NOT touch s_edgeTimeoutCtr here: a storm of
                // garbage edges should NOT look like "signal present."
                s_lastCapture = now;
                return;
            }
            s_period = period;
        }
        s_lastCapture = now;
        s_haveCapture = true;
        s_edgeTimeoutCtr = 0;

        // Phase of THIS edge within the drive cycle, in per-mille
        // (0..999) to keep the accumulator in integer maths.
        if (t1arr > 0)
        {
            uint16_t ph = (uint16_t)((1000UL * t1cnt) / (t1arr + 1));
            if (ph > 999) ph = 999;
            s_phSum += ph;
            s_phCount++;
            if (ph < s_phMin) s_phMin = ph;
            if (ph > s_phMax) s_phMax = ph;
        }
    }

    // Snapshot and clear the ISR accumulators. Called once per reporting
    // interval from update().
    static void collectPhaseStats(uint32_t elapsedMs)
    {
        __disable_irq();
        uint32_t sum = s_phSum;
        uint32_t cnt = s_phCount;
        uint16_t mn  = s_phMin;
        uint16_t mx  = s_phMax;
        s_phSum = 0;
        s_phCount = 0;
        s_phMin = 9999;
        s_phMax = 0;
        __enable_irq();

        if (cnt == 0 || elapsedMs == 0)
        {
            s_phasePct = -1.0f;
            s_phaseSpread = -1.0f;
            s_edgeRate = 0;
            return;
        }

        s_phasePct    = (float)sum / (float)cnt / 10.0f;   // per-mille -> %
        s_phaseSpread = (float)(mx - mn) / 10.0f;
        s_edgeRate    = (uint32_t)((1000ULL * cnt) / elapsedMs);
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

        // Do NOT resume() here. Boot-safe: capture stays paused until
        // startCapture() is explicitly called (e.g. by StateManager
        // right before enabling PWM). Discovered during bench bring-up:
        // with no coil running, the 74HC14 feedback conditioner has no
        // real signal and self-oscillates, flooding PA0 with edges the
        // instant the timer is armed — fast enough to starve the rest
        // of setup() before it can complete (looked like a hang).
    }

    void startCapture()
    {
        reset();
        s_timer->resume();
    }

    void stopCapture()
    {
        s_timer->pause();
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

        // ---- Phase statistics, recomputed every 250ms over EVERY edge --
        {
            static unsigned long tStats = 0;
            unsigned long nowMs = millis();
            if (tStats == 0) tStats = nowMs;
            if (nowMs - tStats >= 250)
            {
                collectPhaseStats(nowMs - tStats);
                tStats = nowMs;
            }
        }

        if (!active)
        {
            s_locked = false;
            return;
        }

        // No valid feedback yet — hold current frequency. Nothing to track.
        if (s_measuredHz == 0)
        {
            s_locked = false;
            return;
        }

#if !PLL_CLOSED_LOOP
        // ---- OPEN LOOP (characterisation mode) ----
        // Measure and report only. The encoder stays in control of
        // frequency so you can sweep manually and record phase against
        // frequency. See PLL_CLOSED_LOOP in config.h.
        //
        // "Locked" here means only that valid feedback is present, not
        // that the loop is regulating anything.
        s_locked = false;
        return;
#endif

        // ---- CLOSED LOOP: steer on PHASE error ----
        // Frequency is not usable as an error signal — the tank current
        // is forced at the drive frequency, so measured always equals
        // commanded. Phase carries the actual information.
        if (s_phasePct < 0.0f)
        {
            s_locked = false;
            return;
        }

        float phaseErr = s_phasePct - PLL_PHASE_TARGET_PCT;
        s_locked = (fabsf(phaseErr) < 2.0f);   // within 2% of period

        // Sign convention is calibrated empirically: sweep in open loop
        // and note which way phase moves as frequency rises. If the loop
        // runs away in the wrong direction, negate PLL_PHASE_GAIN_HZ.
        int32_t target = (int32_t)PwmDrive::getFrequency()
                         + (int32_t)(phaseErr * PLL_PHASE_GAIN_HZ)
                         + s_detuneHz;
        if (target < (int32_t)PWM_FREQ_MIN_HZ) target = PWM_FREQ_MIN_HZ;
        if (target > (int32_t)PWM_FREQ_MAX_HZ) target = PWM_FREQ_MAX_HZ;

        if (target < (int32_t)PWM_FREQ_MIN_HZ) target = PWM_FREQ_MIN_HZ;
        if (target > (int32_t)PWM_FREQ_MAX_HZ) target = PWM_FREQ_MAX_HZ;

        // Slew-limited move toward target.
        int32_t current = (int32_t)PwmDrive::getFrequency();
        int32_t err = target - current;
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

    // Phase of the current zero-crossing within the drive cycle, as a
    // percentage of the PWM period. -1 = no valid feedback.
    float getPhasePct()          { return s_phasePct; }
    float getPhaseSpreadPct()    { return s_phaseSpread; }
    uint32_t getEdgeRateHz()     { return s_edgeRate; }

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
