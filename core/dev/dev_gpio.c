#include "dev_gpio.h"
#include "puppy_log.h"
#include "dtm_irq.h"
#include "syscall.h"

/* STM32F407 EXTI9_5 IRQ number in NVIC external IRQ space. */
#define GPIO_KEY0_EXTI_IRQ_NUM 23U

/* Key0 is PC5; EXTI line 5 routed through SYSCFG EXTICR2[7:4]. */
#define GPIO_KEY0_EXTI_LINE_BIT   (1U << 5)
#define RCC_BASE_ADDR             0x40023800UL
#define SYSCFG_BASE_ADDR          0x40013800UL
#define EXTI_BASE_ADDR            0x40013C00UL
#define RCC_APB2ENR_OFFSET        0x44UL
#define RCC_AHB1ENR_OFFSET        0x30UL
#define RCC_APB2ENR_SYSCFGEN_BIT  (1U << 14)
#define RCC_AHB1ENR_GPIOAEN_BIT   (1U << 0)
#define RCC_AHB1ENR_GPIOCEN_BIT   (1U << 2)
#define RCC_AHB1ENR_GPIOFEN_BIT   (1U << 5)

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

int dev_gpio_irq_bridge_init(void)
{
    volatile uint32_t *apb2enr = (volatile uint32_t *)(RCC_BASE_ADDR + RCC_APB2ENR_OFFSET);
    volatile uint32_t *ahb1enr = (volatile uint32_t *)(RCC_BASE_ADDR + RCC_AHB1ENR_OFFSET);
    pup_syscfg_regs_t *syscfg = (pup_syscfg_regs_t *)SYSCFG_BASE_ADDR;
    pup_exti_regs_t *exti = (pup_exti_regs_t *)EXTI_BASE_ADDR;

    /* Enable SYSCFG clock so EXTICR mapping is writable. */
    *apb2enr |= RCC_APB2ENR_SYSCFGEN_BIT;
    *ahb1enr |= (RCC_AHB1ENR_GPIOAEN_BIT | RCC_AHB1ENR_GPIOCEN_BIT | RCC_AHB1ENR_GPIOFEN_BIT);

    /* Route EXTI line 5 to port C (0x2). EXTICR2 bits [7:4]. */
    syscfg->EXTICR[1] &= ~(0xFU << 4);
    syscfg->EXTICR[1] |= (0x2U << 4);

    /* Unmask line 5 and trigger on both edges to catch press/release. */
    exti->IMR |= GPIO_KEY0_EXTI_LINE_BIT;
    exti->RTSR |= GPIO_KEY0_EXTI_LINE_BIT;
    exti->FTSR |= GPIO_KEY0_EXTI_LINE_BIT;
    exti->PR = GPIO_KEY0_EXTI_LINE_BIT;

    dtm_nvic_enable_irq((DTM_IRQn_Type)(DTM_ChipSpecific_IRQn + GPIO_KEY0_EXTI_IRQ_NUM));
    dtm_nvic_set_priority((DTM_IRQn_Type)(DTM_ChipSpecific_IRQn + GPIO_KEY0_EXTI_IRQ_NUM), 2);

    PUP_LOG_INFO(PUP_LOG_MODULE_DEV,
                 "GPIO IRQ bridge ready: EXTI line5 -> irq=%u",
                 (unsigned int)GPIO_KEY0_EXTI_IRQ_NUM);
    return PUP_DEVFS_OK;
}

void EXTI9_5_IRQHandler(void)
{
    pup_exti_regs_t *exti = (pup_exti_regs_t *)EXTI_BASE_ADDR;

    if (exti->PR & GPIO_KEY0_EXTI_LINE_BIT) {
        /* Clear pending first, then notify user-space IRQ subscribers. */
        exti->PR = GPIO_KEY0_EXTI_LINE_BIT;
        syscall_irq_notify(GPIO_KEY0_EXTI_IRQ_NUM);
    }
}
