#include "intervalTimer.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <limits.h>

typedef struct {
    struct timespec lastTime;
    double min; // milliseconds
    double max; // milliseconds
    double sum; // milliseconds
    int count;
    int firstCall; 
} IntervalStats;

static IntervalStats s_intervals[NUM_INTERVALS];
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;

static long long timespecToNano(struct timespec t) {
    return t.tv_sec * 1000000000LL + t.tv_nsec;
}

void Interval_init(void) {
    for (int i = 0; i < NUM_INTERVALS; i++) {
        Interval_reset(i);
    }
}

void Interval_cleanup(void) {
    // Nothing dynamic to free
}

void Interval_reset(IntervalType type) {
    pthread_mutex_lock(&s_mutex);
    s_intervals[type].min = 1.0e9; 
    s_intervals[type].max = 0;
    s_intervals[type].sum = 0;
    s_intervals[type].count = 0;
    s_intervals[type].firstCall = 1;
    clock_gettime(CLOCK_MONOTONIC, &s_intervals[type].lastTime);
    pthread_mutex_unlock(&s_mutex);
}

void Interval_mark(IntervalType type) {
    struct timespec currentTime;
    clock_gettime(CLOCK_MONOTONIC, &currentTime);

    pthread_mutex_lock(&s_mutex);
    
    if (s_intervals[type].firstCall) {
        s_intervals[type].firstCall = 0;
        s_intervals[type].lastTime = currentTime;
    } else {
        long long diff = timespecToNano(currentTime) - timespecToNano(s_intervals[type].lastTime);
        double diffMs = (double)diff / 1000000.0;

        if (diffMs < s_intervals[type].min) s_intervals[type].min = diffMs;
        if (diffMs > s_intervals[type].max) s_intervals[type].max = diffMs;
        s_intervals[type].sum += diffMs;
        s_intervals[type].count++;
        s_intervals[type].lastTime = currentTime;
    }
    
    pthread_mutex_unlock(&s_mutex);
}

int Interval_getStats(IntervalType type, double* min, double* max, double* avg, int* count) {
    pthread_mutex_lock(&s_mutex);
    
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