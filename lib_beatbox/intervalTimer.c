/*
 * Interval Timer & Stats Module
 * * This module tracks the timing jitter of critical events (Audio refill, Accel poll).
 * It records the time difference between successive calls to `Interval_mark()`
 * and calculates min, max, and average durations.
 * * Used to prove that the system is meeting its real-time deadlines.
 */

#include "intervalTimer.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <limits.h>

// --- Internal Data Structures ---

typedef struct {
    struct timespec lastTime; // Timestamp of the previous mark
    double min; // Minimum interval seen (milliseconds)
    double max; // Maximum interval seen (milliseconds)
    double sum; // Accumulator for calculating average
    int count;  // Number of samples collected
    int firstCall; // Flag to ignore the very first call (setup time)
} IntervalStats;

// --- Internal State ---

static IntervalStats s_intervals[NUM_INTERVALS];
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;

// --- Private Helpers ---

// Convert timespec to raw nanoseconds for easier subtraction
static long long timespecToNano(struct timespec t) {
    return t.tv_sec * 1000000000LL + t.tv_nsec;
}

// --- Public API ---

void Interval_init(void) {
    for (int i = 0; i < NUM_INTERVALS; i++) {
        Interval_reset(i);
    }
}

void Interval_cleanup(void) {
    // No dynamic memory to free
}

void Interval_reset(IntervalType type) {
    pthread_mutex_lock(&s_mutex);
    
    // Reset stats to extremes so the first real sample overwrites them
    s_intervals[type].min = 1.0e9; 
    s_intervals[type].max = 0;
    s_intervals[type].sum = 0;
    s_intervals[type].count = 0;
    
    // Mark next call as the "first" (reference) call
    s_intervals[type].firstCall = 1;
    clock_gettime(CLOCK_MONOTONIC, &s_intervals[type].lastTime);
    
    pthread_mutex_unlock(&s_mutex);
}

void Interval_mark(IntervalType type) {
    struct timespec currentTime;
    clock_gettime(CLOCK_MONOTONIC, &currentTime);

    pthread_mutex_lock(&s_mutex);
    
    if (s_intervals[type].firstCall) {
        // First time calling: just store the time as a reference point.
        // We can't calculate an interval (delta) yet.
        s_intervals[type].firstCall = 0;
        s_intervals[type].lastTime = currentTime;
    } else {
        // Calculate time passed since last mark
        long long diff = timespecToNano(currentTime) - timespecToNano(s_intervals[type].lastTime);
        double diffMs = (double)diff / 1000000.0;

        // Update statistics
        if (diffMs < s_intervals[type].min) s_intervals[type].min = diffMs;
        if (diffMs > s_intervals[type].max) s_intervals[type].max = diffMs;
        s_intervals[type].sum += diffMs;
        s_intervals[type].count++;
        
        // Update reference for next time
        s_intervals[type].lastTime = currentTime;
    }
    
    pthread_mutex_unlock(&s_mutex);
}

int Interval_getStats(IntervalType type, double* min, double* max, double* avg, int* count) {
    pthread_mutex_lock(&s_mutex);
    
    // If no data collected, return failure (0)
    if (s_intervals[type].count == 0) {
        pthread_mutex_unlock(&s_mutex);
        return 0;
    }

    *min = s_intervals[type].min;
    *max = s_intervals[type].max;
    *count = s_intervals[type].count;
    *avg = s_intervals[type].sum / (double)(*count);
    
    pthread_mutex_unlock(&s_mutex);
    return 1;
}