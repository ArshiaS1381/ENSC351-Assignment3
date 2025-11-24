#ifndef INPUTMAN_H
#define INPUTMAN_H

#include "audioMixer.h"
#include <time.h>

// Initialize the Input Manager.
// This starts a background thread that polls the Joystick and Accelerometer.
void InputMan_init(wavedata_t* pBase, wavedata_t* pSnare, wavedata_t* pHiHat);

// Stop the input thread and cleanup resources.
void InputMan_cleanup(void);

// Call this when the volume is changed via the Web UI or UDP.
// It triggers a temporary lockout of joystick volume control to prevent conflicts.
void InputMan_notifyManualVolumeSet(void);

#endif