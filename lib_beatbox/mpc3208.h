#ifndef MPC3208_H
#define MPC3208_H

// Initialize the SPI file descriptor and configuration.
void mpc3208_init(void);

// Read a single channel (0-7) from the ADC.
// Returns a value between 0 and 4095 (12-bit).
// Returns -1 or 0 on error.
int mpc3208_read_channel(int ch);

// Close the SPI file descriptor.
void mpc3208_cleanup(void);

#endif