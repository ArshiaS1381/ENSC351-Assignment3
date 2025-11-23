#include "accelerometer.h"
#include "intervalTimer.h"
#include "audioMixer.h"
#include "mpc3208.h" // Uses the real MPC3208 driver
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Accelerometer on ADC channels 2, 3, 4
#define ACCEL_X_CHANNEL 2 
#define ACCEL_Y_CHANNEL 3 
#define ACCEL_Z_CHANNEL 4 

// Trigger threshold (change in ADC value)
#define TRIG_THRESHOLD 1000 

// Debounce in samples (polls)
#define DEBOUNCE_POLLS 15 

static wavedata_t* s_pBase = NULL;
static wavedata_t* s_pSnare = NULL;
static wavedata_t* s_pHiHat = NULL;

static int s_lastX = 0, s_lastY = 0, s_lastZ = 0;
static int s_debounceX = 0, s_debounceY = 0, s_debounceZ = 0;

static int readAccelChannel(int channel) {
    return mpc3208_read_channel(channel);
}

void Accelerometer_init(wavedata_t* pBase, wavedata_t* pSnare, wavedata_t* pHiHat) {
    s_pBase = pBase;
    s_pSnare = pSnare;
    s_pHiHat = pHiHat;

    s_lastX = readAccelChannel(ACCEL_X_CHANNEL);
    s_lastY = readAccelChannel(ACCEL_Y_CHANNEL);
    s_lastZ = readAccelChannel(ACCEL_Z_CHANNEL);
}

void Accelerometer_cleanup(void) {
}

void Accelerometer_poll(void) {
    Interval_mark(INTERVAL_ACCEL);

    int x = readAccelChannel(ACCEL_X_CHANNEL);
    int y = readAccelChannel(ACCEL_Y_CHANNEL);
    int z = readAccelChannel(ACCEL_Z_CHANNEL);

    if (s_debounceX > 0) s_debounceX--;
    if (s_debounceY > 0) s_debounceY--;
    if (s_debounceZ > 0) s_debounceZ--;

    // Check X Axis (e.g., Snare)
    if (s_debounceX == 0 && abs(x - s_lastX) > TRIG_THRESHOLD) {
        AudioMixer_queueSound(s_pSnare);
        s_debounceX = DEBOUNCE_POLLS;
    }

    // Check Y Axis (e.g., HiHat)
    if (s_debounceY == 0 && abs(y - s_lastY) > TRIG_THRESHOLD) {
        AudioMixer_queueSound(s_pHiHat);
        s_debounceY = DEBOUNCE_POLLS;
    }

    // Check Z Axis (e.g., Base)
    if (s_debounceZ == 0 && abs(z - s_lastZ) > TRIG_THRESHOLD) {
        AudioMixer_queueSound(s_pBase);
        s_debounceZ = DEBOUNCE_POLLS;
    }

    s_lastX = x;
    s_lastY = y;
    s_lastZ = z;
}