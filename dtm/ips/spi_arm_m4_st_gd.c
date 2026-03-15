#include "dtm_bindings.h"

#ifdef DTM_BINFDING_SPI_ARM_M4_ST_GD
#include "dtm_spi.h"
#include "dtm_gpio.h"
#include "dtm_clk.h"

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

/**
 * @brief       将SPI基地址转换为对应的时钟ID
 * @param       spi_base: SPI的基地址
 * @return      对应的时钟ID
 */
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

static void spi_wait_not_busy(SPI_TypeDef *spi)
{
    while (spi->SR & SPI_SR_BSY) {
    }
}

/**
 * @brief       初始化SPI
 * @param       spi_base: SPI的基地址
 * @param       config: SPI配置结构体
 * @retval      None
 */
void dtm_spi_init(uint32_t spi_base, const dtm_spi_config_t *config)
{
    SPI_TypeDef *spi = (SPI_TypeDef *)spi_base;
    int clk_id;

    if (config == 0) {
        return;
    }
    
    /* 使能SPI时钟 */
    clk_id = spi_to_clk_id(spi_base);
    if (clk_id != -1) {
        clk_enable(clk_id);
    }
    
    /* 禁用SPI */
    dtm_spi_disable(spi_base);
    
    /* 配置SPI CR1寄存器 */
    spi->CR1 = 0;
    spi->CR2 = 0;
    
    /* 设置SPI模式 */
    if (config->mode & 0x01) {
        spi->CR1 |= SPI_CR1_CPHA;
    }
    if (config->mode & 0x02) {
        spi->CR1 |= SPI_CR1_CPOL;
    }
    
    /* 设置数据大小 */
    if (config->datasize == DTM_SPI_DATASIZE_16BIT) {
        spi->CR1 |= SPI_CR1_DFF;
    }
    
    /* 设置波特率预分频器 */
    spi->CR1 |= (config->baudrate_prescaler & 0x7U) << SPI_CR1_BR_SHIFT;
    
    /* 设置第一个位 */
    if (config->firstbit) {
        spi->CR1 |= SPI_CR1_LSBFIRST;
    }
    
    /* 设置主模式 */
    spi->CR1 |= SPI_CR1_MSTR;

    /* 使用软件NSS，避免主机模式下出现MODF */
    spi->CR1 |= SPI_CR1_SSM | SPI_CR1_SSI;
    
    /* 启用SPI */
    dtm_spi_enable(spi_base);
}

/**
 * @brief       启用SPI
 * @param       spi_base: SPI的基地址
 * @retval      None
 */
void dtm_spi_enable(uint32_t spi_base)
{
    SPI_TypeDef *spi = (SPI_TypeDef *)spi_base;
    spi->CR1 |= SPI_CR1_SPE;
}

/**
 * @brief       禁用SPI
 * @param       spi_base: SPI的基地址
 * @retval      None
 */
void dtm_spi_disable(uint32_t spi_base)
{
    SPI_TypeDef *spi = (SPI_TypeDef *)spi_base;
    spi->CR1 &= ~SPI_CR1_SPE;
}

/**
 * @brief       发送一个字节
 * @param       spi_base: SPI的基地址
 * @param       data: 要发送的数据
 * @retval      接收到的数据
 */
uint8_t dtm_spi_transmit_byte(uint32_t spi_base, uint8_t data)
{
    SPI_TypeDef *spi = (SPI_TypeDef *)spi_base;
    
    /* 等待发送缓冲区为空 */
    while (!(spi->SR & SPI_SR_TXE));
    
    /* 发送数据 */
    *(volatile uint8_t *)&spi->DR = data;
    
    /* 等待接收缓冲区非空 */
    while (!(spi->SR & SPI_SR_RXNE));

    spi_wait_not_busy(spi);
    
    /* 返回接收到的数据 */
    return *(volatile uint8_t *)&spi->DR;
}

/**
 * @brief       发送一个半字（16位）
 * @param       spi_base: SPI的基地址
 * @param       data: 要发送的数据
 * @retval      接收到的数据
 */
uint16_t dtm_spi_transmit_halfword(uint32_t spi_base, uint16_t data)
{
    SPI_TypeDef *spi = (SPI_TypeDef *)spi_base;
    
    /* 等待发送缓冲区为空 */
    while (!(spi->SR & SPI_SR_TXE));
    
    /* 发送数据 */
    *(volatile uint16_t *)&spi->DR = data;
    
    /* 等待接收缓冲区非空 */
    while (!(spi->SR & SPI_SR_RXNE));

    spi_wait_not_busy(spi);
    
    /* 返回接收到的数据 */
    return *(volatile uint16_t *)&spi->DR;
}

/**
 * @brief       发送数据
 * @param       spi_base: SPI的基地址
 * @param       data: 要发送的数据
 * @param       length: 数据长度
 * @retval      None
 */
void dtm_spi_transmit(uint32_t spi_base, const uint8_t *data, uint32_t length)
{
    uint32_t i;
    for (i = 0; i < length; i++) {
        dtm_spi_transmit_byte(spi_base, data[i]);
    }
}

/**
 * @brief       接收数据
 * @param       spi_base: SPI的基地址
 * @param       data: 接收数据的缓冲区
 * @param       length: 数据长度
 * @retval      None
 */
void dtm_spi_receive(uint32_t spi_base, uint8_t *data, uint32_t length)
{
    uint32_t i;
    for (i = 0; i < length; i++) {
        data[i] = dtm_spi_transmit_byte(spi_base, 0xFF);
    }
}

/**
 * @brief       同时发送和接收数据
 * @param       spi_base: SPI的基地址
 * @param       tx_data: 要发送的数据
 * @param       rx_data: 接收数据的缓冲区
 * @param       length: 数据长度
 * @retval      None
 */
void dtm_spi_transmit_receive(uint32_t spi_base, const uint8_t *tx_data, uint8_t *rx_data, uint32_t length)
{
    uint32_t i;
    for (i = 0; i < length; i++) {
        rx_data[i] = dtm_spi_transmit_byte(spi_base, tx_data[i]);
    }
}

#endif /* DTM_BINFDING_SPI_ARM_M4_ST_GD */