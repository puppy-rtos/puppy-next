#include "libc.h"
#include "syscall.h"

#define GPIOA_BASE 0x40020000U
#define GPIOC_BASE 0x40020800U
#define GPIOF_BASE 0x40021400U

#define GPIO_PIN0   (1U << 0)
#define GPIO_PIN5   (1U << 5)
#define GPIO_PIN11  (1U << 11)

#define GPIO_DIR_IN   0
#define GPIO_DIR_OUT  1

#define GPIO_IOCTL_SET_DIR      0x2001
#define GPIO_IOCTL_GET_DIR      0x2002
#define GPIO_IOCTL_SET_VALUE    0x2003
#define GPIO_IOCTL_GET_VALUE    0x2004

#define GPIO_DEV_COUNT 3

typedef struct {
    volatile unsigned int MODER;
    volatile unsigned int OTYPER;
    volatile unsigned int OSPEEDR;
    volatile unsigned int PUPDR;
    volatile unsigned int IDR;
    volatile unsigned int ODR;
    volatile unsigned int BSRR;
} gpio_regs_t;

typedef struct {
    const char* name;
    unsigned int gpio_base;
    unsigned short pin_mask;
    int device_id;
    int direction;
    int value;
    int pull_up;
} gpio_user_dev_t;

static int g_server_channel = -1;
static int g_key_irq_handle = -1;

static gpio_user_dev_t g_devs[GPIO_DEV_COUNT] = {
    {"gpio0", GPIOA_BASE, GPIO_PIN0,  0, GPIO_DIR_OUT, 0, 0},
    {"led0",  GPIOF_BASE, GPIO_PIN11, 1, GPIO_DIR_OUT, 0, 0},
    {"key0",  GPIOC_BASE, GPIO_PIN5,  2, GPIO_DIR_IN,  0, 1},
};

static int gpio_pin_index(unsigned short pin_mask)
{
    int i;

    for (i = 0; i < 16; i++) {
        if ((pin_mask & (1U << i)) != 0U) {
            return i;
        }
    }

    return -1;
}

static void gpio_config(gpio_user_dev_t* dev)
{
    gpio_regs_t* regs;
    int pin;
    unsigned int mode_val;
    unsigned int pull_val;

    if (!dev) {
        return;
    }

    regs = (gpio_regs_t*)dev->gpio_base;
    pin = gpio_pin_index(dev->pin_mask);
    if (pin < 0) {
        return;
    }

    mode_val = (dev->direction == GPIO_DIR_IN) ? 0U : 1U;
    regs->MODER &= ~(3U << (pin * 2));
    regs->MODER |= (mode_val << (pin * 2));

    if (dev->direction == GPIO_DIR_OUT) {
        regs->OTYPER &= ~(1U << pin);
        regs->OSPEEDR &= ~(3U << (pin * 2));
        regs->OSPEEDR |= (1U << (pin * 2));
    }

    pull_val = dev->pull_up ? 1U : 0U;
    regs->PUPDR &= ~(3U << (pin * 2));
    regs->PUPDR |= (pull_val << (pin * 2));
}

static void gpio_write_level(gpio_user_dev_t* dev, int value)
{
    gpio_regs_t* regs;

    if (!dev) {
        return;
    }

    regs = (gpio_regs_t*)dev->gpio_base;
    if (value) {
        regs->BSRR = (unsigned int)dev->pin_mask;
    } else {
        regs->BSRR = ((unsigned int)dev->pin_mask << 16);
    }

    dev->value = value ? 1 : 0;
}

static int gpio_read_level(gpio_user_dev_t* dev)
{
    gpio_regs_t* regs;

    if (!dev) {
        return 0;
    }

    regs = (gpio_regs_t*)dev->gpio_base;
    return ((regs->IDR & dev->pin_mask) != 0U) ? 1 : 0;
}

static int gpio_handle_request(gpio_user_dev_t* dev, pup_devfs_proxy_payload_t* req)
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

static gpio_user_dev_t* gpio_find_by_id(int device_id)
{
    int i;

    for (i = 0; i < GPIO_DEV_COUNT; i++) {
        if (g_devs[i].device_id == device_id) {
            return &g_devs[i];
        }
    }

    return (gpio_user_dev_t*)0;
}

static void gpio_handle_pulse(const pup_channel_msg_t* msg)
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
}

static void gpio_service_loop(void)
{
    if (g_server_channel < 0) {
        return;
    }

    for (;;) {
        pup_channel_msg_t msg;
        int cookie = msg_receive(g_server_channel, &msg);

        if (cookie < 0) {
            printf("[driver] channel receive failed, stop service\n");
            return;
        }

        if (msg.type == PUP_MSG_TYPE_PULSE) {
            gpio_handle_pulse(&msg);
            continue;
        }

        if (msg.type != PUP_MSG_TYPE_REQUEST || msg.buf == 0 || cookie <= 0) {
            if (cookie > 0) {
                msg_reply(g_server_channel, cookie, -1);
            }
            continue;
        }

        {
            pup_devfs_proxy_payload_t* req = (pup_devfs_proxy_payload_t*)msg.buf;
            gpio_user_dev_t* dev = gpio_find_by_id(req->device_id);
            int ret = gpio_handle_request(dev, req);
            msg_reply(g_server_channel, cookie, ret);
        }
    }
}

static int gpio_register_one(gpio_user_dev_t* dev)
{
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

void app_main(const char* arg)
{
    int i;

    (void)arg;

    printf("[driver] start\n");

    g_server_channel = channel_create();
    if (g_server_channel < 0) {
        printf("[driver] channel_create failed\n");
        pthread_exit((void*)-1);
    }

    for (i = 0; i < GPIO_DEV_COUNT; i++) {
        if (gpio_register_one(&g_devs[i]) != 0) {
            printf("[driver] failed at index=%d\n", i);
            channel_destroy(g_server_channel);
            pthread_exit((void*)-1);
        }
    }

    {
        pup_user_irq_subscription_t sub;
        sub.irq_num = PUP_IRQ_EXTI9_5;
        sub.flags = 0;
        sub.channel_id = g_server_channel;
        sub.reserved1 = 0;
        g_key_irq_handle = irq_subscribe(&sub);
        if (g_key_irq_handle < 0) {
            printf("[driver] irq_subscribe failed for EXTI9_5\n");
        } else {
            printf("[driver] irq subscribed: EXTI9_5 handle=%d\n", g_key_irq_handle);
        }
    }

    printf("[driver] ready\n");
    gpio_service_loop();

    if (g_server_channel >= 0) {
        channel_destroy(g_server_channel);
        g_server_channel = -1;
    }

    pthread_exit((void*)-1);
}
