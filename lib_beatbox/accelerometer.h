#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

#include "audioMixer.h"

void Accelerometer_init(wavedata_t* pBase, wavedata_t* pSnare, wavedata_t* pHiHat);
void Accelerometer_cleanup(void);

// Polls the three axes, checks for shake events, and queues sounds
void Accelerometer_poll(void);

#endif