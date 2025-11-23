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
#include <unistd.h> // for usleep()

// --- Configuration ---
#define DEBOUNCE_SECS 2 // Lockout period after UDP volume set
#define POLL_PERIOD_MS 10 // For Accel/Joystick polling (100Hz)
#define JOYSTICK_DEBOUNCE_POLLS 10 // For joystick press/hold debounce (~100ms)
#define VOLUME_STEP 5 

// --- Internal Global State ---
static pthread_t s_inputThreadId;
static volatile bool s_stopping = false;
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;
static time_t s_lastManualVolumeSet = 0; 
static int s_joystickDebounceCounter = 0; 

// --- Private Function Prototypes ---
static void* inputThread(void* _arg);
static void printStats(void);
static void handleJoystick(void);


// --- Public Functions ---

void InputMan_init(wavedata_t* pBase, wavedata_t* pSnare, wavedata_t* pHiHat) {
    
    // Initialize shared/low-level modules
    mpc3208_init(); 
    Interval_init();

    // Initialize individual input modules
    Accelerometer_init(pBase, pSnare, pHiHat);
    Joystick_init();
    Rotary_init(); 

    s_lastManualVolumeSet = time(NULL); 
    s_stopping = false;
    pthread_create(&s_inputThreadId, NULL, inputThread, NULL);
}

void InputMan_cleanup(void) {
    s_stopping = true;
    pthread_join(s_inputThreadId, NULL);
    
    Rotary_cleanup();
    Accelerometer_cleanup();
    Joystick_cleanup();
    Interval_cleanup();
    mpc3208_cleanup(); 
}

void InputMan_notifyManualVolumeSet(void) {
    pthread_mutex_lock(&s_mutex);
    s_lastManualVolumeSet = time(NULL);
    s_joystickDebounceCounter = JOYSTICK_DEBOUNCE_POLLS; 
    pthread_mutex_unlock(&s_mutex);
}

// --- Private Thread Helpers ---

static void printStats(void) {
    double minAudio, maxAudio, avgAudio;
    int countAudio;
    double minAccel, maxAccel, avgAccel;
    int countAccel;
    
    // Beat Mode, Tempo, Volume
    BeatMode mode = BeatGenerator_getMode();
    int tempo = BeatGenerator_getTempo();
    int volume = AudioMixer_getVolume();
    
    // Print required status line prefix
    printf("MO %d %dbpm vol:%d ", (int)mode, tempo, volume);
    
    // Audio Buffer Refill Stats
    if (Interval_getStats(INTERVAL_AUDIO, &minAudio, &maxAudio, &avgAudio, &countAudio)) {
        printf("Audio [%.3f, %.3f] avg %.3f/%d ", minAudio, maxAudio, avgAudio, countAudio);
        Interval_reset(INTERVAL_AUDIO);
    } else {
        printf("Audio [N/A, N/A] avg N/A/0 ");
    }
    
    // Accelerometer Poll Stats
    if (Interval_getStats(INTERVAL_ACCEL, &minAccel, &maxAccel, &avgAccel, &countAccel)) {
        printf("Accel [%.3f, %.3f] avg %.3f/%d", minAccel, maxAccel, avgAccel, countAccel);
        Interval_reset(INTERVAL_ACCEL);
    } else {
        printf("Accel [N/A, N/A] avg N/A/0");
    }
    
    printf("\n");
}


static void handleJoystick(void) {
    int direction = Joystick_readVolumeDirection();
    int current_volume;

    // Check if manual volume debounce lockout is active
    bool allowJoystick = true;
    pthread_mutex_lock(&s_mutex);
    time_t currentTime = time(NULL);
    if (currentTime - s_lastManualVolumeSet < DEBOUNCE_SECS) {
        allowJoystick = false; 
        s_joystickDebounceCounter = JOYSTICK_DEBOUNCE_POLLS; 
    }
    pthread_mutex_unlock(&s_mutex);

    if (allowJoystick) {
        if (s_joystickDebounceCounter > 0) {
            s_joystickDebounceCounter--;
        } else if (direction != 0) {
            
            current_volume = AudioMixer_getVolume(); 
            current_volume += direction * VOLUME_STEP;

            if (current_volume < 0) current_volume = 0; 
            if (current_volume > 100) current_volume = 100;

            AudioMixer_setVolume(current_volume);
            
            s_joystickDebounceCounter = JOYSTICK_DEBOUNCE_POLLS; 
        }
    }
}


// --- Main Input Thread ---

static void* inputThread(void* _arg) {
    (void)_arg;

    time_t lastPrintTime = time(NULL);

    while (!s_stopping) {
        
        // 1. Poll Accelerometer for Air-Drumming
        Accelerometer_poll(); 

        // 2. Handle Joystick Volume Control
        handleJoystick();

        // 3. Statistics Printing (Once per second)
        if (time(NULL) != lastPrintTime) {
            printStats();
            lastPrintTime = time(NULL);
        }

        // 4. Sleep/Poll Delay
        usleep(POLL_PERIOD_MS * 1000); 
    }
    return NULL;
}