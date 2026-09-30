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
    static Protection::FaultType s_lastFastFault = Protection::FAULT_NONE;

    // Poll timers
    static unsigned long tOcp = 0, tTemp = 0, tPll = 0, tDisp = 0, tStatus = 0;
    static unsigned long tRunStart = 0;

    // Long-press detection for encoder-mode cycling vs start/stop:
    // short press = start/stop/reset, we cycle modes only in IDLE
    // via a separate rule (see handleButton).

    // ---- forward decls ------------------------------------
    static void enterState(SystemState s);
    static void startup();
    static void shutdown(SystemState reason);
    static void handleButton();
    static void handleSerial();
    static void printSerialHelp();
    static void handleEncoder();
    static void applyEncoderDelta(int32_t delta);

    // ---- init ---------------------------------------------
    void init()
    {
        s_state = STATE_IDLE;
        s_encMode = ENC_MODE_FREQUENCY;
        Display::forceRedraw();
        printSerialHelp();
    }

    SystemState state()       { return s_state; }
    EncoderMode encoderMode() { return s_encMode; }
    Protection::FaultType lastFastFault() { return s_lastFastFault; }

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

        // Reset PLL state.
        // NOTE: clearBreak() deliberately does NOT happen here. BIF is a
        // latch, and the noisiest event in this whole sequence (contactor
        // pull-in) is still ahead of us. Clearing it now and arming 500ms
        // later meant a single transient during the charge window left BIF
        // set, and the first checkFast() in RUNNING reported OCP_HW even
        // though the hardware had long recovered. BIF is now cleared
        // immediately before enableOutputs(), below.
        PllControl::reset();

        // Arm the frequency-feedback capture only now (right before
        // enabling PWM) — was left paused since init() to avoid a
        // self-oscillating/floating HC14 input flooding PA0 at boot
        // with no coil running. See PllControl.cpp for details.
        PllControl::startCapture();

        // Energize contactor (checks mains, optional zero-cross)
        if (!MainsControl::energize())
        {
            Serial.println("Start failed: no mains");
            shutdown(STATE_FAULT_MAINS);
            return;
        }

        // Charge the DC bus
        delay(BUS_CHARGE_MS);

        // ---- Pre-arm gate ----
        // Read PB12 as a live GPIO rather than trusting the latched BIF
        // flag. A LOW means the OCP comparator is over threshold, the
        // board is unpowered, or the cable is off.
#if BKIN_ENABLED
        if (!PwmDrive::faultLineHealthy())
        {
            Serial.println("Start aborted: OCP fault line LOW "
                            "(over threshold, board unpowered, or cable off)");
            Serial.println("  -> check the OCP board's ARMED LED");
            s_lastFastFault = Protection::FAULT_OCP_HW;
            shutdown(STATE_FAULT_OCP);
            return;
        }
#else
        // OCP tripping disabled (BKIN_ENABLED false). Report the line
        // state for information, but never block the run on it.
        Serial.printf("OCP line: %s (tripping DISABLED — informational only)\n",
                      PwmDrive::faultLineHealthy() ? "HIGH/armed" : "LOW/over-threshold");
#endif

        // Discard any BIF latched during contactor pull-in and the bus
        // charge window, then arm. Harmless when BKE is never set.
        PwmDrive::clearBreak();

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

        // 1b. Disarm frequency-feedback capture — no need for it while
        // stopped, and keeps PA0 quiet (see startCapture() call site).
        PllControl::stopCapture();

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
            // OCP board v4 has no latch of its own — TIM1's BIF is the
            // latch, and we just cleared it. So if the line is STILL low,
            // the fault is genuinely present right now (or the board is
            // unpowered / the cable is off). Say so, otherwise the next
            // start attempt aborts at the pre-arm gate for no obvious
            // reason.
            if (!PwmDrive::faultLineHealthy())
            {
                Serial.println("  WARNING: OCP fault line still LOW — the fault is "
                                "live, not just latched.");
                Serial.println("  Check the ARMED LED, the board's 5V, and the PB12 cable.");
            }
            break;
        }
    }

    // ---- serial command handling --------------------------
    //  Bench control so a run can be triggered without a hand on the
    //  encoder — needed when both hands are on scope probes. Uses the
    //  same startup()/shutdown() paths as the button, so there is no
    //  second code path to keep in sync and no bypassed safety check.
    //
    //  Commands are newline-terminated. 'g' rather than 's' for start,
    //  so a stray character is less likely to energize the contactor.
    //
    //    g   go / start        (same as an encoder press from IDLE)
    //    x   stop
    //    c   clear a fault back to IDLE
    //    b   report PB12 live level + BIF latch (no state change)
    //    ?   report state
    static void printSerialHelp()
    {
        Serial.println("Commands: g=start  x=stop  c=clear fault  "
                        "b=break status  ?=state");
    }

    static const char* stateName(SystemState s)
    {
        switch (s)
        {
        case STATE_IDLE:         return "IDLE";
        case STATE_STARTING:     return "STARTING";
        case STATE_RUNNING:      return "RUNNING";
        case STATE_FAULT_OCP:    return "FAULT_OCP";
        case STATE_FAULT_TEMP:   return "FAULT_TEMP";
        case STATE_FAULT_MAINS:  return "FAULT_MAINS";
        case STATE_FAULT_MANUAL: return "STOPPED (manual)";
        default:                 return "?";
        }
    }

    static void reportBreak()
    {
        Serial.print("[break] PB12=");
        Serial.print(PwmDrive::faultLineHealthy() ? "HIGH (healthy)" : "LOW (fault)");
        Serial.print("  BIF=");
        Serial.println(PwmDrive::breakTripped() ? "LATCHED" : "clear");
    }

    static void handleSerial()
    {
        static char buf[12];
        static uint8_t idx = 0;

        while (Serial.available())
        {
            char c = (char)Serial.read();
            if (c != '\n' && c != '\r')
            {
                if (idx < sizeof(buf) - 1) buf[idx++] = c;
                continue;
            }

            buf[idx] = '\0';
            uint8_t len = idx;
            idx = 0;
            if (len == 0) continue;

            switch (buf[0])
            {
            case 'g':
                if (s_state == STATE_IDLE) startup();
                else Serial.printf("Ignored: state is %s (press c to clear)\n",
                                   stateName(s_state));
                break;

            case 'x':
                if (s_state == STATE_RUNNING || s_state == STATE_STARTING)
                    shutdown(STATE_FAULT_MANUAL);
                else
                    Serial.printf("Not running (state %s)\n", stateName(s_state));
                break;

            case 'c':
                if (s_state == STATE_IDLE || s_state == STATE_RUNNING ||
                    s_state == STATE_STARTING)
                {
                    Serial.printf("Nothing to clear (state %s)\n", stateName(s_state));
                }
                else
                {
                    PwmDrive::clearBreak();
                    PllControl::reset();
                    enterState(STATE_IDLE);
                    Serial.println("Fault cleared -> IDLE");
                    if (!PwmDrive::faultLineHealthy())
                        Serial.println("  WARNING: OCP fault line still LOW — "
                                        "the fault is live, not just latched");
                }
                break;

            case 'b':
                reportBreak();
                break;

            case '?':
                Serial.printf("State: %s   freq=%lu Hz   OCP thresh=%d\n",
                              stateName(s_state),
                              (unsigned long)PwmDrive::getFrequency(),
                              Protection::getOcpThreshold());
                reportBreak();
                break;

            default:
                Serial.println("? unknown cmd");
                printSerialHelp();
                break;
            }
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

#if LOOP_DIAG_ENABLED
        // ---- Timing diagnostic: isolate which block is slow ----
        unsigned long t0 = micros();
#endif

        // User input first
        handleButton();
        handleSerial();
        handleEncoder();

#if LOOP_DIAG_ENABLED
        unsigned long t1 = micros();
#endif

        // ---- Fast protection (only meaningful while running) ----
        if (s_state == STATE_RUNNING && (now - tOcp >= OCP_CHECK_MS))
        {
            tOcp = now;
            Protection::FaultType f = Protection::checkFast();
            if (f == Protection::FAULT_OCP_HW)
            {
                Serial.println("FAST FAULT: OCP_HW (BKIN/PB12 break tripped)");
                s_lastFastFault = Protection::FAULT_OCP_HW;
                shutdown(STATE_FAULT_OCP);
            }
            else if (f == Protection::FAULT_OCP_SW)
            {
                Serial.print("FAST FAULT: OCP_SW (PA1 raw=");
                Serial.print(Sensing::ocpRaw());
                Serial.print(", threshold=");
                Serial.print(Protection::getOcpThreshold());
                Serial.println(")");
                s_lastFastFault = Protection::FAULT_OCP_SW;
                shutdown(STATE_FAULT_OCP);
            }
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

#if LOOP_DIAG_ENABLED
        unsigned long t2 = micros();
#endif

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

#if LOOP_DIAG_ENABLED
        unsigned long t3 = micros();
#endif

        // ---- Display ----
        if (now - tDisp >= DISPLAY_MS)
        {
            tDisp = now;
            Display::render(s_state, s_encMode);
        }

        // ---- PLL bring-up status, once per second while running ----
        // One readable line rather than a scroll. Everything needed to
        // judge whether the PLL is tracking: what we command, what the
        // feedback measures, the error between them, lock state, detune,
        // the live OCP line, and heatsink temperature.
        if (s_state == STATE_RUNNING && (now - tStatus >= 1000))
        {
            tStatus = now;
            uint32_t cmd  = PwmDrive::getFrequency();
            uint32_t meas = PllControl::getMeasuredFreqHz();
            char tb[10], pb[10];
            dtostrf(Sensing::temperatureSmoothedC(), 4, 1, tb);
            float ph = PllControl::getPhasePct();
            if (ph < 0.0f) strcpy(pb, " --.-");
            else           dtostrf(ph, 5, 1, pb);

            char sb[10];
            float sp = PllControl::getPhaseSpreadPct();
            if (sp < 0.0f) strcpy(sb, " --");
            else           dtostrf(sp, 3, 0, sb);

            Serial.printf("[pll] cmd=%lu  edg/s=%lu  phase=%s%% sp=%s%%  "
                          "%s  det=%+ld  OCP=%s  T=%sC%s\n",
                          (unsigned long)cmd,
                          (unsigned long)PllControl::getEdgeRateHz(),
                          pb, sb,
                          meas == 0               ? "NO FEEDBACK" :
                          PllControl::isLocked()  ? "LOCK       " : "           ",
                          (long)PllControl::getDetuneHz(),
                          PwmDrive::faultLineHealthy() ? "ok " : "LOW",
                          tb,
                          PLL_CLOSED_LOOP ? "" : "  [open loop]");
        }

#if LOOP_DIAG_ENABLED
        unsigned long t4 = micros();

        // Report any block slower than 5ms — normal logic here should be
        // well under 1ms combined. See LOOP_DIAG_ENABLED in config.h.
        unsigned long dButtons = t1 - t0;
        unsigned long dProtect = t2 - t1;
        unsigned long dPll     = t3 - t2;
        unsigned long dDisplay = t4 - t3;
        if (dButtons > 5000 || dProtect > 5000 || dPll > 5000 || dDisplay > 5000)
        {
            Serial.print("[timing] btn/enc=");
            Serial.print(dButtons);
            Serial.print("us  protect=");
            Serial.print(dProtect);
            Serial.print("us  pll=");
            Serial.print(dPll);
            Serial.print("us  display=");
            Serial.print(dDisplay);
            Serial.println("us");
        }
#endif
    }
}
