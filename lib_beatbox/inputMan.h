#ifndef INPUTMAN_H
#define INPUTMAN_H

#include "audioMixer.h"
#include <time.h>

void InputMan_init(wavedata_t* pBase, wavedata_t* pSnare, wavedata_t* pHiHat);
void InputMan_cleanup(void);

// Notifies InputMan that the volume was manually changed (via UDP/web)
// This triggers a brief lockout of Accel/Joystick volume control (2 seconds).
void InputMan_notifyManualVolumeSet(void);

#endif