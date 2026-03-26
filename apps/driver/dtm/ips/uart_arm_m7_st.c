#include "dtm_bindings.h"

#ifdef DTM_BINFDING_UART_ARM_M7_ST
#include "dtm_uart.h"
#include "dtm_gpio.h"
#include "dtm_clk.h"
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

#define USART_X ((USART_TypeDef *)DTM_DEBUG_UART) /* Define the base address of USART1, change to DTM_UART2_BASE for USART2 */

/* UART接收回调函数指针 */
static dtm_uart_rx_callback_t uart_rx_callback = NULL;

/* 接收数据缓存 */
static volatile uint8_t uart_rx_data = 0;

int dtm_uart_putc(char ch)
{
    while ((USART_X->ISR & 0X80) == 0);     /* Wait for the previous character to be sent (TXE flag) */

    USART_X->TDR = (uint8_t)ch;             /* Write the character ch to the TDR register to be sent */
    return ch;
}

/**
 * @brief       将UART基地址转换为对应的时钟ID
 * @param       uart_base: UART基地址
 * @return      对应的时钟ID
 */
static int uart_to_clk_id(uint32_t uart_base)
{
    int clk_id = -1;
    
    switch (uart_base) {
        case DTM_UART1_BASE: clk_id = CLK_UART1; break;
        case DTM_UART2_BASE: clk_id = CLK_UART2; break;
        case DTM_UART3_BASE: clk_id = CLK_UART3; break;
        case DTM_UART4_BASE: clk_id = CLK_UART4; break;
        case DTM_UART5_BASE: clk_id = CLK_UART5; break;
        case DTM_UART7_BASE: clk_id = CLK_UART7; break;
        case DTM_UART8_BASE: clk_id = CLK_UART8; break;
        default: break;
    }
    
    return clk_id;
}

/**
 * @brief       USARTX initialization function
 * @param       baudrate: Baud rate, set the baud rate value according to your needs
 * @retval      int: 0 if initialization succeeds, -1 otherwise
 */
void dtm_uart_init(uint32_t baudrate)
{
    uint32_t pclk;
    uint32_t usartdiv;
    int clk_id;
    
    /* Check the baud rate parameter */
    if (baudrate == 0) {
        return;
    }
    
    /* IO and clock configuration */

    dtm_gpio_set(DTM_DEBUG_UART_TX_PIN,
                 DTM_GPIO_MODE_AF, DTM_GPIO_OTYPE_PP, DTM_GPIO_SPEED_MID, DTM_GPIO_PUPD_PU);    /* Set the mode of the TX pin */

    dtm_gpio_set(DTM_DEBUG_UART_RX_PIN,
                 DTM_GPIO_MODE_AF, DTM_GPIO_OTYPE_PP, DTM_GPIO_SPEED_MID, DTM_GPIO_PUPD_PU);    /* Set the mode of the RX pin */

    dtm_gpio_af_set(DTM_DEBUG_UART_TX_PIN, DTM_DEBUG_UART_TX_AF);    /* Select the TX pin multiplexing function, must be set correctly */
    dtm_gpio_af_set(DTM_DEBUG_UART_RX_PIN, DTM_DEBUG_UART_RX_AF);    /* Select the RX pin multiplexing function, must be set correctly */

    /* 使能UART时钟并获取时钟频率 */
    clk_id = uart_to_clk_id(DTM_DEBUG_UART);
    if (clk_id != -1) {
        clk_enable(clk_id);
        pclk = clk_get_rate(clk_id);  /* 获取时钟频率，单位Hz */
    } else {
        /* 默认值，用于兼容性 */
        /* 默认时钟频率，根据不同芯片可能需要调整 */
        /* UART1通常连接到APB2，其他UART通常连接到APB1 */
        pclk = (DTM_DEBUG_UART == DTM_UART1_BASE) ? 84000000 : 42000000;
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
