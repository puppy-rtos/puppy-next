#include "dtm_bindings.h"

#ifdef DTM_BINFDING_UART_ARM_M7_ST
#include "dtm_uart.h"
#include "dtm_gpio.h"
#include "dtm_clk.h"
#include "dtm_board.h"
#include "dtm_of_driver.h"
#include "libcm.h"
#include "stdint.h"
#include <stddef.h>

typedef struct
{
  volatile uint32_t CR1;    /*!< USART Control register 1,                 Address offset: 0x00 */
  volatile uint32_t CR2;    /*!< USART Control register 2,                 Address offset: 0x04 */
  volatile uint32_t CR3;    /*!< USART Control register 3,                 Address offset: 0x08 */
  volatile uint32_t BRR;    /*!< USART Baud rate register,                 Address offset: 0x0C */
  volatile uint32_t GTPR;   /*!< USART Guard time and prescaler register,  Address offset: 0x10 */
  volatile uint32_t RTOR;   /*!< USART Receiver Time Out register,         Address offset: 0x14 */
  volatile uint32_t RQR;    /*!< USART Request register,                   Address offset: 0x18 */
  volatile uint32_t ISR;    /*!< USART Interrupt and status register,      Address offset: 0x1C */
  volatile uint32_t ICR;    /*!< USART Interrupt flag Clear register,      Address offset: 0x20 */
  volatile uint32_t RDR;    /*!< USART Receive Data register,              Address offset: 0x24 */
  volatile uint32_t TDR;    /*!< USART Transmit Data register,             Address offset: 0x28 */
  volatile uint32_t PRESC;  /*!< USART clock Prescaler register,           Address offset: 0x2C */
} USART_TypeDef;

/* USART register bit definitions */
#define USART_CR1_M            ((uint32_t)0x00001000)  /* Word length */
#define USART_CR1_PCE          ((uint32_t)0x00000400)  /* Parity control enable */
#define USART_CR1_PS           ((uint32_t)0x00000200)  /* Parity selection */
#define USART_CR1_TE           ((uint32_t)0x00000008)  /* Transmitter enable */
#define USART_CR1_RE           ((uint32_t)0x00000004)  /* Receiver enable */
#define USART_CR1_UE           ((uint32_t)0x00000001)  /* USART enable */
#define USART_CR1_OVER8        ((uint32_t)0x00008000)  /* Oversampling by 8 mode */
#define USART_CR1_RXNEIE       ((uint32_t)0x00000020)  /* RXNE interrupt enable */

#define USART_CR2_STOP         ((uint32_t)0x00003000)  /* STOP bits */
#define USART_CR2_STOP_1       ((uint32_t)0x00000000)  /* 1 stop bit */
#define USART_CR2_STOP_0_5     ((uint32_t)0x00001000)  /* 0.5 stop bit */
#define USART_CR2_STOP_2       ((uint32_t)0x00002000)  /* 2 stop bits */
#define USART_CR2_STOP_1_5     ((uint32_t)0x00003000)  /* 1.5 stop bits */

#define USART_CR3_CTSE         ((uint32_t)0x00000200)  /* CTS enable */
#define USART_CR3_RTSE         ((uint32_t)0x00000100)  /* RTS enable */

#define USART_PRESC_PRESCALER  ((uint32_t)0x0000000F)  /* Prescaler value */
#define USART_PRESC_PRESCALER_1 ((uint32_t)0x00000000) /* Prescaler = 1 */
#define USART_ISR_RXNE         ((uint32_t)0x00000020)
#define USART_ISR_ORE          ((uint32_t)0x00000008)
#define USART_ICR_ORECF        ((uint32_t)0x00000008)

static uint32_t g_uart_base = DTM_DEBUG_UART;

#define USART_X ((USART_TypeDef *)g_uart_base)

/* UART接收回调函数指针 */
static dtm_uart_rx_callback_t uart_rx_callback = NULL;

/* 接收数据缓存 */
static volatile uint8_t uart_rx_data = 0;

static int dtm_uart_arm_m7_st_of_probe(dtm_ofw_node_t node)
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

static const char *const g_dtm_uart_arm_m7_st_compatibles[] = {
    "puppy,uart-arm-m7-st",
    "st,stm32-uart",
};

const dtm_of_driver_t g_dtm_uart_arm_m7_st_of_driver = {
    "uart-arm-m7-st",
    g_dtm_uart_arm_m7_st_compatibles,
    DTM_ARRAY_SIZE(g_dtm_uart_arm_m7_st_compatibles),
    dtm_uart_arm_m7_st_of_probe,
};

int dtm_uart_putc(char ch)
{
    while ((USART_X->ISR & 0X80) == 0);     /* Wait for the previous character to be sent (TXE flag) */

    USART_X->TDR = (uint8_t)ch;             /* Write the character ch to the TDR register to be sent */
    return ch;
}

static int uart_to_clk_id(uint32_t uart_base)
{
    switch (uart_base) {
        case DTM_UART1_BASE: return CLK_UART1;
        case DTM_UART2_BASE: return CLK_UART2;
        case DTM_UART3_BASE: return CLK_UART3;
        case DTM_UART4_BASE: return CLK_UART4;
        case DTM_UART5_BASE: return CLK_UART5;
        case DTM_UART6_BASE: return CLK_UART6;
        case DTM_UART7_BASE: return CLK_UART7;
        case DTM_UART8_BASE: return CLK_UART8;
        default: return -1;
    }
}

/**
 * @brief       USARTX initialization function
 * @param       baudrate: Baud rate, set the baud rate value according to your needs
 * @retval      int: 0 if initialization succeeds, -1 otherwise
 */
void dtm_uart_init(uint32_t baudrate)
{
    uint32_t pclk = 0;
    uint32_t usartdiv;
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
    
    /* IO and clock configuration */

    dtm_gpio_set(tx_pin.gpio_base, tx_pin.pin_mask,
                 DTM_GPIO_MODE_AF, DTM_GPIO_OTYPE_PP, DTM_GPIO_SPEED_MID, DTM_GPIO_PUPD_PU);    /* Set the mode of the TX pin */

    dtm_gpio_set(rx_pin.gpio_base, rx_pin.pin_mask,
                 DTM_GPIO_MODE_AF, DTM_GPIO_OTYPE_PP, DTM_GPIO_SPEED_MID, DTM_GPIO_PUPD_PU);    /* Set the mode of the RX pin */

    dtm_gpio_af_set(tx_pin.gpio_base, tx_pin.pin_mask, tx_pin.af);
    dtm_gpio_af_set(rx_pin.gpio_base, rx_pin.pin_mask, rx_pin.af);

    /* 使能UART时钟并获取时钟频率 */
    if (clk_id == -1) {
        clk_id = uart_to_clk_id(g_uart_base);
    }
    if (clk_id != -1 && clk_enable(clk_id) == 0) {
        pclk = clk_get_rate(clk_id);  /* 获取时钟频率，单位Hz */
    }

    /* Ensure clock is valid */
    if (pclk == 0) {
        return;
    }

    /*-------------------------- USART CR1 Configuration -----------------------*/
    /* Clear M, PCE, PS, TE, RE and OVER8 bits */
    USART_X->CR1 &= ~(USART_CR1_M | USART_CR1_PCE | USART_CR1_PS | USART_CR1_TE | USART_CR1_RE | USART_CR1_OVER8);
    
    /* Configure word length: 8-bit */
    USART_X->CR1 &= ~USART_CR1_M;
    
    /* Configure parity: None */
    USART_X->CR1 &= ~USART_CR1_PCE;
    
    /* Configure oversampling: 16x */
    USART_X->CR1 &= ~USART_CR1_OVER8;
    
    /* Configure mode: TX and RX */
    USART_X->CR1 |= USART_CR1_TE;
    USART_X->CR1 |= USART_CR1_RE;

    /*-------------------------- USART CR2 Configuration -----------------------*/
    /* Clear STOP bits */
    USART_X->CR2 &= ~USART_CR2_STOP;
    
    /* Configure stop bits: 1 stop bit */
    USART_X->CR2 |= USART_CR2_STOP_1;

    /*-------------------------- USART CR3 Configuration -----------------------*/
    /* Clear CTSE and RTSE bits */
    USART_X->CR3 &= ~(USART_CR3_CTSE | USART_CR3_RTSE);
    
    /* Configure hardware flow control: None */
    USART_X->CR3 &= ~(USART_CR3_CTSE | USART_CR3_RTSE);

    /*-------------------------- USART PRESC Configuration -----------------------*/
    /* Clear PRESCALER bits */
    USART_X->PRESC &= ~USART_PRESC_PRESCALER;
    
    /* Configure clock prescaler: 1 */
    USART_X->PRESC |= USART_PRESC_PRESCALER_1;

    /*-------------------------- USART BRR Configuration -----------------------*/
    /* Calculate baud rate with rounding */
    if ((USART_X->CR1 & USART_CR1_OVER8) != 0) {
        /* 8x oversampling */
        usartdiv = (uint32_t)((((uint64_t)pclk * 2) + (baudrate / 2)) / baudrate);
        
        /* USARTDIV must be greater than or equal to 0d16 for 8x oversampling */
        if (usartdiv < 16) {
            return;
        }
        
        /* Adjust for 8x oversampling */
        USART_X->BRR = (usartdiv & 0xFFF0U) | ((usartdiv & 0x000FU) >> 1U);
    } else {
        /* 16x oversampling */
        usartdiv = (uint32_t)((((uint64_t)pclk) + (baudrate / 2)) / baudrate);
        
        /* USARTDIV must be greater than or equal to 0d16 for 16x oversampling */
        if (usartdiv < 16) {
            return;
        }
        
        USART_X->BRR = usartdiv;
    }

    /*-------------------------- USART Enable -----------------------*/
    USART_X->CR1 |= USART_CR1_UE;   /* Enable USART */
}

void dtm_uart_set_rx_callback(dtm_uart_rx_callback_t callback)
{
    uart_rx_callback = callback;
}

void dtm_uart_enable_rx_it(void)
{
    USART_X->CR1 |= USART_CR1_RXNEIE;
}

void dtm_uart_disable_rx_it(void)
{
    USART_X->CR1 &= ~USART_CR1_RXNEIE;
}

uint8_t dtm_uart_getc(void)
{
    while ((USART_X->ISR & USART_ISR_RXNE) == 0) {
    }
    return (uint8_t)(USART_X->RDR & 0xFF);
}

static void dtm_uart_common_irq_handler(void)
{
    if (USART_X->ISR & USART_ISR_ORE) {
        USART_X->ICR = USART_ICR_ORECF;
    }

    if (USART_X->ISR & USART_ISR_RXNE) {
        uart_rx_data = (uint8_t)(USART_X->RDR & 0xFF);
        if (uart_rx_callback) {
            uart_rx_callback(uart_rx_data);
        }
    }
}

void USART1_IRQHandler(void)
{
    dtm_uart_common_irq_handler();
}

void USART2_IRQHandler(void)
{
    dtm_uart_common_irq_handler();
}

void USART3_IRQHandler(void)
{
    dtm_uart_common_irq_handler();
}

void UART4_IRQHandler(void)
{
    dtm_uart_common_irq_handler();
}

void UART5_IRQHandler(void)
{
    dtm_uart_common_irq_handler();
}

void USART6_IRQHandler(void)
{
    dtm_uart_common_irq_handler();
}

void UART7_IRQHandler(void)
{
    dtm_uart_common_irq_handler();
}

void UART8_IRQHandler(void)
{
    dtm_uart_common_irq_handler();
}

#endif /* DTM_BINFDING_UART_ARM_M7_ST */
