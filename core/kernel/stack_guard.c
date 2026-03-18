#include "stack_guard.h"

#include "puppy_log.h"

#ifndef PUP_STACK_GUARD_MARGIN
#define PUP_STACK_GUARD_MARGIN 32U
#endif

extern pup_ubase_t arch_get_stack_pointer(void);

static int pup_stack_guard_sp_in_region(pup_ubase_t sp,
                                        void* stackaddr,
                                        pup_size_t stacksize) {
    pup_ubase_t stack_low;
    pup_ubase_t stack_high;

    if (sp == 0U || !stackaddr || stacksize == 0U) {
        return 0;
    }

    stack_low = (pup_ubase_t)stackaddr;
    stack_high = stack_low + stacksize;
    return sp >= stack_low && sp < stack_high;
}

static pup_ubase_t pup_stack_guard_get_saved_user_sp(pthread_t thread) {
    pup_uint8_t* kernel_stackaddr;
    pup_size_t kernel_stacksize;

    kernel_stackaddr = (pup_uint8_t*)pup_thread_get_kernel_stackaddr(thread);
    kernel_stacksize = pup_thread_get_kernel_stacksize(thread);
    if (!kernel_stackaddr || kernel_stacksize < sizeof(pup_ubase_t)) {
        return 0;
    }

    return *(pup_ubase_t*)(kernel_stackaddr + kernel_stacksize - sizeof(pup_ubase_t));
}

static int pup_stack_guard_reserved_ok(void* stackaddr, pup_size_t stacksize) {
    pup_uint8_t* guard;
    pup_size_t i;
    pup_size_t guard_bytes;

    if (!stackaddr || stacksize == 0U) {
        return 1;
    }

    guard_bytes = stacksize > PUP_STACK_GUARD_RESERVED_BYTES ? PUP_STACK_GUARD_RESERVED_BYTES : stacksize;
    guard = (pup_uint8_t*)stackaddr;
    for (i = 0; i < guard_bytes; i++) {
        if (guard[i] != (pup_uint8_t)PUP_STACK_FILL_BYTE) {
            return 0;
        }
    }

    return 1;
}

static int pup_stack_guard_report(const char* stack_name,
                                  pthread_t thread,
                                  pup_ubase_t sp,
                                  pup_ubase_t stack_low,
                                  pup_ubase_t stack_high,
                                  const char* reason) {
    PUP_LOG_ERROR(PUP_LOG_MODULE_THREAD,
                  "%s stack guard violated: thread=%s sp=0x%08x range=[0x%08x,0x%08x) reason=%s",
                  stack_name,
                  pthread_getname(thread),
                  (unsigned int)sp,
                  (unsigned int)stack_low,
                  (unsigned int)stack_high,
                  reason);
    return -1;
}

static int pup_stack_guard_check_region(const char* stack_name,
                                        pthread_t thread,
                                        void* stackaddr,
                                        pup_size_t stacksize,
                                        pup_ubase_t sp) {
    pup_ubase_t stack_low;
    pup_ubase_t stack_high;

    if (!thread || !stackaddr || stacksize == 0U) {
        return 0;
    }

    stack_low = (pup_ubase_t)stackaddr;
    stack_high = stack_low + stacksize;

    if (!pup_stack_guard_reserved_ok(stackaddr, stacksize)) {
        return pup_stack_guard_report(stack_name, thread, sp, stack_low, stack_high, "stack-fill-guard");
    }

    if (sp != 0U && (sp < stack_low || sp > stack_high)) {
        return pup_stack_guard_report(stack_name, thread, sp, stack_low, stack_high, "stack-pointer-range");
    }

    if (sp != 0U && sp < (stack_low + PUP_STACK_GUARD_MARGIN)) {
        return pup_stack_guard_report(stack_name, thread, sp, stack_low, stack_high, "stack-margin");
    }

    return 0;
}

int pup_stack_guard_check(pthread_t thread) {
    pup_ubase_t current_sp;
    void* kernel_stackaddr;
    pup_size_t kernel_stacksize;

    if (!thread) {
        return 0;
    }

    current_sp = arch_get_stack_pointer();
    kernel_stackaddr = pup_thread_get_kernel_stackaddr(thread);
    kernel_stacksize = pup_thread_get_kernel_stacksize(thread);

    if (pup_stack_guard_sp_in_region(current_sp, kernel_stackaddr, kernel_stacksize)) {
        return pup_stack_guard_check_syscall_context(thread);
    }

    return pup_stack_guard_check_region("thread",
                                        thread,
                                        pup_thread_get_stackaddr(thread),
                                        pup_thread_get_stacksize(thread),
                                        current_sp);
}

int pup_stack_guard_check_thread(pthread_t thread) {
    return pup_stack_guard_check_region("thread",
                                        thread,
                                        pup_thread_get_stackaddr(thread),
                                        pup_thread_get_stacksize(thread),
                                        arch_get_stack_pointer());
}

int pup_stack_guard_check_syscall(pthread_t thread) {
    void* kernel_stackaddr = pup_thread_get_kernel_stackaddr(thread);
    pup_size_t kernel_stacksize = pup_thread_get_kernel_stacksize(thread);

    if (!kernel_stackaddr || kernel_stacksize == 0U) {
        return pup_stack_guard_check_thread(thread);
    }

    return pup_stack_guard_check_region("syscall",
                                        thread,
                                        kernel_stackaddr,
                                        kernel_stacksize,
                                        arch_get_stack_pointer());
}

int pup_stack_guard_check_syscall_context(pthread_t thread) {
    pup_ubase_t saved_user_sp;

    if (!thread) {
        return 0;
    }

    saved_user_sp = pup_stack_guard_get_saved_user_sp(thread);
    if (pup_stack_guard_check_region("thread",
                                     thread,
                                     pup_thread_get_stackaddr(thread),
                                     pup_thread_get_stacksize(thread),
                                     saved_user_sp) != 0) {
        return -1;
    }

    return pup_stack_guard_check_syscall(thread);
}