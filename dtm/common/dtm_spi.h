#include <stdint.h>

/* SPI mode definitions */
#define DTM_SPI_MODE_0              0   /* CPOL=0, CPHA=0 */
#define DTM_SPI_MODE_1              1   /* CPOL=0, CPHA=1 */
#define DTM_SPI_MODE_2              2   /* CPOL=1, CPHA=0 */
#define DTM_SPI_MODE_3              3   /* CPOL=1, CPHA=1 */

/* SPI data size definitions */
#define DTM_SPI_DATASIZE_8BIT       0   /* 8-bit data size */
#define DTM_SPI_DATASIZE_16BIT      1   /* 16-bit data size */

/* SPI baud rate prescaler definitions */
#define DTM_SPI_BAUDRATE_PRESCALER_2    0   /* fPCLK/2 */
#define DTM_SPI_BAUDRATE_PRESCALER_4    1   /* fPCLK/4 */
#define DTM_SPI_BAUDRATE_PRESCALER_8    2   /* fPCLK/8 */
#define DTM_SPI_BAUDRATE_PRESCALER_16   3   /* fPCLK/16 */
#define DTM_SPI_BAUDRATE_PRESCALER_32   4   /* fPCLK/32 */
#define DTM_SPI_BAUDRATE_PRESCALER_64   5   /* fPCLK/64 */
#define DTM_SPI_BAUDRATE_PRESCALER_128  6   /* fPCLK/128 */
#define DTM_SPI_BAUDRATE_PRESCALER_256  7   /* fPCLK/256 */

/* SPI configuration structure */
typedef struct {
    uint32_t mode;                  /* SPI mode (0-3) */
    uint32_t datasize;              /* Data size (8 or 16 bits) */
    uint32_t baudrate_prescaler;    /* Baud rate prescaler */
    uint32_t firstbit;              /* First bit (0: MSB first, 1: LSB first) */
    uint32_t crc_polarity;          /* CRC polarity */
} dtm_spi_config_t;

/* SPI functions */
void dtm_spi_init(uint32_t spi_base, const dtm_spi_config_t *config);
void dtm_spi_enable(uint32_t spi_base);
void dtm_spi_disable(uint32_t spi_base);
uint8_t dtm_spi_transmit_byte(uint32_t spi_base, uint8_t data);
uint16_t dtm_spi_transmit_halfword(uint32_t spi_base, uint16_t data);
void dtm_spi_transmit(uint32_t spi_base, const uint8_t *data, uint32_t length);
void dtm_spi_receive(uint32_t spi_base, uint8_t *data, uint32_t length);
void dtm_spi_transmit_receive(uint32_t spi_base, const uint8_t *tx_data, uint8_t *rx_data, uint32_t length);