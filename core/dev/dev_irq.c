#include "dev_irq.h"
#include "puppy_log.h"
#include "dtm_irq.h"
#include "dtm_board.h"
#include "dtm_of_driver.h"
#include "app_manager.h"
#include "pup_channel.h"
#include <libcm.h>

/* Maximum number of user IRQ subscriptions */
#define MAX_USER_IRQ_SUBSCRIPTIONS 8

/* IRQ control commands */
#define IRQ_CONTROL_ENABLE       1
#define IRQ_CONTROL_DISABLE      2
#define IRQ_CONTROL_SET_PRIORITY 3

/* User IRQ subscription entry */
typedef struct {
    int used;
    int owner_pid;
    pup_user_irq_subscription_t subscription;
    unsigned int pending_count;
    int channel_id;   /* channel to deliver IRQ pulses; -1 = legacy irq_wait mode */
} user_irq_subscription_entry_t;

static user_irq_subscription_entry_t g_user_irq_subscriptions[MAX_USER_IRQ_SUBSCRIPTIONS];

/* -----------------------------------------------------------------------
 * NVIC Control Functions
 * --------------------------------------------------------------------- */

int dev_irq_nvic_enable(unsigned int irq_num)
{
    if (irq_num > 240) {
        return -1;
    }
    DTM_NVIC->ISER[irq_num >> 5] = (1UL << (irq_num & 0x1F));
    return 0;
}

int dev_irq_nvic_disable(unsigned int irq_num)
{
    if (irq_num > 240) {
        return -1;
    }
    DTM_NVIC->ICER[irq_num >> 5] = (1UL << (irq_num & 0x1F));
    return 0;
}

int dev_irq_nvic_set_priority(unsigned int irq_num, int priority)
{
    if (irq_num > 240 || priority < 0 || priority > 15) {
        return -1;
    }
    DTM_NVIC->IP[irq_num] = (uint8_t)(priority << (8 - __NVIC_PRIO_BITS));
    return 0;
}

/* -----------------------------------------------------------------------
 * User IRQ Subscription Management
 * --------------------------------------------------------------------- */

int dev_irq_subscribe(const pup_user_irq_subscription_t* subscription, int owner_pid)
{
    int i;

    if (!subscription || owner_pid <= 0) {
        return -1;
    }

    for (i = 0; i < MAX_USER_IRQ_SUBSCRIPTIONS; i++) {
        if (!g_user_irq_subscriptions[i].used) {
            g_user_irq_subscriptions[i].used = 1;
            g_user_irq_subscriptions[i].owner_pid = owner_pid;
            g_user_irq_subscriptions[i].subscription = *subscription;
            g_user_irq_subscriptions[i].pending_count = 0;
            g_user_irq_subscriptions[i].channel_id = subscription->channel_id;
            PUP_LOG_INFO(PUP_LOG_MODULE_DEV,
                         "dev_irq_subscribe: pid=%d irq=%u handle=%d",
                         owner_pid,
                         subscription->irq_num,
                         i);
            return i;
        }
    }

    return -1;
}

int dev_irq_unsubscribe(int handle, int owner_pid)
{
    if (handle < 0 || handle >= MAX_USER_IRQ_SUBSCRIPTIONS) {
        return -1;
    }

    if (!g_user_irq_subscriptions[handle].used ||
        g_user_irq_subscriptions[handle].owner_pid != owner_pid) {
        return -1;
    }

    libcm_memset(&g_user_irq_subscriptions[handle], 0, sizeof(g_user_irq_subscriptions[handle]));
    return 0;
}

int dev_irq_wait(int handle, int owner_pid, int timeout_ticks)
{
    user_irq_subscription_entry_t* entry;

    if (handle < 0 || handle >= MAX_USER_IRQ_SUBSCRIPTIONS) {
        return -1;
    }

    entry = &g_user_irq_subscriptions[handle];
    if (!entry->used || entry->owner_pid != owner_pid) {
        return -1;
    }

    if (entry->pending_count > 0U) {
        entry->pending_count--;
        return 1;
    }

    if (timeout_ticks > 0) {
        pthread_delay(timeout_ticks);
    }

    return 0;
}

void dev_irq_notify(unsigned int irq_num)
{
    int i;

    for (i = 0; i < MAX_USER_IRQ_SUBSCRIPTIONS; i++) {
        user_irq_subscription_entry_t *entry = &g_user_irq_subscriptions[i];

        if (!entry->used || entry->subscription.irq_num != irq_num) {
            continue;
        }

        if (entry->channel_id >= 0 && pup_channel_is_valid(entry->channel_id)) {
            /* Preferred path: IRQ event as channel pulse. */
            if (pup_channel_notify(entry->channel_id, (int)irq_num, 0) == 0) {
                continue;
            }
        }

        /* Fallback path for legacy irq_wait() mode or pulse queue full. */
        entry->pending_count++;
    }
}

void dev_irq_cleanup_owner(int owner_pid)
{
    int i;

    for (i = 0; i < MAX_USER_IRQ_SUBSCRIPTIONS; i++) {
        if (g_user_irq_subscriptions[i].used && g_user_irq_subscriptions[i].owner_pid == owner_pid) {
            libcm_memset(&g_user_irq_subscriptions[i], 0, sizeof(g_user_irq_subscriptions[i]));
        }
    }
}

/* -----------------------------------------------------------------------
 * IRQ Control Interface (for syscall)
 * --------------------------------------------------------------------- */

int dev_irq_control(unsigned int irq_num, int cmd, int value, int owner_pid)
{
    if (owner_pid <= 0) {
        return -1;
    }

    switch (cmd) {
    case IRQ_CONTROL_ENABLE:
        return dev_irq_nvic_enable(irq_num);

    case IRQ_CONTROL_DISABLE:
        return dev_irq_nvic_disable(irq_num);

    case IRQ_CONTROL_SET_PRIORITY:
        return dev_irq_nvic_set_priority(irq_num, value);

    default:
        return -1;
    }
}

/* -----------------------------------------------------------------------
 * GPIO EXTI IRQ Handler
 * --------------------------------------------------------------------- */

/* STM32F407 EXTI9_5 IRQ number in NVIC external IRQ space. */
#define GPIO_KEY0_EXTI_IRQ_NUM 23U

#define EXTI_BASE_ADDR            0x40013C00UL

typedef struct {
    volatile uint32_t IMR;
    volatile uint32_t EMR;
    volatile uint32_t RTSR;
    volatile uint32_t FTSR;
    volatile uint32_t SWIER;
    volatile uint32_t PR;
} pup_exti_regs_t;

static void dev_irq_get_key_exti(pup_dtm_exti_config_t *config)
{
    if (!config) {
        return;
    }

    if (dtm_board_get_key_exti(config) == 0 && config->valid) {
        return;
    }

    config->valid = 1U;
    config->exti_base = EXTI_BASE_ADDR;
    config->irq_num = GPIO_KEY0_EXTI_IRQ_NUM;
    config->line = 5U;
    config->priority = 2U;
}

static int dev_irq_syscfg_of_probe(dtm_ofw_node_t node)
{
    pup_dtm_exti_config_t config;
    uint64_t addr = 0;

    if (dtm_ofw_get_reg(node, 0, &addr, 0) < 0 ||
        dtm_board_get_key_exti(&config) != 0) {
        return -1;
    }

    config.syscfg_base = (uint32_t)addr;
    dtm_board_set_key_exti(&config);
    return 0;
}

static int dev_irq_exti_controller_of_probe(dtm_ofw_node_t node)
{
    pup_dtm_exti_config_t config;
    uint64_t addr = 0;
    uint32_t value;

    if (dtm_board_get_key_exti(&config) != 0) {
        return -1;
    }

    if (dtm_ofw_get_reg(node, 0, &addr, 0) == 0) {
        config.exti_base = (uint32_t)addr;
    }
    if (dtm_ofw_prop_read_u32(node, "puppy,priority", &value) == 0) {
        config.priority = value;
    }

    dtm_board_set_key_exti(&config);
    return 0;
}

static int dev_irq_gpio_key_of_probe(dtm_ofw_node_t node)
{
    pup_dtm_exti_config_t config;
    uint32_t value;

    if (dtm_board_get_key_exti(&config) != 0) {
        return -1;
    }

    config.valid = 1U;
    if (dtm_ofw_prop_read_u32_index(node, "interrupts", 0, &value) == 0) {
        config.irq_num = value;
    }
    if (dtm_ofw_prop_read_u32(node, "puppy,exti-line", &value) == 0) {
        config.line = value;
    }
    if (dtm_ofw_prop_read_u32(node, "puppy,exti-port", &value) == 0) {
        config.port = value;
    }
    if (dtm_ofw_prop_read_u32(node, "puppy,irq-priority", &value) == 0) {
        config.priority = value;
    }

    dtm_board_set_key_exti(&config);
    return 0;
}

static const char *const g_dev_irq_syscfg_compatibles[] = {
    "st,stm32-syscfg",
};

const dtm_of_driver_t g_dev_irq_syscfg_of_driver = {
    "syscfg",
    g_dev_irq_syscfg_compatibles,
    DTM_ARRAY_SIZE(g_dev_irq_syscfg_compatibles),
    dev_irq_syscfg_of_probe,
};

static const char *const g_dev_irq_exti_controller_compatibles[] = {
    "st,stm32-exti",
};

const dtm_of_driver_t g_dev_irq_exti_controller_of_driver = {
    "exti",
    g_dev_irq_exti_controller_compatibles,
    DTM_ARRAY_SIZE(g_dev_irq_exti_controller_compatibles),
    dev_irq_exti_controller_of_probe,
};

static const char *const g_dev_irq_gpio_key_compatibles[] = {
    "puppy,gpio-key",
};

const dtm_of_driver_t g_dev_irq_gpio_key_of_driver = {
    "gpio-key-irq",
    g_dev_irq_gpio_key_compatibles,
    DTM_ARRAY_SIZE(g_dev_irq_gpio_key_compatibles),
    dev_irq_gpio_key_of_probe,
};

void EXTI9_5_IRQHandler(void)
{
    pup_dtm_exti_config_t config;
    pup_exti_regs_t *exti;
    uint32_t line_bit;

    dev_irq_get_key_exti(&config);
    if (config.line >= 32U || config.exti_base == 0U) {
        return;
    }

    exti = (pup_exti_regs_t *)config.exti_base;
    line_bit = 1U << config.line;

    if (exti->PR & line_bit) {
        /* Clear pending first, then notify user-space IRQ subscribers. */
        exti->PR = line_bit;
        dev_irq_notify(config.irq_num);
    }
}

/* -----------------------------------------------------------------------
 * Initialization
 * --------------------------------------------------------------------- */

int dev_irq_init(void)
{
    pup_dtm_exti_config_t config;

    dev_irq_get_key_exti(&config);
    PUP_LOG_INFO(PUP_LOG_MODULE_DEV,
                 "IRQ driver initialized: EXTI line%u -> irq=%u",
                 (unsigned int)config.line,
                 (unsigned int)config.irq_num);
    return 0;
}
