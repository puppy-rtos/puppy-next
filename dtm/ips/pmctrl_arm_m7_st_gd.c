
#include "dtm_bindings.h"

#ifdef DTM_BINFDING_PMCTRL_ARM_M7_ST_GD
#include "dtm_pmctrl.h"

#include <stdint.h>

typedef struct
{
    volatile uint32_t CR1;       /*!< PWR power control register 1,            Address offset: 0x00 */
    volatile uint32_t CSR1;      /*!< PWR power control status register 1,     Address offset: 0x04 */
    volatile uint32_t CR2;       /*!< PWR power control register 2,            Address offset: 0x08 */
    volatile uint32_t CR3;       /*!< PWR power control register 3,            Address offset: 0x0C */
    volatile uint32_t CPUCR;     /*!< PWR CPU control register,                Address offset: 0x10 */
    uint32_t RESERVED0; /*!< Reserved,                                Address offset: 0x14 */
    volatile uint32_t D3CR;      /*!< PWR D3 domain control register,          Address offset: 0x18 */
    uint32_t RESERVED1; /*!< Reserved,                                Address offset: 0x1C */
    volatile uint32_t WKUPCR;    /*!< PWR wakeup clear register,               Address offset: 0x20 */
    volatile uint32_t WKUPFR;    /*!< PWR wakeup flag register,                Address offset: 0x24 */
    volatile uint32_t WKUPEPR;   /*!< PWR wakeup enable and polarity register, Address offset: 0x28 */
} DTM_PWR_Type;

typedef struct
{
    uint32_t RESERVED1;           /*!< Reserved,                                           Address offset: 0x00        */
    volatile uint32_t PMCR;           /*!< SYSCFG peripheral mode configuration register,      Address offset: 0x04        */
    volatile uint32_t EXTICR[4];      /*!< SYSCFG external interrupt configuration registers,  Address offset: 0x08-0x14   */
    volatile uint32_t CFGR;           /*!< SYSCFG configuration registers,                     Address offset: 0x18        */
    uint32_t RESERVED2;           /*!< Reserved,                                           Address offset: 0x1C        */
    volatile uint32_t CCCSR;          /*!< SYSCFG compensation cell control/status register,   Address offset: 0x20        */
    volatile uint32_t CCVR;           /*!< SYSCFG compensation cell value register,            Address offset: 0x24        */
    volatile uint32_t CCCR;           /*!< SYSCFG compensation cell code register,             Address offset: 0x28        */
    volatile uint32_t PWRCR;          /*!< PWR control register,                               Address offset: 0x2C        */
    uint32_t     RESERVED3[61];  /*!< Reserved, 0x30-0x120                                                            */
    volatile uint32_t PKGR;          /*!< SYSCFG package register,                            Address offset: 0x124       */
    uint32_t     RESERVED4[118]; /*!< Reserved, 0x128-0x2FC                                                           */
} DTM_SYSCFG_Type;

#ifndef DTM_PMCTRL_BASE
#define DTM_PMCTRL_BASE 0x40007000UL
#endif
#ifndef DTM_SYSCFG_BASE
#define DTM_SYSCFG_BASE 0x58000400UL
#endif

#define PWR ((DTM_PWR_Type *)DTM_PMCTRL_BASE)
#define SYSCFG ((DTM_SYSCFG_Type *)DTM_SYSCFG_BASE)

int dtm_pmctrl_init(void) {
#ifdef DTM_PMCTRL_HIGHPERF_MODE
    /* Enable LDO */
    PWR->CR3 |= (1 << 1); // Enable LDO (bit 1 corresponds to LDOEN)
    /* Wait till voltage level flag ACTVOSRDY is set */
    while (!(PWR->CSR1 & (1 << 13))) {
        // Do nothing, just wait
    }
    /* Configure the Voltage Scaling 1 */
    /* PWR_D3CR_VOS D3CR[15：14]  */
    PWR->D3CR |= DTM_PMCTRL_HIGHPERF_MODE; // Set voltage scaling range 1 for high performance mode


    /* Enable the PWR overdrive */
    SYSCFG->PWRCR |= 1; // Enable PWR overdrive mode

    /* Wait till voltage level flag VOSRDY is set */
    while (!(PWR->D3CR & (1 << 13))) {
        // Do nothing, just wait
    }
#endif
    return 0;
}

#endif /* DTM_BINFDING_PMCTRL_ARM_M4_ST_GD */
