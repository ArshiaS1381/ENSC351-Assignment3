#include "beatGenerator.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>    // For bool, true, false
#include <pthread.h>
#include <unistd.h>     // For sleep()
#include <time.h>       // For nanosleep()

// --- Internal (static) variables ---

// Threading
static pthread_t s_beatThreadId;
static volatile bool s_stopping = false;

// Mutex for safely accessing shared state
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;

// State variables
static int s_tempo = 120; // Default 120 BPM
static BeatMode s_mode = BEAT_ROCK; // Default rock beat
static int s_beatCount = 0; // Current beat in the measure

// Pointers to the loaded sound data
static wavedata_t* s_pBaseSound = NULL;
static wavedata_t* s_pSnareSound = NULL;
static wavedata_t* s_pHiHatSound = NULL;

// --- Private function prototypes ---
static void* playbackThread(void* _arg);
static long long getNsPerHalfBeat(void);

// --- Public function definitions ---

void BeatGenerator_init(wavedata_t* pBaseSound, wavedata_t* pSnareSound, wavedata_t* pHiHatSound)
{
    // Save pointers to sound data
    s_pBaseSound = pBaseSound;
    s_pSnareSound = pSnareSound;
    s_pHiHatSound = pHiHatSound;

    // Start the playback thread
    s_stopping = false;
    if (pthread_create(&s_beatThreadId, NULL, playbackThread, NULL) != 0) {
        perror("Error creating beat generator thread");
        exit(EXIT_FAILURE);
    }
}

void BeatGenerator_cleanup(void)
{
    // Signal the thread to stop
    s_stopping = true;

    // Wait for the thread to join
    if (pthread_join(s_beatThreadId, NULL) != 0) {
        perror("Error joining beat generator thread");
    }

    // Clean up mutex (optional, but good practice)
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
        s_beatCount = 0; // Reset beat count when changing mode
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

// Gets the sleep time in nanoseconds per half-beat
static long long getNsPerHalfBeat(void)
{
    int tempo = BeatGenerator_getTempo();
    double secondsPerBeat = 60.0 / (double)tempo;
    double secondsPerHalfBeat = secondsPerBeat / 2.0;
    return (long long)(secondsPerHalfBeat * 1000000000.0);
}

// Thread to generate the beat
static void* playbackThread(void* _arg)
{
    (void)_arg; // Suppress unused parameter warning

    while (!s_stopping)
    {
        // Get current state
        BeatMode currentMode = BeatGenerator_getMode();
        
        // --- Play sounds based on mode and beat ---
        if (currentMode == BEAT_ROCK) {
            // Standard rock beat
            // Beat 1:   Hi-hat, Base
            // Beat 1.5: Hi-hat
            // Beat 2:   Hi-hat, Snare
            // Beat 2.5: Hi-hat
            // Beat 3:   Hi-hat, Base
            // Beat 3.5: Hi-hat
            // Beat 4:   Hi-hat, Snare
            // Beat 4.5: Hi-hat
            
            // We use a counter from 0 to 7 (for 8 half-beats)
            int beat = s_beatCount % 8;

            if (beat == 0) { // 1
                AudioMixer_queueSound(s_pHiHatSound);
                AudioMixer_queueSound(s_pBaseSound);
            } else if (beat == 1) { // 1.5
                AudioMixer_queueSound(s_pHiHatSound);
            } else if (beat == 2) { // 2
                AudioMixer_queueSound(s_pHiHatSound);
                AudioMixer_queueSound(s_pSnareSound);
            } else if (beat == 3) { // 2.5
                AudioMixer_queueSound(s_pHiHatSound);
            } else if (beat == 4) { // 3
                AudioMixer_queueSound(s_pHiHatSound);
                AudioMixer_queueSound(s_pBaseSound);
            } else if (beat == 5) { // 3.5
                AudioMixer_queueSound(s_pHiHatSound);
            } else if (beat == 6) { // 4
                AudioMixer_queueSound(s_pHiHatSound);
                AudioMixer_queueSound(s_pSnareSound);
            } else if (beat == 7) { // 4.5
                AudioMixer_queueSound(s_pHiHatSound);
            }
        
        } else if (currentMode == BEAT_CUSTOM) {
            // TODO: Implement your custom beat here
            // Example: Play hi-hat on every half-beat, snare on 2 and 4
            int beat = s_beatCount % 8;
            AudioMixer_queueSound(s_pHiHatSound);
            if (beat == 2 || beat == 6) {
                AudioMixer_queueSound(s_pSnareSound);
            }
        }
        // If mode is BEAT_NONE, do nothing

        // --- Increment beat counter ---
        pthread_mutex_lock(&s_mutex);
        {
            s_beatCount++;
        }
        pthread_mutex_unlock(&s_mutex);


        // --- Sleep for one half-beat ---
        struct timespec req = {0};
        req.tv_nsec = getNsPerHalfBeat();
        nanosleep(&req, NULL);
    }

    return NULL;
}