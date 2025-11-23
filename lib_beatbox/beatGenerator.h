#ifndef BEATGENERATOR_H
#define BEATGENERATOR_H

#include "audioMixer.h"

// Matches the modes expected by beatbox_ui.js
typedef enum {
    BEAT_NONE = 0,
    BEAT_ROCK = 1,
    BEAT_CUSTOM = 2
} BeatMode;

void BeatGenerator_init(wavedata_t* pBaseSound, wavedata_t* pSnareSound, wavedata_t* pHiHatSound);
void BeatGenerator_cleanup(void);

void BeatGenerator_setTempo(int newTempo);
int BeatGenerator_getTempo(void);

void BeatGenerator_setMode(BeatMode newMode);
BeatMode BeatGenerator_getMode(void);

#endif