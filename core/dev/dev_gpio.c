#include "dev_gpio.h"
#include "puppy_log.h"
#include "dtm_gpio.h"
#include <libcm.h>

typedef struct {
    uint32_t gpiox;
    uint16_t pinx;
    uint8_t mode;
    uint8_t value;
    uint8_t pupd;
} gpio_dev_data_t;

#define GPIO_DEV_COUNT    3

static gpio_dev_data_t gpio_devs[GPIO_DEV_COUNT] = {
    {0x40020000, DTM_GPIO_PIN0, DTM_GPIO_MODE_OUT, 0, DTM_GPIO_PUPD_NONE},
    {0x40021400, DTM_GPIO_PIN11, DTM_GPIO_MODE_OUT, 0, DTM_GPIO_PUPD_NONE}, // PF11, onboard LED
    {0x40020800, DTM_GPIO_PIN5, DTM_GPIO_MODE_IN, 0, DTM_GPIO_PUPD_PU}, // PC5, key0
};

static int gpio_open(pup_devfs_file_t *file)
{
    gpio_dev_data_t *data = (gpio_dev_data_t *)file->private_data;
    PUP_LOG_DEBUG(PUP_LOG_MODULE_DEV, "gpio_open: gpiox=0x%x, pin=%d", 
                  data->gpiox, data->pinx);
    
    dtm_gpio_set(data->gpiox, data->pinx, data->mode, 
                 DTM_GPIO_OTYPE_PP, DTM_GPIO_SPEED_LOW, data->pupd);
    
    return PUP_DEVFS_OK;
}

static int gpio_close(pup_devfs_file_t *file)
{
    PUP_LOG_DEBUG(PUP_LOG_MODULE_DEV, "gpio_close");
    return PUP_DEVFS_OK;
}

static int gpio_read(pup_devfs_file_t *file, char *buf, int len)
{
    gpio_dev_data_t *data = (gpio_dev_data_t *)file->private_data;
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_DEV, "gpio_read: gpiox=0x%x, pin=%d", 
                  data->gpiox, data->pinx);
    
    if (len > 0) {
        buf[0] = dtm_gpio_pin_get(data->gpiox, data->pinx);
        return 1;
    }
    
    return 0;
}

static int gpio_write(pup_devfs_file_t *file, const char *buf, int len)
{
    gpio_dev_data_t *data = (gpio_dev_data_t *)file->private_data;
    int value;
    unsigned char ch;

    if (len <= 0) {
        return 0;
    }

    ch = (unsigned char)buf[0];
    if (ch == '\n' || ch == '\r') {
        return 0;
    }

    if (ch == '0') {
        value = 0;
    } else if (ch == '1') {
        value = 1;
    } else {
        value = ch ? 1 : 0;
    }
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_DEV, "gpio_write: gpiox=0x%x, pin=%d, raw=%d, value=%d", 
                  data->gpiox, data->pinx, ch, value);

    dtm_gpio_pin_set(data->gpiox, data->pinx, value);
    data->value = (uint8_t)value;
    return 1;
    
}

static int gpio_ioctl(pup_devfs_file_t *file, int cmd, void *arg)
{
    gpio_dev_data_t *data = (gpio_dev_data_t *)file->private_data;
    
    switch (cmd) {
    case GPIO_IOCTL_SET_DIR:
        data->mode = *(int *)arg == GPIO_DIR_IN ? DTM_GPIO_MODE_IN : DTM_GPIO_MODE_OUT;
        dtm_gpio_set(data->gpiox, data->pinx, data->mode, 
                     DTM_GPIO_OTYPE_PP, DTM_GPIO_SPEED_LOW, data->pupd);
        PUP_LOG_DEBUG(PUP_LOG_MODULE_DEV, "GPIO direction set to %s", 
                      data->mode == DTM_GPIO_MODE_IN ? "INPUT" : "OUTPUT");
        return PUP_DEVFS_OK;
        
    case GPIO_IOCTL_GET_DIR:
        *(int *)arg = (data->mode == DTM_GPIO_MODE_IN) ? GPIO_DIR_IN : GPIO_DIR_OUT;
        return PUP_DEVFS_OK;
        
    case GPIO_IOCTL_SET_VALUE:
        dtm_gpio_pin_set(data->gpiox, data->pinx, *(int *)arg ? 1 : 0);
        data->value = *(int *)arg ? 1 : 0;
        return PUP_DEVFS_OK;
        
    case GPIO_IOCTL_GET_VALUE:
        *(int *)arg = dtm_gpio_pin_get(data->gpiox, data->pinx);
        return PUP_DEVFS_OK;
        
    default:
        PUP_LOG_ERROR(PUP_LOG_MODULE_DEV, "Unknown GPIO ioctl: 0x%x", cmd);
        return PUP_DEVFS_EINVAL;
    }
}

static const pup_devfs_file_ops_t gpio_ops = {
    .open = gpio_open,
    .close = gpio_close,
    .read = gpio_read,
    .write = gpio_write,
    .ioctl = gpio_ioctl
};

static const pup_devfs_device_t gpio_devices[GPIO_DEV_COUNT] = {
    {"gpio0", &gpio_ops, &gpio_devs[0]},
    {"led0", &gpio_ops, &gpio_devs[1]},
    {"key0", &gpio_ops, &gpio_devs[2]},
};

int dev_gpio_init(void)
{
    PUP_LOG_INFO(PUP_LOG_MODULE_DEV, "Registering GPIO devices");
    
    for (int i = 0; i < GPIO_DEV_COUNT; i++) {
        int ret = pup_devfs_register_device(&gpio_devices[i]);
        if (ret != PUP_DEVFS_OK) {
            PUP_LOG_ERROR(PUP_LOG_MODULE_DEV, "Failed to register gpio%d", i);
            return ret;
        }
    }
    
    return PUP_DEVFS_OK;
}
