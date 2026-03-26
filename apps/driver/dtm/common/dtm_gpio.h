
#include <stdint.h>

/* Macro definitions for GPIO settings */
#define DTM_GPIO_MODE_IN            0       /* General input mode */
#define DTM_GPIO_MODE_OUT           1       /* General output mode */
#define DTM_GPIO_MODE_AF            2       /* AF function mode */
#define DTM_GPIO_MODE_AIN           3       /* Analog input mode */

#define DTM_GPIO_SPEED_LOW          0       /* GPIO speed (low speed, 2M) */
#define DTM_GPIO_SPEED_MID          1       /* GPIO speed (medium speed, 25M) */
#define DTM_GPIO_SPEED_FAST         2       /* GPIO speed (fast speed, 50M) */
#define DTM_GPIO_SPEED_HIGH         3       /* GPIO speed (high speed, 100M) */

#define DTM_GPIO_PUPD_NONE          0       /* No pull-up or pull-down */
#define DTM_GPIO_PUPD_PU            1       /* Pull-up */
#define DTM_GPIO_PUPD_PD            2       /* Pull-down */
#define DTM_GPIO_PUPD_RES           3       /* Reserved */

#define DTM_GPIO_OTYPE_PP           0       /* Push-pull output */
#define DTM_GPIO_OTYPE_OD           1       /* Open-drain output */

/* Macro definitions for GPIO pin position */
#define DTM_GPIO_PIN0               1<<0
#define DTM_GPIO_PIN1               1<<1
#define DTM_GPIO_PIN2               1<<2
#define DTM_GPIO_PIN3               1<<3
#define DTM_GPIO_PIN4               1<<4
#define DTM_GPIO_PIN5               1<<5
#define DTM_GPIO_PIN6               1<<6
#define DTM_GPIO_PIN7               1<<7
#define DTM_GPIO_PIN8               1<<8
#define DTM_GPIO_PIN9               1<<9
#define DTM_GPIO_PIN10              1<<10
#define DTM_GPIO_PIN11              1<<11
#define DTM_GPIO_PIN12              1<<12
#define DTM_GPIO_PIN13              1<<13
#define DTM_GPIO_PIN14              1<<14
#define DTM_GPIO_PIN15              1<<15

void dtm_gpio_af_set(uint32_t gpiox, uint16_t pinx, uint8_t afx);
void dtm_gpio_set(uint32_t gpiox, uint16_t pinx, uint32_t mode, uint32_t otype, uint32_t ospeed, uint32_t pupd);
void dtm_gpio_pin_set(uint32_t gpiox, uint16_t pinx, uint8_t status);
uint8_t dtm_gpio_pin_get(uint32_t gpiox, uint16_t pinx);
