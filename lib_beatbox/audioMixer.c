// NOTE: This implementation assumes the concurrency fix from prior discussion is included.
// Hardware volume control is commented out as it requires specific ALSA mixer element names for the USB device.
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

static snd_pcm_t *handle;
static bool s_audioInitialized = false; 

#define AUDIO_DEVICE "plughw:1,0"
#define DEFAULT_VOLUME 80
#define SAMPLE_RATE 44100
#define NUM_CHANNELS 1
#define SAMPLE_SIZE (sizeof(short))

static unsigned long playbackBufferSize = 0;
static short *playbackBuffer = NULL;

#define MAX_SOUND_BITES 30
typedef struct {
	wavedata_t *pSound;
	int location;
} playbackSound_t;
static playbackSound_t soundBites[MAX_SOUND_BITES];

// Playback threading
void* playbackThread(void* arg);
static volatile _Bool stopping = false;
static pthread_t playbackThreadId;
static pthread_mutex_t audioMutex = PTHREAD_MUTEX_INITIALIZER;

static int volume = DEFAULT_VOLUME; // Default volume set here

void AudioMixer_init(void)
{
	for (int i = 0; i < MAX_SOUND_BITES; i++) {
		soundBites[i].pSound = NULL;
	}

	int err = snd_pcm_open(&handle, AUDIO_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0) {
		printf("AudioMixer: Playback open error: %s\n", snd_strerror(err));
        printf("AudioMixer: WARNING: Proceeding in SILENT mode (no audio output).\n");
        s_audioInitialized = false;
		return;
	}
    
    s_audioInitialized = true;

	err = snd_pcm_set_params(handle,
			SND_PCM_FORMAT_S16_LE,
			SND_PCM_ACCESS_RW_INTERLEAVED,
			NUM_CHANNELS,
			SAMPLE_RATE,
			1,			// Allow software resampling
			50000);		// 0.05 seconds per buffer
	if (err < 0) {
		printf("Playback set params error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}

 	unsigned long unusedBufferSize = 0;
	snd_pcm_get_params(handle, &unusedBufferSize, &playbackBufferSize);
	playbackBuffer = malloc(playbackBufferSize * sizeof(*playbackBuffer));

	pthread_create(&playbackThreadId, NULL, playbackThread, NULL);
}

_Bool AudioMixer_readWaveFileIntoMemory(char *fileName, wavedata_t *pSound)
{
	assert(pSound);
	const int PCM_DATA_OFFSET = 44;
	FILE *file = fopen(fileName, "r");
	if (file == NULL) {
		fprintf(stderr, "ERROR: Unable to open file %s.\n", fileName);
		return false;
	}

	fseek(file, 0, SEEK_END);
	int sizeInBytes = ftell(file) - PCM_DATA_OFFSET;
	pSound->numSamples = sizeInBytes / SAMPLE_SIZE;

	fseek(file, PCM_DATA_OFFSET, SEEK_SET);

	pSound->pData = malloc(sizeInBytes);
	if (pSound->pData == 0) {
		fprintf(stderr, "ERROR: Unable to allocate %d bytes for file %s.\n",
				sizeInBytes, fileName);
		return false;
	}

	int samplesRead = fread(pSound->pData, SAMPLE_SIZE, pSound->numSamples, file);
	if (samplesRead != pSound->numSamples) {
		fprintf(stderr, "ERROR: Unable to read %d samples from file %s (read %d).\n",
				pSound->numSamples, fileName, samplesRead);
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

	int freeSlot = -1;
	for (int i = 0; i < MAX_SOUND_BITES; i++) {
		if (soundBites[i].pSound == NULL) {
			freeSlot = i;
			break;
		}
	}

	if (freeSlot != -1) {
		soundBites[freeSlot].pSound = pSound;
		soundBites[freeSlot].location = 0;
	} else {
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
	if (newVolume < 0) newVolume = 0;
    if (newVolume > AUDIOMIXER_MAX_VOLUME) newVolume = AUDIOMIXER_MAX_VOLUME;
	volume = newVolume; 

    /*
     * TODO: Hardware volume control via ALSA mixer.
     * This is required for final deployment on BeagleY-AI with USB Audio.
     * long min, max;
     * snd_mixer_t *mixerHandle;
     * snd_mixer_selem_id_t *sid;
     * // ... (configure card/selem_name for USB Audio) ...
     * // ... (open, attach, register, load mixer) ...
     * // ... (set volume: volume * max / 100) ...
     * // ... (close mixer) ...
     */
}

static void fillPlaybackBuffer(short *buff, int size)
{
    memset(buff, 0, size * SAMPLE_SIZE);

    playbackSound_t localSoundBites[MAX_SOUND_BITES];
    
    pthread_mutex_lock(&audioMutex);
    for (int i = 0; i < MAX_SOUND_BITES; i++) {
        localSoundBites[i] = soundBites[i];
    }
    // Read volume *inside* the lock for thread safety
    double volMultiplier = (double)volume / 100.0;
    pthread_mutex_unlock(&audioMutex);


    for (int i = 0; i < MAX_SOUND_BITES; i++) {
        if (localSoundBites[i].pSound == NULL) continue;

        wavedata_t *sound = localSoundBites[i].pSound;
        int location = localSoundBites[i].location;

        for (int j = 0; j < size; j++) {
            if (location + j >= sound->numSamples) {
                // Sound finished playing
                pthread_mutex_lock(&audioMutex);
                soundBites[i].pSound = NULL; 
                pthread_mutex_unlock(&audioMutex);
                break; 
            }

            // Apply volume and mix
            long sampleData = (long)(sound->pData[location + j] * volMultiplier);
            long sample = buff[j] + sampleData;

            // Clipping
            if (sample > SHRT_MAX) sample = SHRT_MAX;
            else if (sample < SHRT_MIN) sample = SHRT_MIN;
            buff[j] = (short)sample;
        }

        // Update location in the real array if sound is still playing
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
		Interval_mark(INTERVAL_AUDIO); // Mark before filling (per assignment)
		
		fillPlaybackBuffer(playbackBuffer, playbackBufferSize);

		snd_pcm_sframes_t frames = snd_pcm_writei(handle,
				playbackBuffer, playbackBufferSize);

		if (frames < 0) {
			frames = snd_pcm_recover(handle, frames, 1);
		}
		if (frames < 0) {
			fprintf(stderr, "ERROR: Failed writing audio with snd_pcm_writei(): %li\n", frames);
			// Do not exit, just keep trying
		}
	}

	return NULL;
}