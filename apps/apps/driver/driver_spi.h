#ifndef DRIVER_SPI_H
#define DRIVER_SPI_H

#include "shared/pup_spi.h"
#include "shared/pup_devfs_proto.h"
#include "driver_of.h"

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
extern const dtm_of_driver_t g_spi_of_driver;

void spi_resources_init(void);
const pup_dtm_spi_bus_t *spi_bus_by_index(unsigned int index);
const pup_dtm_spi_bus_t *spi_bus(void);
const pup_dtm_spi_cs_t *spi_cs(uint8_t cs_id);
int  spi_bus_index_by_node(dtm_ofw_node_t node);
int spi_cs_from_id(uint8_t cs_id, uint32_t *gpiox, uint16_t *pinx);
int  spi_bus_init(void);
void spi2_gpio_init(void);
int  spi_handle_request(pup_devfs_proxy_payload_t *req);
int  spi_register_one(void);

#endif /* DRIVER_SPI_H */
