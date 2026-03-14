#ifndef __PUPPY_LOG_H__
#define __PUPPY_LOG_H__

#include <puppy_core.h>

#define PUP_LOG_LEVEL_NONE    0
#define PUP_LOG_LEVEL_ERROR   1
#define PUP_LOG_LEVEL_WARNING 2
#define PUP_LOG_LEVEL_INFO    3
#define PUP_LOG_LEVEL_DEBUG   4

#define PUP_LOG_MODULE_CORE      0
#define PUP_LOG_MODULE_SYSCALL   1
#define PUP_LOG_MODULE_SCHED     2
#define PUP_LOG_MODULE_THREAD    3
#define PUP_LOG_MODULE_APP       4
#define PUP_LOG_MODULE_ARCH      5
#define PUP_LOG_MODULE_RELOC     6
#define PUP_LOG_MODULE_MPU       7
#define PUP_LOG_MODULE_FS        8
#define PUP_LOG_MODULE_DEV       9
#define PUP_LOG_MODULE_KHEAP     10
#define PUP_LOG_MODULE_MAX       11

#ifndef PUP_LOG_LEVEL_DEFAULT
#define PUP_LOG_LEVEL_DEFAULT PUP_LOG_LEVEL_INFO
#endif

#define PUP_LOG_LEVEL_CORE      PUP_LOG_LEVEL_DEFAULT
#define PUP_LOG_LEVEL_SYSCALL   PUP_LOG_LEVEL_ERROR
#define PUP_LOG_LEVEL_SCHED     PUP_LOG_LEVEL_DEFAULT
#define PUP_LOG_LEVEL_THREAD    PUP_LOG_LEVEL_DEFAULT
#define PUP_LOG_LEVEL_APP       PUP_LOG_LEVEL_DEFAULT
#define PUP_LOG_LEVEL_ARCH      PUP_LOG_LEVEL_DEFAULT
#define PUP_LOG_LEVEL_RELOC     PUP_LOG_LEVEL_DEFAULT
#define PUP_LOG_LEVEL_MPU       PUP_LOG_LEVEL_DEFAULT
#define PUP_LOG_LEVEL_FS        PUP_LOG_LEVEL_DEFAULT
#define PUP_LOG_LEVEL_DEV       PUP_LOG_LEVEL_DEFAULT
#define PUP_LOG_LEVEL_KHEAP     PUP_LOG_LEVEL_DEFAULT

static const int pup_log_levels[PUP_LOG_MODULE_MAX] = {
    PUP_LOG_LEVEL_CORE,
    PUP_LOG_LEVEL_SYSCALL,
    PUP_LOG_LEVEL_SCHED,
    PUP_LOG_LEVEL_THREAD,
    PUP_LOG_LEVEL_APP,
    PUP_LOG_LEVEL_ARCH,
    PUP_LOG_LEVEL_RELOC,
    PUP_LOG_LEVEL_MPU,
    PUP_LOG_LEVEL_FS,
    PUP_LOG_LEVEL_DEV,
    PUP_LOG_LEVEL_KHEAP,
};

static const char* const pup_log_module_names[PUP_LOG_MODULE_MAX] = {
    "CORE",
    "SYSCALL",
    "SCHED",
    "THREAD",
    "APP",
    "ARCH",
    "RELOC",
    "MPU",
    "FS",
    "DEV",
    "KHEAP",
};

static const char* const pup_log_level_names[] = {
    "NONE",
    "E",
    "W",
    "I",
    "D",
};

#define PUP_LOG_IMPL(module, level, fmt, ...) \
    do { \
        if (pup_log_levels[module] >= level) { \
            PUP_PRINTK("[%s/%s] " fmt "\n", \
                       pup_log_module_names[module], \
                       pup_log_level_names[level], \
                       ##__VA_ARGS__); \
        } \
    } while(0)

#define PUP_LOG_ERROR(module, fmt, ...) \
    PUP_LOG_IMPL(module, PUP_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)

#define PUP_LOG_WARNING(module, fmt, ...) \
    PUP_LOG_IMPL(module, PUP_LOG_LEVEL_WARNING, fmt, ##__VA_ARGS__)

#define PUP_LOG_INFO(module, fmt, ...) \
    PUP_LOG_IMPL(module, PUP_LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)

#define PUP_LOG_DEBUG(module, fmt, ...) \
    PUP_LOG_IMPL(module, PUP_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)

#endif
