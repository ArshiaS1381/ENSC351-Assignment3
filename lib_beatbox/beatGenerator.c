/*
 * Beat Generator Module
 * * This module runs a background thread that acts as the "drummer".
 * It handles the timing (BPM) and sequencing of the drum patterns.
 * It sleeps for the duration of a half-beat, wakes up, plays the 
 * sounds for the current step, and repeats.
 */

#include "beatGenerator.h"
#include "audioMixer.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

// --- Configuration Constants ---

#define BPM_DEFAULT 120
#define BPM_MIN 40      // Slowest allowed tempo
#define BPM_MAX 300     // Fastest allowed tempo

// --- Internal State ---

static pthread_t s_beatThreadId;
static volatile bool s_stopping = false;
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;

// Beat parameters (protected by mutex)
static int s_tempo = BPM_DEFAULT; 
static BeatMode s_mode = BEAT_ROCK; 
static int s_beatCount = 0; // Tracks the current step in the measure (0-7 for 8th notes)

static wavedata_t* s_pBaseSound = NULL;
static wavedata_t* s_pSnareSound = NULL;
static wavedata_t* s_pHiHatSound = NULL;

// --- Private helper prototypes ---
static void* playbackThread(void* _arg);
static long long getNsPerHalfBeat(void);

// --- Public API ---

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
        // Clamp the tempo to safe limits
        if (newTempo < BPM_MIN) newTempo = BPM_MIN;
        if (newTempo > BPM_MAX) newTempo = BPM_MAX;
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
        // Reset count so the new beat starts from the beginning (step 0)
        // This prevents feeling "lost" in the measure when switching styles.
        s_beatCount = 0; 
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

// --- Private Implementation ---

// Calculate the sleep duration for a half-beat (8th note) in nanoseconds.
// Formula: Time (sec) = (60 sec / BPM) / 2
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
        
        // We use an 8-step sequencer (1 measure of 8th notes)
        // 0 = Beat 1
        // 1 = Beat 1.5 (&)
        // 2 = Beat 2
        // ...
        int beat = s_beatCount % 8; 

        if (currentMode == BEAT_ROCK) {
            // --- Standard Rock Beat ---
            // On 1 and 3: Base Drum + Hi-Hat
            if (beat == 0 || beat == 4) { 
                AudioMixer_queueSound(s_pHiHatSound);
                AudioMixer_queueSound(s_pBaseSound);
            } 
            // On 2 and 4: Snare + Hi-Hat
            else if (beat == 2 || beat == 6) { 
                AudioMixer_queueSound(s_pHiHatSound);
                AudioMixer_queueSound(s_pSnareSound);
            } 
            // On all "and" beats (1.5, 2.5...): Hi-Hat only
            else if (beat % 2 != 0) { 
                AudioMixer_queueSound(s_pHiHatSound);
            }
        
        } else if (currentMode == BEAT_CUSTOM) {
            // --- Custom Half-Time Feel ---
            // Hi-hat keeps time on every 8th note
            AudioMixer_queueSound(s_pHiHatSound);
            
            // Base on 1 only
            if (beat == 0) { 
                AudioMixer_queueSound(s_pBaseSound);
            }
            // Snare on 3 only (Beat 3 is index 4 in 0-7 counting)
            if (beat == 4) { 
                AudioMixer_queueSound(s_pSnareSound);
            }
        }
        // If BEAT_NONE, we just sleep without queuing sounds.

        // Advance to the next step in the measure
        pthread_mutex_lock(&s_mutex);
        s_beatCount++;
        pthread_mutex_unlock(&s_mutex);

        // Wait for the duration of one 8th note
        struct timespec req = {0};
        req.tv_nsec = getNsPerHalfBeat();
        nanosleep(&req, NULL);
    }

    return NULL;
}