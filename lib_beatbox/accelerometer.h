#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

#include "audioMixer.h"

// Initializes the accelerometer module and stores pointers to the drum sounds
void Accelerometer_init(wavedata_t* pBase, wavedata_t* pSnare, wavedata_t* pHiHat);

// Cleans up any resources (if necessary)
void Accelerometer_cleanup(void);

// Main worker function.
// Should be called periodically (e.g., every 10ms) from the input thread.
// Reads the hardware, detects motion, and queues audio events.
void Accelerometer_poll(void);

#endif