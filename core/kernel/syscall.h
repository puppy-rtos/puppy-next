#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include <puppy_core.h>
#include "shared/pup_heap_info.h"
#include "shared/pup_ipc.h"
#include "shared/pup_app_memory.h"

#define SYS_FD_STDIN   0
#define SYS_FD_STDOUT  1
#define SYS_FD_STDERR  2

#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_EXIT    3
#define SYS_YIELD   4
#define SYS_THREAD_CREATE   5
#define SYS_THREAD_JOIN     6
#define SYS_OPEN    7
#define SYS_CLOSE   8
#define SYS_IOCTL   9
#define SYS_GETDENTS   10
#define SYS_PS         11
#define SYS_EXECVE     12
#define SYS_WAITPID    13
#define SYS_GETPID     14
#define SYS_DELAY      15
#define SYS_APPS       16
#define SYS_TICK_GET   17
#define SYS_FREE       18

#define SYS_SEM_OPEN   19
#define SYS_SEM_CLOSE  20
#define SYS_SEM_WAIT   21
#define SYS_SEM_POST   22

#define SYS_SHARED_MALLOC    23
#define SYS_SHARED_FREE      24
#define SYS_SHARED_MEMALIGN  25
#define SYS_APP_RAM_RANGE    26

typedef long (*syscall_func_t)(long arg1, long arg2, long arg3, long arg4);

long syscall_handler(long syscall_num, long arg1, long arg2, long arg3, long arg4);
void syscall_uart_rx_init(void);

static inline long syscall_write(int fd, const char* buf, long len) {
    return syscall_handler(SYS_WRITE, fd, (long)buf, len, 0);
}

static inline long syscall_read(int fd, char* buf, long len) {
    return syscall_handler(SYS_READ, fd, (long)buf, len, 0);
}

static inline void syscall_exit(int code) {
    syscall_handler(SYS_EXIT, code, 0, 0, 0);
}

static inline void syscall_yield(void) {
    syscall_handler(SYS_YIELD, 0, 0, 0, 0);
}

#endif
