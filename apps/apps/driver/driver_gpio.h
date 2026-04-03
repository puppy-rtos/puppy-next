#ifndef DRIVER_GPIO_H
#define DRIVER_GPIO_H

#include "shared/pup_devfs_proto.h"
#include "shared/pup_channel_abi.h"
#include "syscall.h"

#define GPIO_DIR_IN   0
#define GPIO_DIR_OUT  1

#define GPIO_IOCTL_SET_DIR      0x2001
#define GPIO_IOCTL_GET_DIR      0x2002
#define GPIO_IOCTL_SET_VALUE    0x2003
#define GPIO_IOCTL_GET_VALUE    0x2004

#define GPIO_DEV_COUNT 3

typedef struct {
    const char* name;
    uint32_t gpio_base;
    uint16_t pin_mask;
    int device_id;
    int direction;
    int value;
    int pull_up;
} gpio_user_dev_t;

extern gpio_user_dev_t g_devs[GPIO_DEV_COUNT];

void gpio_config(gpio_user_dev_t* dev);
void gpio_write_level(gpio_user_dev_t* dev, int value);
int  gpio_read_level(gpio_user_dev_t* dev);
int  gpio_handle_request(gpio_user_dev_t* dev, pup_devfs_proxy_payload_t* req);
gpio_user_dev_t* gpio_find_by_id(int device_id);
void gpio_handle_pulse(const pup_channel_msg_t* msg);
void gpio_irq_config(void);
int  gpio_register_one(gpio_user_dev_t* dev);

#endif /* DRIVER_GPIO_H */
