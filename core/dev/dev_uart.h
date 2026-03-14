#ifndef __DEV_UART_H__
#define __DEV_UART_H__

#include <puppy_core.h>
#include "pup_devfs.h"

#define UART_IOCTL_SET_BAUDRATE    0x1001
#define UART_IOCTL_GET_BAUDRATE    0x1002

int dev_uart_init(void);

#endif
