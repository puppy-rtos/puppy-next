#include "libc.h"
#include "syscall.h"
#include "dtm_bindings.h"
#include "dtm_gpio.h"
#include "shared/pup_devfs_proto.h"
#include "shared/pup_driver_abi.h"
#include "driver_gpio.h"

#ifndef GPIOA
#define GPIOA 0U
#endif

#ifndef GPIOC
#define GPIOC 0U
#endif

#ifndef GPIOF
#define GPIOF 0U
#endif

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

gpio_user_dev_t g_devs[GPIO_DEV_MAX];
int g_dev_count;

static pup_dtm_exti_config_t g_gpio_key_exti;
static int g_gpio_using_dtb;

static int gpio_of_probe(dtm_ofw_node_t node);

static const char *const g_gpio_compatibles[] = {
    "puppy,gpio-dev",
    "puppy,gpio-led",
    "puppy,gpio-key",
};

const dtm_of_driver_t g_gpio_of_driver = {
    "gpio",
    g_gpio_compatibles,
    DTM_ARRAY_SIZE(g_gpio_compatibles),
    gpio_of_probe,
};

static void gpio_clear_devices(void)
{
    libcm_memset(g_devs, 0, sizeof(g_devs));
    g_dev_count = 0;
}

static void gpio_begin_dtb_probe(void)
{
    if (g_gpio_using_dtb) {
        return;
    }

    gpio_clear_devices();
    g_gpio_using_dtb = 1;
}

static gpio_user_dev_t *gpio_alloc_dev(void)
{
    if (g_dev_count >= GPIO_DEV_MAX) {
        return (gpio_user_dev_t *)0;
    }

    return &g_devs[g_dev_count];
}

static void gpio_parse_exti_controllers_from_dtb(void)
{
    dtm_ofw_node_t node;
    uint64_t addr = 0;
    uint32_t value;

    node = dtm_ofw_find_node_by_compatible(DTM_OFW_NODE_INVALID, "st,stm32-syscfg");
    if (node >= 0 && dtm_ofw_get_reg(node, 0, &addr, 0) == 0) {
        g_gpio_key_exti.syscfg_base = (uint32_t)addr;
    }

    node = dtm_ofw_find_node_by_compatible(DTM_OFW_NODE_INVALID, "st,stm32-exti");
    if (node >= 0) {
        if (dtm_ofw_get_reg(node, 0, &addr, 0) == 0) {
            g_gpio_key_exti.exti_base = (uint32_t)addr;
        }
        if (dtm_ofw_prop_read_u32(node, "puppy,priority", &value) == 0) {
            g_gpio_key_exti.priority = value;
        }
    }
}

void gpio_resources_init(void)
{
    gpio_clear_devices();
    libcm_memset(&g_gpio_key_exti, 0, sizeof(g_gpio_key_exti));
    g_gpio_using_dtb = 0;

    if (driver_of_is_ready()) {
        gpio_parse_exti_controllers_from_dtb();
    }
}

static int gpio_of_probe(dtm_ofw_node_t node)
{
    gpio_user_dev_t *dev;
    const char *name = 0;
    const char *direction = 0;
    uint32_t value;
    uint32_t flags = 0;

    gpio_begin_dtb_probe();
    dev = gpio_alloc_dev();
    if (!dev) {
        return -1;
    }

    libcm_memset(dev, 0, sizeof(*dev));
    if (driver_of_parse_gpio_spec(node, "gpios", 0,
                                  &dev->gpio_base,
                                  &dev->pin_mask,
                                  &flags) < 0) {
        return -1;
    }

    if (driver_of_prop_read_string(node, "puppy,dev-name", &name) < 0) {
        name = driver_of_node_base_name(dtm_ofw_node_name(node));
    }
    driver_of_copy_name(dev->name, name);

    if (dtm_ofw_prop_read_u32(node, "puppy,device-id", &value) == 0) {
        dev->device_id = (int)value;
    } else {
        dev->device_id = g_dev_count;
    }

    dev->direction = GPIO_DIR_OUT;
    if ((driver_of_prop_read_string(node, "puppy,direction", &direction) == 0 &&
         libcm_strcmp(direction, "in") == 0) ||
        dtm_ofw_node_is_compatible(node, "puppy,gpio-key")) {
        dev->direction = GPIO_DIR_IN;
    }

    if (driver_of_prop_has(node, "bias-pull-up") ||
        (dtm_ofw_prop_read_u32(node, "puppy,pull-up", &value) == 0 && value != 0U)) {
        dev->pull_up = 1;
    }

    (void)flags;

    if (dtm_ofw_prop_read_u32_index(node, "interrupts", 0, &value) == 0) {
        dev->exti = g_gpio_key_exti;
        dev->exti.valid = 1U;
        dev->exti.irq_num = value;
        if (dtm_ofw_prop_read_u32(node, "puppy,exti-line", &value) == 0) {
            dev->exti.line = value;
        }
        if (dtm_ofw_prop_read_u32(node, "puppy,exti-port", &value) == 0) {
            dev->exti.port = value;
        }
        if (dtm_ofw_prop_read_u32(node, "puppy,irq-priority", &value) == 0) {
            dev->exti.priority = value;
        }
        g_gpio_key_exti = dev->exti;
    }

    g_dev_count++;
    return 0;
}

int gpio_key_irq_num(void)
{
    return g_gpio_key_exti.valid ? (int)g_gpio_key_exti.irq_num : 0;
}

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

    for (i = 0; i < g_dev_count; i++) {
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
    int i;

    if (!msg || msg->pulse_code != gpio_key_irq_num()) {
        return;
    }

    key_dev = (gpio_user_dev_t *)0;
    for (i = 0; i < g_dev_count; i++) {
        if (g_devs[i].exti.valid && g_devs[i].exti.irq_num == (uint32_t)msg->pulse_code) {
            key_dev = &g_devs[i];
            break;
        }
    }
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
    const pup_dtm_exti_config_t *cfg = &g_gpio_key_exti;
    pup_syscfg_regs_t *syscfg;
    pup_exti_regs_t *exti;
    uint32_t line_bit;
    uint32_t exticr_index;
    uint32_t exticr_shift;

    if (!cfg->valid || cfg->syscfg_base == 0U || cfg->exti_base == 0U || cfg->line >= 16U) {
        printf("[driver] GPIO IRQ skipped: no GPIO EXTI config\n");
        return;
    }

    syscfg = (pup_syscfg_regs_t *)cfg->syscfg_base;
    exti = (pup_exti_regs_t *)cfg->exti_base;
    line_bit = 1U << cfg->line;
    exticr_index = cfg->line / 4U;
    exticr_shift = (cfg->line % 4U) * 4U;

    syscfg->EXTICR[exticr_index] &= ~(0xFU << exticr_shift);
    syscfg->EXTICR[exticr_index] |= (cfg->port & 0xFU) << exticr_shift;

    exti->IMR |= line_bit;
    exti->RTSR |= line_bit;
    exti->FTSR |= line_bit;
    exti->PR = line_bit;

    irq_control(cfg->irq_num, IRQ_CONTROL_SET_PRIORITY, (int)cfg->priority);
    irq_control(cfg->irq_num, IRQ_CONTROL_ENABLE, 0);

    printf("[driver] GPIO IRQ configured: EXTI line%u -> irq=%u\n",
           (unsigned int)cfg->line,
           (unsigned int)cfg->irq_num);
}
