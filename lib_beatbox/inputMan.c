/*
 * Input Manager Module
 * * This module centralizes input handling. It runs a dedicated thread that
 * polls hardware inputs (Joystick, Accelerometer) at a fixed rate (e.g. 100Hz).
 * * Responsibilities:
 * 1. Initialize low-level drivers (ADC, Timer, Rotary, Joystick).
 * 2. Poll the accelerometer for air-drumming events.
 * 3. Poll the joystick for volume control.
 * 4. Enforce debounce logic (preventing volume changes immediately after a remote update).
 * 5. Print system statistics to the console once per second.
 */

#include "inputMan.h"
#include "audioMixer.h"
#include "accelerometer.h"
#include "joystick.h"
#include "rotary.h"     
#include "mpc3208.h"    
#include "intervalTimer.h"
#include "beatGenerator.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h> 

// --- Configuration Constants ---

#define LOCKOUT_DURATION_SEC 2   // How long to ignore joystick after web/UDP volume change
#define POLL_RATE_MS 10          // Polling period (10ms = 100Hz)
#define JOYSTICK_DEBOUNCE_CYCLES 25 // Hold-down delay for joystick volume
#define VOLUME_INCREMENT 5       // Step size for joystick volume change

// --- Internal State ---

static pthread_t s_inputThreadId;
static volatile bool s_stopping = false;
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;

static time_t s_lastManualVolumeSet = 0; // Timestamp of last remote volume change
static int s_joystickDebounceCounter = 0; 

// --- Private Helpers ---
static void* inputThread(void* _arg);
static void printStats(void);
static void handleJoystick(void);


// --- Public API ---

void InputMan_init(wavedata_t* pBase, wavedata_t* pSnare, wavedata_t* pHiHat) {
    
    // 1. Initialize hardware drivers
    // IMPORTANT: mpc3208 must be init before Accelerometer/Joystick try to read it.
    mpc3208_init(); 
    Interval_init();

    // 2. Initialize the specific input sub-modules
    // Accelerometer will read initial state here, so SPI must be ready.
    Accelerometer_init(pBase, pSnare, pHiHat);
    Joystick_init();
    Rotary_init(); 

    // 3. Initialize state and start thread
    s_lastManualVolumeSet = time(NULL); 
    s_stopping = false;
    pthread_create(&s_inputThreadId, NULL, inputThread, NULL);
}

void InputMan_cleanup(void) {
    s_stopping = true;
    pthread_join(s_inputThreadId, NULL);
    
    // Cleanup hardware drivers
    Rotary_cleanup();
    Accelerometer_cleanup();
    Joystick_cleanup();
    Interval_cleanup();
    mpc3208_cleanup(); 
}

// Called by UDP/Web handlers when they change the volume.
// This sets a timer that temporarily disables the joystick to prevent fighting.
void InputMan_notifyManualVolumeSet(void) {
    pthread_mutex_lock(&s_mutex);
    s_lastManualVolumeSet = time(NULL);
    s_joystickDebounceCounter = JOYSTICK_DEBOUNCE_CYCLES; 
    pthread_mutex_unlock(&s_mutex);
}

// --- Internal Logic ---

// Prints the dashboard string required by the assignment:
// "MO <Mode> <BPM>bpm vol:<Vol> Audio [...] Accel [...]"
static void printStats(void) {
    double minAudio, maxAudio, avgAudio;
    int countAudio;
    double minAccel, maxAccel, avgAccel;
    int countAccel;
    
    BeatMode mode = BeatGenerator_getMode();
    int tempo = BeatGenerator_getTempo();
    int volume = AudioMixer_getVolume();
    
    // Basic status info
    printf("MO %d %dbpm vol:%d ", (int)mode, tempo, volume);
    
    // Audio timing stats (jitter analysis)
    if (Interval_getStats(INTERVAL_AUDIO, &minAudio, &maxAudio, &avgAudio, &countAudio)) {
        printf("Audio [%.3f, %.3f] avg %.3f/%d ", minAudio, maxAudio, avgAudio, countAudio);
        Interval_reset(INTERVAL_AUDIO);
    } else {
        printf("Audio [N/A, N/A] avg N/A/0 ");
    }
    
    // Accelerometer polling stats
    if (Interval_getStats(INTERVAL_ACCEL, &minAccel, &maxAccel, &avgAccel, &countAccel)) {
        printf("Accel [%.3f, %.3f] avg %.3f/%d", minAccel, maxAccel, avgAccel, countAccel);
        Interval_reset(INTERVAL_ACCEL);
    } else {
        printf("Accel [N/A, N/A] avg N/A/0");
    }
    
    printf("\n");
}

static void handleJoystick(void) {
    // Read the GPIO direction (abstracted by Joystick module)
    int direction = Joystick_readVolumeDirection();
    int current_volume;

    // Check if we are in the lockout period (after a web interface change)
    bool allowJoystick = true;
    pthread_mutex_lock(&s_mutex);
    time_t currentTime = time(NULL);
    if (currentTime - s_lastManualVolumeSet < LOCKOUT_DURATION_SEC) {
        allowJoystick = false; 
        s_joystickDebounceCounter = JOYSTICK_DEBOUNCE_CYCLES; // Keep resetting debounce
    }
    pthread_mutex_unlock(&s_mutex);

    if (allowJoystick) {
        if (s_joystickDebounceCounter > 0) {
            // Waiting for debounce cooldown
            s_joystickDebounceCounter--;
        } else if (direction != 0) {
            // Valid press detected
            
            current_volume = AudioMixer_getVolume(); 
            current_volume += direction * VOLUME_INCREMENT;

            // Clamp bounds
            if (current_volume < 0) current_volume = 0; 
            if (current_volume > 100) current_volume = 100;

            AudioMixer_setVolume(current_volume);
            
            // Reset debounce timer to prevent rapid-fire changes
            s_joystickDebounceCounter = JOYSTICK_DEBOUNCE_CYCLES; 
        }
    }
}


// --- Main Polling Thread ---

static void* inputThread(void* _arg) {
    (void)_arg;

    time_t lastPrintTime = time(NULL);

    while (!s_stopping) {
        
        // 1. Poll Hardware
        Accelerometer_poll(); 
        handleJoystick();

        // 2. Output Statistics (Once per second)
        if (time(NULL) != lastPrintTime) {
            printStats();
            lastPrintTime = time(NULL);
        }

        // 3. Sleep for sample period (10ms)
        usleep(POLL_RATE_MS * 1000); 
    }
    return NULL;
}