// ============================================================
//  Induction Heater PLL Controller — STM32 Nucleo F446RE
//  Main orchestrator (thin — all logic lives in modules).
//
//  Modules:
//    config.h       — pins, thresholds, tunables (single source)
//    PwmDrive       — TIM1 complementary PWM + dead-time + BKIN
//    PllControl     — TIM2 capture, resonance tracking, detune
//    Protection     — OCP + temperature fault detection
//    Sensing        — ADC acquisition + conversion + smoothing
//    MainsControl   — contactor relay + zero-cross switching
//    Encoder        — TIM3 quadrature + button
//    Display        — ST7735 TFT UI
//    StateManager   — the brain: state machine + sequencing
//
//  See docs/FIRMWARE.md for architecture and the phased
//  bring-up plan. Scaffold — validate on the AD3 before
//  connecting any power stage.
//
//  SAFETY: Boots with contactor OFF and PWM outputs disabled.
//  Nothing switches until StateManager runs the guarded startup.
// ============================================================

#include <Arduino.h>
#include "config.h"
#include "PwmDrive.h"
#include "PllControl.h"
#include "Protection.h"
#include "Sensing.h"
#include "MainsControl.h"
#include "Encoder.h"
#include "Display.h"
#include "StateManager.h"

// ============================================================
//  PWM_TEST_MODE — bench verification of the gate signals.
//  Enable by building with -DPWM_TEST_MODE (see platformio.ini).
//
//  Bypasses the state machine, contactor, and all protections.
//  Just generates complementary PWM on PA8/PB13 so you can probe
//  frequency, complementary behavior, and dead-time on a scope.
//
//  SAFE ONLY WITH NO POWER STAGE CONNECTED. This drives the gate
//  signal pins directly with no interlocks.
//
//  Serial commands (115200):
//    f<hz>   set frequency, e.g. "f75000"
//    d<ns>   set dead-time, e.g. "d200"
//    e       enable outputs
//    x       disable outputs
//    ?       print current state
// ============================================================
#ifdef PWM_TEST_MODE

static void testPrintState()
{
    Serial.printf("PWM: freq=%lu Hz  deadtime=%u ns  outputs=%s\n",
                  (unsigned long)PwmDrive::getFrequency(),
                  PwmDrive::getDeadTimeNs(),
                  PwmDrive::outputsEnabled() ? "ENABLED" : "disabled");
}

void setup()
{
    Serial.begin(115200);
    delay(300);
    Serial.println("=====================================");
    Serial.println(" PWM TEST MODE — F446RE");
    Serial.printf( " Core: %lu MHz\n", SystemCoreClock / 1000000UL);
    Serial.println(" Probe PA8 (CH1) and PB13 (CH1N) on the AD3");
    Serial.println(" NO POWER STAGE — signal generation only");
    Serial.println("=====================================");
    Serial.println(" Commands: f<hz>  d<ns>  e(nable)  x(disable)  ?(status)");

    PwmDrive::init();                       // TIM1 up, outputs still off
    PwmDrive::setFrequency(PWM_FREQ_START_HZ);
    PwmDrive::setDeadTimeNs(DEADTIME_NS_DEFAULT);
    PwmDrive::enableOutputs();              // start driving immediately

    Serial.println("Outputs ENABLED at startup for probing.");
    testPrintState();
}

void loop()
{
    // Simple serial command parser for live tuning while scoping.
    static char buf[16];
    static uint8_t idx = 0;

    while (Serial.available())
    {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r')
        {
            buf[idx] = '\0';
            if (idx > 0)
            {
                switch (buf[0])
                {
                case 'f':
                    PwmDrive::setFrequency((uint32_t)atol(buf + 1));
                    testPrintState();
                    break;
                case 'd':
                    PwmDrive::setDeadTimeNs((uint16_t)atoi(buf + 1));
                    testPrintState();
                    break;
                case 'e':
                    PwmDrive::enableOutputs();
                    testPrintState();
                    break;
                case 'x':
                    PwmDrive::disableOutputs();
                    testPrintState();
                    break;
                case '?':
                    testPrintState();
                    break;
                default:
                    Serial.println("? unknown cmd");
                    break;
                }
            }
            idx = 0;
        }
        else if (idx < sizeof(buf) - 1)
        {
            buf[idx++] = c;
        }
    }
}

#elif defined(MANUAL_DRIVE_MODE)
// ============================================================
//  MANUAL_DRIVE_MODE — full real run, encoder-only frequency
//  control, NO PLL feedback armed. Hardware OCP (BKIN) is active
//  if the OCP board is connected; software OCP is not.
//
//  Enable by building with -DMANUAL_DRIVE_MODE (see platformio.ini).
//
//  This is a deliberate, temporary bring-up mode: get a real,
//  manually-tuned run on the actual bridge + coil, exactly like
//  the pre-DeCosta ESP32 "tune for maximum smoke" process, using
//  the same manual-instrument approach (IR temp gun, DC bus V/A,
//  LED-across-coil check) instead of automated feedback.
//
//  SAFETY — read before using:
//   - PllControl is NEVER started in this mode. PA0 stays quiet,
//     no coil-feedback signal reaches the Nucleo at all.
//   - PB12 (BKIN): hardware OCP is active IF the OCP board v3 is
//     connected (TIM1 kills the gates in ~6ns, with no software
//     involvement, so it works in this mode too). With no board
//     connected, v3's fail-safe wiring leaves PB12 low and startup
//     aborts at the pre-arm gate — by design.
//   - Software OCP (PA1 ADC) is disabled project-wide, see
//     SOFT_OCP_ENABLED in config.h. PA1 has no sensor.
//   - Note the limit of the hardware OCP: ~25-60us end to end, so it
//     catches progressive overcurrent, NOT a shoot-through short
//     (IGBT desat is ~10us). The variac ramp and the operator's own
//     judgement are still the primary protection layer in this mode.
//     Treat it that seriously.
//   - Over-temperature (NTC, 500ms poll, TEMP_SHUTDOWN_C) is the
//     ONLY automated protection active in this mode.
//   - Contactor/mains sequencing (energize, bus charge) is the
//     same safety-checked path as normal firmware.
//
//  Controls (encoder):
//    Rotate  -> adjust PWM frequency (coarse step, see below)
//    Press   -> START (from idle) / STOP (from running)
//
//  Frequency step is coarser than normal FREQUENCY mode (500Hz/
//  click instead of 100Hz/click) so sweeping the full 20-100kHz
//  range and dialing toward resonance doesn't take forever.
// ============================================================

static uint32_t s_manualFreq = PWM_FREQ_START_HZ;
static bool s_manualRunning = false;

static void manualPrintState()
{
    Serial.printf("MANUAL: freq=%lu Hz  running=%s  mains=%s\n",
                  (unsigned long)s_manualFreq,
                  s_manualRunning ? "YES" : "no",
                  MainsControl::isEnergized() ? "energized" : "off");
}

static void manualStart()
{
    if (s_manualRunning) return;

    if (NTC_ENABLED && Sensing::temperatureSmoothedC() >= TEMP_SHUTDOWN_C)
    {
        Serial.println("Start blocked: too hot");
        return;
    }

    PwmDrive::disableOutputs();

    if (!MainsControl::energize())
    {
        Serial.println("Start failed: no mains");
        return;
    }

    delay(BUS_CHARGE_MS);

    // Same pre-arm gate as normal firmware: check the live PB12 level,
    // then clear BIF only once, immediately before arming. clearBreak()
    // used to run before energize(), which left any transient latched
    // during contactor pull-in sitting in BIF.
    if (!PwmDrive::faultLineHealthy())
    {
        Serial.println("Start aborted: OCP fault line LOW "
                        "(latch tripped, board unpowered, or cable off)");
        MainsControl::deEnergize();
        return;
    }
    PwmDrive::clearBreak();

    PwmDrive::setFrequency(s_manualFreq);
    PwmDrive::enableOutputs();
    s_manualRunning = true;
    Serial.println("RUNNING (manual, no PLL)");
    manualPrintState();
}

static void manualStop()
{
    PwmDrive::disableOutputs();
    MainsControl::deEnergize();
    s_manualRunning = false;
    Serial.println("STOPPED");
    manualPrintState();
}

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println("=====================================");
    Serial.println(" MANUAL DRIVE MODE — F446RE");
    Serial.println(" NO PLL feedback armed. HW OCP via BKIN if wired.");
    Serial.println(" NTC over-temp is the only automated protection.");
    Serial.println(" Variac ramp + operator judgement = the safety layer.");
    Serial.println("=====================================");

    // ---- Safety-critical outputs FIRST ----
    pinMode(PIN_LED_STATUS, OUTPUT);
    pinMode(PIN_LED_FAULT, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);
    digitalWrite(PIN_LED_FAULT, LOW);

    MainsControl::init();   // contactor forced OFF
    PwmDrive::init();       // TIM1 configured, outputs DISABLED (MOE=0)

    // ---- Sensing (NTC only matters here; OCP/AC sense unused) ----
    Sensing::init();
    Protection::init();

    // NOTE: PllControl::init()/startCapture() are intentionally NOT
    // called in this mode. PA0 stays completely quiet.

    Encoder::init();
    Display::init();
    Display::splash();
    delay(SPLASH_MS);

    Serial.println("Ready. Idle. Rotate encoder to set freq, press to START.");
    manualPrintState();
}

void loop()
{
    static unsigned long tTemp = 0, tDisp = 0, tStatus = 0;
    unsigned long now = millis();

    // ---- Encoder: frequency adjust ----
    int32_t d = Encoder::readDelta();
    if (d != 0)
    {
        int32_t f = (int32_t)s_manualFreq + d * 500;   // coarse: 500Hz/click
        if (f < (int32_t)PWM_FREQ_MIN_HZ) f = PWM_FREQ_MIN_HZ;
        if (f > (int32_t)PWM_FREQ_MAX_HZ) f = PWM_FREQ_MAX_HZ;
        s_manualFreq = (uint32_t)f;
        if (s_manualRunning) PwmDrive::setFrequency(s_manualFreq);
        manualPrintState();
    }

    // ---- Encoder button: start/stop ----
    if (Encoder::buttonPressed())
    {
        if (s_manualRunning) manualStop();
        else                 manualStart();
    }

    // ---- Only automated protection in this mode: NTC over-temp ----
    if (now - tTemp >= TEMP_CHECK_MS)
    {
        tTemp = now;
        if (NTC_ENABLED)
        {
            Sensing::updateTemperature();
            if (s_manualRunning && Sensing::temperatureSmoothedC() >= TEMP_SHUTDOWN_C)
            {
                Serial.println("SHUTDOWN: OVER TEMP");
                manualStop();
            }
        }
    }

    // ---- Mains-loss check while running ----
    if (s_manualRunning && !Sensing::mainsPresent())
    {
        Serial.println("SHUTDOWN: MAINS LOST");
        manualStop();
    }

    // ---- Status LEDs ----
    digitalWrite(PIN_LED_STATUS, s_manualRunning ? HIGH : LOW);
    digitalWrite(PIN_LED_FAULT, LOW);

    // ---- Display ----
    if (now - tDisp >= DISPLAY_MS)
    {
        tDisp = now;
        // Reuse the normal fault/idle/running screens via SystemState
        // mapping: IDLE when stopped, RUNNING when running. No PLL
        // lock/detune data is meaningful here (PllControl isn't running).
        Display::render(s_manualRunning ? STATE_RUNNING : STATE_IDLE,
                         ENC_MODE_FREQUENCY);
    }

    // ---- Periodic serial status while running ----
    if (s_manualRunning && (now - tStatus >= 2000))
    {
        tStatus = now;
        manualPrintState();
    }
}

#elif defined(TFT_HELLO_MODE)
// ============================================================
//  TFT_HELLO_MODE — bare-minimum display sanity check.
//  Enable by building with -DTFT_HELLO_MODE (see platformio.ini).
//
//  Touches ONLY the TFT (software SPI) + its backlight pin.
//  No PWM, no sensing, no state machine, no other modules.
//  Prints progress over Serial (115200) at each step so a hang
//  can be localized precisely (backlight -> initR -> fillScreen
//  -> text) without any of the project's other code involved.
// ============================================================
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

static Adafruit_ST7735 tft(PIN_TFT_CS, PIN_TFT_DC,
                            PIN_TFT_MOSI, PIN_TFT_SCK, PIN_TFT_RST);

void setup()
{
    Serial.begin(115200);
    delay(300);
    Serial.println("=====================================");
    Serial.println(" TFT HELLO WORLD — bare minimum test");
    Serial.printf(" SCK=%d MOSI=%d CS=%d DC=%d RST=%d BLK=%d\n",
                  PIN_TFT_SCK, PIN_TFT_MOSI, PIN_TFT_CS,
                  PIN_TFT_DC, PIN_TFT_RST, PIN_TFT_BLK);
    Serial.println("=====================================");

    Serial.println("Step 1: backlight pin HIGH...");
    pinMode(PIN_TFT_BLK, OUTPUT);
    digitalWrite(PIN_TFT_BLK, HIGH);
    Serial.println("  done.");

    Serial.println("Step 2: tft.initR() [software SPI handshake]...");
    tft.initR(INITR_BLACKTAB);
    Serial.println("  done. (if you see this, SPI init did NOT hang)");

    Serial.println("Step 3: setRotation + fillScreen...");
    tft.setRotation(0);
    tft.fillScreen(ST77XX_BLACK);
    Serial.println("  done.");

    Serial.println("Step 4: draw text...");
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_GREEN);
    tft.setCursor(4, 40);
    tft.print("Hello");
    tft.setCursor(4, 64);
    tft.print("World!");
    Serial.println("  done.");

    Serial.println("ALL STEPS COMPLETE. TFT should show green text.");
}

void loop()
{
    // Nothing to do — static screen.
}

#elif defined(PIN_CHECK_MODE)
// ============================================================
//  PIN_CHECK_MODE — Nucleo pin integrity / ADC leakage check.
//  Enable by building with -DPIN_CHECK_MODE (see platformio.ini).
//
//  Purpose: decide whether any Nucleo pin was damaged during the
//  OCP bring-up, before wiring the new OCP board v3 in. Touches
//  NOTHING except the pins under test. No contactor, no PWM
//  outputs, no state machine, no display.
//
//  Why leakage and not "does it read something": injection damage
//  on an STM32 pin shows up as elevated input leakage current, not
//  as a dead pin. A damaged ADC input still reads — it just reads
//  wrong, and the error scales with the source impedance. So we
//  measure the pin twice through two known impedances and solve
//  for the leakage.
//
//    V_soft  = V_mid + I_leak * 50k    (100k || 100k)
//    V_stiff = V_mid + I_leak * 500R   (1k   || 1k)
//    I_leak  = (V_soft - V_stiff) / 49.5k
//
//  Taking the DIFFERENCE cancels V_mid, so the absolute accuracy of
//  the 3.3V rail drops out of the answer entirely.
//
//  SUSPECTS (see docs/OCP_BOARD_Final.md and BOARD_V2_DIRECT.md:320):
//    PA1 — prime suspect. A 74HC14 output was miswired here while
//          checkFast() was analogRead()ing it every 1ms. PA1 is
//          5V-tolerant as a DIGITAL pin, but that tolerance does
//          not hold in analog mode: the ADC analog switch is only
//          rated to VDDA, so a 5V drive clamps through the
//          protection diode. Check the HC14's supply rail first —
//          if it ran on 3.3V there was never an overstress.
//    PC2 — secondary. Non-isolated tank tap, but the 10M top leg
//          limits DC injection to tens of uA, well inside the
//          +/-5mA spec. Mechanism would be dV/dt coupling through
//          the 10M's parasitic capacitance, not the divider itself.
//    PB12 — low risk. Only ever saw 3.3V through 3k (or the earlier
//          10k placeholder), current-limited throughout, with the
//          PC817 isolating everything upstream.
//
//  WHAT YOU NEED
//    - 2x 100k resistors (matched — measure a few with a DMM and
//      pick the closest pair; see the tolerance note below)
//    - 2x 1k resistors (likewise)
//    - 1x 1k resistor for the digital drive test
//    - a flying lead from each divider midpoint
//    - nothing else. No HV, no OCP board, no power stage.
//
//  IMPORTANT: PA4/PC0/PC1 have real sensor circuits on them (bus
//  divider, NTC divider, AC sense). Unplug those JST leads before
//  testing those channels or their own circuits will load the test
//  divider and the reading will be meaningless. PA1 and PC2 should
//  already be disconnected.
//
//  Serial commands (115200):
//    l        list pins and which readings have been taken
//    p<n>     select pin n as the pin under test
//    s        record SOFT reading  (100k/100k divider on the pin)
//    f        record STIFF reading (1k/1k divider on the pin)
//    t        print the results table + verdicts
//    d        digital pull-up/pull-down test on PB12
//    o        PB12 output drive test (needs 1k to a rail)
//    b        BKIN break-latch test (does NOT enable gate outputs)
//    c        clear a latched break
//    v        re-measure VDDA from VREFINT
//    x        discard all recorded readings
//    h        help
// ============================================================

#include "stm32f4xx.h"
#include "PwmDrive.h"

namespace
{
    struct TestPin
    {
        uint32_t    pin;
        const char* name;
        const char* role;
    };

    const TestPin kPins[] = {
        { PA1, "PA1", "ADC_OCP   PRIME SUSPECT (5V HC14 miswire in analog mode)" },
        { PC2, "PC2", "ex-TANKV  suspect (non-isolated tank tap)" },
        { PA4, "PA4", "ADC_VBUS  control  (unplug sensor lead first)" },
        { PC0, "PC0", "ADC_NTC   control  (unplug sensor lead first)" },
        { PC1, "PC1", "ADC_AC    control  (unplug sensor lead first)" },
    };
    const int kNumPins = (int)(sizeof(kPins) / sizeof(kPins[0]));

    // Parallel resistance each divider presents at the pin.
    const float R_SOFT  = 50000.0f;   // 100k || 100k
    const float R_STIFF = 500.0f;     // 1k   || 1k

    // Leakage verdict thresholds (uA). The PASS band is set by the
    // resistor tolerance, not by the silicon: two 5% resistors can
    // mismatch a divider midpoint by ~2.5% (~41mV), which masquerades
    // as ~0.8uA. With 1% parts you can trust down to ~0.2uA.
    const float LEAK_PASS_UA    = 2.0f;
    const float LEAK_SUSPECT_UA = 10.0f;

    const float NOT_TAKEN = -999.0f;

    float g_soft[kNumPins];
    float g_stiff[kNumPins];
    int   g_sel  = 0;
    float g_vdda = 3.30f;

    // ---- float formatting helper ------------------------------
    // This core links with --specs=nano.specs and without
    // -u _printf_float, so "%f" in printf() produces garbage. The rest
    // of the project works around this with dtostrf() (see Display.cpp);
    // same approach here. Rotating buffers so several fmt() calls can
    // appear in one printf() argument list safely — each gets its own
    // buffer, so evaluation order doesn't matter.
    const char* fmt(float v, int width, int prec)
    {
        static char bufs[6][20];
        static uint8_t which = 0;
        char* b = bufs[which++ % 6];
        dtostrf(v, width, prec, b);
        return b;
    }

    // ---- heavily averaged read -------------------------------
    // The 32 discarded reads matter: at 50k source impedance the
    // ADC sample-and-hold cannot settle in one default-length
    // sampling window. Reading the same channel repeatedly leaves
    // the sample cap already near the node voltage, so the later
    // conversions are valid. This is exactly the settling problem
    // that made the old 1.67M PC2 divider corrupt its neighbours.
    float readAvgVolts(uint32_t pin)
    {
        for (int i = 0; i < 32; i++) (void)analogRead(pin);

        uint32_t sum = 0;
        for (int i = 0; i < 256; i++) sum += analogRead(pin);

        return ((float)sum / 256.0f) * g_vdda / 4095.0f;
    }

    void measureVdda()
    {
#ifdef AVREF
        // VREFINT is a bandgap reference, nominally 1.21V, independent
        // of VDDA. Reading it backwards gives us VDDA.
        for (int i = 0; i < 8; i++) (void)analogRead(AVREF);
        uint32_t sum = 0;
        for (int i = 0; i < 64; i++) sum += analogRead(AVREF);
        float raw = (float)sum / 64.0f;
        if (raw > 100.0f)
        {
            g_vdda = 1.21f * 4095.0f / raw;
            Serial.printf("VDDA measured from VREFINT: %s V (raw %d)\n",
                          fmt(g_vdda, 5, 3), (int)raw);
            if (g_vdda < 3.15f || g_vdda > 3.45f)
                Serial.println("  ** VDDA out of the expected 3.15-3.45V band — "
                                "investigate the rail before trusting any ADC result");
        }
        else
        {
            Serial.println("VREFINT read failed; assuming VDDA = 3.30 V");
        }
#else
        Serial.println("VREFINT not available in this core; assuming VDDA = 3.30 V");
#endif
    }

    void printHelp()
    {
        Serial.println("--- commands ---------------------------------------");
        Serial.println(" l     list pins / recorded readings");
        Serial.println(" p<n>  select pin under test (e.g. p0)");
        Serial.println(" s     record SOFT reading  (100k/100k on the pin)");
        Serial.println(" f     record STIFF reading (1k/1k on the pin)");
        Serial.println(" t     results table + verdicts");
        Serial.println(" d     PB12 pull-up/pull-down test");
        Serial.println(" o     PB12 output drive test (needs 1k to a rail)");
        Serial.println(" b     BKIN break-latch test (gates stay OFF)");
        Serial.println(" c     clear latched break");
        Serial.println(" v     re-measure VDDA");
        Serial.println(" x     discard all readings");
        Serial.println(" h     this help");
        Serial.println("----------------------------------------------------");
    }

    void listPins()
    {
        Serial.println("idx  pin   role                                                  soft    stiff");
        for (int i = 0; i < kNumPins; i++)
        {
            Serial.printf(" %s%d   %-4s  %-52s ",
                          (i == g_sel) ? ">" : " ", i, kPins[i].name, kPins[i].role);
            if (g_soft[i] == NOT_TAKEN) Serial.print("  --   ");
            else                        Serial.printf("%s ", fmt(g_soft[i], 6, 4));
            if (g_stiff[i] == NOT_TAKEN) Serial.println("   --");
            else                         Serial.printf("  %s\n", fmt(g_stiff[i], 6, 4));
        }
        Serial.printf("selected: %s\n", kPins[g_sel].name);
    }

    void recordSoft()
    {
        Serial.printf("Reading %s through the SOFT (100k/100k) divider...\n",
                      kPins[g_sel].name);
        g_soft[g_sel] = readAvgVolts(kPins[g_sel].pin);
        Serial.printf("  %s soft  = %s V\n",
                      kPins[g_sel].name, fmt(g_soft[g_sel], 6, 4));
        if (g_stiff[g_sel] == NOT_TAKEN)
            Serial.println("  now move the STIFF (1k/1k) divider onto the same pin and press 'f'");
        else
            Serial.println("  both readings present — press 't' for the verdict");
    }

    void recordStiff()
    {
        Serial.printf("Reading %s through the STIFF (1k/1k) divider...\n",
                      kPins[g_sel].name);
        g_stiff[g_sel] = readAvgVolts(kPins[g_sel].pin);
        Serial.printf("  %s stiff = %s V\n",
                      kPins[g_sel].name, fmt(g_stiff[g_sel], 6, 4));
        if (g_soft[g_sel] == NOT_TAKEN)
            Serial.println("  now move the SOFT (100k/100k) divider onto the same pin and press 's'");
        else
            Serial.println("  both readings present — press 't' for the verdict");
    }

    void printTable()
    {
        Serial.println();
        Serial.println("=========== ADC PIN LEAKAGE RESULTS ===========");
        Serial.printf("VDDA = %s V   I_leak = (V_soft - V_stiff) / 49.5k\n\n",
                      fmt(g_vdda, 5, 3));
        Serial.println("pin    V_soft   V_stiff   delta      I_leak    verdict");

        bool any = false;
        for (int i = 0; i < kNumPins; i++)
        {
            if (g_soft[i] == NOT_TAKEN || g_stiff[i] == NOT_TAKEN) continue;
            any = true;

            float dv   = g_soft[i] - g_stiff[i];
            float leak = dv / (R_SOFT - R_STIFF) * 1.0e6f;   // uA
            float mag  = fabsf(leak);

            const char* verdict = (mag < LEAK_PASS_UA)    ? "PASS"
                                : (mag < LEAK_SUSPECT_UA) ? "SUSPECT"
                                                           : "FAIL";

            Serial.printf("%-4s  %s  %s  %s  %suA  %s\n",
                          kPins[i].name,
                          fmt(g_soft[i],  7, 4),
                          fmt(g_stiff[i], 7, 4),
                          fmt(dv,         8, 4),
                          fmt(leak,       8, 2),
                          verdict);
        }

        if (!any)
        {
            Serial.println("(no pin has both readings yet)");
            Serial.println("===============================================");
            return;
        }

        Serial.println();
        Serial.println("Reading the result:");
        Serial.printf("  PASS    < %suA  — tolerance-limited; the pin is fine\n",
                      fmt(LEAK_PASS_UA, 3, 1));
        Serial.printf("  SUSPECT < %suA  — re-test with 1%% resistors before believing it\n",
                      fmt(LEAK_SUSPECT_UA, 3, 1));
        Serial.println("  FAIL            — real leakage, the pin is degraded");
        Serial.println();
        Serial.println("Compare suspects against the control pins on this same chip —");
        Serial.println("that is what makes the number trustworthy. A single pin sitting");
        Serial.println("well outside its neighbours is the signal you are looking for.");
        Serial.println("===============================================");
    }

    void digitalTest()
    {
        Serial.println();
        Serial.println("--- PB12 pull-up / pull-down test ---");
        Serial.println("Disconnect anything on PB12 for this test.");

        pinMode(PB12, INPUT_PULLUP);
        delay(5);
        int up = digitalRead(PB12);

        pinMode(PB12, INPUT_PULLDOWN);
        delay(5);
        int dn = digitalRead(PB12);

        pinMode(PB12, INPUT);

        Serial.printf("  INPUT_PULLUP   -> %d  (expect 1)\n", up);
        Serial.printf("  INPUT_PULLDOWN -> %d  (expect 0)\n", dn);

        if (up == 1 && dn == 0)
            Serial.println("  PASS — the pad follows both internal pulls.");
        else if (up == dn)
            Serial.println("  FAIL — pin is stuck; it ignores the internal pulls. "
                            "Either damaged or still externally driven.");
        else
            Serial.println("  FAIL — inverted result, which should be impossible. "
                            "Check for an external connection on PB12.");
    }

    void outputDriveTest()
    {
        Serial.println();
        Serial.println("--- PB12 output drive test ---");
        Serial.println("Run this TWICE:");
        Serial.println("  1) with a 1k from PB12 to GND   (tests the high-side source)");
        Serial.println("  2) with a 1k from PB12 to 3.3V  (tests the low-side sink)");
        Serial.println("A healthy pin drives 3.3mA and still reads back correctly.");
        Serial.println("Make sure the OCP board is NOT connected — this drives the pin.");

        pinMode(PB12, OUTPUT);

        digitalWrite(PB12, HIGH);
        delay(5);
        int hi = (GPIOB->IDR & (1u << 12)) ? 1 : 0;

        digitalWrite(PB12, LOW);
        delay(5);
        int lo = (GPIOB->IDR & (1u << 12)) ? 1 : 0;

        pinMode(PB12, INPUT);

        Serial.printf("  drive HIGH -> pad reads %d  (expect 1)\n", hi);
        Serial.printf("  drive LOW  -> pad reads %d  (expect 0)\n", lo);

        if (hi == 1 && lo == 0)
            Serial.println("  PASS — pin sources and sinks against 1k.");
        else
            Serial.println("  FAIL — the pad does not follow its own output. "
                            "Weak or damaged driver (or the load is too heavy).");
    }

    bool g_breakArmed = false;

    void breakTest()
    {
        Serial.println();
        Serial.println("--- BKIN break-latch test ---");
        Serial.println("This configures TIM1 with the break input enabled but NEVER");
        Serial.println("sets MOE, so PA8/PB13 are not driven and no gate signals are");
        Serial.println("produced. BIF latches on a BKIN event regardless of MOE, so");
        Serial.println("the break path is fully testable with the gates off.");
        Serial.println("Disconnect the power stage anyway — with MOE=0 and OSSI=0 the");
        Serial.println("outputs are released to Hi-Z, which gate drivers dislike.");
        Serial.println();

        PwmDrive::init();       // BKE set, MOE stays 0
        g_breakArmed = true;

        Serial.printf("  PB12 live level : %s\n",
                      PwmDrive::faultLineHealthy() ? "HIGH (healthy)" : "LOW (fault)");
        Serial.printf("  BIF latched     : %s\n",
                      PwmDrive::breakTripped() ? "YES" : "no");
        Serial.println();
        Serial.println("Now ground PB12 through a 1k resistor. BIF should latch and");
        Serial.println("stay latched after you remove the short. Press 'c' to clear.");
        Serial.println("Changes will be reported automatically.");
    }
}

void setup()
{
    // ---- Safety-critical outputs FIRST, before anything else ----
    // A reset-state GPIO is high-impedance and cannot drive the contactor
    // opto, so the contactor is already de-energised here. Forcing it
    // explicitly anyway: this is a diagnostic build that gets run
    // repeatedly on a rig with a mains contactor, and "safe by omission"
    // is not good enough for that. Matches the main firmware's boot order.
    pinMode(PIN_CONTACTOR, OUTPUT);
    digitalWrite(PIN_CONTACTOR, LOW);
    pinMode(PIN_LED_STATUS, OUTPUT);
    pinMode(PIN_LED_FAULT, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);
    digitalWrite(PIN_LED_FAULT, LOW);

    Serial.begin(115200);
    delay(400);

    for (int i = 0; i < kNumPins; i++)
    {
        g_soft[i]  = NOT_TAKEN;
        g_stiff[i] = NOT_TAKEN;
    }

    analogReadResolution(12);

    Serial.println();
    Serial.println("=====================================");
    Serial.println(" PIN CHECK MODE — F446RE");
    Serial.println(" Nucleo pin integrity / ADC leakage");
    Serial.println(" No HV. No power stage. Contactor forced OFF.");
    Serial.println("=====================================");
    measureVdda();
    Serial.println();
    Serial.println("Procedure per pin: touch the SOFT (100k/100k) divider midpoint");
    Serial.println("to the pin and press 's', then the STIFF (1k/1k) midpoint and");
    Serial.println("press 'f'. Press 't' when you have done the pins you care about.");
    Serial.println();
    Serial.println("Do PA1 and PC2 (suspects) AND at least two of PA4/PC0/PC1");
    Serial.println("(controls) — the comparison is what makes the result meaningful.");
    Serial.println("Unplug the sensor leads from the control channels first.");
    Serial.println();
    printHelp();
    listPins();
}

void loop()
{
    // ---- Report BKIN changes if the break test is armed ----
    if (g_breakArmed)
    {
        static bool lastLine = true, lastBif = false;
        static bool first = true;
        bool line = PwmDrive::faultLineHealthy();
        bool bif  = PwmDrive::breakTripped();

        if (first || line != lastLine || bif != lastBif)
        {
            first = false;
            Serial.printf("[bkin] PB12=%s  BIF=%s\n",
                          line ? "HIGH" : "LOW ", bif ? "LATCHED" : "clear");
            lastLine = line;
            lastBif  = bif;
        }
    }

    // ---- Command parser ----
    static char buf[16];
    static uint8_t idx = 0;

    while (Serial.available())
    {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r')
        {
            buf[idx] = '\0';
            if (idx > 0)
            {
                switch (buf[0])
                {
                case 'l': listPins();   break;
                case 's': recordSoft(); break;
                case 'f': recordStiff();break;
                case 't': printTable(); break;
                case 'd': digitalTest();break;
                case 'o': outputDriveTest(); break;
                case 'b': breakTest();  break;
                case 'v': measureVdda();break;
                case 'h': printHelp();  break;

                case 'c':
                    if (!g_breakArmed)
                    {
                        Serial.println("Break test not armed — press 'b' first.");
                    }
                    else
                    {
                        PwmDrive::clearBreak();
                        Serial.printf("Break cleared. BIF now %s, PB12 %s\n",
                                      PwmDrive::breakTripped() ? "STILL LATCHED "
                                                                 "(line is still low)" : "clear",
                                      PwmDrive::faultLineHealthy() ? "HIGH" : "LOW");
                    }
                    break;

                case 'p':
                {
                    int n = atoi(buf + 1);
                    if (n >= 0 && n < kNumPins)
                    {
                        g_sel = n;
                        Serial.printf("Selected %s (%s)\n",
                                      kPins[n].name, kPins[n].role);
                    }
                    else
                    {
                        Serial.printf("? pin index out of range (0..%d)\n", kNumPins - 1);
                    }
                    break;
                }

                case 'x':
                    for (int i = 0; i < kNumPins; i++)
                    {
                        g_soft[i]  = NOT_TAKEN;
                        g_stiff[i] = NOT_TAKEN;
                    }
                    Serial.println("All readings discarded.");
                    break;

                default:
                    Serial.println("? unknown cmd — press 'h' for help");
                    break;
                }
            }
            idx = 0;
        }
        else if (idx < sizeof(buf) - 1)
        {
            buf[idx++] = c;
        }
    }
}

#else  // ===================== NORMAL FIRMWARE =====================

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println("=====================================");
    Serial.println(" Induction Heater PLL — F446RE");
    Serial.printf( " Core: %lu MHz\n", SystemCoreClock / 1000000UL);
    Serial.println("=====================================");

    // ---- Safety-critical outputs FIRST ----
    // Contactor off, PWM disabled before anything else initializes.
    pinMode(PIN_LED_STATUS, OUTPUT);
    pinMode(PIN_LED_FAULT, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);
    digitalWrite(PIN_LED_FAULT, LOW);

    MainsControl::init();   // contactor forced OFF
    PwmDrive::init();       // TIM1 configured, outputs DISABLED (MOE=0)

    // ---- Sensing + protection ----
    Sensing::init();
    Protection::init();

    // ---- PLL feedback capture (starts PAUSED — see PllControl.cpp) ----
    PllControl::init();

    // ---- User interface ----
    Encoder::init();
    Display::init();
    Display::splash();
    delay(SPLASH_MS);

    // ---- State machine ----
    StateManager::init();

    Serial.println("Ready. Idle. Press encoder to START.");
}

void loop()
{
#if LOOP_DIAG_ENABLED
    // ---- Loop-timing diagnostic (see LOOP_DIAG_ENABLED in config.h) ----
    // Flags any single pass through StateManager::update() slower than
    // LOOP_DIAG_MS, pinpointing whether the MCU loop itself is stalling
    // rather than something external. This is what identified the 548ms
    // software-SPI display render.
    static unsigned long s_lastLoopTime = 0;
    unsigned long loopStart = millis();
    if (s_lastLoopTime != 0)
    {
        unsigned long gap = loopStart - s_lastLoopTime;
        if (gap > LOOP_DIAG_MS)
        {
            Serial.print("[loop] STALL: ");
            Serial.print(gap);
            Serial.println(" ms since previous loop() entry");
        }
    }

    StateManager::update();

    unsigned long loopEnd = millis();
    unsigned long duration = loopEnd - loopStart;
    if (duration > LOOP_DIAG_MS)
    {
        Serial.print("[loop] StateManager::update() took ");
        Serial.print(duration);
        Serial.println(" ms");
    }
    s_lastLoopTime = loopEnd;
#else
    StateManager::update();
#endif
}

#endif // PWM_TEST_MODE
