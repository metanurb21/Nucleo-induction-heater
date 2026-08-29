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

    // Software SPI constructor: (CS, DC, MOSI, SCK, RST)
    static Adafruit_ST7735 tft(PIN_TFT_CS, PIN_TFT_DC,
                               PIN_TFT_MOSI, PIN_TFT_SCK, PIN_TFT_RST);

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
        tft.setCursor(4, 92);
        bool mains = Sensing::mainsPresent();
        tft.setTextColor(mains ? COL_GOOD : COL_DANGER, COL_BG);
        tft.print(mains ? "MAINS: OK   " : "MAINS: NONE ");

        // Encoder mode hint
        tft.setTextColor(COL_ACCENT, COL_BG);
        tft.setCursor(4, 112);
        tft.printf("Adj: %-8s ", encModeName(encMode));

        // Version
        tft.setTextColor(COL_DIM, COL_BG);
        tft.setCursor(4, 148);
        tft.print("IH-PLL v0.1");
    }

    static void drawRunning(EncoderMode encMode)
    {
        drawStatusBar(PllControl::isLocked() ? "RUNNING [LOCK]" : "RUNNING", COL_GOOD);

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

        // Current (OCP ADC)
        tft.setTextColor(COL_TEXT, COL_BG);
        tft.setCursor(4, 78);
        tft.printf("I: %4d/%4d ",
                   (int)Sensing::ocpSmoothed(), Protection::getOcpThreshold());
        drawBar(4, 90, 120, 10, (int)Sensing::ocpSmoothed(), Protection::getOcpThreshold());

        // Detune indicator
        tft.setTextColor(COL_DIM, COL_BG);
        tft.setCursor(4, 108);
        tft.printf("Detune: %ld Hz    ", (long)PllControl::getDetuneHz());

        // Stop hint + encoder mode
        tft.setTextColor(COL_ACCENT, COL_BG);
        tft.setCursor(4, 124);
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
            tft.print("OVERCURRENT");
            tft.setCursor(4, 50);
            tft.printf("ADC: %d", Sensing::ocpRaw());
            tft.setCursor(4, 64);
            tft.printf("Threshold: %d", Protection::getOcpThreshold());
            break;
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
