#include "dev_irq.h"
#include "puppy_log.h"
#include "dtm_irq.h"
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

/* Key0 is PC5; EXTI line 5 routed through SYSCFG EXTICR2[7:4]. */
#define GPIO_KEY0_EXTI_LINE_BIT   (1U << 5)
#define EXTI_BASE_ADDR            0x40013C00UL

typedef struct {
    volatile uint32_t IMR;
    volatile uint32_t EMR;
    volatile uint32_t RTSR;
    volatile uint32_t FTSR;
    volatile uint32_t SWIER;
    volatile uint32_t PR;
} pup_exti_regs_t;

void EXTI9_5_IRQHandler(void)
{
    pup_exti_regs_t *exti = (pup_exti_regs_t *)EXTI_BASE_ADDR;

    if (exti->PR & GPIO_KEY0_EXTI_LINE_BIT) {
        /* Clear pending first, then notify user-space IRQ subscribers. */
        exti->PR = GPIO_KEY0_EXTI_LINE_BIT;
        dev_irq_notify(GPIO_KEY0_EXTI_IRQ_NUM);
    }
}

/* -----------------------------------------------------------------------
 * Initialization
 * --------------------------------------------------------------------- */

int dev_irq_init(void)
{
    PUP_LOG_INFO(PUP_LOG_MODULE_DEV,
                 "IRQ driver initialized: EXTI line5 -> irq=%u",
                 (unsigned int)GPIO_KEY0_EXTI_IRQ_NUM);
    return 0;
}
