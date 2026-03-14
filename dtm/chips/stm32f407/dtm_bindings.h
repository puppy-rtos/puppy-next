#pragma once

/*
 * 1. IP Description 
 */
/* CLK IP Description */
#define DTM_BINFDING_CLK_ARM_M4_ST_GD
#define DTM_CLK_BASE           0x40023800UL

/* GPIO IP Description */
#define DTM_BINFDING_GPIO_ARM_M4_ST_GD
#define DTM_GPIO_BASE          0x40020000UL
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
#define DTM_GPIOC_CLK_EN      (1 << 2)  // GPIOC clock enable
#define DTM_GPIOD_CLK_EN      (1 << 3)  // GPIOD clock enable
#define DTM_GPIOE_CLK_EN      (1 << 4)  // GPIOE clock enable
#define DTM_GPIOF_CLK_EN      (1 << 5)  // GPIOF clock enable
// UART CLK Definitions
#define DTM_UART1_CLK_EN      (1 << 4)  // USART1 clock enable
#define DTM_UART2_CLK_EN      (1 << 17) // USART2 clock enable
#define DTM_UART3_CLK_EN      (1 << 18) // USART3 clock enable

/* UART IP Description */
#define DTM_BINFDING_UART_ARM_M4_ST_GD
#define DTM_UART_BASE         0x40011000UL // UART base address
#define DTM_UART1_BASE        (DTM_UART_BASE + 0x0000) // UART1 base address
#define DTM_UART2_BASE        (DTM_UART_BASE + 0x0400) // UART2 base address
#define DTM_UART3_BASE        (DTM_UART_BASE + 0x0800) // UART3 base address

/*
 * 2. IP Description
 */
 /* pmctrl */
#define DTM_BINFDING_PMCTRL_ARM_M4_ST_GD
#define DTM_PMCTRL_BASE        0x40007000UL

/* flash */
#define DTM_BINFDING_FLASH_ARM_M4_ST_GD
#define DTM_FLASH_BASE         0x40023C00UL

/* pinctrl */
#define DTM_BINFDING_PINCTRL_ARM_M4_ST_GD
#define DTM_PINCTRL_BASE       0x40020000UL

/* mpu */
#define DTM_BINFDING_MPU_ARM_M4_ST_GD

/* Timer IP Description */
#define DTM_BINFDING_TIMER_ARM_M4_ST_GD
#define DTM_TIMER_BASE         0x40000000UL // TIM2基地址
#define TIM1                  0x40010000UL // TIM1 base address
#define TIM2                  0x40000000UL // TIM2 base address
#define TIM3                  0x40000400UL // TIM3 base address
#define TIM4                  0x40000800UL // TIM4 base address
#define TIM5                  0x40000C00UL // TIM5 base address
#define TIM6                  0x40001000UL // TIM6 base address
#define TIM7                  0x40001400UL // TIM7 base address
#define TIM8                  0x40010400UL // TIM8 base address
#define TIM9                  0x40014000UL // TIM9 base address
#define TIM10                 0x40014400UL // TIM10 base address
#define TIM11                 0x40014800UL // TIM11 base address
#define TIM12                 0x40001800UL // TIM12 base address
#define TIM13                 0x40001C00UL // TIM13 base address
#define TIM14                 0x40002000UL // TIM14 base address
// Timer Clk Definitions
#define DTM_TIM1_CLK_EN       (1 << 0)   // TIM1 clock enable
#define DTM_TIM2_CLK_EN       (1 << 0)   // TIM2 clock enable
#define DTM_TIM3_CLK_EN       (1 << 1)   // TIM3 clock enable
#define DTM_TIM4_CLK_EN       (1 << 2)   // TIM4 clock enable
#define DTM_TIM5_CLK_EN       (1 << 3)   // TIM5 clock enable
#define DTM_TIM6_CLK_EN       (1 << 4)   // TIM6 clock enable
#define DTM_TIM7_CLK_EN       (1 << 5)   // TIM7 clock enable
#define DTM_TIM8_CLK_EN       (1 << 1)   // TIM8 clock enable
#define DTM_TIM9_CLK_EN       (1 << 16)  // TIM9 clock enable
#define DTM_TIM10_CLK_EN      (1 << 17)  // TIM10 clock enable
#define DTM_TIM11_CLK_EN      (1 << 18)  // TIM11 clock enable
#define DTM_TIM12_CLK_EN      (1 << 6)   // TIM12 clock enable
#define DTM_TIM13_CLK_EN      (1 << 7)   // TIM13 clock enable
#define DTM_TIM14_CLK_EN      (1 << 8)   // TIM14 clock enable
#define DTM_TIM2_IRQn         28          // TIM2_IRQn

/*
 * 3. IP Configuration
 */
 /* Pmctrl IP configuration */
#define DTM_PMCTRL_HIGHPERF_MODE (3 << 14) // 高性能模式，时钟可达168MHz

 /* 内存配置 */
#define DTM_RAM_SIZE          (192 * 1024)  // 192KB RAM
#define DTM_STACK_SIZE        (16 * 1024)   // 16KB栈

/* CLK IP Configuration */
#define DTM_HSE_FREQ           8000000      // 8MHz外部晶振
#define DTM_SYSCLK_FREQ        168000000    // 目标系统时钟168MHz

/* UART配置 */
#define DTM_DEBUG_UART         DTM_UART1_BASE
#define DTM_DEBUG_UART_CLK_EN  DTM_UART1_CLK_EN // USART1 clock enable
#define DTM_DEBUG_UART_TX_PIN  GPIOA, DTM_GPIO_PIN9 // USART1 TX pin (PA9)
#define DTM_DEBUG_UART_RX_PIN  GPIOA, DTM_GPIO_PIN10 // USART1 RX pin (PA10)
#define DTM_DEBUG_UART_TX_AF   7                // USART1 TX pin alternate function (AF7)
#define DTM_DEBUG_UART_RX_AF   7                // USART1 RX pin alternate function (AF7)
#define DTM_DEBUG_UART_IRQn    37               // USART1_IRQn

/* LED */
#define DTM_LED_PIN            GPIOF, DTM_GPIO_PIN11 // LED pin (PF11)
