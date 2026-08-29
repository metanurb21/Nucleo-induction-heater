// ============================================================
//  SystemState — shared state enum used across modules.
//  Kept in its own header to avoid circular dependencies
//  between StateManager and Display.
// ============================================================
#pragma once

enum SystemState
{
    STATE_IDLE,        // Contactor off, PWM off — safe, waiting to start
    STATE_STARTING,    // Contactor energizing, bus charging
    STATE_RUNNING,     // PWM active, PLL tracking, heating
    STATE_FAULT_OCP,   // Overcurrent (hardware or software)
    STATE_FAULT_TEMP,  // Over-temperature
    STATE_FAULT_MAINS, // Mains lost
    STATE_FAULT_MANUAL // Manual stop
};
