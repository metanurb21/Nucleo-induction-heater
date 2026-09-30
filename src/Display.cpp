// ============================================================
//  Display implementation — ST7735 via Adafruit GFX.
//
//  Pulls live values from the other modules directly (Sensing,
//  PwmDrive, PllControl, Protection) so main.cpp stays thin.
//
//  Software SPI is used to avoid HW SPI bus conflicts (same
//  approach as the ESP32 v2.1 board). Swap to hardware SPI later
//  if refresh rate needs it.
// ============================================================

#include "Display.h"
#include "Sensing.h"
#include "PwmDrive.h"
#include "PllControl.h"
#include "Protection.h"
#include "StateManager.h"
#include "config.h"

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

namespace Display
{
    // Colors
    static const uint16_t COL_BG     = ST77XX_BLACK;
    static const uint16_t COL_TEXT   = ST77XX_WHITE;
    static const uint16_t COL_DIM    = 0x7BEF;
    static const uint16_t COL_GOOD   = ST77XX_GREEN;
    static const uint16_t COL_WARN   = ST77XX_YELLOW;
    static const uint16_t COL_DANGER = ST77XX_RED;
    static const uint16_t COL_ACCENT = ST77XX_CYAN;
    static const uint16_t COL_BAR_BG = 0x2104;

    // HARDWARE SPI constructor: (CS, DC, RST) — uses SPI1.
    //
    // PIN_TFT_SCK (PA5) and PIN_TFT_MOSI (PA7) ARE the Nucleo's SPI1
    // SCK and MOSI pins, so this needs no rewiring at all — the same
    // physical wires are now driven by the SPI peripheral instead of
    // being bit-banged.
    //
    // Why this changed: software SPI measured 548 ms per render (see the
    // [timing] diagnostic), which dropped the main loop to ~1.8 Hz. That
    // starved Protection::checkFast() — meant to run every 1 ms, it was
    // running twice a second — and generated tens of thousands of fast
    // GPIO edges every 250 ms, a plausible noise source in its own right.
    // Hardware SPI should bring a render under ~25 ms.
    //
    // MISO (PA6) is unused; the ST7735 is write-only here.
    //
    // TO REVERT to software SPI, restore:
    //   static Adafruit_ST7735 tft(PIN_TFT_CS, PIN_TFT_DC,
    //                              PIN_TFT_MOSI, PIN_TFT_SCK, PIN_TFT_RST);
    // and verify with: pio run -e tft_hello -t upload
    static Adafruit_ST7735 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

    static SystemState s_lastState = (SystemState)255;
    static bool s_forceRedraw = true;

    // ---- helpers ------------------------------------------
    static uint16_t tempColor(float t)
    {
        if (t >= TEMP_SHUTDOWN_C) return COL_DANGER;
        if (t >= TEMP_WARNING_C)  return COL_WARN;
        return COL_GOOD;
    }

    static const char* encModeName(EncoderMode m)
    {
        switch (m)
        {
        case ENC_MODE_FREQUENCY: return "FREQ";
        case ENC_MODE_DETUNE:    return "DETUNE";
        case ENC_MODE_DEADTIME:  return "DEADTIME";
        case ENC_MODE_OCP:       return "OCP";
        default:                 return "?";
        }
    }

    static void drawStatusBar(const char* label, uint16_t color)
    {
        tft.fillRect(0, 0, 128, 20, color);
        tft.setTextColor(COL_BG);
        tft.setTextSize(1);
        tft.setCursor(4, 6);
        tft.print(label);
    }

    static void drawBar(int x, int y, int w, int h, int val, int maxVal)
    {
        if (maxVal <= 0) maxVal = 1;
        int fill = map(constrain(val, 0, maxVal), 0, maxVal, 0, w);
        uint16_t c = COL_GOOD;
        if (fill > w * 3 / 4) c = COL_DANGER;
        else if (fill > w / 2) c = COL_WARN;
        tft.fillRect(x, y, fill, h, c);
        tft.fillRect(x + fill, y, w - fill, h, COL_BAR_BG);
        tft.drawRect(x - 1, y - 1, w + 2, h + 2, COL_DIM);
    }

    // ---- init ---------------------------------------------
    void init()
    {
        pinMode(PIN_TFT_BLK, OUTPUT);
        digitalWrite(PIN_TFT_BLK, HIGH);   // backlight on

        tft.initR(INITR_BLACKTAB);
        tft.setRotation(0);
        tft.fillScreen(COL_BG);
    }

    void splash()
    {
        tft.fillScreen(COL_BG);
        tft.setTextSize(2);
        tft.setTextColor(COL_ACCENT);
        tft.setCursor(2, 20);
        tft.print("Induction");
        tft.setCursor(2, 44);
        tft.print("Heater PLL");
        tft.setTextSize(1);
        tft.setTextColor(COL_DIM);
        tft.setCursor(2, 80);
        tft.print("STM32 F446RE");
        tft.setCursor(2, 92);
        tft.print("Digital PLL Control");
    }

    void forceRedraw() { s_forceRedraw = true; }

    // ---- screens ------------------------------------------
    static void drawIdle(EncoderMode encMode)
    {
        drawStatusBar("IDLE - press START", COL_ACCENT);

        // Temperature
        tft.setTextSize(2);
        float t = Sensing::temperatureSmoothedC();
        tft.setTextColor(NTC_ENABLED ? tempColor(t) : COL_DIM, COL_BG);
        tft.setCursor(4, 28);
        if (NTC_ENABLED)
        {
            char b[10]; dtostrf(t, 5, 1, b);
            tft.print(b); tft.print((char)247); tft.print("C ");
        }
        else tft.print(" --.-  ");

        // Frequency setting
        tft.setTextSize(1);
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.setCursor(4, 52);
        tft.printf("Freq: %5lu Hz   ", (unsigned long)PwmDrive::getFrequency());

        // Dead-time + detune
        tft.setCursor(4, 64);
        tft.printf("DT: %u ns  Det: %ld ",
                   PwmDrive::getDeadTimeNs(), (long)PllControl::getDetuneHz());

        // OCP threshold
        tft.setCursor(4, 76);
        tft.printf("OCP: %d   ", Protection::getOcpThreshold());

        // Mains status
        tft.setCursor(4, 88);
        bool mains = Sensing::mainsPresent();
        tft.setTextColor(mains ? COL_GOOD : COL_DANGER, COL_BG);
        tft.print(mains ? "MAINS: OK   " : "MAINS: NONE ");

        // DC bus level (raw ADC counts, PA4). Replaces the old "Tank:"
        // field — the PC2 tank divider is removed as of OCP board v3.
        // Your [loop] STALL diagnosis was right: 1.67MΩ into an
        // unbuffered ADC input was the cause. PA4 is already wired and
        // correctly referenced. Raw counts, not volts, until the bus
        // divider ratio is measured and added to config.h.
        tft.setTextColor(COL_DIM, COL_BG);
        tft.setCursor(4, 100);
        tft.printf("Bus: %4d      ", Sensing::readBusRaw());

        // Encoder mode hint
        tft.setTextColor(COL_ACCENT, COL_BG);
        tft.setCursor(4, 116);
        tft.printf("Adj: %-8s ", encModeName(encMode));

        // Version
        tft.setTextColor(COL_DIM, COL_BG);
        tft.setCursor(4, 148);
        tft.print("IH-PLL v0.1");
    }

    static void drawRunning(EncoderMode encMode)
    {
        drawStatusBar(PLL_CLOSED_LOOP
                        ? (PllControl::isLocked() ? "RUNNING [LOCK]" : "RUNNING")
                        : "RUNNING [OPEN]",
                      COL_GOOD);

        // Commanded frequency (big)
        tft.setTextSize(2);
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.setCursor(2, 26);
        char fb[8]; dtostrf(PwmDrive::getFrequency() / 1000.0f, 5, 1, fb);
        tft.print(fb);
        tft.setTextSize(1);
        tft.setTextColor(COL_ACCENT, COL_BG);
        tft.setCursor(70, 32);
        tft.print("kHz");

        // Measured resonance
        tft.setTextSize(1);
        tft.setTextColor(COL_DIM, COL_BG);
        tft.setCursor(4, 48);
        tft.printf("Res: %5lu Hz  ", (unsigned long)PllControl::getMeasuredFreqHz());

        // Temperature
        float t = Sensing::temperatureSmoothedC();
        tft.setTextColor(tempColor(t), COL_BG);
        tft.setCursor(4, 62);
        char tb[10]; dtostrf(t, 5, 1, tb);
        tft.printf("Temp:%s%cC ", tb, 247);

        // PHASE — the resonance error signal, and the number to record
        // during PLL characterisation. Replaces the old "I:" field and
        // bar graph, which read PA1 and therefore showed only noise
        // (no current sensor is fitted to that pin).
        //
        // Shown large because this is the reading being taken by hand at
        // the bench, with no USB and no serial monitor attached.
        float ph = PllControl::getPhasePct();
        char pb[10];
        if (ph < 0.0f) strcpy(pb, "--.-");
        else           dtostrf(ph, 4, 1, pb);

        tft.setTextColor(COL_DIM, COL_BG);
        tft.setCursor(4, 78);
        tft.print("Phase");

        tft.setTextSize(2);
        tft.setTextColor(ph < 0.0f ? COL_DANGER : COL_ACCENT, COL_BG);
        tft.setCursor(4, 90);
        tft.print(pb);
        tft.setTextSize(1);
        tft.setCursor(56, 96);
        tft.print("%   ");

        // Spread — the diagnostic that says whether the phase mean is
        // meaningful. Small spread = fixed relationship = usable error
        // signal. Approaching 100 = uniformly scattered = unusable.
        float sp = PllControl::getPhaseSpreadPct();
        char sb[10];
        if (sp < 0.0f) strcpy(sb, "--");
        else           dtostrf(sp, 3, 0, sb);
        tft.setTextColor(sp < 0.0f   ? COL_DIM :
                         sp < 10.0f  ? COL_GOOD :
                         sp < 40.0f  ? COL_WARN : COL_DANGER, COL_BG);
        tft.setCursor(76, 96);
        tft.printf("sp%s  ", sb);

        // Feedback edge rate. Should match the commanded frequency 1:1.
        tft.setTextColor(COL_DIM, COL_BG);
        tft.setCursor(4, 110);
        tft.printf("Edg/s:%6lu ", (unsigned long)PllControl::getEdgeRateHz());

        // Detune indicator
        tft.setCursor(4, 122);
        tft.printf("Detune: %ld Hz    ", (long)PllControl::getDetuneHz());

        // Stop hint + encoder mode
        tft.setTextColor(COL_ACCENT, COL_BG);
        tft.setCursor(4, 134);
        tft.printf("Adj: %-8s ", encModeName(encMode));
        tft.setTextColor(COL_DANGER, COL_BG);
        tft.setCursor(4, 148);
        tft.print("Press to STOP");
    }

    static void drawFault(SystemState state)
    {
        const char* title = "FAULT";
        uint16_t tcol = COL_DANGER;
        switch (state)
        {
        case STATE_FAULT_OCP:    title = "!! OCP !!";  break;
        case STATE_FAULT_TEMP:   title = "!! TEMP !!"; break;
        case STATE_FAULT_MAINS:  title = "NO MAINS";   break;
        case STATE_FAULT_MANUAL: title = "STOPPED"; tcol = COL_WARN; break;
        default: break;
        }
        drawStatusBar(title, tcol);

        tft.setTextSize(1);
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.setCursor(4, 30);

        switch (state)
        {
        case STATE_FAULT_OCP:
        {
            Protection::FaultType which = StateManager::lastFastFault();
            tft.print(which == Protection::FAULT_OCP_HW ? "OVERCURRENT (HW)" : "OVERCURRENT (SW)");
            tft.setCursor(4, 50);
            tft.printf("ADC: %d", Sensing::ocpRaw());
            tft.setCursor(4, 64);
            tft.printf("Threshold: %d", Protection::getOcpThreshold());
            tft.setCursor(4, 78);
            tft.setTextColor(COL_DIM, COL_BG);
            tft.print(which == Protection::FAULT_OCP_HW
                       ? "BKIN break (PB12)"
                       : "PA1 current sense");
            break;
        }
        case STATE_FAULT_TEMP:
            tft.print("OVER TEMPERATURE");
            tft.setCursor(4, 50);
            tft.printf("Temp: %.1f%cC", Sensing::temperatureSmoothedC(), 247);
            tft.setCursor(4, 64);
            tft.printf("Limit: %.0f%cC", TEMP_SHUTDOWN_C, 247);
            break;
        case STATE_FAULT_MAINS:
            tft.print("MAINS LOST");
            break;
        case STATE_FAULT_MANUAL:
            tft.print("Manual stop");
            tft.setCursor(4, 50);
            tft.setTextColor(COL_DIM, COL_BG);
            tft.print("Contactor dropped");
            break;
        default: break;
        }

        tft.setTextColor(COL_TEXT, COL_BG);
        tft.setCursor(4, 148);
        tft.print("Press to reset");
    }

    // ---- render dispatcher --------------------------------
    void render(SystemState state, EncoderMode encMode)
    {
        if (state != s_lastState || s_forceRedraw)
        {
            tft.fillScreen(COL_BG);
            s_lastState = state;
            s_forceRedraw = false;
        }

        if (state == STATE_IDLE)
            drawIdle(encMode);
        else if (state == STATE_RUNNING || state == STATE_STARTING)
            drawRunning(encMode);
        else
            drawFault(state);
    }
}
