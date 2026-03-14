#pragma once

#include <stdint.h>
#include <default_config.h>

#define DTM_MPU_REGION_INVALID      0x00
#define DTM_MPU_REGION_VALID        0x01

#define DTM_MPU_ATT_READ     0x01
#define DTM_MPU_ATT_WRITE    0x02
#define DTM_MPU_ATT_EXEC     0x04
#define DTM_MPU_ATT_FULL     0x08

#define DTM_MPU_MAX_REGIONS  8

struct dtm_mpu_region {
    int valid;
    void *addr;
    uint32_t size;
    uint32_t attribute;
    void *user_data;
};

struct dtm_mpu_info {
    struct dtm_mpu_region regions[DTM_MPU_MAX_REGIONS];
};

int dtm_mpu_init(void);
void dtm_mpu_enable(void);
void dtm_mpu_disable(void);

int dtm_mpu_region_set(uint32_t region_num, uint32_t base_addr, 
                       uint32_t size, uint32_t attributes);
void dtm_mpu_region_clear(uint32_t region_num);

void dtm_mpu_load_regions(struct dtm_mpu_info *mpu_info);
void dtm_mpu_clear_regions(struct dtm_mpu_info *mpu_info);

void dtm_mpu_dump(void);

#define DTM_MPU_KERNEL_FLASH_BASE   PUP_KERNEL_FLASH_BASE
#define DTM_MPU_KERNEL_FLASH_SIZE   PUP_KERNEL_FLASH_SIZE
#define DTM_MPU_KERNEL_RAM_BASE     PUP_KERNEL_RAM_BASE
#define DTM_MPU_KERNEL_RAM_SIZE     PUP_KERNEL_RAM_SIZE
#define DTM_MPU_USER_RAM_BASE       PUP_APP_RAM_BASE
#define DTM_MPU_USER_RAM_SIZE       PUP_APP_RAM_SIZE

int dtm_mpu_setup_user_protection(void);
int dtm_mpu_setup_kernel_protection(void);

/*
 * DTM MPU Driver Usage
 * ====================
 *
 * This driver provides MPU (Memory Protection Unit) support for ARM Cortex-M4
 * series MCUs. It implements the hardware-level MPU operations.
 *
 * Basic Usage:
 * -----------
 *
 * 1. Initialize MPU:
 *    dtm_mpu_init();
 *
 * 2. Enable/Disable MPU:
 *    dtm_mpu_enable();
 *    dtm_mpu_disable();
 *
 * 3. Set memory region:
 *    dtm_mpu_region_set(region_num, base_addr, size, attributes);
 *
 *    Attributes:
 *    - DTM_MPU_ATT_READ  : Read access
 *    - DTM_MPU_ATT_WRITE : Write access
 *    - DTM_MPU_ATT_EXEC  : Execute access
 *    - DTM_MPU_ATT_FULL  : Full access (read/write/execute)
 *
 * 4. Clear memory region:
 *    dtm_mpu_region_clear(region_num);
 *
 * 5. Setup protection modes:
 *    - Kernel mode: dtm_mpu_setup_kernel_protection();
 *      Allows full access to all memory regions
 *
 *    - User mode: dtm_mpu_setup_user_protection();
 *      Restricts access to kernel RAM, allows user RAM access
 *
 * Memory Layout:
 * -------------
 *
 * Region 0: Flash (0x08000000, 1MB)
 * Region 1: Kernel RAM (0x20000000, 128KB) - privileged only in user mode
 * Region 2: User RAM (0x20020000, 64KB) - full access in user mode
 *
 * Integration with Puppy Kernel:
 * -----------------------------
 *
 * The kernel's mpu.c provides wrapper functions that call these DTM functions:
 * - pup_mpu_init() -> dtm_mpu_init()
 * - pup_mpu_enable() -> dtm_mpu_enable()
 * - pup_mpu_disable() -> dtm_mpu_disable()
 * - pup_mpu_region_set() -> dtm_mpu_region_set()
 * - etc.
 *
 * Porting to New Chips:
 * --------------------
 *
 * 1. Add DTM_BINFDING_MPU_ARM_M4_ST_GD to dtm_bindings.h
 * 2. Adjust memory layout constants if needed:
 *    - DTM_MPU_KERNEL_FLASH_BASE
 *    - DTM_MPU_KERNEL_RAM_BASE
 *    - DTM_MPU_USER_RAM_BASE
 */
