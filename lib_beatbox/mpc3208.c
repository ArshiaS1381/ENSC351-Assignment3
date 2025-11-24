/*
 * MPC3208 ADC Driver
 * * Handles low-level SPI communication with the MCP3208 Analog-to-Digital Converter.
 * It sends the specific bit-sequence required by the chip to request a reading
 * from a specific channel and reconstructs the 12-bit result.
 */

#include "mpc3208.h"
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

// --- Configuration Constants ---

#define SPI_DEVICE_PATH "/dev/spidev0.0" 
#define SPI_MODE        0u
#define SPI_BITS_PER_WORD 8u
#define SPI_SPEED_HZ    250000u // 250kHz (Conservative speed for stability)

// --- Internal State ---

static int spi_fd = -1;

// --- Public API ---

void mpc3208_init(void)
{
    // 1. Open SPI Device
    spi_fd = open(SPI_DEVICE_PATH, O_RDWR);
    if (spi_fd < 0) {
        perror("MPC3208: Failed to open SPI device");
        return;
    }
    
    // 2. Configure SPI Parameters
    // We use local variables so we can pass their pointers to ioctl
    int mode = SPI_MODE;
    int bits = SPI_BITS_PER_WORD;
    int speed = SPI_SPEED_HZ;
    
    if (ioctl(spi_fd, SPI_IOC_WR_MODE, &mode) < 0) perror("MPC3208: Set Mode Error");
    if (ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) perror("MPC3208: Set Bits Error");
    if (ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) perror("MPC3208: Set Speed Error");
}

int mpc3208_read_channel(int ch)
{
    if (spi_fd < 0) return 0;

    // 3. Construct the SPI Message
    // The MCP3208 requires a start bit, single-ended/diff bit, and channel bits.
    // Byte 0: Start bit (logic 1) + S/D bit + D2
    // Byte 1: D1 + D0 + Sample time
    // Byte 2: Don't care (clocking out data)
    
    // Command Logic: 0x06 (Binary 00000110) sets Start=1, S/D=1 (Single Ended).
    // The channel bits are split across the bytes.
    uint8_t tx[3] = { 
        (uint8_t)(0x06 | ((ch & 0x04) >> 2)), // Start bit + S/D + Chan bit 2
        (uint8_t)((ch & 0x03) << 6),          // Chan bit 1 + Chan bit 0
        0x00 
    };
    uint8_t rx[3] = {0};
    
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = 3,
        .speed_hz = SPI_SPEED_HZ,
        .bits_per_word = SPI_BITS_PER_WORD
    };
    
    // 4. Perform Transfer
    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) < 1) {
        perror("MPC3208: SPI transfer failed");
        return -1;
    }
    
    // 5. Reconstruct Result
    // The result is 12 bits.
    // rx[1] contains the upper 4 bits (masked with 0x0F).
    // rx[2] contains the lower 8 bits.
    return ((rx[1] & 0x0F) << 8) | rx[2]; // Range 0 to 4095
}

void mpc3208_cleanup(void)
{
    if (spi_fd >= 0) close(spi_fd);
    spi_fd = -1;
}