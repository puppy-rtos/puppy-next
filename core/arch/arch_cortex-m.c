/*
 * Copyright (c) 2022-2023, The Puppy RTOS Authors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <puppy_core.h>
#include <string.h>
#include "app_loader.h"
#include "app_manager.h"
#include "puppy_log.h"
#include "kheap.h"

#define PUP_GET_CPU_ID() 0

typedef struct {
    volatile pup_uint32_t CPUID;
    volatile pup_uint32_t ICSR;
    volatile pup_uint32_t VTOR;
    volatile pup_uint32_t AIRCR;
    volatile pup_uint32_t SCR;
    volatile pup_uint32_t CCR;
    volatile pup_uint32_t SHPR[3U];
    volatile pup_uint32_t SHCSR;
    volatile pup_uint32_t CFSR;
    volatile pup_uint32_t HFSR;
    volatile pup_uint32_t DFSR;
    volatile pup_uint32_t MMFAR;
    volatile pup_uint32_t BFAR;
} SCB_Type_Ext;

#define SCS_BASE_EXT            (0xE000E000UL)
#define SCB_BASE_EXT            (SCS_BASE_EXT +  0x0D00UL)

#ifdef PUP_ARCH_CORTEX_M

__attribute__((always_inline)) inline pup_base_t arch_irq_lock(void) {
    pup_base_t key;

    __asm volatile(
    "mrs %0, PRIMASK;"
        "cpsid i"
        : "=r" (key)
        :
        : "memory");

    return key;
}

__attribute__((always_inline)) inline void arch_irq_unlock(pup_base_t key) {
    __asm volatile(
    "msr PRIMASK, %0;"
    :
    : "r" (key)
    : "memory");
}

__attribute__((always_inline)) inline pup_uint8_t arch_irq_locked(pup_base_t key) {
    return key != 0U;
}
__attribute__((always_inline)) inline pup_uint8_t arch_in_irq(void) {
    volatile int tmp = 0;
    __asm volatile("mrs %0, IPSR;"
    : "=r" (tmp)
        :
        : "memory");
    return tmp != 0U;
}

__attribute__((always_inline)) inline pup_uint8_t arch_in_privileged(void) {
    volatile int tmp = 0;
    __asm volatile("mrs %0, CONTROL;"
    : "=r" (tmp)
        :
        : "memory");
    return (tmp & 0x1) == 0U;
}

void arch_enter_user_mode(void) {
    volatile int tmp = 0;
    __asm volatile(
        "mrs %0, CONTROL;"
        "orr %0, %0, #1;"
        "msr CONTROL, %0;"
        "dsb;"
        "isb;"
    : "=r" (tmp)
        :
        : "memory");
}

pup_weak void arch_spin_lock_init(arch_spinlock_t* lock) {

}

pup_weak void arch_spin_lock(arch_spinlock_t* lock) {

}

pup_weak void arch_spin_unlock(arch_spinlock_t* lock) {

}

typedef struct {
    pup_uint32_t CPUID;                  /*!< Offset: 0x000 (R/ )  CPUID Base Register */
    pup_uint32_t ICSR;                   /*!< Offset: 0x004 (R/W)  Interrupt Control and State Register */
    pup_uint32_t RESERVED0;
    pup_uint32_t AIRCR;                  /*!< Offset: 0x00C (R/W)  Application Interrupt and Reset Control Register */
    pup_uint32_t SCR;                    /*!< Offset: 0x010 (R/W)  System Control Register */
    pup_uint32_t CCR;                    /*!< Offset: 0x014 (R/W)  Configuration Control Register */
    pup_uint32_t RESERVED1;
    pup_uint32_t SHPR[2U];               /*!< Offset: 0x01C (R/W)  System Handlers Priority Registers. [0] is RESERVED */
    pup_uint32_t SHCSR;                  /*!< Offset: 0x024 (R/W)  System Handler Control and State Register */
} SCB_Type;
#define SCS_BASE            (0xE000E000UL)                             /*!< System Control Space Base Address */
#define SCB_BASE            (SCS_BASE +  0x0D00UL)                    /*!< System Control Block Base Address */
#define SCB                 ((SCB_Type       *)     SCB_BASE         ) 
#define SCB_ICSR_PENDSVSET_Pos             28U                                            /*!< SCB ICSR: PENDSVSET Position */
#define SCB_ICSR_PENDSVSET_Msk             (1UL << SCB_ICSR_PENDSVSET_Pos)    
#define SCB_SHPR3           (*(volatile pup_uint32_t*)(SCB_BASE + 0x20UL))
#define SCB_SHPR3_PENDSV_Msk (0xFFUL << 16)
#define SCB_SHPR3_SYSTICK_Msk (0xFFUL << 24)

void arch_sched_init(void) {
    /* Keep PendSV/SysTick at the lowest priority so context switch only runs after IRQ return. */
    pup_uint32_t shpr3 = SCB_SHPR3;
    shpr3 &= ~(SCB_SHPR3_PENDSV_Msk | SCB_SHPR3_SYSTICK_Msk);
    shpr3 |= SCB_SHPR3_PENDSV_Msk | SCB_SHPR3_SYSTICK_Msk;
    SCB_SHPR3 = shpr3;
}

struct arch_thread {
    pup_uint32_t stack_ptr;
    pup_uint32_t kernel_stack_ptr;
    void* kernel_stack_base;
    pup_uint32_t kernel_stack_size;
};

struct arch_cpu {
    void* from_sp_swap;
    void* to_sp_swap;
} arch_cpu[PUP_CPU_NR] = { 0 };

typedef struct exc_stack_frame {
    pup_uint32_t r0;
    pup_uint32_t r1;
    pup_uint32_t r2;
    pup_uint32_t r3;
    pup_uint32_t r12;
    pup_uint32_t lr;
    pup_uint32_t pc;
    pup_uint32_t psr;
}_esf_t;

typedef struct stack_frame {
    pup_uint32_t ctrl_reg;
    pup_uint32_t r4;
    pup_uint32_t r5;
    pup_uint32_t r6;
    pup_uint32_t r7;
    pup_uint32_t r8;
    pup_uint32_t r9;
    pup_uint32_t r10;
    pup_uint32_t r11;

    _esf_t esf;
}_sf_t;

void* arch_new_thread(void* entry,
                      void* param1,
                      void* param2,
                      void* stack_addr,
                      pup_uint32_t stack_size,
                      pup_uint8_t use_kernel_stack) {
    pup_uint32_t i;
    struct arch_thread* arch_data;
    _sf_t* sf;
    void* kernel_stack_base = PUP_NULL;

    arch_data = (struct arch_thread*)kheap_calloc(1, sizeof(struct arch_thread));
    if (!arch_data) {
        return PUP_NULL;
    }

    if (use_kernel_stack) {
        kernel_stack_base = kheap_memalign(8, PUP_USER_SYSCALL_STACK_SIZE);
        if (!kernel_stack_base) {
            kheap_free(arch_data);
            return PUP_NULL;
        }

        memset(kernel_stack_base, 0x23, PUP_USER_SYSCALL_STACK_SIZE);
    }

    sf = (_sf_t*)PUP_ALIGN_DOWN(((pup_uint32_t)stack_addr + stack_size) - sizeof(_sf_t), 8UL);

    /* init all register */
    for (i = 0; i < sizeof(_sf_t) / sizeof(pup_uint32_t); i++) {
        ((pup_uint32_t*)sf)[i] = 0xdeadbeef;
    }

    sf->esf.r0 = (pup_uint32_t)param1;
    sf->esf.r1 = (pup_uint32_t)param2;
    sf->esf.r2 = 0;
    sf->esf.r3 = 0;
    sf->esf.r12 = 0;
    sf->esf.lr = 0;
    sf->esf.pc = (pup_uint32_t)entry;
    sf->esf.psr = 0x01000000UL;

    sf->ctrl_reg = 0x02;

    arch_data->stack_ptr = ((pup_uint32_t)sf);
    arch_data->kernel_stack_base = kernel_stack_base;
    arch_data->kernel_stack_size = use_kernel_stack ? PUP_USER_SYSCALL_STACK_SIZE : 0;
    arch_data->kernel_stack_ptr = use_kernel_stack ? ((pup_uint32_t)kernel_stack_base + PUP_USER_SYSCALL_STACK_SIZE) : 0;
    
    return arch_data;
}

void arch_delete_thread(void* arch_data_ptr) {
    struct arch_thread* arch_data = (struct arch_thread*)arch_data_ptr;

    if (!arch_data) {
        return;
    }

    if (arch_data->kernel_stack_base) {
        kheap_free(arch_data->kernel_stack_base);
    }

    kheap_free(arch_data);
}

void* arch_thread_get_kernel_stackaddr(void* arch_data_ptr) {
    struct arch_thread* arch_data = (struct arch_thread*)arch_data_ptr;

    if (!arch_data) {
        return PUP_NULL;
    }

    return arch_data->kernel_stack_base;
}

pup_size_t arch_thread_get_kernel_stacksize(void* arch_data_ptr) {
    struct arch_thread* arch_data = (struct arch_thread*)arch_data_ptr;

    if (!arch_data) {
        return 0;
    }

    return arch_data->kernel_stack_size;
}

void arch_swap(void* from, void* to) {
#ifndef P_ARCH_CORTEX_M0
    __asm ("CLREX");
#endif
    arch_cpu[PUP_GET_CPU_ID()].from_sp_swap = from ? &((struct arch_thread*)from)->stack_ptr : PUP_NULL;
    arch_cpu[PUP_GET_CPU_ID()].to_sp_swap = &((struct arch_thread*)to)->stack_ptr;

    /* set pending bit to make sure we will take a PendSV exception */
    SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;

}
extern void* arch_get_from_sp(void);
extern void* arch_get_to_sp(void);

void* arch_get_from_sp(void) {
    return arch_cpu[PUP_GET_CPU_ID()].from_sp_swap;
}
void* arch_get_to_sp(void) {
    return arch_cpu[PUP_GET_CPU_ID()].to_sp_swap;
}

__attribute__((naked)) void PendSV_Handler(void) {
    __asm (
    "    MRS     r2, PRIMASK\n"
        "    CPSID   I\n"

        "    PUSH    {r2, lr}\n"
        "    BL      arch_get_from_sp\n"
        "    POP     {r2, lr}\n"
        "    CBZ     r0, switch_to_thread \n"

        "    MRS     r1, psp              \n"

        "    STMFD   r1!, {r4 - r11}      \n"
        "    MRS     r3, CONTROL          \n"
        "    STMFD   r1!, {r3}            \n"
        "    STR     r1, [r0]             \n"

        "switch_to_thread:\n"
        "    PUSH    {r2, lr}\n"
        "    BL      arch_get_to_sp\n"
        "    POP     {r2, lr}\n"

        "    LDR     r1, [r0]\n"
        "    LDMFD   r1!, {r3}            \n"
        "    MSR     CONTROL, r3          \n"
        "    LDMFD   r1!, {r4 - r11}\n"
        "    MSR     psp, r1\n"

        "    PUSH    {r2, lr}\n"
        "    BL      pup_sched_swap_done_cb\n"
        "    POP     {r2, lr}\n"

        "    MSR     PRIMASK, r2\n"

        "    MRS     r1, CONTROL\n"
        "    TST     r1, #0x01\n"
        "    BNE     ret_to_user\n"
        "    ORR     lr, lr, #0x04\n"
        "    BX      lr\n"

        "ret_to_user:\n"
        "    ORR     lr, lr, #0x04\n"
        "    BX      lr\n"
        );
}

struct exception_info {
    pup_uint32_t exc_return;
    struct stack_frame stack_frame;
};

static void dump_contex(struct stack_frame* context) {
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "psr: 0x%08x", context->esf.psr);
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "r00: 0x%08x", context->esf.r0);
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "r01: 0x%08x", context->esf.r1);
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "r02: 0x%08x", context->esf.r2);
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "r03: 0x%08x", context->esf.r3);
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "r04: 0x%08x", context->r4);
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "r05: 0x%08x", context->r5);
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "r06: 0x%08x", context->r6);
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "r07: 0x%08x", context->r7);
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "r08: 0x%08x", context->r8);
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "r09: 0x%08x", context->r9);
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "r10: 0x%08x", context->r10);
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "r11: 0x%08x", context->r11);
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "r12: 0x%08x", context->esf.r12);
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, " lr: 0x%08x", context->esf.lr);
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, " pc: 0x%08x", context->esf.pc);
}

void arch_hardfault_exception(struct exception_info* exception_info) {
    struct stack_frame* context = &exception_info->stack_frame;
    extern int pup_mpu_setup_kernel_protection(void);
    app_process_t* app = app_get_current();

    if (exception_info->exc_return & (1 << 2)) {
        pthread_t thread = pthread_self();
        const char* name = pthread_getname(thread);
        PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "hard fault on thread: %s", name);
    } else {
        PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "hard fault on handler");
    }

    if ((exception_info->exc_return & 0x10) == 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "FPU active!");
    }

    SCB_Type_Ext* scb = (SCB_Type_Ext*)SCS_BASE_EXT;
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "HFSR: 0x%08x", scb->HFSR);
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "CFSR: 0x%08x", scb->CFSR);
    if (scb->CFSR & 0x8000) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "BFARVALID: BFAR contains valid fault address");
        PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "BFAR: 0x%08x", scb->BFAR);
    }
    if (scb->CFSR & 0x80) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "MMARVALID: MMFAR contains valid fault address");
        PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "MMFAR: 0x%08x", scb->MMFAR);
    }

    dump_contex(context);

    if (app && (exception_info->exc_return & (1 << 2))) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "User application fault, terminating...");
        extern int pthread_terminate_self(void);
        if (app->thread == pthread_self()) {
            app->exit_code = -1;
            app->state = APP_STATE_EXITED;
        }
        pup_mpu_setup_kernel_protection();
        pthread_terminate_self();
        return;
    }

    while (1);
}

#define SCB_EXT                 ((SCB_Type_Ext *)SCB_BASE_EXT)

#define SCB_CFSR_MEMFAULTSR_Pos    0U
#define SCB_CFSR_MEMFAULTSR_Msk    (0xFFUL << SCB_CFSR_MEMFAULTSR_Pos)

void arch_memmanage_exception(struct exception_info* exception_info) {
    struct stack_frame* context = &exception_info->stack_frame;
    pup_uint32_t cfsr = SCB_EXT->CFSR;
    pup_uint32_t mmfar = SCB_EXT->MMFAR;
    extern int pup_mpu_setup_kernel_protection(void);
    app_process_t* app = app_get_current();
    
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "=== MPU Memory Management Fault ===");
    
    if (exception_info->exc_return & (1 << 2)) {
        pthread_t thread = pthread_self();
        const char* name = pthread_getname(thread);
        PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "Fault on thread: %s (user mode)", name);
    } else {
        PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "Fault on handler (kernel mode)");
    }
    
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "CFSR: 0x%08x", cfsr);
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "MMFAR: 0x%08x", mmfar);
    
    if (cfsr & (1 << 7)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "MMARVALID: MMFAR contains valid fault address");
    }
    if (cfsr & (1 << 5)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "MLSPERR: MemManage fault on floating-point lazy state preservation");
    }
    if (cfsr & (1 << 4)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "MSTKERR: MemManage fault on stacking");
    }
    if (cfsr & (1 << 3)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "MUNSTKERR: MemManage fault on unstacking");
    }
    if (cfsr & (1 << 1)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "DACCVIOL: Data access violation");
    }
    if (cfsr & (1 << 0)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "IACCVIOL: Instruction access violation");
    }
    
    dump_contex(context);
    
    PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "====================================");
    
    if (app && (exception_info->exc_return & (1 << 2))) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_ARCH, "User application memory fault, terminating...");
        SCB_EXT->CFSR = cfsr;
        extern int pthread_terminate_self(void);
        if (app->thread == pthread_self()) {
            app->exit_code = -1;
            app->state = APP_STATE_EXITED;
        }
        pup_mpu_setup_kernel_protection();
        pthread_terminate_self();
        return;
    }
    
    while (1);
}

__attribute__((naked)) void MemManage_Handler(void) {
    __asm ("    TST     lr, #0x04\n"
    "    ITE     EQ\n"
        "    MRSEQ   r0, msp\n"
        "    MRSNE   r0, psp\n"
        "    STMFD   r0!, {r4 - r11}\n"
        "    MRS     r1, CONTROL\n"
        "    STMFD   r0!, {r1}\n"
        "    STMFD   r0!, {lr}\n"

        "    TST     lr, #0x04\n"
        "    ITE     EQ\n"
        "    MSREQ   msp, r0\n"
        "    MSRNE   psp, r0\n"

        "    PUSH    {lr}\n"
        "    BL      arch_memmanage_exception\n"
        "    POP     {lr}\n"
        

        "    ORR     lr, lr, #0x04\n"
        "    BX      lr\n"
        );
}

__attribute__((naked)) void HardFault_Handler(void) {
    __asm ("    TST     lr, #0x04\n"
    "    ITE     EQ\n"
        "    MRSEQ   r0, msp\n"
        "    MRSNE   r0, psp\n"
        "    STMFD   r0!, {r4 - r11}\n"
        "    MRS     r1, CONTROL\n"
        "    STMFD   r0!, {r1}\n"
        "    STMFD   r0!, {lr}\n"

        "    TST     lr, #0x04\n"
        "    ITE     EQ\n"
        "    MSREQ   msp, r0\n"
        "    MSRNE   psp, r0\n"

        "    PUSH    {lr}\n"
        "    BL      arch_hardfault_exception\n"
        "    POP     {r0}\n"
        
        "    LDR     r1, =0xE000ED94\n"
        "    MOV     r2, #0\n"
        "    STR     r2, [r1]\n"

        "    MOV     lr, #0xFFFFFFF9\n"
        "    BX      lr\n"
        );
}

extern long syscall_handler(long syscall_num, long arg1, long arg2, long arg3, long arg4);

struct arch_thread* arch_get_current_thread(void) {
    extern pthread_t pthread_self(void);
    extern void* pup_thread_get_arch_data(pthread_t thread);
    pthread_t thread = pthread_self();
    if (thread) {
        return (struct arch_thread*)pup_thread_get_arch_data(thread);
    }
    return PUP_NULL;
}

/*
 * Syscall entry and SVC handler are implemented in syscall_entry.S
 * for easier debugging with breakpoints.
 */
extern void return_user_space(void);
extern void syscall_entry(void);
extern void SVC_Handler(void);

/**
 * @brief Get current stack pointer
 * Returns the current stack pointer (PSP in thread mode, MSP otherwise)
 */
pup_ubase_t arch_get_stack_pointer(void) {
    pup_ubase_t sp = 0;
    __asm volatile("MRS %0, PSP" : "=r" (sp));
    
    /* If PSP is 0, we're likely in handler mode, read MSP instead */
    if (sp == 0) {
        __asm volatile("MRS %0, MSP" : "=r" (sp));
    }
    
    return sp;
}

#endif
