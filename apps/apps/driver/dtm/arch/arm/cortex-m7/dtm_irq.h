#pragma once

#include <stdint.h>

#define __IOM volatile
#define __IM  volatile const
#define __OM  volatile

#define __NVIC_PRIO_BITS 4

typedef enum {
    DTM_NMI_IRQn            = -14,
    DTM_HardFault_IRQn      = -13,
    DTM_MemManage_IRQn      = -12,
    DTM_BusFault_IRQn       = -11,
    DTM_UsageFault_IRQn     = -10,
    DTM_SVCall_IRQn         = -5,
    DTM_DebugMonitor_IRQn   = -4,
    DTM_PendSV_IRQn         = -2,
    DTM_SysTick_IRQn        = -1,
    DTM_ChipSpecific_IRQn   = 0
} DTM_IRQn_Type;

typedef struct {
    __IOM uint32_t ISER[8];
    uint32_t RESERVED0[24];
    __IOM uint32_t ICER[8];
    uint32_t RESERVED1[24];
    __IOM uint32_t ISPR[8];
    uint32_t RESERVED2[24];
    __IOM uint32_t ICPR[8];
    uint32_t RESERVED3[24];
    __IOM uint32_t IABR[8];
    uint32_t RESERVED4[56];
    __IOM uint8_t  IP[240];
    uint32_t RESERVED5[644];
    __OM  uint32_t STIR;
} DTM_NVIC_Type;

#define DTM_NVIC_BASE  (0xE000E100UL)
#define DTM_NVIC ((DTM_NVIC_Type *)DTM_NVIC_BASE)

static inline void dtm_nvic_enable_irq(DTM_IRQn_Type IRQn) {
    if ((int32_t)IRQn >= 0) {
        DTM_NVIC->ISER[((uint32_t)IRQn) >> 5] = (1UL << ((uint32_t)IRQn & 0x1F));
    }
}

static inline void dtm_nvic_set_priority(DTM_IRQn_Type IRQn, uint8_t priority) {
    if ((int32_t)IRQn >= 0) {
        DTM_NVIC->IP[(uint32_t)IRQn] = (priority << (8 - __NVIC_PRIO_BITS));
    }
}
