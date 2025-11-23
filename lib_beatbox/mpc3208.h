#ifndef MPC3208_H
#define MPC3208_H

// Initialize the SPI connection to the MPC3208 chip
void mpc3208_init(void);

// Read the raw 12-bit value (0-4095) from a specific channel (0-7)
// Returns -1 on SPI error, or 0 if not initialized.
int mpc3208_read_channel(int ch);

// Close the SPI connection
void mpc3208_cleanup(void);

#endif