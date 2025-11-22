#ifndef BEAT_GENERATOR_H
#define BEAT_GENERATOR_H

#include "audioMixer.h" // For wavedata_t
#include <stdbool.h>

// Defines the different beat modes the generator can be in
typedef enum {
    BEAT_NONE,
    BEAT_ROCK,
    BEAT_CUSTOM
} BeatMode;

/*
 * Initializes and starts the beat generator thread.
 * Must be called once at startup.
 * It takes pointers to the audio data for the three
 * sounds it's responsible for playing.
 */
void BeatGenerator_init(wavedata_t* pBaseSound, wavedata_t* pSnareSound, wavedata_t* pHiHatSound);

/*
 * Stops and cleans up the beat generator thread.
 * Must be called once at shutdown to join the thread.
 */
void BeatGenerator_cleanup(void);

/*
 * Sets the current tempo in Beats Per Minute (BPM).
 * This function is thread-safe.
 * The tempo will be clamped to the valid range [40, 300].
 */
void BeatGenerator_setTempo(int newTempo);

/*
 * Gets the current tempo in BPM.
 * This function is thread-safe.
 */
int BeatGenerator_getTempo(void);

/*
 * Sets the current beat mode (None, Rock, Custom).
 * This function is thread-safe.
 */
void BeatGenerator_setMode(BeatMode newMode);

/*
 * Gets the current beat mode.
 * This function is thread-safe.
 */
BeatMode BeatGenerator_getMode(void);

#endif // BEAT_GENERATOR_H