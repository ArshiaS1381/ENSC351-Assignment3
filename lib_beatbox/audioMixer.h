#ifndef AUDIOMIXER_H
#define AUDIOMIXER_H

#include <stdbool.h>

#define AUDIOMIXER_MAX_VOLUME 100

// Data structure to hold raw PCM audio in memory
typedef struct {
	int numSamples;
	short *pData; // Array of 16-bit samples
} wavedata_t;

// Initialize the ALSA playback system and mixing thread
void AudioMixer_init(void);
void AudioMixer_cleanup(void);

// Helper to load a WAV file from disk into a wavedata_t struct
_Bool AudioMixer_readWaveFileIntoMemory(char *fileName, wavedata_t *pSound);
void AudioMixer_freeWaveFileData(wavedata_t *pSound);

// Request a sound to be played.
// This adds the sound to the mixer queue. It will be mixed with any currently playing sounds.
void AudioMixer_queueSound(wavedata_t *pSound);

// Get/Set global volume (0 - 100)
void AudioMixer_setVolume(int newVolume);
int AudioMixer_getVolume();

#endif