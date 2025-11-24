#ifndef INTERVALTIMER_H
#define INTERVALTIMER_H

// Supported interval types to track
typedef enum {
    INTERVAL_AUDIO, // Time between audio buffer refills
    INTERVAL_ACCEL, // Time between accelerometer polls
    NUM_INTERVALS   // Total count (Keep at end)
} IntervalType;

void Interval_init(void);
void Interval_cleanup(void);

// Resets the statistics for a specific interval type.
// Typically called after printing stats to start a fresh second.
void Interval_reset(IntervalType type);

// Call this function every time the event happens.
// It tracks the time difference between this call and the previous one.
void Interval_mark(IntervalType type);

// Retrieves the current statistics.
// Returns 1 if data is available, 0 if no samples have been collected.
int Interval_getStats(IntervalType type, double* min, double* max, double* avg, int* count);

#endif