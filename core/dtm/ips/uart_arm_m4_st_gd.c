#include "dtm_bindings.h"

#ifdef DTM_BINFDING_UART_ARM_M4_ST_GD
#include "dtm_uart.h"
#include "dtm_gpio.h"
#include "dtm_clk.h"
#include "dtm_board.h"
#include "dtm_of_driver.h"
#include "arch/arm/cortex-m4/dtm_irq.h"
#include "libcm.h"
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

static uint32_t g_uart_base = DTM_DEBUG_UART;

#define USART_X ((USART_TypeDef *)g_uart_base)

/* UART接收回调函数指针 */
static dtm_uart_rx_callback_t uart_rx_callback = NULL;

/* 接收数据缓存 */
static volatile uint8_t uart_rx_data = 0;

static int dtm_uart_arm_m4_st_gd_of_probe(dtm_ofw_node_t node)
{
    pup_dtm_uart_config_t config;
    uint64_t addr = 0;
    uint32_t value;
    pup_dtm_pin_t pins[2];
    int pin_count;

    if (dtm_ofw_chosen_stdout() != node ||
        dtm_ofw_get_reg(node, 0, &addr, 0) < 0) {
        return -1;
    }

    if (dtm_board_get_console(&config) != 0) {
        libcm_memset(&config, 0, sizeof(config));
    }

    config.valid = 1U;
    config.base = (uint32_t)addr;

    if (dtm_ofw_prop_read_u32(node, "current-speed", &value) == 0) {
        config.baudrate = value;
    }
    if (dtm_ofw_prop_read_u32_index(node, "interrupts", 0, &value) == 0) {
        config.irq_num = value;
    }
    if (dtm_of_parse_clock_id(node, "clocks", 0, &value) == 0) {
        config.clk_id = value;
    }

    libcm_memset(pins, 0, sizeof(pins));
    pin_count = dtm_of_parse_pinctrl(node, pins, 2U);
    if (pin_count >= 2) {
        config.tx = pins[0];
        config.rx = pins[1];
    }

    dtm_board_set_console(&config);
    return 0;
}

static const char *const g_dtm_uart_arm_m4_st_gd_compatibles[] = {
    "puppy,uart-arm-m4-st-gd",
    "st,stm32-uart",
};

const dtm_of_driver_t g_dtm_uart_arm_m4_st_gd_of_driver = {
    "uart-arm-m4-st-gd",
    g_dtm_uart_arm_m4_st_gd_compatibles,
    DTM_ARRAY_SIZE(g_dtm_uart_arm_m4_st_gd_compatibles),
    dtm_uart_arm_m4_st_gd_of_probe,
};

int dtm_uart_putc(char ch)
{
    while ((USART_X->SR & 0X40) == 0);
    USART_X->DR = (uint8_t)ch;
    return ch;
}

void dtm_uart_init(uint32_t baudrate)
{
    uint32_t temp;
    uint32_t sclk = 0;
    int clk_id = -1;
    pup_dtm_uart_config_t config;
    uint32_t uart_base = DTM_DEBUG_UART;
    pup_dtm_pin_t tx_pin = { DTM_DEBUG_UART_TX_PIN, DTM_DEBUG_UART_TX_AF, 0 };
    pup_dtm_pin_t rx_pin = { DTM_DEBUG_UART_RX_PIN, DTM_DEBUG_UART_RX_AF, 0 };

    if (dtm_board_get_console(&config) == 0 && config.valid) {
        uart_base = config.base;
        tx_pin = config.tx;
        rx_pin = config.rx;
        if (config.clk_id > 0U && config.clk_id < CLK_COUNT) {
            clk_id = (int)config.clk_id;
        }
        if (baudrate == 0U) {
            baudrate = config.baudrate;
        }
    }

    if (baudrate == 0U) {
        baudrate = 115200U;
    }

    g_uart_base = uart_base;
    
    dtm_gpio_set(tx_pin.gpio_base, tx_pin.pin_mask,
                 DTM_GPIO_MODE_AF, DTM_GPIO_OTYPE_PP, DTM_GPIO_SPEED_MID, DTM_GPIO_PUPD_PU);

    dtm_gpio_set(rx_pin.gpio_base, rx_pin.pin_mask,
                 DTM_GPIO_MODE_AF, DTM_GPIO_OTYPE_PP, DTM_GPIO_SPEED_MID, DTM_GPIO_PUPD_PU);

    dtm_gpio_af_set(tx_pin.gpio_base, tx_pin.pin_mask, tx_pin.af);
    dtm_gpio_af_set(rx_pin.gpio_base, rx_pin.pin_mask, rx_pin.af);

    if (clk_id == -1) {
        return;
    }
    if (clk_id != -1 && clk_enable(clk_id) == 0) {
        sclk = clk_get_rate(clk_id) / 1000000;
    }

    if (sclk == 0U) {
        return;
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
