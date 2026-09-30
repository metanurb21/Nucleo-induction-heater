// ============================================================
//  Protection implementation.
// ============================================================

#include "Protection.h"
#include "Sensing.h"
#include "PwmDrive.h"
#include "config.h"

namespace Protection
{
    static int s_ocpThreshold = OCP_THRESHOLD;

    void init()
    {
        s_ocpThreshold = OCP_THRESHOLD;
    }

    void setOcpThreshold(int counts)
    {
        if (counts < OCP_THRESH_MIN) counts = OCP_THRESH_MIN;
        if (counts > OCP_THRESH_MAX) counts = OCP_THRESH_MAX;
        s_ocpThreshold = counts;
    }

    int getOcpThreshold() { return s_ocpThreshold; }

    // ---- fast checks: hardware break + software OCP -------
    FaultType checkFast()
    {
        // 1. Hardware break latched? (already killed PWM in hardware)
        if (PwmDrive::breakTripped())
        {
            return FAULT_OCP_HW;
        }

        // 2. Software OCP — compare RAW (not smoothed) for fast trip.
        //
        //    Gated by SOFT_OCP_ENABLED (config.h), currently false: PA1
        //    has no sensor wired, and OCP board v3 doesn't connect it.
        //    Trip-testing a floating ADC input was producing false
        //    OCP_SW faults on contactor close. We still refresh the
        //    reading so the display/bar graph stays live.
        int raw = Sensing::updateOcp();
        if (SOFT_OCP_ENABLED && raw >= s_ocpThreshold)
        {
            return FAULT_OCP_SW;
        }

        return FAULT_NONE;
    }

    // ---- slow checks: temperature -------------------------
    FaultType checkSlow()
    {
        if (NTC_ENABLED)
        {
            Sensing::updateTemperature();
            if (Sensing::temperatureSmoothedC() >= TEMP_SHUTDOWN_C)
            {
                return FAULT_TEMP;
            }
        }
        return FAULT_NONE;
    }

    const char* faultName(FaultType f)
    {
        switch (f)
        {
        case FAULT_OCP_HW:  return "OCP (HW)";
        case FAULT_OCP_SW:  return "OCP (SW)";
        case FAULT_TEMP:    return "OVER TEMP";
        case FAULT_NO_MAINS:return "NO MAINS";
        case FAULT_NONE:
        default:            return "NONE";
        }
    }
}
