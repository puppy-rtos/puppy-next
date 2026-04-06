
/*
 * Copyright (c) 2022-2023, The Puppy RTOS Authors
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PUPPY_INC_DEF_CONFIG_H__
#define PUPPY_INC_DEF_CONFIG_H__

#ifndef PUP_ALIGN_SIZE
#define PUP_ALIGN_SIZE  4
#endif

#ifndef PUP_CONFIG_PTHREAD_STACK_MIN
#define PUP_CONFIG_PTHREAD_STACK_MIN     512
#endif

#ifndef PUP_CONFIG_PTHREAD_STACK_DEFAULT
#define PUP_CONFIG_PTHREAD_STACK_DEFAULT 1024
#endif

#ifndef PUP_IDLE_THREAD_STACK_SIZE
#define PUP_IDLE_THREAD_STACK_SIZE  512
#endif

#ifndef PUP_MAIN_THREAD_STACK_SIZE
#define PUP_MAIN_THREAD_STACK_SIZE  1024
#endif

#ifndef PUP_USER_SYSCALL_STACK_SIZE
#define PUP_USER_SYSCALL_STACK_SIZE  1024
#endif

#ifndef PUP_PRINTK_BUF_SIZE
#define PUP_PRINTK_BUF_SIZE  512
#endif

/*
 * Address-space layout (single source of truth).
 * Update these macros to adjust kernel/app/romfs partitioning.
 */
#ifndef PUP_KERNEL_FLASH_BASE
#define PUP_KERNEL_FLASH_BASE        0x08000000UL
#endif

#ifndef PUP_KERNEL_FLASH_SIZE
#define PUP_KERNEL_FLASH_SIZE        (128UL * 1024UL)
#endif

#ifndef PUP_ROMFS_FLASH_BASE
#define PUP_ROMFS_FLASH_BASE         (PUP_KERNEL_FLASH_BASE + PUP_KERNEL_FLASH_SIZE)
#endif

#ifndef PUP_ROMFS_FLASH_SIZE
#define PUP_ROMFS_FLASH_SIZE         (512UL * 1024UL)
#endif

#ifndef PUP_KERNEL_RAM_BASE
#define PUP_KERNEL_RAM_BASE          0x20000000UL
#endif

#ifndef PUP_KERNEL_RAM_SIZE
#define PUP_KERNEL_RAM_SIZE          (64UL * 1024UL)
#endif

#ifndef PUP_APP_RAM_BASE
#define PUP_APP_RAM_BASE             0x20010000UL
#endif

#ifndef PUP_APP_RAM_SIZE
#define PUP_APP_RAM_SIZE             (64UL * 1024UL)
#endif

#ifndef PUP_SHARED_RAM_BASE
#define PUP_SHARED_RAM_BASE          0x10000000UL
#endif

#ifndef PUP_SHARED_RAM_SIZE
#define PUP_SHARED_RAM_SIZE          (64UL * 1024UL)
#endif

#ifndef PUP_MPU_APP_ISOLATION_ENABLED
#define PUP_MPU_APP_ISOLATION_ENABLED 0
#endif

#ifndef PUP_APP_MEMORY_ALIGN_SHARED
#define PUP_APP_MEMORY_ALIGN_SHARED 8
#endif

#ifndef PUP_APP_MEMORY_ALIGN_ISOLATED
#define PUP_APP_MEMORY_ALIGN_ISOLATED 4096
#endif

#ifndef PUP_RAM_ADDRESS_START
#define PUP_RAM_ADDRESS_START        PUP_KERNEL_RAM_BASE
#endif

#ifndef PUP_RAM_ADDRESS_END
#define PUP_RAM_ADDRESS_END          (PUP_APP_RAM_BASE + PUP_APP_RAM_SIZE)
#endif

#ifndef PUP_MOUNT_ROMFS_BASE
#define PUP_MOUNT_ROMFS_BASE         PUP_ROMFS_FLASH_BASE
#endif

#define PUP_SCHED_PRIO_DEFAULT 12
#define PUP_NAME_MAX 8

#ifndef PUP_CPU_NR
#define PUP_CPU_NR 1
#endif

#define PUP_ASSERT(x) if(!(x)) while(1);
#include "libcm.h"
#define PUP_PRINTK(fmt, ...) libcm_printf(fmt, ##__VA_ARGS__) 

#define dtm_printf libcm_printf

#endif
