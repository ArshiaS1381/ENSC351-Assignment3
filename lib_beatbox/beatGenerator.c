#include "beatGenerator.h"
#include "audioMixer.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

// --- Internal (static) variables ---
static pthread_t s_beatThreadId;
static volatile bool s_stopping = false;
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;
static int s_tempo = 120; // Default 120 BPM
static BeatMode s_mode = BEAT_ROCK; 
static int s_beatCount = 0; 

static wavedata_t* s_pBaseSound = NULL;
static wavedata_t* s_pSnareSound = NULL;
static wavedata_t* s_pHiHatSound = NULL;

// --- Private function prototypes ---
static void* playbackThread(void* _arg);
static long long getNsPerHalfBeat(void);

// --- Public function definitions ---

void BeatGenerator_init(wavedata_t* pBaseSound, wavedata_t* pSnareSound, wavedata_t* pHiHatSound)
{
    s_pBaseSound = pBaseSound;
    s_pSnareSound = pSnareSound;
    s_pHiHatSound = pHiHatSound;

    s_stopping = false;
    pthread_create(&s_beatThreadId, NULL, playbackThread, NULL);
}

void BeatGenerator_cleanup(void)
{
    s_stopping = true;
    pthread_join(s_beatThreadId, NULL);
    pthread_mutex_destroy(&s_mutex);
}

void BeatGenerator_setTempo(int newTempo)
{
    pthread_mutex_lock(&s_mutex);
    {
        if (newTempo < 40) newTempo = 40;
        if (newTempo > 300) newTempo = 300;
        s_tempo = newTempo;
    }
    pthread_mutex_unlock(&s_mutex);
}

int BeatGenerator_getTempo(void)
{
    int tempo = 0;
    pthread_mutex_lock(&s_mutex);
    {
        tempo = s_tempo;
    }
    pthread_mutex_unlock(&s_mutex);
    return tempo;
}

void BeatGenerator_setMode(BeatMode newMode)
{
    pthread_mutex_lock(&s_mutex);
    {
        s_mode = newMode;
        s_beatCount = 0; // Reset beat count on mode change
    }
    pthread_mutex_unlock(&s_mutex);
}

BeatMode BeatGenerator_getMode(void)
{
    BeatMode mode = BEAT_NONE;
    pthread_mutex_lock(&s_mutex);
    {
        mode = s_mode;
    }
    pthread_mutex_unlock(&s_mutex);
    return mode;
}

// --- Private function definitions ---

// Calculates sleep time using: Time For Half Beat [sec] = 60[sec/min] / BPM / 2
static long long getNsPerHalfBeat(void)
{
    int tempo = BeatGenerator_getTempo();
    double secondsPerBeat = 60.0 / (double)tempo;
    double secondsPerHalfBeat = secondsPerBeat / 2.0;
    return (long long)(secondsPerHalfBeat * 1000000000.0);
}

static void* playbackThread(void* _arg)
{
    (void)_arg;

    while (!s_stopping)
    {
        BeatMode currentMode = BeatGenerator_getMode();
        int beat = s_beatCount % 8; // Counter 0-7 (8 half-beats for a 4-beat measure)

        if (currentMode == BEAT_ROCK) {
            // Standard rock beat pattern
            if (beat == 0 || beat == 4) { // 1 or 3 (Base + Hi-Hat)
                AudioMixer_queueSound(s_pHiHatSound);
                AudioMixer_queueSound(s_pBaseSound);
            } else if (beat == 2 || beat == 6) { // 2 or 4 (Snare + Hi-Hat)
                AudioMixer_queueSound(s_pHiHatSound);
                AudioMixer_queueSound(s_pSnareSound);
            } else if (beat == 1 || beat == 3 || beat == 5 || beat == 7) { // Half beats (Hi-Hat only)
                AudioMixer_queueSound(s_pHiHatSound);
            }
        
        } else if (currentMode == BEAT_CUSTOM) {
            // Rock #2 / Custom Beat: Half-time feel
            // Hi-hat on every half beat. Base on 1, Snare on 3
            AudioMixer_queueSound(s_pHiHatSound);
            if (beat == 0) { // Beat 1
                AudioMixer_queueSound(s_pBaseSound);
            }
            if (beat == 4) { // Beat 3
                AudioMixer_queueSound(s_pSnareSound);
            }
        }
        // BEAT_NONE does nothing

        pthread_mutex_lock(&s_mutex);
        s_beatCount++;
        pthread_mutex_unlock(&s_mutex);

        // Sleep for one half-beat
        struct timespec req = {0};
        req.tv_nsec = getNsPerHalfBeat();
        nanosleep(&req, NULL);
    }

    return NULL;
}