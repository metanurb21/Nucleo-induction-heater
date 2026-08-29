// ============================================================
//  StateManager implementation.
// ============================================================

#include "StateManager.h"
#include "PwmDrive.h"
#include "PllControl.h"
#include "Protection.h"
#include "Sensing.h"
#include "MainsControl.h"
#include "Encoder.h"
#include "Display.h"
#include "config.h"

namespace StateManager
{
    static SystemState s_state = STATE_IDLE;
    static EncoderMode s_encMode = ENC_MODE_FREQUENCY;

    // Poll timers
    static unsigned long tOcp = 0, tTemp = 0, tPll = 0, tDisp = 0;
    static unsigned long tRunStart = 0;

    // Long-press detection for encoder-mode cycling vs start/stop:
    // short press = start/stop/reset, we cycle modes only in IDLE
    // via a separate rule (see handleButton).

    // ---- forward decls ------------------------------------
    static void enterState(SystemState s);
    static void startup();
    static void shutdown(SystemState reason);
    static void handleButton();
    static void handleEncoder();
    static void applyEncoderDelta(int32_t delta);

    // ---- init ---------------------------------------------
    void init()
    {
        s_state = STATE_IDLE;
        s_encMode = ENC_MODE_FREQUENCY;
        Display::forceRedraw();
    }

    SystemState state()       { return s_state; }
    EncoderMode encoderMode() { return s_encMode; }

    // ---- state entry --------------------------------------
    static void enterState(SystemState s)
    {
        s_state = s;
        Display::forceRedraw();

        // LED indication
        switch (s)
        {
        case STATE_RUNNING:
            digitalWrite(PIN_LED_STATUS, HIGH);
            digitalWrite(PIN_LED_FAULT, LOW);
            break;
        case STATE_IDLE:
            digitalWrite(PIN_LED_STATUS, LOW);
            digitalWrite(PIN_LED_FAULT, LOW);
            break;
        default: // any fault
            digitalWrite(PIN_LED_STATUS, LOW);
            digitalWrite(PIN_LED_FAULT, HIGH);
            break;
        }
    }

    // ---- startup sequence ---------------------------------
    static void startup()
    {
        if (s_state != STATE_IDLE) return;

        // Safety gate: temperature
        if (NTC_ENABLED && Sensing::temperatureSmoothedC() >= TEMP_SHUTDOWN_C)
        {
            Serial.println("Start blocked: too hot");
            return;
        }

        Serial.println("STARTING...");
        enterState(STATE_STARTING);

        // Ensure PWM is off before energizing
        PwmDrive::disableOutputs();

        // Reset PLL state and clear any latched break
        PllControl::reset();
        PwmDrive::clearBreak();

        // Energize contactor (checks mains, optional zero-cross)
        if (!MainsControl::energize())
        {
            Serial.println("Start failed: no mains");
            shutdown(STATE_FAULT_MAINS);
            return;
        }

        // Charge the DC bus
        delay(BUS_CHARGE_MS);

        // Start PWM at the default/start frequency, then enable outputs
        PwmDrive::setFrequency(PWM_FREQ_START_HZ);
        PwmDrive::enableOutputs();

        tRunStart = millis();
        enterState(STATE_RUNNING);
        Serial.println("RUNNING");
    }

    // ---- shutdown sequence --------------------------------
    static void shutdown(SystemState reason)
    {
        // 1. Kill PWM immediately (gates off)
        PwmDrive::disableOutputs();

        // 2. Drop the contactor (no zero-cross wait)
        MainsControl::deEnergize();

        // 3. Enter fault/stop state
        enterState(reason);

        Serial.print("SHUTDOWN: ");
        Serial.println(
            reason == STATE_FAULT_OCP    ? "OCP" :
            reason == STATE_FAULT_TEMP   ? "TEMP" :
            reason == STATE_FAULT_MAINS  ? "MAINS" :
            reason == STATE_FAULT_MANUAL ? "MANUAL" : "UNKNOWN");
    }

    // ---- button handling ----------------------------------
    static void handleButton()
    {
        if (!Encoder::buttonPressed()) return;

        switch (s_state)
        {
        case STATE_IDLE:
            startup();
            break;
        case STATE_RUNNING:
        case STATE_STARTING:
            shutdown(STATE_FAULT_MANUAL);
            break;
        default: // any fault -> clear back to idle
            PwmDrive::clearBreak();
            PllControl::reset();
            enterState(STATE_IDLE);
            Serial.println("Fault cleared -> IDLE");
            break;
        }
    }

    // ---- encoder handling ---------------------------------
    // Rotation adjusts the value for the current s_encMode.
    // (Mode cycling can be added via long-press or a second button
    //  during bring-up; for now the mode is fixed to FREQUENCY and
    //  can be changed in code or extended later.)
    static void handleEncoder()
    {
        int32_t d = Encoder::readDelta();
        if (d != 0) applyEncoderDelta(d);
    }

    static void applyEncoderDelta(int32_t delta)
    {
        switch (s_encMode)
        {
        case ENC_MODE_FREQUENCY:
        {
            // Manual frequency nudge (mainly useful pre-lock / setup)
            int32_t f = (int32_t)PwmDrive::getFrequency() + delta * 100;
            PwmDrive::setFrequency((uint32_t)f);
            break;
        }
        case ENC_MODE_DETUNE:
            PllControl::setDetuneHz(PllControl::getDetuneHz() + delta * DETUNE_STEP_HZ);
            break;
        case ENC_MODE_DEADTIME:
            PwmDrive::setDeadTimeNs(PwmDrive::getDeadTimeNs() + delta * 10);
            break;
        case ENC_MODE_OCP:
            Protection::setOcpThreshold(Protection::getOcpThreshold() + delta * OCP_THRESH_STEP);
            break;
        default: break;
        }
    }

    // ---- main update --------------------------------------
    void update()
    {
        unsigned long now = millis();

        // User input first
        handleButton();
        handleEncoder();

        // ---- Fast protection (only meaningful while running) ----
        if (s_state == STATE_RUNNING && (now - tOcp >= OCP_CHECK_MS))
        {
            tOcp = now;
            Protection::FaultType f = Protection::checkFast();
            if (f == Protection::FAULT_OCP_HW)      shutdown(STATE_FAULT_OCP);
            else if (f == Protection::FAULT_OCP_SW) shutdown(STATE_FAULT_OCP);
        }
        else if (s_state == STATE_IDLE && (now - tOcp >= 100))
        {
            tOcp = now;
            Sensing::updateOcp(); // keep display value fresh
        }

        // ---- Slow protection: temperature ----
        if (now - tTemp >= TEMP_CHECK_MS)
        {
            tTemp = now;
            Protection::FaultType f = Protection::checkSlow();
            if (s_state == STATE_RUNNING && f == Protection::FAULT_TEMP)
            {
                shutdown(STATE_FAULT_TEMP);
            }
            else if (s_state != STATE_RUNNING)
            {
                Sensing::updateTemperature(); // keep fresh for display
            }
        }

        // ---- PLL tracking while running ----
        if (now - tPll >= PLL_UPDATE_MS)
        {
            tPll = now;
            PllControl::update(s_state == STATE_RUNNING);

            // Mains loss detection during run
            if (s_state == STATE_RUNNING && !Sensing::mainsPresent())
            {
                shutdown(STATE_FAULT_MAINS);
            }
        }

        // ---- Display ----
        if (now - tDisp >= DISPLAY_MS)
        {
            tDisp = now;
            Display::render(s_state, s_encMode);
        }
    }
}
