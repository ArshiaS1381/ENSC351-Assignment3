#include "joystick.h"
#include "mpc3208.h" // Uses the real MPC3208 driver
#include <stdio.h>
#include <stdlib.h>

// Joystick vertical axis on ADC channel 1
#define JOY_ADC_CHANNEL 1 

// Thresholds for 12-bit ADC (0-4095)
#define THRESHOLD_UP   3500 // Pushed up (high voltage)
#define THRESHOLD_DOWN 500  // Pushed down (low voltage)

void Joystick_init(void) {
    // Initialization handled by MPC3208_init in InputMan
}

void Joystick_cleanup(void) {
}

int Joystick_readVolumeDirection(void) {
    int val = mpc3208_read_channel(JOY_ADC_CHANNEL);
    
    if (val == -1) return 0; // Error

    if (val < THRESHOLD_DOWN) return -1; // Down (Decrease Volume)
    if (val > THRESHOLD_UP)   return  1; // Up (Increase Volume)
    
    return 0; // Center
}