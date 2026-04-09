#pragma once

/*
 * 1. IP Description 
 */
/* CLK IP Description */
#define DTM_BINFDING_CLK_ARM_M7_ST
#define DTM_CLK_BASE           0x58024400UL

/* GPIO IP Description */
#define DTM_BINFDING_GPIO_ARM_M4_ST_GD
#define DTM_GPIO_BASE          0x58020000UL
#define GPIOA                  (DTM_GPIO_BASE + 0x0000)
#define GPIOB                  (DTM_GPIO_BASE + 0x0400)
#define GPIOC                  (DTM_GPIO_BASE + 0x0800)
#define GPIOD                  (DTM_GPIO_BASE + 0x0C00)
#define GPIOE                  (DTM_GPIO_BASE + 0x1000)
#define GPIOF                  (DTM_GPIO_BASE + 0x1400)
#define GPIOG                  (DTM_GPIO_BASE + 0x1800)
#define GPIOH                  (DTM_GPIO_BASE + 0x1C00)
#define GPIOI                  (DTM_GPIO_BASE + 0x2000)
#define GPIOJ                  (DTM_GPIO_BASE + 0x2400)
#define GPIOK                  (DTM_GPIO_BASE + 0x2800)
#define DTM_GPIOA_CLK_EN      (1 << 0)
#define DTM_GPIOB_CLK_EN      (1 << 1)
#define DTM_GPIOC_CLK_EN      (1 << 2)
#define DTM_GPIOD_CLK_EN      (1 << 3)
#define DTM_GPIOE_CLK_EN      (1 << 4)
#define DTM_GPIOF_CLK_EN      (1 << 5)
#define DTM_UART1_CLK_EN      (1 << 4)
#define DTM_UART2_CLK_EN      (1 << 17)
#define DTM_UART3_CLK_EN      (1 << 18)

/* UART IP Description */
#define DTM_BINFDING_UART_ARM_M7_ST
#define DTM_UART1_BASE        0x40011000UL
#define DTM_UART2_BASE        0x40004400UL
#define DTM_UART3_BASE        0x40004800UL
#define DTM_UART4_BASE        0x40004C00UL
#define DTM_UART5_BASE        0x40005000UL
#define DTM_UART6_BASE        0x40011400UL
#define DTM_UART7_BASE        0x40007800UL
#define DTM_UART8_BASE        0x40007C00UL

/*
 * 2. IP Description
 */
/* pmctrl */
#define DTM_BINFDING_PMCTRL_ARM_M7_ST_GD
#define DTM_PMCTRL_BASE        0x58024800UL

/* flash */
#define DTM_BINFDING_FLASH_ARM_M4_ST_GD
#define DTM_FLASH_BASE         0x52002000UL

/* pinctrl */
#define DTM_BINFDING_PINCTRL_ARM_M7_ST_GD
#define DTM_PINCTRL_BASE       DTM_GPIO_BASE

/* mpu */
#define DTM_BINFDING_MPU_ARM_M4_ST_GD

/* Timer IP Description */
#define DTM_BINFDING_TIMER_ARM_M4_ST_GD
#define DTM_TIMER_BASE         0x40000000UL
#define TIM1                  0x40010000UL
#define TIM2                  0x40000000UL
#define TIM3                  0x40000400UL
#define TIM4                  0x40000800UL
#define TIM5                  0x40000C00UL
#define TIM6                  0x40001000UL
#define TIM7                  0x40001400UL
#define TIM8                  0x40010400UL
#define TIM9                  0x40014000UL
#define TIM10                 0x40014400UL
#define TIM11                 0x40014800UL
#define TIM12                 0x40001800UL
#define TIM13                 0x40001C00UL
#define TIM14                 0x40002000UL
#define DTM_TIM1_CLK_EN       (1 << 0)
#define DTM_TIM2_CLK_EN       (1 << 0)
#define DTM_TIM3_CLK_EN       (1 << 1)
#define DTM_TIM4_CLK_EN       (1 << 2)
#define DTM_TIM5_CLK_EN       (1 << 3)
#define DTM_TIM6_CLK_EN       (1 << 4)
#define DTM_TIM7_CLK_EN       (1 << 5)
#define DTM_TIM8_CLK_EN       (1 << 1)
#define DTM_TIM2_IRQn         28

/* SPI IP Description */
#define DTM_BINFDING_SPI_ARM_M4_ST_GD
#define DTM_SPI1_BASE         0x40013000UL
#define DTM_SPI2_BASE         0x40003800UL
#define DTM_SPI3_BASE         0x40003C00UL
#define DTM_SPI4_BASE         0x40013400UL
#define DTM_SPI1_CLK_EN       (1 << 12)
#define DTM_SPI2_CLK_EN       (1 << 14)
#define DTM_SPI3_CLK_EN       (1 << 15)

/*
 * 3. IP Configuration
 */
/* Pmctrl IP configuration */
#define DTM_PMCTRL_HIGHPERF_MODE (3 << 14)
#define DTM_PMCTRL_VOS_RDY_FLAG  (1 << 13)

/* 内存配置 */
#define DTM_RAM_SIZE          (512 * 1024)
#define DTM_STACK_SIZE        (16 * 1024)

/* CLK IP Configuration - HSI only, no external crystal */
#define DTM_HSE_FREQ           0
#define DTM_SYSCLK_FREQ        480000000
#define DTM_PLLM               32
#define DTM_PLLN               300
#define DTM_PLLP               2
#define DTM_PLL_SRC_HSI        1

/* UART配置 - USART3 on PD8/PD9 */
#define DTM_DEBUG_UART         DTM_UART3_BASE
#define DTM_DEBUG_UART_CLK_EN  DTM_UART3_CLK_EN
#define DTM_DEBUG_UART_TX_PIN  GPIOD, DTM_GPIO_PIN8
#define DTM_DEBUG_UART_RX_PIN  GPIOD, DTM_GPIO_PIN9
#define DTM_DEBUG_UART_TX_AF   7
#define DTM_DEBUG_UART_RX_AF   7
#define DTM_DEBUG_UART_IRQn    39

/* LED - PB0 */
#define DTM_LED_PIN            GPIOB, DTM_GPIO_PIN0

/* Disable SPI and Flash for M7 build */
#define DTM_NO_SPI_FLASH       1
