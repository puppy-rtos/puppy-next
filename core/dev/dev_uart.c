#include "dev_uart.h"
#include "puppy_log.h"
#include "dtm_uart.h"
#include <libcm.h>

typedef struct {
    uint32_t baudrate;
} uart_dev_data_t;

static uart_dev_data_t uart0_data = {
    .baudrate = 115200
};

static int uart_open(pup_devfs_file_t *file)
{
    PUP_LOG_DEBUG(PUP_LOG_MODULE_DEV, "uart_open");
    return PUP_DEVFS_OK;
}

static int uart_close(pup_devfs_file_t *file)
{
    PUP_LOG_DEBUG(PUP_LOG_MODULE_DEV, "uart_close");
    return PUP_DEVFS_OK;
}

static int uart_read(pup_devfs_file_t *file, char *buf, int len)
{
    PUP_LOG_DEBUG(PUP_LOG_MODULE_DEV, "uart_read: len=%d", len);
    
    for (int i = 0; i < len; i++) {
        buf[i] = (char)dtm_uart_getc();
    }
    
    return len;
}

static int uart_write(pup_devfs_file_t *file, const char *buf, int len)
{
    PUP_LOG_DEBUG(PUP_LOG_MODULE_DEV, "uart_write: len=%d", len);
    
    for (int i = 0; i < len; i++) {
        if (buf[i] == '\n') {
            dtm_uart_putc('\r');
        }
        dtm_uart_putc(buf[i]);
    }
    
    return len;
}

static int uart_ioctl(pup_devfs_file_t *file, int cmd, void *arg)
{
    uart_dev_data_t *data = (uart_dev_data_t *)file->private_data;
    
    switch (cmd) {
    case UART_IOCTL_SET_BAUDRATE:
        data->baudrate = *(uint32_t *)arg;
        dtm_uart_init(data->baudrate);
        PUP_LOG_INFO(PUP_LOG_MODULE_DEV, "UART baudrate set to %d", data->baudrate);
        return PUP_DEVFS_OK;
        
    case UART_IOCTL_GET_BAUDRATE:
        *(uint32_t *)arg = data->baudrate;
        return PUP_DEVFS_OK;
        
    default:
        PUP_LOG_ERROR(PUP_LOG_MODULE_DEV, "Unknown UART ioctl: 0x%x", cmd);
        return PUP_DEVFS_EINVAL;
    }
}

static const pup_devfs_file_ops_t uart_ops = {
    .open = uart_open,
    .close = uart_close,
    .read = uart_read,
    .write = uart_write,
    .ioctl = uart_ioctl
};

static const pup_devfs_device_t uart0_device = {
    .name = "uart0",
    .ops = &uart_ops,
    .private_data = &uart0_data
};

int dev_uart_init(void)
{
    PUP_LOG_INFO(PUP_LOG_MODULE_DEV, "Registering UART device");
    return pup_devfs_register_device(&uart0_device);
}
