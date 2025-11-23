#ifndef INTERVALTIMER_H
#define INTERVALTIMER_H

typedef enum {
    INTERVAL_AUDIO,
    INTERVAL_ACCEL,
    NUM_INTERVALS // Must be last
} IntervalType;

void Interval_init(void);
void Interval_cleanup(void);

// Reset stats for a given interval type
void Interval_reset(IntervalType type);

// Mark the current time for a given interval type
void Interval_mark(IntervalType type);

// Get statistics. Returns 0 if count is 0, 1 otherwise.
int Interval_getStats(IntervalType type, double* min, double* max, double* avg, int* count);

#endif