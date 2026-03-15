#ifndef __USER_SYSCALL_H__
#define __USER_SYSCALL_H__

#include "shared/pup_heap_info.h"

#define USER_FD_STDIN   0
#define USER_FD_STDOUT  1
#define USER_FD_STDERR  2

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

long syscall(long syscall_num, long arg1, long arg2, long arg3, long arg4);

static inline long user_write(int fd, const char* buf, long len) {
    return syscall(SYS_WRITE, fd, (long)buf, len, 0);
}

static inline long user_read(int fd, char* buf, long len) {
    return syscall(SYS_READ, fd, (long)buf, len, 0);
}

static inline long user_write_stdout(const char* buf, long len) {
    return user_write(USER_FD_STDOUT, buf, len);
}

static inline long user_write_stderr(const char* buf, long len) {
    return user_write(USER_FD_STDERR, buf, len);
}

static inline long user_read_stdin(char* buf, long len) {
    return user_read(USER_FD_STDIN, buf, len);
}

static inline void user_exit(int code) {
    syscall(SYS_EXIT, code, 0, 0, 0);
}

static inline void user_yield(void) {
    syscall(SYS_YIELD, 0, 0, 0, 0);
}

static inline int user_thread_create(void* (*entry)(void*), void* arg, 
                                      void* stackaddr, unsigned int stacksize, 
                                      int priority) {
    return (int)syscall(SYS_THREAD_CREATE, (long)entry, (long)arg, 
                        (long)stackaddr, (long)((stacksize << 16) | (priority & 0xFFFF)));
}

static inline int user_thread_join(int thread_id) {
    return (int)syscall(SYS_THREAD_JOIN, thread_id, 0, 0, 0);
}

static inline int user_open(const char* path, int flags) {
    return (int)syscall(SYS_OPEN, (long)path, flags, 0, 0);
}

static inline int user_close(int fd) {
    return (int)syscall(SYS_CLOSE, fd, 0, 0, 0);
}

static inline int user_ioctl(int fd, int cmd, void* arg) {
    return (int)syscall(SYS_IOCTL, fd, cmd, (long)arg, 0);
}

static inline int user_getdents(int fd, void* dirp, int count) {
    return (int)syscall(SYS_GETDENTS, fd, (long)dirp, count, 0);
}

static inline int user_ps(void) {
    return (int)syscall(SYS_PS, 0, 0, 0, 0);
}

static inline int user_execve_with_arg(const char* path, const char* name, const char* arg, int wait) {
    return (int)syscall(SYS_EXECVE, (long)path, (long)name, wait, (long)arg);
}

static inline int user_execve(const char* path, const char* name, int wait) {
    return user_execve_with_arg(path, name, 0, wait);
}

static inline int user_waitpid(int pid, int* status) {
    return (int)syscall(SYS_WAITPID, pid, (long)status, 0, 0);
}

static inline int user_getpid(void) {
    return (int)syscall(SYS_GETPID, 0, 0, 0, 0);
}

static inline int user_delay(int ticks) {
    return (int)syscall(SYS_DELAY, ticks, 0, 0, 0);
}

static inline int user_apps(void) {
    return (int)syscall(SYS_APPS, 0, 0, 0, 0);
}

static inline unsigned long user_tick_get(void) {
    return (unsigned long)syscall(SYS_TICK_GET, 0, 0, 0, 0);
}

static inline int user_free(pup_heap_info_t* info) {
    return (int)syscall(SYS_FREE, (long)info, 0, 0, 0);
}

#endif
