#ifndef __PUP_SPI_H__
#define __PUP_SPI_H__

#include <stdint.h>

#define PUP_SPI_NAME_MAX                16

#define PUP_SPI_IOCTL_REGISTER_SLAVE    0x3001
#define PUP_SPI_IOCTL_TRANSFER          0x3002

/* SPI2 board chip-select id */
#define PUP_SPI2_CS0                    0

typedef struct {
    char name[PUP_SPI_NAME_MAX];
    uint8_t cs_id;
    uint8_t mode;
    uint8_t baudrate_prescaler;
    uint8_t datasize;
    uint8_t firstbit;
    int32_t slave_id;
} pup_spi_slave_config_t;

typedef struct {
    int32_t slave_id;
    const uint8_t *tx_buf;
    uint8_t *rx_buf;
    uint32_t len;
    uint32_t actual_len;
} pup_spi_transfer_t;

#endif