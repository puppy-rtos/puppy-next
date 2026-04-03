#ifndef DRIVER_SPI_H
#define DRIVER_SPI_H

#include "shared/pup_spi.h"
#include "shared/pup_devfs_proto.h"

#define SPI_DEVICE_ID  3
#define SPI_MAX_SLAVES 4

typedef struct {
    int     active;
    int     slave_id;
    char    name[PUP_SPI_NAME_MAX];
    uint8_t cs_id;
    uint32_t cs_gpiox;
    uint16_t cs_pinx;
    uint8_t mode;
    uint8_t baudrate_prescaler;
    uint8_t datasize;
    uint8_t firstbit;
} spi_slave_t;

extern int  g_spi_next_slave_id;
extern int  g_spi_initialized;
extern spi_slave_t g_spi_slaves[SPI_MAX_SLAVES];

int spi_cs_from_id(uint8_t cs_id, uint32_t *gpiox, uint16_t *pinx);
void spi2_gpio_init(void);
int  spi_handle_request(pup_devfs_proxy_payload_t *req);
int  spi_register_one(void);

#endif /* DRIVER_SPI_H */
