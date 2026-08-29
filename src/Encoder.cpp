// ============================================================
//  Encoder implementation — TIM3 encoder mode + polled button.
//
//  TIM3_CH1 (PB4) and TIM3_CH2 (PB5) as encoder inputs.
//  The timer counts up/down automatically with quadrature.
//  We read CNT deltas; the hardware handles all edge decoding.
//
//  NOTE: register/AF details need AD3/scope validation during
//  bring-up. PB4 is also SWO/JTAG remnant — should be fine as
//  TIM3_CH1 (AF2) but verify it isn't held by debug config.
// ============================================================

#include "Encoder.h"
#include "config.h"
#include "stm32f4xx.h"

namespace Encoder
{
    static uint16_t s_lastCnt = 0;
    static bool s_lastBtn = HIGH;
    static unsigned long s_lastBtnTime = 0;

    // Encoders often produce 4 counts per detent; divide to get
    // one logical "click" per detent. Adjust if your EC11 differs.
    static const int32_t COUNTS_PER_DETENT = 4;
    static int32_t s_residual = 0;

    void init()
    {
        RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
        RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

        // PB4 -> AF2 (TIM3_CH1), PB5 -> AF2 (TIM3_CH2)
        GPIOB->MODER &= ~((3u << (4 * 2)) | (3u << (5 * 2)));
        GPIOB->MODER |=  ((2u << (4 * 2)) | (2u << (5 * 2)));  // AF mode
        GPIOB->AFR[0] &= ~((0xFu << (4 * 4)) | (0xFu << (5 * 4)));
        GPIOB->AFR[0] |=  ((2u   << (4 * 4)) | (2u   << (5 * 4))); // AF2
        // Pull-ups for open encoder contacts
        GPIOB->PUPDR &= ~((3u << (4 * 2)) | (3u << (5 * 2)));
        GPIOB->PUPDR |=  ((1u << (4 * 2)) | (1u << (5 * 2)));

        // TIM3 encoder mode 3 (count on both TI1 and TI2 edges)
        TIM3->SMCR &= ~TIM_SMCR_SMS;
        TIM3->SMCR |= (3u << TIM_SMCR_SMS_Pos);   // encoder mode 3

        // CC1/CC2 as inputs mapped to TI1/TI2
        TIM3->CCMR1 &= ~(TIM_CCMR1_CC1S | TIM_CCMR1_CC2S);
        TIM3->CCMR1 |= (1u << TIM_CCMR1_CC1S_Pos) | (1u << TIM_CCMR1_CC2S_Pos);
        // Light input filtering to debounce mechanical encoder
        TIM3->CCMR1 |= (0xFu << TIM_CCMR1_IC1F_Pos) | (0xFu << TIM_CCMR1_IC2F_Pos);

        // Non-inverted
        TIM3->CCER &= ~(TIM_CCER_CC1P | TIM_CCER_CC2P);
        TIM3->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E;

        TIM3->ARR = 0xFFFF;
        TIM3->CNT = 0;
        TIM3->CR1 |= TIM_CR1_CEN;

        s_lastCnt = (uint16_t)TIM3->CNT;

        // Button
        pinMode(PIN_ENC_BTN, INPUT_PULLUP);
        s_lastBtn = digitalRead(PIN_ENC_BTN);
    }

    int32_t readDelta()
    {
        uint16_t cnt = (uint16_t)TIM3->CNT;
        int16_t raw = (int16_t)(cnt - s_lastCnt);   // signed wrap-safe
        s_lastCnt = cnt;

        // Accumulate and reduce by counts-per-detent so callers get
        // clean +1 / -1 per physical click.
        s_residual += raw;
        int32_t detents = s_residual / COUNTS_PER_DETENT;
        s_residual -= detents * COUNTS_PER_DETENT;
        return detents;
    }

    bool buttonPressed()
    {
        bool now = digitalRead(PIN_ENC_BTN);
        unsigned long t = millis();
        bool pressed = false;

        if (now == LOW && s_lastBtn == HIGH &&
            (t - s_lastBtnTime > BTN_DEBOUNCE_MS))
        {
            pressed = true;
            s_lastBtnTime = t;
        }
        s_lastBtn = now;
        return pressed;
    }
}
