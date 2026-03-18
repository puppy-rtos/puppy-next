#ifndef __STACK_GUARD_H__
#define __STACK_GUARD_H__

#include <puppy_core.h>

#ifndef PUP_STACK_FILL_BYTE
#define PUP_STACK_FILL_BYTE 0x23U
#endif

#ifndef PUP_STACK_GUARD_WORDS
#define PUP_STACK_GUARD_WORDS 4U
#endif

#define PUP_STACK_GUARD_RESERVED_BYTES (PUP_STACK_GUARD_WORDS * sizeof(pup_uint32_t))

int pup_stack_guard_check(pthread_t thread);
int pup_stack_guard_check_thread(pthread_t thread);
int pup_stack_guard_check_syscall(pthread_t thread);
int pup_stack_guard_check_syscall_context(pthread_t thread);

#endif