#include "dtm_bindings.h"

#ifdef DTM_BINFDING_SPI_ARM_M4_ST_GD
#include "dtm_spi.h"
#include "dtm_gpio.h"
#include "dtm_clk.h"
#include <stdio.h>

/* SPI register structure definition */
typedef struct {
  volatile uint32_t CR1;         /*!< SPI control register 1,                Address offset: 0x00 */
  volatile uint32_t CR2;         /*!< SPI control register 2,                Address offset: 0x04 */
  volatile uint32_t SR;          /*!< SPI status register,                   Address offset: 0x08 */
  volatile uint32_t DR;          /*!< SPI data register,                     Address offset: 0x0C */
  volatile uint32_t CRCPR;       /*!< SPI CRC polynomial register,           Address offset: 0x10 */
  volatile uint32_t RXCRCR;      /*!< SPI RX CRC register,                   Address offset: 0x14 */
  volatile uint32_t TXCRCR;      /*!< SPI TX CRC register,                   Address offset: 0x18 */
  volatile uint32_t I2SCFGR;     /*!< SPI_I2S configuration register,        Address offset: 0x1C */
  volatile uint32_t I2SPR;       /*!< SPI_I2S prescaler register,            Address offset: 0x20 */
} SPI_TypeDef;

#define SPI_CR1_CPHA        (1U << 0)
#define SPI_CR1_CPOL        (1U << 1)
#define SPI_CR1_MSTR        (1U << 2)
#define SPI_CR1_BR_SHIFT    3U
#define SPI_CR1_SPE         (1U << 6)
#define SPI_CR1_LSBFIRST    (1U << 7)
#define SPI_CR1_SSI         (1U << 8)
#define SPI_CR1_SSM         (1U << 9)
#define SPI_CR1_DFF         (1U << 11)

#define SPI_SR_RXNE         (1U << 0)
#define SPI_SR_TXE          (1U << 1)
#define SPI_SR_BSY          (1U << 7)

static int spi_to_clk_id(uint32_t spi_base)
{
    int clk_id = -1;

    switch (spi_base) {
        case DTM_SPI1_BASE: clk_id = CLK_SPI1; break;
        case DTM_SPI2_BASE: clk_id = CLK_SPI2; break;
        case DTM_SPI3_BASE: clk_id = CLK_SPI3; break;
        default: break;
    }

    return clk_id;
}

#define DTM_SPI_TIMEOUT  0xFFFFFU

static void spi_wait_not_busy(SPI_TypeDef *spi)
{
    volatile unsigned int timeout = DTM_SPI_TIMEOUT;
    while (spi->SR & SPI_SR_BSY) {
        if (--timeout == 0) {
            break;
        }
    }
}

static int spi_wait_txe(SPI_TypeDef *spi)
{
    volatile unsigned int timeout = DTM_SPI_TIMEOUT;
    while (!(spi->SR & SPI_SR_TXE)) {
        if (--timeout == 0) {
            return -1;
        }
    }
    return 0;
}

static int spi_wait_rxne(SPI_TypeDef *spi)
{
    volatile unsigned int timeout = DTM_SPI_TIMEOUT;
    while (!(spi->SR & SPI_SR_RXNE)) {
        if (--timeout == 0) {
            return -1;
        }
    }
    return 0;
}

void dtm_spi_init(uint32_t spi_base, const dtm_spi_config_t *config)
{
    SPI_TypeDef *spi = (SPI_TypeDef *)spi_base;
    int clk_id;

    if (config == 0) {
        return;
    }

    clk_id = spi_to_clk_id(spi_base);
    if (clk_id != -1) {
        clk_enable(clk_id);
    }

    dtm_spi_disable(spi_base);

    spi->CR1 = 0;
    spi->CR2 = 0;

    if (config->mode & 0x01) {
        spi->CR1 |= SPI_CR1_CPHA;
    }
    if (config->mode & 0x02) {
        spi->CR1 |= SPI_CR1_CPOL;
    }

    if (config->datasize == DTM_SPI_DATASIZE_16BIT) {
        spi->CR1 |= SPI_CR1_DFF;
    }

    spi->CR1 |= (config->baudrate_prescaler & 0x7U) << SPI_CR1_BR_SHIFT;       

    if (config->firstbit) {
        spi->CR1 |= SPI_CR1_LSBFIRST;
    }

    spi->CR1 |= SPI_CR1_MSTR;

    spi->CR1 |= SPI_CR1_SSM | SPI_CR1_SSI;

    dtm_spi_enable(spi_base);
}

void dtm_spi_enable(uint32_t spi_base)
{
    SPI_TypeDef *spi = (SPI_TypeDef *)spi_base;
    spi->CR1 |= SPI_CR1_SPE;
}

void dtm_spi_disable(uint32_t spi_base)
{
    SPI_TypeDef *spi = (SPI_TypeDef *)spi_base;
    spi->CR1 &= ~SPI_CR1_SPE;
}

uint8_t dtm_spi_transmit_byte(uint32_t spi_base, uint8_t data)
{
    SPI_TypeDef *spi = (SPI_TypeDef *)spi_base;
    uint8_t rx = 0xFF;
    int txe_rc, rxne_rc;

    txe_rc = spi_wait_txe(spi);
    if (txe_rc != 0) {
        printf("[spi] tx_byte: TXE timeout SR=0x%08lX\n", (unsigned long)spi->SR);
        return rx;
    }

    *(volatile uint8_t *)&spi->DR = data;

    rxne_rc = spi_wait_rxne(spi);
    if (rxne_rc != 0) {
        printf("[spi] tx_byte: RXNE timeout SR=0x%08lX (sent=0x%02X)\n",       
               (unsigned long)spi->SR, data);
        return rx;
    }

    spi_wait_not_busy(spi);

    rx = *(volatile uint8_t *)&spi->DR;
    return rx;
}

uint16_t dtm_spi_transmit_halfword(uint32_t spi_base, uint16_t data)
{
    SPI_TypeDef *spi = (SPI_TypeDef *)spi_base;
    uint16_t rx = 0xFFFF;

    if (spi_wait_txe(spi) != 0) {
        return rx;
    }

    *(volatile uint16_t *)&spi->DR = data;

    if (spi_wait_rxne(spi) != 0) {
        return rx;
    }

    spi_wait_not_busy(spi);

    return *(volatile uint16_t *)&spi->DR;
}

void dtm_spi_transmit(uint32_t spi_base, const uint8_t *data, uint32_t length) 
{
    uint32_t i;
    for (i = 0; i < length; i++) {
        dtm_spi_transmit_byte(spi_base, data[i]);
    }
}

void dtm_spi_receive(uint32_t spi_base, uint8_t *data, uint32_t length)        
{
    uint32_t i;
    for (i = 0; i < length; i++) {
        data[i] = dtm_spi_transmit_byte(spi_base, 0xFF);
    }
}

void dtm_spi_transmit_receive(uint32_t spi_base, const uint8_t *tx_data, uint8_t *rx_data, uint32_t length)
{
    uint32_t i;
    for (i = 0; i < length; i++) {
        rx_data[i] = dtm_spi_transmit_byte(spi_base, tx_data[i]);
    }
}

#endif /* DTM_BINFDING_SPI_ARM_M4_ST_GD */
