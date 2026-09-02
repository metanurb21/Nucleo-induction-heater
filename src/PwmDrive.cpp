// ============================================================
//  PwmDrive implementation — direct TIM1 register access.
//
//  Why registers instead of Arduino HardwareTimer:
//   - HardwareTimer doesn't cleanly expose CH1N complementary
//     output, dead-time generator, or break input.
//   - These are exactly the features that make the STM32 worth
//     using for this project, so we configure TIM1 directly.
//
//  TIM1 clock domain on F446RE: APB2 timer clock = 180 MHz.
//
//  NOTE: Register-level setup below is written against the STM32
//  reference manual (RM0390). Pin alternate-function numbers:
//    PA8  -> TIM1_CH1  = AF1
//    PB13 -> TIM1_CH1N = AF1
//    PB12 -> TIM1_BKIN = AF1
//  Verify AF mapping in the F446RE datasheet Table 11 during
//  bring-up. This is scaffolding — validate on the AD3 before
//  connecting any power stage.
// ============================================================

#include "PwmDrive.h"
#include "config.h"
#include "stm32f4xx.h"

namespace PwmDrive
{
    static uint32_t s_freqHz = PWM_FREQ_START_HZ;
    static uint16_t s_deadNs = DEADTIME_NS_DEFAULT;
    static bool s_enabled = false;
    static uint32_t s_arr = 0;

    // ---- helpers ------------------------------------------

    // Compute ARR for a target frequency: f = TIM1CLK / (PSC+1) / (ARR+1)
    // We keep PSC = 0 for maximum resolution.
    static uint32_t freqToArr(uint32_t freqHz)
    {
        if (freqHz < PWM_FREQ_MIN_HZ) freqHz = PWM_FREQ_MIN_HZ;
        if (freqHz > PWM_FREQ_MAX_HZ) freqHz = PWM_FREQ_MAX_HZ;
        return (TIM1_CLOCK_HZ / freqHz) - 1;
    }

    // Convert dead-time in ns to the DTG[7:0] register value.
    // For the simplest range (DTG[7:5]=0), each step = 1/TIM1CLK.
    // At 180MHz, 1 step = ~5.56ns. Range with this encoding:
    //   0..127 steps => 0..~706ns. Covers our 100-1000ns need
    //   using the extended encodings for the higher end.
    static uint8_t deadNsToDtg(uint16_t ns)
    {
        if (ns < DEADTIME_NS_MIN) ns = DEADTIME_NS_MIN;
        if (ns > DEADTIME_NS_MAX) ns = DEADTIME_NS_MAX;

        // t_dtg per step in ns for the base range
        const float step_ns = 1000000000.0f / (float)TIM1_CLOCK_HZ; // ~5.56ns

        // Base range: DTG[7:5] = 0xx -> deadtime = DTG[7:0] * step
        uint32_t steps = (uint32_t)((float)ns / step_ns + 0.5f);
        if (steps <= 127)
        {
            return (uint8_t)steps; // 0xxxxxxx
        }

        // Extended range: DTG[7:5] = 10x -> deadtime = (64 + DTG[5:0]) * 2 * step
        // Covers up to ~1400ns which comfortably includes our MAX.
        float dtval = ((float)ns / (2.0f * step_ns)) - 64.0f;
        if (dtval < 0) dtval = 0;
        if (dtval > 63) dtval = 63;
        return (uint8_t)(0x80 | (uint8_t)dtval); // 10xxxxxx
    }

    // ---- public API ---------------------------------------

    void init()
    {
        // Enable clocks: TIM1, GPIOA, GPIOB
        RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
        RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;

        // ---- GPIO alternate function config ----
        // PA8 -> AF1 (TIM1_CH1)
        GPIOA->MODER &= ~(3u << (8 * 2));
        GPIOA->MODER |=  (2u << (8 * 2));      // AF mode
        GPIOA->AFR[1] &= ~(0xFu << ((8 - 8) * 4));
        GPIOA->AFR[1] |=  (1u   << ((8 - 8) * 4)); // AF1
        GPIOA->OSPEEDR |= (3u << (8 * 2));     // very high speed

        // PB13 -> AF1 (TIM1_CH1N)
        GPIOB->MODER &= ~(3u << (13 * 2));
        GPIOB->MODER |=  (2u << (13 * 2));
        GPIOB->AFR[1] &= ~(0xFu << ((13 - 8) * 4));
        GPIOB->AFR[1] |=  (1u   << ((13 - 8) * 4));
        GPIOB->OSPEEDR |= (3u << (13 * 2));

        // PB12 -> AF1 (TIM1_BKIN)
        GPIOB->MODER &= ~(3u << (12 * 2));
        GPIOB->MODER |=  (2u << (12 * 2));
        GPIOB->AFR[1] &= ~(0xFu << ((12 - 8) * 4));
        GPIOB->AFR[1] |=  (1u   << ((12 - 8) * 4));
        // BKIN is active-LOW in our wiring; enable pull-up so a
        // disconnected/floating input reads as "no fault" only when
        // pulled high externally. We use BKP polarity below.
        GPIOB->PUPDR &= ~(3u << (12 * 2));
        GPIOB->PUPDR |=  (1u << (12 * 2));     // pull-up

        // ---- Timer base config ----
        TIM1->PSC = 0;                          // no prescale, full resolution
        s_arr = freqToArr(s_freqHz);
        TIM1->ARR = s_arr;
        TIM1->CCR1 = (s_arr + 1) / 2;           // 50% duty

        // ---- Output compare: CH1 PWM mode 1, enable CH1 + CH1N ----
        TIM1->CCMR1 &= ~TIM_CCMR1_OC1M;
        TIM1->CCMR1 |= (6u << TIM_CCMR1_OC1M_Pos);  // PWM mode 1
        TIM1->CCMR1 |= TIM_CCMR1_OC1PE;             // preload enable

        // Enable CH1 (CC1E) and complementary CH1N (CC1NE)
        TIM1->CCER |= TIM_CCER_CC1E | TIM_CCER_CC1NE;

        // ---- Polarity: INVERTED to compensate for the IRLB8721 ----
        // level-shift circuit (Board v2). Each PWM channel passes through
        // an IRLB8721 MOSFET (gate <- PA8/PB13, drain -> 5V via pull-up,
        // source -> GND) to shift 3.3V logic to 5V for the IXDN604. That
        // circuit is inverting: Nucleo LOW -> MOSFET off -> drain pulled to
        // 5V (HIGH); Nucleo HIGH -> MOSFET on -> drain pulled to GND (LOW).
        //
        // Dead-time is generated on the internal reference waveform BEFORE
        // the CCxP/CCxNP polarity bit is applied. During the dead-time gap
        // (both channels "inactive"), with CCxP=0 both Nucleo pins would go
        // LOW -> through the inverting MOSFET level-shift both IXDN604
        // inputs would go HIGH -> BOTH GATE DRIVERS ON during dead-time.
        // That is backwards and dangerous (shoot-through risk instead of
        // protection). Inverting CC1P + CC1NP here makes both Nucleo pins
        // go HIGH during the gap -> through the MOSFETs both IXDN604 inputs
        // go LOW -> both drivers OFF during dead-time, which is correct.
        // Outside the gap, the double-inversion (here + MOSFET) cancels out
        // and the complementary PWM relationship is preserved normally.
        //
        // See docs/BOARD_V2_DIRECT.md for the full level-shift circuit.
        TIM1->CCER |= (TIM_CCER_CC1P | TIM_CCER_CC1NP);

        // ---- Dead-time + break (BDTR) ----
        uint32_t bdtr = 0;
        bdtr |= deadNsToDtg(s_deadNs);          // DTG[7:0]
        bdtr |= TIM_BDTR_BKE;                   // break enable
        // BKP polarity: 0 = break active LOW. Our fault line is active LOW.
        bdtr &= ~TIM_BDTR_BKP;
        // AOE = 0: do NOT auto-recover after break (require manual clear).
        // OSSR/OSSI = 0: outputs disabled (Hi-Z via gate) when MOE=0.
        TIM1->BDTR = bdtr;

        // Generate update to load preload registers
        TIM1->EGR |= TIM_EGR_UG;

        // Enable counter (outputs still off until MOE set)
        TIM1->CR1 |= TIM_CR1_ARPE;              // ARR preload
        TIM1->CR1 |= TIM_CR1_CEN;               // counter enable

        s_enabled = false; // MOE not set yet — gates off
    }

    void setFrequency(uint32_t freqHz)
    {
        s_freqHz = freqHz;
        s_arr = freqToArr(freqHz);
        TIM1->ARR = s_arr;
        TIM1->CCR1 = (s_arr + 1) / 2;           // maintain 50% duty
    }

    uint32_t getFrequency()
    {
        return s_freqHz;
    }

    void setDeadTimeNs(uint16_t ns)
    {
        s_deadNs = ns;
        // Update only the DTG bits; preserve break config.
        uint32_t bdtr = TIM1->BDTR;
        bdtr &= ~TIM_BDTR_DTG;
        bdtr |= deadNsToDtg(ns);
        TIM1->BDTR = bdtr;
    }

    uint16_t getDeadTimeNs()
    {
        return s_deadNs;
    }

    void enableOutputs()
    {
        TIM1->BDTR |= TIM_BDTR_MOE;             // master output enable
        s_enabled = true;
    }

    void disableOutputs()
    {
        TIM1->BDTR &= ~TIM_BDTR_MOE;            // gates off
        s_enabled = false;
    }

    bool outputsEnabled()
    {
        return (TIM1->BDTR & TIM_BDTR_MOE) != 0;
    }

    bool breakTripped()
    {
        // Break sets the BIF flag in SR and clears MOE (when AOE=0).
        return (TIM1->SR & TIM_SR_BIF) != 0;
    }

    void clearBreak()
    {
        TIM1->SR &= ~TIM_SR_BIF;                // clear break flag
        // MOE must be re-set explicitly by calling enableOutputs()
        // after the caller confirms the fault condition is cleared.
    }
}
