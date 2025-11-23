#ifndef ROTARY_H
#define ROTARY_H

// Initializes the GPIO thread to monitor the momentary switch for mode cycling
void Rotary_init(void);

// Cleans up the GPIO resources and joins the thread
void Rotary_cleanup(void);

#endif