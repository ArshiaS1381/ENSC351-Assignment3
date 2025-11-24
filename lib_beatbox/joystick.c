/*
 * Joystick Module
 * * Handles reading the analog joystick via the ADC.
 * It provides a simple abstraction to interpret the analog voltage
 * as discrete "Up", "Down", or "Center" events for volume control.
 */

#include "joystick.h"
#include "mpc3208.h" // Uses the shared ADC driver
#include <stdio.h>
#include <stdlib.h>

// --- Configuration Constants ---

// Which ADC channel the Joystick Y-axis (vertical) is connected to
#define JOYSTICK_ADC_CHANNEL 1 

// Voltage Thresholds (for 12-bit ADC: 0 to 4095)
// Up = Voltage approaching VCC (high value)
// Down = Voltage approaching GND (low value)
#define THRESHOLD_UP   3500 
#define THRESHOLD_DOWN 500  

// --- Public API ---

void Joystick_init(void) {
    // The hardware initialization is shared via mpc3208_init(),
    // so we don't need to do anything specific here.
}

void Joystick_cleanup(void) {
    // No specific cleanup needed
}

int Joystick_readVolumeDirection(void) {
    // Read raw voltage value
    int val = mpc3208_read_channel(JOYSTICK_ADC_CHANNEL);
    
    if (val == -1) return 0; // Hardware error check

    // Interpret direction
    if (val < THRESHOLD_DOWN) return -1; // Stick pushed DOWN (Decrease Volume)
    if (val > THRESHOLD_UP)   return  1; // Stick pushed UP (Increase Volume)
    
    return 0; // Stick is Centered (Deadzone)
}