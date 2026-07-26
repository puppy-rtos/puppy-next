#ifndef __DTM_RESOURCE_H__
#define __DTM_RESOURCE_H__

#include <stdint.h>
#include "shared/pup_dtm.h"

#define PUP_DTM_NAME_MAX             16
#define PUP_DTM_MAX_GPIO_DEVICES     4
#define PUP_DTM_MAX_SPI_BUSES        2
#define PUP_DTM_MAX_SPI_PINS         4
#define PUP_DTM_MAX_SPI_CS           4
#define PUP_DTM_MAX_FLASH_DEVICES    2

#define PUP_DTM_GPIO_DIR_IN          0
#define PUP_DTM_GPIO_DIR_OUT         1

#define PUP_DTM_GPIO_ACTIVE_LOW      0x00000001U

typedef struct {
    uint32_t gpio_base;
    uint16_t pin_mask;
    uint8_t af;
    uint8_t reserved;
} pup_dtm_pin_t;

typedef struct {
    uint32_t valid;
    uint32_t base;
    uint32_t irq_num;
    uint32_t baudrate;
    uint32_t clk_id;
    pup_dtm_pin_t tx;
    pup_dtm_pin_t rx;
} pup_dtm_uart_config_t;

typedef struct {
    uint32_t valid;
    uint32_t base;
    uint32_t hse_frequency;
    uint32_t sysclk_frequency;
    uint32_t pll_m;
    uint32_t pll_n;
    uint32_t pll_p;
    uint32_t pll_q;
    uint32_t hclk_divider;
    uint32_t pclk1_divider;
    uint32_t pclk2_divider;
    uint32_t pclk3_divider;
    uint32_t pclk4_divider;
} pup_dtm_clock_config_t;

typedef struct {
    uint32_t valid;
    uint32_t syscfg_base;
    uint32_t exti_base;
    uint32_t irq_num;
    uint32_t line;
    uint32_t port;
    uint32_t priority;
} pup_dtm_exti_config_t;

typedef struct {
    uint32_t valid;
    char name[PUP_DTM_NAME_MAX];
    uint32_t gpio_base;
    uint16_t pin_mask;
    uint8_t device_id;
    uint8_t direction;
    uint8_t pull_up;
    uint8_t flags;
    pup_dtm_exti_config_t exti;
} pup_dtm_gpio_device_t;

typedef struct {
    uint8_t cs_id;
    uint8_t flags;
    uint16_t pin_mask;
    uint32_t gpio_base;
} pup_dtm_spi_cs_t;

typedef struct {
    uint32_t valid;
    char name[PUP_DTM_NAME_MAX];
    uint32_t base;
    uint8_t device_id;
    uint8_t pin_count;
    uint8_t cs_count;
    uint8_t default_mode;
    uint8_t default_datasize;
    uint8_t default_prescaler;
    uint8_t default_firstbit;
    uint8_t reserved;
    pup_dtm_pin_t pins[PUP_DTM_MAX_SPI_PINS];
    pup_dtm_spi_cs_t cs[PUP_DTM_MAX_SPI_CS];
} pup_dtm_spi_bus_t;

typedef struct {
    uint32_t valid;
    char name[PUP_DTM_NAME_MAX];
    uint8_t device_id;
    uint8_t spi_bus_index;
    uint8_t cs_id;
    uint8_t mode;
    uint8_t datasize;
    uint8_t prescaler;
    uint8_t firstbit;
    uint8_t reserved;
} pup_dtm_flash_device_t;

#endif
