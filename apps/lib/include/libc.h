#ifndef __APP_LIBC_H__
#define __APP_LIBC_H__

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

typedef void* pthread_t;
typedef unsigned long pup_tick_t;
typedef struct {
    const char* name;
    void* stackaddr;
    size_t stacksize;
    int priority;
    int detachstate;
} pthread_attr_t;

#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

int pthread_attr_init(pthread_attr_t* attr);
int pthread_attr_destroy(pthread_attr_t* attr);
int pthread_attr_setstackaddr(pthread_attr_t* attr, void* stackaddr);
int pthread_attr_setstacksize(pthread_attr_t* attr, size_t stacksize);
int pthread_attr_setname(pthread_attr_t* attr, const char* name);
int pthread_attr_setpriority(pthread_attr_t* attr, int priority);

int pthread_create(pthread_t* thread, pthread_attr_t* attr, 
                   void* (*start_routine)(void*), void* arg);
void pthread_exit(void* retval);
pthread_t pthread_self(void);
int pthread_join(pthread_t thread, void** retval);
int pthread_delay(int ticks);

pup_tick_t pup_tick_get(void);

int sched_yield(void);

int printf(const char* fmt, ...);
int vprintf(const char* fmt, va_list args);
int snprintf(char* buf, size_t size, const char* fmt, ...);
int vsnprintf(char* buf, size_t size, const char* fmt, va_list args);

size_t strlen(const char* s);
char* strcpy(char* dest, const char* src);
char* strcat(char* dest, const char* src);
int strcmp(const char* s1, const char* s2);
void* memset(void* s, int c, size_t n);
void* memcpy(void* dest, const void* src, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);

#endif
