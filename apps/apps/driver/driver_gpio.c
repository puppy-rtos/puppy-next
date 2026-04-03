#include "libc.h"
#include "syscall.h"
#include "dtm_gpio.h"
#include "dtm_bindings.h"
#include "shared/pup_devfs_proto.h"
#include "shared/pup_driver_abi.h"
#include "driver_gpio.h"

#define SYSCFG_BASE_ADDR          0x40013800UL
#define EXTI_BASE_ADDR            0x40013C00UL
#define GPIO_KEY0_EXTI_LINE_BIT   (1U << 5)

typedef struct {
    volatile uint32_t MEMRMP;
    volatile uint32_t PMC;
    volatile uint32_t EXTICR[4];
} pup_syscfg_regs_t;

typedef struct {
    volatile uint32_t IMR;
    volatile uint32_t EMR;
    volatile uint32_t RTSR;
    volatile uint32_t FTSR;
    volatile uint32_t SWIER;
    volatile uint32_t PR;
} pup_exti_regs_t;

gpio_user_dev_t g_devs[GPIO_DEV_COUNT] = {
    {"gpio0", GPIOA, DTM_GPIO_PIN0,  0, GPIO_DIR_OUT, 0, 0},
    {"led0",  GPIOF, DTM_GPIO_PIN11, 1, GPIO_DIR_OUT, 0, 0},
    {"key0",  GPIOC, DTM_GPIO_PIN5,  2, GPIO_DIR_IN,  0, 1},
};

void gpio_config(gpio_user_dev_t* dev)
{
    uint32_t mode;
    uint32_t pupd;

    if (!dev) {
        return;
    }

    mode = (dev->direction == GPIO_DIR_IN) ? DTM_GPIO_MODE_IN : DTM_GPIO_MODE_OUT;
    pupd = dev->pull_up ? DTM_GPIO_PUPD_PU : DTM_GPIO_PUPD_NONE;

    dtm_gpio_set(dev->gpio_base, dev->pin_mask, mode, DTM_GPIO_OTYPE_PP, DTM_GPIO_SPEED_MID, pupd);
}

void gpio_write_level(gpio_user_dev_t* dev, int value)
{
    if (!dev) {
        return;
    }

    dtm_gpio_pin_set(dev->gpio_base, dev->pin_mask, (uint8_t)value);
    dev->value = value ? 1 : 0;
}

int gpio_read_level(gpio_user_dev_t* dev)
{
    if (!dev) {
        return 0;
    }

    return (int)dtm_gpio_pin_get(dev->gpio_base, dev->pin_mask);
}

int gpio_handle_request(gpio_user_dev_t* dev, pup_devfs_proxy_payload_t* req)
{
    unsigned int* io_arg;

    if (!dev || !req) {
        return -1;
    }

    switch (req->op) {
    case DEVFS_PROXY_OP_OPEN:
        gpio_config(dev);
        return 0;

    case DEVFS_PROXY_OP_CLOSE:
        return 0;

    case DEVFS_PROXY_OP_READ:
        if (!req->data_buf || req->data_len <= 0) {
            return 0;
        }
        ((char*)req->data_buf)[0] = (char)gpio_read_level(dev);
        return 1;

    case DEVFS_PROXY_OP_WRITE:
        if (!req->data_buf || req->data_len <= 0) {
            return 0;
        }

        {
            unsigned char ch = (unsigned char)((char*)req->data_buf)[0];
            int value;

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

            gpio_write_level(dev, value);
            return 1;
        }

    case DEVFS_PROXY_OP_IOCTL:
        io_arg = (unsigned int*)req->data_buf;
        switch (req->cmd) {
        case GPIO_IOCTL_SET_DIR:
            if (!io_arg) {
                return -1;
            }
            dev->direction = (*io_arg == (unsigned int)GPIO_DIR_IN) ? GPIO_DIR_IN : GPIO_DIR_OUT;
            gpio_config(dev);
            return 0;

        case GPIO_IOCTL_GET_DIR:
            if (!io_arg) {
                return -1;
            }
            *io_arg = (unsigned int)dev->direction;
            return 0;

        case GPIO_IOCTL_SET_VALUE:
            if (!io_arg) {
                return -1;
            }
            gpio_write_level(dev, (*io_arg != 0U) ? 1 : 0);
            return 0;

        case GPIO_IOCTL_GET_VALUE:
            if (!io_arg) {
                return -1;
            }
            *io_arg = (unsigned int)gpio_read_level(dev);
            return 0;

        default:
            return -1;
        }

    default:
        return -1;
    }
}

gpio_user_dev_t* gpio_find_by_id(int device_id)
{
    int i;

    for (i = 0; i < GPIO_DEV_COUNT; i++) {
        if (g_devs[i].device_id == device_id) {
            return &g_devs[i];
        }
    }

    return (gpio_user_dev_t*)0;
}

void gpio_handle_pulse(const pup_channel_msg_t* msg)
{
    gpio_user_dev_t* key_dev;
    int state;

    if (!msg || msg->pulse_code != PUP_IRQ_EXTI9_5) {
        return;
    }

    key_dev = gpio_find_by_id(2);
    if (!key_dev) {
        return;
    }

    state = gpio_read_level(key_dev);
    (void)state;
}

int gpio_register_one(gpio_user_dev_t* dev)
{
    extern int g_server_channel;
    pup_user_devfs_registration_t reg;
    char name_buf[17];
    int i;

    if (!dev) {
        return -1;
    }

    for (i = 0; i < 16 && dev->name[i] != '\0'; i++) {
        name_buf[i] = dev->name[i];
    }
    name_buf[i] = '\0';

    reg.name = name_buf;
    reg.flags = PUP_DRIVER_DEVICE_FLAG_NONE;
    reg.capabilities = PUP_DRIVER_CAP_NONE;
    reg.channel_id = g_server_channel;
    reg.device_id = dev->device_id;

    if (devfs_register(&reg) != 0) {
        printf("[driver] devfs_register failed for %s\n", dev->name);
        return -1;
    }

    printf("[driver] registered /dev/%s channel=%d devid=%d\n",
           dev->name,
           g_server_channel,
           dev->device_id);
    return 0;
}

void gpio_irq_config(void)
{
    pup_syscfg_regs_t *syscfg = (pup_syscfg_regs_t *)SYSCFG_BASE_ADDR;
    pup_exti_regs_t *exti = (pup_exti_regs_t *)EXTI_BASE_ADDR;

    syscfg->EXTICR[1] &= ~(0xFU << 4);
    syscfg->EXTICR[1] |= (0x2U << 4);

    exti->IMR |= GPIO_KEY0_EXTI_LINE_BIT;
    exti->RTSR |= GPIO_KEY0_EXTI_LINE_BIT;
    exti->FTSR |= GPIO_KEY0_EXTI_LINE_BIT;
    exti->PR = GPIO_KEY0_EXTI_LINE_BIT;

    irq_control(PUP_IRQ_EXTI9_5, IRQ_CONTROL_SET_PRIORITY, 2);
    irq_control(PUP_IRQ_EXTI9_5, IRQ_CONTROL_ENABLE, 0);

    printf("[driver] GPIO IRQ configured: EXTI line5 -> irq=%u\n", (unsigned int)PUP_IRQ_EXTI9_5);
}
