
#include "dtm_bindings.h"

#ifdef DTM_BINFDING_GPIO_ARM_M4_ST_GD
#include "dtm_gpio.h"
#include "dtm_clk.h"

/* GPIO寄存器结构定义 */
typedef struct
{
  volatile uint32_t MODER;    /*!< GPIO port mode register,               Address offset: 0x00      */
  volatile uint32_t OTYPER;   /*!< GPIO port output type register,        Address offset: 0x04      */
  volatile uint32_t OSPEEDR;  /*!< GPIO port output speed register,       Address offset: 0x08      */
  volatile uint32_t PUPDR;    /*!< GPIO port pull-up/pull-down register,  Address offset: 0x0C      */
  volatile uint32_t IDR;      /*!< GPIO port input data register,         Address offset: 0x10      */
  volatile uint32_t ODR;      /*!< GPIO port output data register,        Address offset: 0x14      */
  volatile uint32_t BSRR;     /*!< GPIO port bit set/reset register,      Address offset: 0x18      */
  volatile uint32_t LCKR;     /*!< GPIO port configuration lock register, Address offset: 0x1C      */
  volatile uint32_t AFR[2];   /*!< GPIO alternate function registers,     Address offset: 0x20-0x24 */
} DTM_GPIO_Type;

#ifndef DTM_GPIO_BASE
#define DTM_GPIO_BASE 0x40020000UL
#endif

/**
 * @brief       GPIO alternate function selection setting
 * @param       p_gpiox: GPIOA~GPIOI, GPIO pointer
 * @param       pinx: 0X0000~0XFFFF, pin position, each bit represents an IO, the 0th bit represents Px0, the 1st bit represents Px1, and so on. For example, 0X0101 represents setting Px0 and Px8 at the same time.
 *   @arg       DTM_GPIO_PIN0~DTM_GPIO_PIN15, 1<<0 ~ 1<<15
 * @param       afx:0~15, representing AF0~AF15.
 *              AF0~15 settings (only common ones are listed here, for details, please refer to the STM32F407xx datasheet, Table 7):
 *   @arg       AF0:MCO/SWD/SWCLK/RTC       AF1:TIM1/TIM2               AF2:TIM3~5                  AF3:TIM8~11
 *   @arg       AF4:I2C1~I2C3               AF5:SPI1/SPI2/I2S2          AF6:SPI3/I2S3               AF7:USART1~3
 *   @arg       AF8:USART4~6                AF9;CAN1/CAN2/TIM12~14      AF10:USB_OTG/USB_HS         AF11:ETH
 *   @arg       AF12:FSMC/SDIO/OTG_FS       AF13:DCIM                   AF14:                       AF15:EVENTOUT
 * @retval      None
 */
void dtm_gpio_af_set(uint32_t gpiox, uint16_t pinx, uint8_t afx)
{
    DTM_GPIO_Type *p_gpiox = (DTM_GPIO_Type *)gpiox; /* Cast to GPIO type */
    uint32_t pinpos = 0, pos = 0, curpin = 0;

    for (pinpos = 0; pinpos < 16; pinpos++)
    {
        pos = 1 << pinpos;      /* Check each bit */
        curpin = pinx & pos;    /* Check if the pin needs to be set */

        if (curpin == pos)      /* Need to set */
        {
            p_gpiox->AFR[pinpos >> 3] &= ~(0X0FUL << ((pinpos & 0X07) * 4));
            p_gpiox->AFR[pinpos >> 3] |= (uint32_t)afx << ((pinpos & 0X07) * 4);
        }
    }
}

/**
 * @brief       将GPIO端口号转换为对应的时钟ID
 * @param       gpiox: GPIOA~GPIOI的基地址
 * @return      对应的时钟ID
 */
static int gpio_to_clk_id(uint32_t gpiox)
{
    int clk_id = -1;
    
    switch (gpiox) {
        case GPIOA: clk_id = CLK_GPIOA; break;
        case GPIOB: clk_id = CLK_GPIOB; break;
        case GPIOC: clk_id = CLK_GPIOC; break;
        case GPIOD: clk_id = CLK_GPIOD; break;
        case GPIOE: clk_id = CLK_GPIOE; break;
        case GPIOF: clk_id = CLK_GPIOF; break;
        case GPIOG: clk_id = CLK_GPIOG; break;
        case GPIOH: clk_id = CLK_GPIOH; break;
        case GPIOI: clk_id = CLK_GPIOI; break;
        default: break;
    }
    
    return clk_id;
}

/**
 * @brief       GPIO general configuration
 * @param       gpiox: GPIOA~GPIOI, GPIO pointer
 * @param       pinx: 0X0000~0XFFFF, pin position, each bit represents an IO, the 0th bit represents Px0, the 1st bit represents Px1, and so on. For example, 0X0101 represents setting Px0 and Px8 at the same time.
 *   @arg       DTM_GPIO_PIN0~DTM_GPIO_PIN15, 1<<0 ~ 1<<15
 *
 * @param       mode: 0~3; mode selection, set as follows:
 *   @arg       DTM_GPIO_MODE_IN,  0, input mode (default state after system reset)
 *   @arg       DTM_GPIO_MODE_OUT, 1, output mode
 *   @arg       DTM_GPIO_MODE_AF,  2, alternate function mode
 *   @arg       DTM_GPIO_MODE_AIN, 3, analog input mode
 *
 * @param       otype: 0 / 1; output type selection, set as follows:
 *   @arg       DTM_GPIO_OTYPE_PP, 0, push-pull output
 *   @arg       DTM_GPIO_OTYPE_OD, 1, open-drain output
 *
 * @param       ospeed: 0~3; output speed, set as follows:
 *   @arg       DTM_GPIO_SPEED_LOW,  0, low speed
 *   @arg       DTM_GPIO_SPEED_MID,  1, medium speed
 *   @arg       DTM_GPIO_SPEED_FAST, 2, fast speed
 *   @arg       DTM_GPIO_SPEED_HIGH, 3, high speed
 *
 * @param       pupd: 0~3: pull-up/pull-down setting, set as follows:
 *   @arg       DTM_GPIO_PUPD_NONE, 0, no pull-up/pull-down
 *   @arg       DTM_GPIO_PUPD_PU,   1, pull-up
 *   @arg       DTM_GPIO_PUPD_PD,   2, pull-down
 *   @arg       DTM_GPIO_PUPD_RES,  3, reserved
 *
 * @note:       Note: In input mode (normal input/analog input), OTYPE and OSPEED parameters are invalid!!
 * @retval      None
 */
void dtm_gpio_set(uint32_t gpiox, uint16_t pinx, uint32_t mode, uint32_t otype, uint32_t ospeed, uint32_t pupd)
{
    /* 使能GPIO时钟 */
    int clk_id = gpio_to_clk_id(gpiox);
    if (clk_id != -1) {
        clk_enable(clk_id);
    }
    
    DTM_GPIO_Type *p_gpiox = (DTM_GPIO_Type *)gpiox; /* Cast to GPIO type */
    uint32_t pinpos = 0, pos = 0, curpin = 0;

    for (pinpos = 0; pinpos < 16; pinpos++)
    {
        pos = 1 << pinpos;      /* Check each bit */
        curpin = pinx & pos;    /* Check if the pin needs to be set */

        if (curpin == pos)      /* Need to set */
        {
            p_gpiox->MODER &= ~(3UL << (pinpos * 2)); /* Clear previous settings */
            p_gpiox->MODER |= mode << (pinpos * 2); /* Set new mode */

            if ((mode == 0X01) || (mode == 0X02))   /* If it is output mode/alternate function mode */
            {
                p_gpiox->OSPEEDR &= ~(3UL << (pinpos * 2));       /* Clear previous settings */
                p_gpiox->OSPEEDR |= (ospeed << (pinpos * 2));   /* Set new speed value */
                p_gpiox->OTYPER &= ~(1 << pinpos) ;             /* Clear previous settings */
                p_gpiox->OTYPER |= otype << pinpos;             /* Set new output mode */
            }

            p_gpiox->PUPDR &= ~(3UL << (pinpos * 2)); /* Clear previous settings */
            p_gpiox->PUPDR |= pupd << (pinpos * 2); /* Set new pull-up/pull-down */
        }
    }
}

/**
 * @brief       Set the output status of a GPIO pin
 * @param       p_gpiox: GPIOA~GPIOI, GPIO pointer
 * @param       pinx: 0X0000~0XFFFF, pin position, each bit represents an IO, the 0th bit represents Px0, the 1st bit represents Px1, and so on. For example, 0X0101 represents setting Px0 and Px8 at the same time.
 *   @arg       DTM_GPIO_PIN0~DTM_GPIO_PIN15, 1<<0 ~ 1<<15
 * @param       status: 0/1, pin status (only the lowest bit is valid), set as follows:
 *   @arg       0, output low level
 *   @arg       1, output high level
 * @retval      None
 */
void dtm_gpio_pin_set(uint32_t gpiox, uint16_t pinx, uint8_t status)
{
    DTM_GPIO_Type *p_gpiox = (DTM_GPIO_Type *)gpiox; /* Cast to GPIO type */
    if (status & 0X01)
    {
        p_gpiox->BSRR |= pinx;  /* Set GPIOx pinx to 1 */
    }
    else
    {
        p_gpiox->BSRR |= (uint32_t)pinx << 16;  /* Set GPIOx pinx to 0 */
    }
}

/**
 * @brief       Read the status of a GPIO pin
 * @param       p_gpiox: GPIOA~GPIOG, GPIO pointer
 * @param       pinx: 0X0000~0XFFFF, pin position, each bit represents an IO, the 0th bit represents Px0, the 1st bit represents Px1, and so on. Only one GPIO can be read at a time!
 *   @arg       DTM_GPIO_PIN0~DTM_GPIO_PIN15, 1<<0 ~ 1<<15
 * @retval      Return pin status, 0, low level; 1, high level
 */
uint8_t dtm_gpio_pin_get(uint32_t gpiox, uint16_t pinx)
{
    DTM_GPIO_Type *p_gpiox = (DTM_GPIO_Type *)gpiox; /* Cast to GPIO type */

    if (p_gpiox->IDR & pinx)
    {
        return 1;   /* Pinx status is 1 */
    }
    else
    {
        return 0;   /* Pinx status is 0 */
    }
}

#endif /* DTM_BINFDING_GPIO_ARM_M4_ST_GD */
