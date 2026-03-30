#pragma once

/*
 * 1. IP Description 
 */
/* CLK IP Description */
#define DTM_BINFDING_CLK_ARM_M7_GD
#define DTM_CLK_BASE           0x58024400UL

/* GPIO IP Description */
#define DTM_BINFDING_GPIO_ARM_M4_ST_GD
#define DTM_GPIO_BASE          0x58020000UL
#define GPIOA                  (DTM_GPIO_BASE + 0x0000) // GPIOA base address
#define GPIOB                  (DTM_GPIO_BASE + 0x0400) // GPIOB base address
#define GPIOC                  (DTM_GPIO_BASE + 0x0800) // GPIOC base address
#define GPIOD                  (DTM_GPIO_BASE + 0x0C00) // GPIOD base address
#define GPIOE                  (DTM_GPIO_BASE + 0x1000) // GPIOE base address
#define GPIOF                  (DTM_GPIO_BASE + 0x1400) // GPIOF base address
#define GPIOG                  (DTM_GPIO_BASE + 0x1800) // GPIOG base address
#define GPIOH                  (DTM_GPIO_BASE + 0x1C00) // GPIOH base address
#define GPIOI                  (DTM_GPIO_BASE + 0x2000) // GPIOI base address
#define GPIOJ                  (DTM_GPIO_BASE + 0x2400) // GPIOJ base address
#define GPIOK                  (DTM_GPIO_BASE + 0x2800) // GPIOK base address
// GPIO Clk Definitions
#define DTM_GPIOA_CLK_EN      (1 << 0)  // GPIOA clock enable
#define DTM_GPIOB_CLK_EN      (1 << 1)  // GPIOB clock enable
#define DTM_GPIOF_CLK_EN      (1 << 5)  // GPIOF clock enable
// UART CLK Definitions
#define DTM_UART1_CLK_EN      (1 << 4)  // USART1 clock enable
#define DTM_UART2_CLK_EN      (1 << 17) // USART2 clock enable
#define DTM_UART3_CLK_EN      (1 << 18) // USART3 clock enable
#define DTM_UART4_CLK_EN      (1 << 19) // USART4 clock enable

/* UART IP Description */
#define DTM_BINFDING_UART_ARM_M7_ST
#define DTM_UART1_BASE        (0x40011000UL) // UART1 base address
#define DTM_UART2_BASE        (0x40004400UL) // UART2 base address
#define DTM_UART3_BASE        (0x40004800UL) // UART3 base address
#define DTM_UART4_BASE        (0x40004C00UL) // UART4 base address
#define DTM_UART5_BASE        (0x40005000UL) // UART5 base address
#define DTM_UART6_BASE        (0x40011400UL) // UART6 base address
#define DTM_UART7_BASE        (0x40007800UL) // UART7 base address
#define DTM_UART8_BASE        (0x40007C00UL) // UART8 base address

 /* 内存配置 */
#define DTM_RAM_SIZE          (192 * 1024)  // 192KB RAM
#define DTM_STACK_SIZE        (16 * 1024)   // 16KB栈

/* CLK IP Configuration */
#define DTM_HSE_FREQ           25000000      // 25MHz外部晶振
#define DTM_SYSCLK_FREQ        200000000    // 目标系统时钟200MHz

/* UART配置 */
#define DTM_DEBUG_UART         DTM_UART1_BASE
#define DTM_DEBUG_UART_CLK_EN  DTM_UART1_CLK_EN // USART1 clock enable
#define DTM_DEBUG_UART_TX_PIN  GPIOA, DTM_GPIO_PIN9 // USART1 TX pin (PA9)
#define DTM_DEBUG_UART_RX_PIN  GPIOA, DTM_GPIO_PIN10 // USART1 RX pin (PA10)
#define DTM_DEBUG_UART_TX_AF   7                // USART1 TX pin alternate function (AF7)
#define DTM_DEBUG_UART_RX_AF   7                // USART1 RX pin alternate function (AF7)
#define DTM_DEBUG_UART_IRQn    37               // USART1_IRQn

/* LED */
#define DTM_LED_PIN            GPIOD, DTM_GPIO_PIN11 // LED pin (GPIOD11)
