#ifndef ROTARY_H
#define ROTARY_H

// Initializes the GPIO monitoring thread.
// Uses libgpiod to watch the rotary encoder lines.
void Rotary_init(void);

// Signals the thread to stop and cleans up GPIO resources.
void Rotary_cleanup(void);

#endif