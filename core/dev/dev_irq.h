#ifndef __DEV_IRQ_H__
#define __DEV_IRQ_H__

#include <puppy_core.h>
#include "shared/pup_driver_abi.h"

/* -----------------------------------------------------------------------
 * NVIC Control Interface
 * --------------------------------------------------------------------- */

/**
 * Enable NVIC interrupt
 * @param irq_num: IRQ number (0-240)
 * @return 0 on success, -1 on error
 */
int dev_irq_nvic_enable(unsigned int irq_num);

/**
 * Disable NVIC interrupt
 * @param irq_num: IRQ number (0-240)
 * @return 0 on success, -1 on error
 */
int dev_irq_nvic_disable(unsigned int irq_num);

/**
 * Set NVIC interrupt priority
 * @param irq_num: IRQ number (0-240)
 * @param priority: Priority value (0-15, lower = higher priority)
 * @return 0 on success, -1 on error
 */
int dev_irq_nvic_set_priority(unsigned int irq_num, int priority);

/* -----------------------------------------------------------------------
 * User IRQ Subscription Management
 * --------------------------------------------------------------------- */

/**
 * Subscribe to an IRQ
 * @param subscription: Subscription parameters
 * @param owner_pid: Owner process ID
 * @return Handle on success, -1 on error
 */
int dev_irq_subscribe(const pup_user_irq_subscription_t* subscription, int owner_pid);

/**
 * Unsubscribe from an IRQ
 * @param handle: Subscription handle
 * @param owner_pid: Owner process ID
 * @return 0 on success, -1 on error
 */
int dev_irq_unsubscribe(int handle, int owner_pid);

/**
 * Wait for IRQ (legacy mode)
 * @param handle: Subscription handle
 * @param owner_pid: Owner process ID
 * @param timeout_ticks: Timeout in ticks (0 = no wait)
 * @return 1 if IRQ pending, 0 if timeout, -1 on error
 */
int dev_irq_wait(int handle, int owner_pid, int timeout_ticks);

/**
 * Notify all subscribers of an IRQ
 * @param irq_num: IRQ number
 */
void dev_irq_notify(unsigned int irq_num);

/**
 * Cleanup all subscriptions for an owner
 * @param owner_pid: Owner process ID
 */
void dev_irq_cleanup_owner(int owner_pid);

/* -----------------------------------------------------------------------
 * IRQ Control Interface (for syscall)
 * --------------------------------------------------------------------- */

/**
 * Control IRQ settings
 * @param irq_num: IRQ number
 * @param cmd: Command (1=enable, 2=disable, 3=set_priority)
 * @param value: Value for command
 * @param owner_pid: Owner process ID
 * @return 0 on success, -1 on error
 */
int dev_irq_control(unsigned int irq_num, int cmd, int value, int owner_pid);

/* -----------------------------------------------------------------------
 * Initialization
 * --------------------------------------------------------------------- */

/**
 * Initialize IRQ driver
 * @return 0 on success, -1 on error
 */
int dev_irq_init(void);

#endif /* __DEV_IRQ_H__ */
