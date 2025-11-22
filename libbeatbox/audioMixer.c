// Incomplete implementation of an audio mixer. Search for "REVISIT" to find things
// which are left as incomplete.
// Note: Generates low latency audio on BeagleBone Black; higher latency found on host.
#include "audioMixer.h"
#include <stdio.h>      // for printf, fprintf
#include <stdlib.h>     // for malloc, free, exit
#include <assert.h>     // for assert()
#include <string.h>     // for memset, strcmp, etc.
#include <alsa/asoundlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <limits.h>
#include <alloca.h> // needed for mixer


static snd_pcm_t *handle;

#define DEFAULT_VOLUME 80

#define SAMPLE_RATE 44100
#define NUM_CHANNELS 1
#define SAMPLE_SIZE (sizeof(short)) 			// bytes per sample
// Sample size note: This works for mono files because each sample ("frame') is 1 value.
// If using stereo files then a frame would be two samples.

static unsigned long playbackBufferSize = 0;
static short *playbackBuffer = NULL;


// Currently active (waiting to be played) sound bites
#define MAX_SOUND_BITES 30
typedef struct {
	// A pointer to a previously allocated sound bite (wavedata_t struct).
	// Note that many different sound-bite slots could share the same pointer
	// (overlapping cymbal crashes, for example)
	wavedata_t *pSound;

	// The offset into the pData of pSound. Indicates how much of the
	// sound has already been played (and hence where to start playing next).
	int location;
} playbackSound_t;
static playbackSound_t soundBites[MAX_SOUND_BITES];

// Playback threading
void* playbackThread(void* arg);
static volatile _Bool stopping = false;
static pthread_t playbackThreadId;
static pthread_mutex_t audioMutex = PTHREAD_MUTEX_INITIALIZER;

static int volume = 0;

void AudioMixer_init(void)
{
	// AudioMixer_setVolume(DEFAULT_VOLUME);

	// Initialize the currently active sound-bites being played
		for (int i = 0; i < MAX_SOUND_BITES; i++) {
			soundBites[i].pSound = NULL;
		}


	// Open the PCM output
	int err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	// Configure parameters of PCM output
	err = snd_pcm_set_params(handle,
			SND_PCM_FORMAT_S16_LE,
			SND_PCM_ACCESS_RW_INTERLEAVED,
			NUM_CHANNELS,
			SAMPLE_RATE,
			1,			// Allow software resampling
			50000);		// 0.05 seconds per buffer
	if (err < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	// Allocate this software's playback buffer to be the same size as the
	// the hardware's playback buffers for efficient data transfers.
	// ..get info on the hardware buffers:
 	unsigned long unusedBufferSize = 0;
	snd_pcm_get_params(handle, &unusedBufferSize, &playbackBufferSize);
	// ..allocate playback buffer:
	playbackBuffer = malloc(playbackBufferSize * sizeof(*playbackBuffer));

	// Launch playback thread:
	pthread_create(&playbackThreadId, NULL, playbackThread, NULL);
}


// Client code must call AudioMixer_freeWaveFileData to free dynamically allocated data.
_Bool AudioMixer_readWaveFileIntoMemory(char *fileName, wavedata_t *pSound)
{
	assert(pSound);

	// The PCM data in a wave file starts after the header:
	const int PCM_DATA_OFFSET = 44;

	// Open the wave file
	FILE *file = fopen(fileName, "r");
	if (file == NULL) {
		fprintf(stderr, "ERROR: Unable to open file %s.\n", fileName);
		return false;
	}

	// Get file size
	fseek(file, 0, SEEK_END);
	int sizeInBytes = ftell(file) - PCM_DATA_OFFSET;
	pSound->numSamples = sizeInBytes / SAMPLE_SIZE;

	// Search to the start of the data in the file
	fseek(file, PCM_DATA_OFFSET, SEEK_SET);

	// Allocate space to hold all PCM data
	pSound->pData = malloc(sizeInBytes);
	if (pSound->pData == 0) {
		fprintf(stderr, "ERROR: Unable to allocate %d bytes for file %s.\n",
				sizeInBytes, fileName);
		return false;
	}

	// Read PCM data from wave file into memory
	int samplesRead = fread(pSound->pData, SAMPLE_SIZE, pSound->numSamples, file);
	if (samplesRead != pSound->numSamples) {
		fprintf(stderr, "ERROR: Unable to read %d samples from file %s (read %d).\n",
				pSound->numSamples, fileName, samplesRead);
		return false;
	}

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
	// Ensure we are only being asked to play "good" sounds:
	assert(pSound->numSamples > 0);
	assert(pSound->pData);

	// Insert the sound by searching for an empty sound bite spot
	// 1. Lock the mutex to ensure thread safety
	pthread_mutex_lock(&audioMutex);

	// 2. Search for a free slot
	int freeSlot = -1;
	for (int i = 0; i < MAX_SOUND_BITES; i++) {
		if (soundBites[i].pSound == NULL) {
			freeSlot = i;
			break;
		}
	}

	// 3. If a free slot is found, "queue" the sound
	if (freeSlot != -1) {
		soundBites[freeSlot].pSound = pSound;
		soundBites[freeSlot].location = 0; // Start playing from the beginning
	} else {
		// 4. If no free slot, print an error
		printf("ERROR: No free sound bite slots available, skipping sound.\n");
	}

	// Unlock the mutex
	pthread_mutex_unlock(&audioMutex);

}

void AudioMixer_cleanup(void)
{
	printf("Stopping audio...\n");

	// Stop the PCM generation thread
	stopping = true;
	pthread_join(playbackThreadId, NULL);

	// Shutdown the PCM output, allowing any pending sound to play out (drain)
	snd_pcm_drain(handle);
	snd_pcm_close(handle);

	// Free playback buffer
	// (note that any wave files read into wavedata_t records must be freed
	//  in addition to this by calling AudioMixer_freeWaveFileData() on that struct.)
	free(playbackBuffer);
	playbackBuffer = NULL;

	printf("Done stopping audio...\n");
	fflush(stdout);
}


int AudioMixer_getVolume()
{
	// Return the cached volume; good enough unless someone is changing
	// the volume through other means and the cached value is out of date.
	return volume;
}

// Function copied from:
// http://stackoverflow.com/questions/6787318/set-alsa-master-volume-from-c-code
// Written by user "trenki".
void AudioMixer_setVolume(int newVolume)
{
	// Ensure volume is reasonable; If so, cache it for later getVolume() calls.
	if (newVolume < 0 || newVolume > AUDIOMIXER_MAX_VOLUME) {
		printf("ERROR: Volume must be between 0 and 100.\n");
		return;
	}
	volume = newVolume; // <-- THIS LINE IS GOOD. IT SAVES THE VALUE.

    /*
     * TODO: This hardware-specific code must be un-commented
     * when building for the BeagleBone. It is commented out
     * to prevent crashing on a PC/WSL.
     *
    long min, max;
    snd_mixer_t *mixerHandle;
    snd_mixer_selem_id_t *sid;
    const char *card = "default";
    // const char *selem_name = "PCM";	// For ZEN cape
    const char *selem_name = "Speaker";	// For USB Audio

    snd_mixer_open(&mixerHandle, 0);
    snd_mixer_attach(mixerHandle, card);
    snd_mixer_selem_register(mixerHandle, NULL, NULL);
    snd_mixer_load(mixerHandle);

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, selem_name);
    snd_mixer_elem_t* elem = snd_mixer_find_selem(mixerHandle, sid);

    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    snd_mixer_selem_set_playback_volume_all(elem, volume * max / 100);

    snd_mixer_close(mixerHandle);
    */
}


// Fill the buff array with new PCM values to output.
//    buff: buffer to fill with new PCM data from sound bites.
//    size: the number of *values* to store into buff
static void fillPlaybackBuffer(short *buff, int size)
{
    // 1. Wipe the buffer to all 0's (silence)
    memset(buff, 0, size * SAMPLE_SIZE);

    // --- Create a local copy of the sound bites state ---
    playbackSound_t localSoundBites[MAX_SOUND_BITES];
    
    // 2. Lock the mutex
    pthread_mutex_lock(&audioMutex);

    // 3. Quickly copy the state
    for (int i = 0; i < MAX_SOUND_BITES; i++) {
        localSoundBites[i] = soundBites[i];
    }
    
    // 4. Unlock the mutex *immediately*
    pthread_mutex_unlock(&audioMutex);

    // --- THIS IS THE FIX ---
    // Get the current volume as a multiplier (0.0 to 1.0)
    // We can read 'volume' directly because it's a static var in this file.
    double volMultiplier = (double)volume / 100.0;
    // --- END FIX ---


    // 5. Now, do all the heavy mixing work using the *local copy*
    for (int i = 0; i < MAX_SOUND_BITES; i++) {
        if (localSoundBites[i].pSound == NULL) {
            continue;
        }

        wavedata_t *sound = localSoundBites[i].pSound;
        int location = localSoundBites[i].location;

        for (int j = 0; j < size; j++) {
            if (location + j >= sound->numSamples) {
                // *** IMPORTANT ***
                // We must lock/unlock *only* when changing the *real* array
                pthread_mutex_lock(&audioMutex);
                soundBites[i].pSound = NULL; // Mark real slot as free
                pthread_mutex_unlock(&audioMutex);

                break; 
            }

            // --- THIS IS THE FIX ---
            // Apply volume *before* mixing
            long sampleData = (long)(sound->pData[location + j] * volMultiplier);
            long sample = buff[j] + sampleData;
            // --- END FIX ---


            // --- Clipping ---
            if (sample > SHRT_MAX) sample = SHRT_MAX;
            else if (sample < SHRT_MIN) sample = SHRT_MIN;
            buff[j] = (short)sample;
        }

        // *** IMPORTANT ***
        // Update the location in the *real* array
        if (localSoundBites[i].pSound != NULL) {
            pthread_mutex_lock(&audioMutex);
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
		// Generate next block of audio
		fillPlaybackBuffer(playbackBuffer, playbackBufferSize);

		// Output the audio
		snd_pcm_sframes_t frames = snd_pcm_writei(handle,
				playbackBuffer, playbackBufferSize);

		// Check for (and handle) possible error conditions on output
		if (frames < 0) {
			fprintf(stderr, "AudioMixer: writei() returned %li\n", frames);
			frames = snd_pcm_recover(handle, frames, 1);
		}
		if (frames < 0) {
			fprintf(stderr, "ERROR: Failed writing audio with snd_pcm_writei(): %li\n",
					frames);
			exit(EXIT_FAILURE);
		}
		if (frames > 0 && (unsigned long)frames < playbackBufferSize) {
			printf("Short write (expected %li, wrote %li)\n",
					playbackBufferSize, frames);
		}
	}

	return NULL;
}