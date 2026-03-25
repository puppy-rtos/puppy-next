#ifndef __APP_SYSCALL_H__
#define __APP_SYSCALL_H__

#include "shared/pup_heap_info.h"
#include "shared/pup_ipc.h"
#include "shared/pup_app_memory.h"
#include "shared/pup_user_thread.h"

#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

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

long syscall(long syscall_num, long arg1, long arg2, long arg3, long arg4);

static inline long write(int fd, const char* buf, long len) {
    return syscall(SYS_WRITE, fd, (long)buf, len, 0);
}

static inline long read(int fd, char* buf, long len) {
    return syscall(SYS_READ, fd, (long)buf, len, 0);
}

static inline long write_stdout(const char* buf, long len) {
    return write(STDOUT_FILENO, buf, len);
}

static inline long write_stderr(const char* buf, long len) {
    return write(STDERR_FILENO, buf, len);
}

static inline long read_stdin(char* buf, long len) {
    return read(STDIN_FILENO, buf, len);
}

static inline void exit(int code) {
    syscall(SYS_EXIT, code, 0, 0, 0);
}

static inline void sys_yield(void) {
    syscall(SYS_YIELD, 0, 0, 0, 0);
}

static inline int thread_create(void* (*entry)(void*),
                                const pup_user_thread_create_t* params) {
    if (!params) {
        return -1;
    }

    return (int)syscall(SYS_THREAD_CREATE, (long)entry, (long)params, 0, 0);
}

static inline int thread_join(int thread_id) {
    return (int)syscall(SYS_THREAD_JOIN, thread_id, 0, 0, 0);
}

static inline int open(const char* path, int flags) {
    return (int)syscall(SYS_OPEN, (long)path, flags, 0, 0);
}

static inline int close(int fd) {
    return (int)syscall(SYS_CLOSE, fd, 0, 0, 0);
}

static inline int ioctl(int fd, int cmd, void* arg) {
    return (int)syscall(SYS_IOCTL, fd, cmd, (long)arg, 0);
}

static inline int getdents(int fd, void* dirp, int count) {
    return (int)syscall(SYS_GETDENTS, fd, (long)dirp, count, 0);
}

static inline int ps(void) {
    return (int)syscall(SYS_PS, 0, 0, 0, 0);
}

static inline int execve_with_arg(const char* path, const char* name, const char* arg, int wait) {
    return (int)syscall(SYS_EXECVE, (long)path, (long)name, wait, (long)arg);
}

static inline int execve(const char* path, const char* name, int wait) {
    return execve_with_arg(path, name, 0, wait);
}

static inline int waitpid(int pid, int* status) {
    return (int)syscall(SYS_WAITPID, pid, (long)status, 0, 0);
}

static inline int getpid(void) {
    return (int)syscall(SYS_GETPID, 0, 0, 0, 0);
}

static inline int delay(int ticks) {
    return (int)syscall(SYS_DELAY, ticks, 0, 0, 0);
}

static inline int apps(void) {
    return (int)syscall(SYS_APPS, 0, 0, 0, 0);
}

static inline unsigned long tick_get(void) {
    return (unsigned long)syscall(SYS_TICK_GET, 0, 0, 0, 0);
}

static inline int free_info(pup_heap_info_t* info) {
    return (int)syscall(SYS_FREE, (long)info, 0, 0, 0);
}

static inline int sem_open(const char *name, int flags, unsigned int value) {
    return (int)syscall(SYS_SEM_OPEN, (long)name, (long)flags, (long)value, 0);
}

static inline int sem_close(int handle) {
    return (int)syscall(SYS_SEM_CLOSE, (long)handle, 0, 0, 0);
}

static inline int sem_wait(int handle) {
    return (int)syscall(SYS_SEM_WAIT, (long)handle, 0, 0, 0);
}

static inline int sem_post(int handle) {
    return (int)syscall(SYS_SEM_POST, (long)handle, 0, 0, 0);
}

static inline void* shared_malloc(size_t size) {
    return (void*)syscall(SYS_SHARED_MALLOC, size, 0, 0, 0);
}

static inline void shared_free(void* ptr) {
    syscall(SYS_SHARED_FREE, (long)ptr, 0, 0, 0);
}

static inline void* shared_memalign(size_t align, size_t size) {
    return (void*)syscall(SYS_SHARED_MEMALIGN, align, size, 0, 0);
}

static inline int app_get_ram_range(pup_app_ram_range_t* range) {
    return (int)syscall(SYS_APP_RAM_RANGE, (long)range, 0, 0, 0);
}

#endif
