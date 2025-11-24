#ifndef BEATGENERATOR_H
#define BEATGENERATOR_H

#include "audioMixer.h"

// Drum Beat Modes
// Matches the integer values expected by the JavaScript UI.
typedef enum {
    BEAT_NONE = 0,   // Silence
    BEAT_ROCK = 1,   // Standard Rock Beat
    BEAT_CUSTOM = 2  // Alternative pattern
} BeatMode;

// Initialize the generator thread with the audio assets.
void BeatGenerator_init(wavedata_t* pBaseSound, wavedata_t* pSnareSound, wavedata_t* pHiHatSound);
void BeatGenerator_cleanup(void);

// Control Tempo (BPM)
// Clamped between 40 and 300 BPM.
void BeatGenerator_setTempo(int newTempo);
int BeatGenerator_getTempo(void);

// Control Beat Pattern
void BeatGenerator_setMode(BeatMode newMode);
BeatMode BeatGenerator_getMode(void);

#endif