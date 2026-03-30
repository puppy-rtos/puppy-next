// dtm/arch/arm/cortex-m4/dtm_irq.h
#pragma once

#include <stdint.h>

/* 寄存器访问修饰符 */
#define __IOM volatile       // 读写寄存器
#define __IM  volatile const // 只读寄存器
#define __OM  volatile       // 只写寄存器

/* 中断优先级位数 */
#define __NVIC_PRIO_BITS 4

/* Cortex-M 基础中断类型 */
typedef enum {
    /* 内核中断 */
    DTM_NMI_IRQn            = -14,
    DTM_HardFault_IRQn      = -13,
    DTM_MemManage_IRQn      = -12,
    DTM_BusFault_IRQn       = -11,
    DTM_UsageFault_IRQn     = -10,
    DTM_SVCall_IRQn         = -5,
    DTM_DebugMonitor_IRQn   = -4,
    DTM_PendSV_IRQn         = -2,
    DTM_SysTick_IRQn        = -1,
    
    /* 芯片外设中断从这里开始 (0+) */
    DTM_ChipSpecific_IRQn   = 0
} DTM_IRQn_Type;

/* NVIC 寄存器结构 */
typedef struct {
    __IOM uint32_t ISER[8];      // 中断使能设置
    uint32_t RESERVED0[24];
    __IOM uint32_t ICER[8];      // 中断使能清除
    uint32_t RESERVED1[24];
    __IOM uint32_t ISPR[8];      // 中断挂起设置
    uint32_t RESERVED2[24];
    __IOM uint32_t ICPR[8];      // 中断挂起清除
    uint32_t RESERVED3[24];
    __IOM uint32_t IABR[8];      // 中断活跃位
    uint32_t RESERVED4[56];
    __IOM uint8_t  IP[240];      // 中断优先级
    uint32_t RESERVED5[644];
    __OM  uint32_t STIR;         // 软件触发中断
} DTM_NVIC_Type;

#define DTM_NVIC_BASE  (0xE000E100UL)
#define DTM_NVIC ((DTM_NVIC_Type *)DTM_NVIC_BASE)

/* NVIC 操作函数 */
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