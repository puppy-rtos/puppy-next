#ifndef __DEV_GPIO_H__
#define __DEV_GPIO_H__

#include <puppy_core.h>
#include "pup_devfs.h"

typedef struct {
    uint32_t gpiox;
    uint16_t pinx;
} gpio_pin_t;

#define GPIO_IOCTL_SET_DIR      0x2001
#define GPIO_IOCTL_GET_DIR      0x2002
#define GPIO_IOCTL_SET_VALUE    0x2003
#define GPIO_IOCTL_GET_VALUE    0x2004

#define GPIO_DIR_IN             0
#define GPIO_DIR_OUT            1

int dev_gpio_irq_bridge_init(void);

#endif
