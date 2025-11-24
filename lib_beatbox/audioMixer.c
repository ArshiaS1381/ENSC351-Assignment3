/*
 * Audio Mixer Module
 * * This module manages the ALSA PCM output. It runs a background thread that
 * continually fills an audio buffer by "mixing" (adding) together all active
 * sound effects. 
 * * Features:
 * - Support for playing multiple overlapping sounds (polyphony).
 * - Software volume control.
 * - Clipping protection (prevents integer overflow when adding waves).
 */

// NOTE: This implementation relies on the ALSA library (libasound).
// Assumes the hardware is plugged in as 'plughw:1,0' (typical for USB audio on BeagleBone).

#include "audioMixer.h"
#include "intervalTimer.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <limits.h>
#include <alloca.h>

// --- Configuration Constants ---

#define ALSA_PCM_DEVICE "plughw:1,0"
#define DEFAULT_VOLUME 80
#define SAMPLE_RATE    44100
#define NUM_CHANNELS   1
#define SAMPLE_SIZE    (sizeof(short)) // 16-bit audio = 2 bytes

// Max number of concurrent sound clips we can mix at once.
// If this is exceeded, new sounds will be dropped/ignored.
#define MAX_ACTIVE_SOUNDS 30

// --- Internal State ---

static snd_pcm_t *handle;           // ALSA handle
static bool s_audioInitialized = false; 

static unsigned long playbackBufferSize = 0;
static short *playbackBuffer = NULL; // The buffer we write to ALSA

// Structure to track a currently playing sound
typedef struct {
	wavedata_t *pSound; // Pointer to the raw audio data
	int location;       // Current index (sample offset) into that data
} playbackSound_t;

// Array of "voice slots"
static playbackSound_t soundBites[MAX_ACTIVE_SOUNDS];

// Threading controls
static volatile _Bool stopping = false;
static pthread_t playbackThreadId;
static pthread_mutex_t audioMutex = PTHREAD_MUTEX_INITIALIZER;

static int volume = DEFAULT_VOLUME; 

// Forward declarations
void* playbackThread(void* arg);


// --- Public API ---

void AudioMixer_init(void)
{
    // Initialize the sound bite array to empty
	for (int i = 0; i < MAX_ACTIVE_SOUNDS; i++) {
		soundBites[i].pSound = NULL;
	}

    // Open the PCM device
	int err = snd_pcm_open(&handle, ALSA_PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0) {
		printf("AudioMixer: Playback open error: %s\n", snd_strerror(err));
        printf("AudioMixer: WARNING: Proceeding in SILENT mode (no audio output).\n");
        s_audioInitialized = false;
		return;
	}
    
    s_audioInitialized = true;

    // Configure ALSA parameters: 16-bit Little Endian, 44.1kHz, Mono
	err = snd_pcm_set_params(handle,
			SND_PCM_FORMAT_S16_LE,
			SND_PCM_ACCESS_RW_INTERLEAVED,
			NUM_CHANNELS,
			SAMPLE_RATE,
			1,			// Allow software resampling
			50000);		// Latency: 0.05 seconds
	if (err < 0) {
		printf("Playback set params error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}

    // Allocate the playback buffer based on what ALSA suggests
 	unsigned long unusedBufferSize = 0;
	snd_pcm_get_params(handle, &unusedBufferSize, &playbackBufferSize);
	playbackBuffer = malloc(playbackBufferSize * sizeof(*playbackBuffer));

    // Start the mixing thread
	pthread_create(&playbackThreadId, NULL, playbackThread, NULL);
}

_Bool AudioMixer_readWaveFileIntoMemory(char *fileName, wavedata_t *pSound)
{
	assert(pSound);
    
    // Wave files have a 44-byte header. The raw PCM data starts after that.
	const int PCM_DATA_OFFSET = 44;
    
	FILE *file = fopen(fileName, "r");
	if (file == NULL) {
		fprintf(stderr, "ERROR: Unable to open file %s.\n", fileName);
		return false;
	}

    // Determine file size
	fseek(file, 0, SEEK_END);
	int sizeInBytes = ftell(file) - PCM_DATA_OFFSET;
	pSound->numSamples = sizeInBytes / SAMPLE_SIZE;

    // Allocate memory
	fseek(file, PCM_DATA_OFFSET, SEEK_SET);
	pSound->pData = malloc(sizeInBytes);
	if (pSound->pData == 0) {
		fprintf(stderr, "ERROR: Unable to allocate %d bytes for file %s.\n",
				sizeInBytes, fileName);
        fclose(file);
		return false;
	}

    // Read the raw PCM samples
	int samplesRead = fread(pSound->pData, SAMPLE_SIZE, pSound->numSamples, file);
	if (samplesRead != pSound->numSamples) {
		fprintf(stderr, "ERROR: Unable to read %d samples from file %s (read %d).\n",
				pSound->numSamples, fileName, samplesRead);
        fclose(file);
		return false;
	}

    fclose(file);
	return true;
}

void AudioMixer_freeWaveFileData(wavedata_t *pSound)
{
	pSound->numSamples = 0;
	free(pSound->pData);
	pSound->pData = NULL;
}

void AudioMixer_queueSound(wavedata_t *pSound)
{
	if (!s_audioInitialized) return;
	assert(pSound->numSamples > 0);
	assert(pSound->pData);

	pthread_mutex_lock(&audioMutex);

    // Find the first empty slot in our mixing array
	int freeSlot = -1;
	for (int i = 0; i < MAX_ACTIVE_SOUNDS; i++) {
		if (soundBites[i].pSound == NULL) {
			freeSlot = i;
			break;
		}
	}

	if (freeSlot != -1) {
		soundBites[freeSlot].pSound = pSound;
		soundBites[freeSlot].location = 0; // Start playing from the beginning
	} else {
        // This happens if we try to play > MAX_ACTIVE_SOUNDS at once
		printf("ERROR: No free sound bite slots available, skipping sound.\n");
	}

	pthread_mutex_unlock(&audioMutex);
}

void AudioMixer_cleanup(void)
{
	if (!s_audioInitialized) return;
	stopping = true;
	pthread_join(playbackThreadId, NULL);

	snd_pcm_drain(handle);
	snd_pcm_close(handle);

	free(playbackBuffer);
	playbackBuffer = NULL;
}

int AudioMixer_getVolume()
{
	return volume;
}

void AudioMixer_setVolume(int newVolume)
{
    // Clamp volume 0-100
	if (newVolume < 0) newVolume = 0;
    if (newVolume > AUDIOMIXER_MAX_VOLUME) newVolume = AUDIOMIXER_MAX_VOLUME;
	volume = newVolume; 

    // Note: This only changes software volume. 
    // Ideally, we would also control the hardware mixer (ALSA 'Line Out') here.
}

// --- Mixing Logic ---

// This function fills the buffer with the next chunk of audio.
// It iterates over all active sounds, adds their current samples together,
// handles volume scaling, and clips the result to fit in a 16-bit short.
static void fillPlaybackBuffer(short *buff, int size)
{
    // Start with silence (0)
    memset(buff, 0, size * SAMPLE_SIZE);

    // Make a local copy of the sound bites to minimize mutex lock time
    playbackSound_t localSoundBites[MAX_ACTIVE_SOUNDS];
    
    pthread_mutex_lock(&audioMutex);
    for (int i = 0; i < MAX_ACTIVE_SOUNDS; i++) {
        localSoundBites[i] = soundBites[i];
    }
    double volMultiplier = (double)volume / 100.0;
    pthread_mutex_unlock(&audioMutex);


    // Mix each active sound into the buffer
    for (int i = 0; i < MAX_ACTIVE_SOUNDS; i++) {
        if (localSoundBites[i].pSound == NULL) continue;

        wavedata_t *sound = localSoundBites[i].pSound;
        int location = localSoundBites[i].location;

        for (int j = 0; j < size; j++) {
            // Check if we reached the end of this specific sound clip
            if (location + j >= sound->numSamples) {
                // Mark the sound as finished in the shared array
                pthread_mutex_lock(&audioMutex);
                soundBites[i].pSound = NULL; 
                pthread_mutex_unlock(&audioMutex);
                break; 
            }

            // Get sample, scale by volume
            long sampleData = (long)(sound->pData[location + j] * volMultiplier);
            
            // Add to existing buffer value (MIXING happening here)
            long sample = buff[j] + sampleData;

            // CLIPPING: Ensure we don't overflow the 16-bit integer limits.
            // If we exceed 32767, cap it there. Otherwise, audio wraps around and sounds terrible.
            if (sample > SHRT_MAX) sample = SHRT_MAX;
            else if (sample < SHRT_MIN) sample = SHRT_MIN;
            
            buff[j] = (short)sample;
        }

        // Update the playback head (location) for this sound if it didn't finish
        if (localSoundBites[i].pSound != NULL) {
            pthread_mutex_lock(&audioMutex);
            // Re-check pSound in case it was cancelled externally (race condition safety)
            if (soundBites[i].pSound != NULL) {
                soundBites[i].location += size;
            }
            pthread_mutex_unlock(&audioMutex);
        }
    }
}

void* playbackThread(void* _arg)
{
	(void)_arg;

	while (!stopping) {
		Interval_mark(INTERVAL_AUDIO); // Stats: record buffer fill interval
		
        // 1. Generate the audio data
		fillPlaybackBuffer(playbackBuffer, playbackBufferSize);

        // 2. Send it to the sound card
		snd_pcm_sframes_t frames = snd_pcm_writei(handle,
				playbackBuffer, playbackBufferSize);

        // 3. Error Handling
		if (frames < 0) {
            // Recover from under-runs (when we aren't generating audio fast enough)
			frames = snd_pcm_recover(handle, frames, 1);
		}
		if (frames < 0) {
			fprintf(stderr, "ERROR: Failed writing audio with snd_pcm_writei(): %li\n", frames);
		}
	}

	return NULL;
}