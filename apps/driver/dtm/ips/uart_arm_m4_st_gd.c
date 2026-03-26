#include "dtm_bindings.h"

#ifdef DTM_BINFDING_UART_ARM_M4_ST_GD
#include "dtm_uart.h"
#include "dtm_gpio.h"
#include "dtm_clk.h"
#include "arch/arm/cortex-m4/dtm_irq.h"
#include <stddef.h>

typedef struct
{
  volatile uint32_t SR;         /*!< USART Status register,                   Address offset: 0x00 */
  volatile uint32_t DR;         /*!< USART Data register,                     Address offset: 0x04 */
  volatile uint32_t BRR;        /*!< USART Baud rate register,                Address offset: 0x08 */
  volatile uint32_t CR1;        /*!< USART Control register 1,                Address offset: 0x0C */
  volatile uint32_t CR2;        /*!< USART Control register 2,                Address offset: 0x10 */
  volatile uint32_t CR3;        /*!< USART Control register 3,                Address offset: 0x14 */
  volatile uint32_t GTPR;       /*!< USART Guard time and prescaler register, Address offset: 0x18 */
} USART_TypeDef;

#define USART_X ((USART_TypeDef *)DTM_DEBUG_UART)

/* UART接收回调函数指针 */
static dtm_uart_rx_callback_t uart_rx_callback = NULL;

/* 接收数据缓存 */
static volatile uint8_t uart_rx_data = 0;

int dtm_uart_putc(char ch)
{
    while ((USART_X->SR & 0X40) == 0);
    USART_X->DR = (uint8_t)ch;
    return ch;
}

static int uart_to_clk_id(uint32_t uart_base)
{
    int clk_id = -1;
    
    switch (uart_base) {
        case DTM_UART1_BASE: clk_id = CLK_UART1; break;
        case DTM_UART2_BASE: clk_id = CLK_UART2; break;
        case DTM_UART3_BASE: clk_id = CLK_UART3; break;
        default: break;
    }
    
    return clk_id;
}

void dtm_uart_init(uint32_t baudrate)
{
    uint32_t temp;
    uint32_t sclk;
    int clk_id;
    
    dtm_gpio_set(DTM_DEBUG_UART_TX_PIN,
                 DTM_GPIO_MODE_AF, DTM_GPIO_OTYPE_PP, DTM_GPIO_SPEED_MID, DTM_GPIO_PUPD_PU);

    dtm_gpio_set(DTM_DEBUG_UART_RX_PIN,
                 DTM_GPIO_MODE_AF, DTM_GPIO_OTYPE_PP, DTM_GPIO_SPEED_MID, DTM_GPIO_PUPD_PU);

    dtm_gpio_af_set(DTM_DEBUG_UART_TX_PIN, DTM_DEBUG_UART_TX_AF);
    dtm_gpio_af_set(DTM_DEBUG_UART_RX_PIN, DTM_DEBUG_UART_RX_AF);

    clk_id = uart_to_clk_id(DTM_DEBUG_UART);
    if (clk_id != -1) {
        clk_enable(clk_id);
        sclk = clk_get_rate(clk_id) / 1000000;
    } else {
        sclk = (DTM_DEBUG_UART == DTM_UART1_BASE) ? 84 : 42;
    }

    temp = (sclk * 1000000 + baudrate / 2) / baudrate;
    USART_X->BRR = temp;
    USART_X->CR1 = 0;
    USART_X->CR1 |= 0 << 12;
    USART_X->CR1 |= 0 << 15;
    USART_X->CR1 |= 1 << 3;
    USART_X->CR1 |= 1 << 2;
    USART_X->CR1 |= 1 << 13;
}

void dtm_uart_set_rx_callback(dtm_uart_rx_callback_t callback)
{
    uart_rx_callback = callback;
}

void dtm_uart_enable_rx_it(void)
{
    USART_X->CR1 |= 1 << 5;
}

void dtm_uart_disable_rx_it(void)
{
    USART_X->CR1 &= ~(1 << 5);
}

uint8_t dtm_uart_getc(void)
{
    while ((USART_X->SR & 0X20) == 0);
    return (uint8_t)(USART_X->DR & 0xFF);
}

void USART1_IRQHandler(void)
{
    if (USART_X->SR & 0X20) {
        uart_rx_data = (uint8_t)(USART_X->DR & 0xFF);
        if (uart_rx_callback) {
            uart_rx_callback(uart_rx_data);
        }
    }
}

void USART2_IRQHandler(void)
{
    if (USART_X->SR & 0X20) {
        uart_rx_data = (uint8_t)(USART_X->DR & 0xFF);
        if (uart_rx_callback) {
            uart_rx_callback(uart_rx_data);
        }
    }
}

void USART3_IRQHandler(void)
{
    if (USART_X->SR & 0X20) {
        uart_rx_data = (uint8_t)(USART_X->DR & 0xFF);
        if (uart_rx_callback) {
            uart_rx_callback(uart_rx_data);
        }
    }
}

#endif /* DTM_BINFDING_UART_ARM_M4_ST_GD */
