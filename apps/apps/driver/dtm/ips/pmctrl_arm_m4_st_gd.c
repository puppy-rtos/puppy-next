
#include "dtm_bindings.h"

#ifdef DTM_BINFDING_PMCTRL_ARM_M4_ST_GD
#include "dtm_pmctrl.h"

#include <stdint.h>

typedef struct
{
  volatile uint32_t CR;   /*!< PWR power control register,        Address offset: 0x00 */
  volatile uint32_t CSR;  /*!< PWR power control/status register, Address offset: 0x04 */
} DTM_PWR_Type;

#ifndef DTM_PMCTRL_BASE
#define DTM_PMCTRL_BASE 0x40007000UL
#endif

#define PWR ((DTM_PWR_Type *)DTM_PMCTRL_BASE)


int dtm_pmctrl_init(void) {
#ifdef DTM_PMCTRL_HIGHPERF_MODE
    PWR->CR |= DTM_PMCTRL_HIGHPERF_MODE; // Set voltage scaling range 1 for high performance mode
#endif
    return 0;
}

#endif /* DTM_BINFDING_PMCTRL_ARM_M4_ST_GD */
