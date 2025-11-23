#ifndef AUDIOMIXER_H
#define AUDIOMIXER_H

#include <stdbool.h>

#define AUDIOMIXER_MAX_VOLUME 100

typedef struct {
	int numSamples;
	short *pData;
} wavedata_t;

void AudioMixer_init(void);
void AudioMixer_cleanup(void);

// Read/Free wave data
_Bool AudioMixer_readWaveFileIntoMemory(char *fileName, wavedata_t *pSound);
void AudioMixer_freeWaveFileData(wavedata_t *pSound);

// Playback queue
void AudioMixer_queueSound(wavedata_t *pSound);

// Volume control
void AudioMixer_setVolume(int newVolume);
int AudioMixer_getVolume();

#endif