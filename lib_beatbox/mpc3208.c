#include "mpc3208.h"
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

// Configuration for MPC3208 SPI
#define SPI_DEV        "/dev/spidev0.0" // Adjust if necessary on BeagleY-AI
#define SPI_MODE       0u
#define SPI_BITS       8u
#define SPI_SPEED      250000u // 250kHz

static int spi_fd = -1;

void mpc3208_init(void)
{
    // Open SPI device
    spi_fd = open(SPI_DEV, O_RDWR);
    if (spi_fd < 0) {
        perror("MPC3208: Failed to open SPI device");
        return;
    }
    
    // --- FIX: Store macro values in local variables to pass their address to ioctl ---
    int mode = SPI_MODE;
    int bits = SPI_BITS;
    int speed = SPI_SPEED;
    
    // Set SPI parameters
    ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
    ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    // --------------------------------------------------------------------------------
}

int mpc3208_read_channel(int ch)
{
    if (spi_fd < 0) return 0;

    // Send command to MPC3208 for Single-Ended (0x06) mode on the chosen channel
    uint8_t tx[3] = { (uint8_t)(0x06 | ((ch & 0x04) >> 2)),
                      (uint8_t)((ch & 0x03) << 6), 0x00 };
    uint8_t rx[3] = {0};
    
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx, .rx_buf = (unsigned long)rx,
        .len = 3, .speed_hz = SPI_SPEED, .bits_per_word = SPI_BITS
    };
    
    if (ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr) < 1) {
        perror("MPC3208: SPI transfer failed");
        return -1;
    }
    
    // Result is 12 bits: 4 bits in rx[1] and 8 bits in rx[2]
    return ((rx[1] & 0x0F) << 8) | rx[2]; // Range 0 to 4095
}

void mpc3208_cleanup(void)
{
    if (spi_fd >= 0) close(spi_fd);
    spi_fd = -1;
}