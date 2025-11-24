/*
 * Accelerometer Module
 * * Handles reading the 3-axis accelerometer via the MPC3208 ADC.
 * It implements a "shake" detection algorithm to trigger drum sounds
 * when the board is moved sharply in X, Y, or Z directions.
 */

#include "accelerometer.h"
#include "intervalTimer.h"
#include "audioMixer.h"
#include "mpc3208.h" // Low-level SPI driver for the ADC
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// --- Configuration Constants ---

// ADC Channels corresponding to the accelerometer outputs
#define ACCEL_CHANNEL_X 2 
#define ACCEL_CHANNEL_Y 3 
#define ACCEL_CHANNEL_Z 4 

// Sensitivity Thresholds
// These values represent the change in ADC raw value (0-4095) required to trigger a beat.
// Lower values = More sensitive (easier to trigger).
// Higher values = Less sensitive (requires harder shake).
#define THRESHOLD_SNARE 300 // X-axis triggers Snare
#define THRESHOLD_HIHAT 300 // Y-axis triggers Hi-Hat
#define THRESHOLD_BASE  250 // Z-axis triggers Base Drum (lower due to gravity offset)

// Debounce Counter
// How many polling cycles to ignore after a trigger to prevent "stuttering" 
// (a single shake registering as multiple hits).
#define DEBOUNCE_CYCLES 15 

// --- Private Variables ---

static wavedata_t* s_pBase = NULL;
static wavedata_t* s_pSnare = NULL;
static wavedata_t* s_pHiHat = NULL;

// Store the previous reading to calculate the delta (change)
static int s_lastX = 0, s_lastY = 0, s_lastZ = 0;

// Countdown timers for debouncing each axis independently
static int s_debounceX = 0, s_debounceY = 0, s_debounceZ = 0;

// Helper to wrap the low-level ADC read
static int readAccelChannel(int channel) {
    return mpc3208_read_channel(channel);
}

void Accelerometer_init(wavedata_t* pBase, wavedata_t* pSnare, wavedata_t* pHiHat) {
    s_pBase = pBase;
    s_pSnare = pSnare;
    s_pHiHat = pHiHat;

    // Seed the "last" values with the current state so we don't 
    // trigger a sound immediately upon startup due to a 0 to N jump.
    // NOTE: mpc3208_init() must be called before this! (Handled in inputMan.c)
    s_lastX = readAccelChannel(ACCEL_CHANNEL_X);
    s_lastY = readAccelChannel(ACCEL_CHANNEL_Y);
    s_lastZ = readAccelChannel(ACCEL_CHANNEL_Z);
}

void Accelerometer_cleanup(void) {
    // No specific resource cleanup required for this module
}

void Accelerometer_poll(void) {
    // Mark this event for the statistics module
    Interval_mark(INTERVAL_ACCEL);

    // 1. Read current raw values from the ADC
    int x = readAccelChannel(ACCEL_CHANNEL_X);
    int y = readAccelChannel(ACCEL_CHANNEL_Y);
    int z = readAccelChannel(ACCEL_CHANNEL_Z);

    // 2. Decrement debounce timers if they are active
    if (s_debounceX > 0) s_debounceX--;
    if (s_debounceY > 0) s_debounceY--;
    if (s_debounceZ > 0) s_debounceZ--;

    // 3. Detect Shakes
    // Logic: If the axis is not in cooldown (debounce == 0) AND the change since 
    // the last poll exceeds our sensitivity threshold, play the sound.

    // X Axis -> Snare
    if (s_debounceX == 0 && abs(x - s_lastX) > THRESHOLD_SNARE) {
        AudioMixer_queueSound(s_pSnare);
        s_debounceX = DEBOUNCE_CYCLES; // Start cooldown
    }

    // Y Axis -> Hi-Hat
    if (s_debounceY == 0 && abs(y - s_lastY) > THRESHOLD_HIHAT) {
        AudioMixer_queueSound(s_pHiHat);
        s_debounceY = DEBOUNCE_CYCLES;
    }

    // Z Axis -> Base
    if (s_debounceZ == 0 && abs(z - s_lastZ) > THRESHOLD_BASE) {
        AudioMixer_queueSound(s_pBase);
        s_debounceZ = DEBOUNCE_CYCLES;
    }

    // 4. Update history for the next poll
    s_lastX = x;
    s_lastY = y;
    s_lastZ = z;
}