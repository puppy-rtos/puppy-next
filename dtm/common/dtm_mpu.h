#pragma once

#include <stdint.h>
#include <default_config.h>

/* MPU and SCB register structures */
typedef struct {
    uint32_t TYPE;
    uint32_t CTRL;
    uint32_t RNR;
    uint32_t RBAR;
    uint32_t RASR;
    uint32_t RBAR_A1;
    uint32_t RASR_A1;
    uint32_t RBAR_A2;
    uint32_t RASR_A2;
    uint32_t RBAR_A3;
    uint32_t RASR_A3;
} DTM_MPU_Type;

typedef struct {
    uint32_t CPUID;
    uint32_t ICSR;
    uint32_t VTOR;
    uint32_t AIRCR;
    uint32_t SCR;
    uint32_t CCR;
    uint32_t SHPR[3];
    uint32_t SHCSR;
    uint32_t CFSR;
    uint32_t MMSR;
    uint32_t BFSR;
    uint32_t UFSR;
    uint32_t HFSR;
    uint32_t MMAR;
    uint32_t BFAR;
    uint32_t AFSR;
} DTM_SCB_Type;

#define DTM_MPU_BASE        0xE000ED90UL
#define DTM_SCB_BASE        0xE000ED00UL

/* ARM MPU Access Permission definitions */
#define ARM_MPU_AP_NONE     0U  /* Privileged and Unprivileged - no access */
#define ARM_MPU_AP_PRIV     1U  /* Privileged - Read/Write, Unprivileged - no access */
#define ARM_MPU_AP_URO      2U  /* Privileged - Read/Write, Unprivileged - Read only */
#define ARM_MPU_AP_FULL     3U  /* Privileged - Read/Write, Unprivileged - Read/Write */
#define ARM_MPU_AP_RO       6U  /* Privileged - Read only, Unprivileged - Read only */

/* ARM MPU Region Size definitions */
#define ARM_MPU_REGION_SIZE_32B     4U
#define ARM_MPU_REGION_SIZE_64B     5U
#define ARM_MPU_REGION_SIZE_128B    6U
#define ARM_MPU_REGION_SIZE_256B    7U
#define ARM_MPU_REGION_SIZE_512B    8U
#define ARM_MPU_REGION_SIZE_1KB     9U
#define ARM_MPU_REGION_SIZE_2KB    10U
#define ARM_MPU_REGION_SIZE_4KB    11U
#define ARM_MPU_REGION_SIZE_8KB    12U
#define ARM_MPU_REGION_SIZE_16KB   13U
#define ARM_MPU_REGION_SIZE_32KB   14U
#define ARM_MPU_REGION_SIZE_64KB   15U
#define ARM_MPU_REGION_SIZE_128KB  16U
#define ARM_MPU_REGION_SIZE_256KB  17U
#define ARM_MPU_REGION_SIZE_512KB  18U
#define ARM_MPU_REGION_SIZE_1MB    19U

#define DTM_MPU_REGION_INVALID      0x00
#define DTM_MPU_REGION_VALID        0x01

#define DTM_MPU_ATT_READ     0x01
#define DTM_MPU_ATT_WRITE    0x02
#define DTM_MPU_ATT_EXEC     0x04
#define DTM_MPU_ATT_FULL     0x08
#define DTM_MPU_ATT_NONE     0x00

#define DTM_MPU_MAX_REGIONS  8

#define DTM_MPU_REGION_FLASH        0
#define DTM_MPU_REGION_KERNEL_RAM   1
#define DTM_MPU_REGION_APP_RAM      2
#define DTM_MPU_REGION_SHARED_RAM   3
#define DTM_MPU_REGION_OTHER_APP_0  4
#define DTM_MPU_REGION_OTHER_APP_1  5
#define DTM_MPU_REGION_OTHER_APP_2  6
#define DTM_MPU_REGION_OTHER_APP_3  7

#define DTM_MPU_MAX_ISOLATED_APPS   4

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

typedef struct {
    uint32_t base;
    uint32_t size;
} dtm_mpu_app_region_t;

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
int dtm_mpu_setup_app_protection(uint32_t app_ram_base, uint32_t app_ram_size);
int dtm_mpu_isolate_app_region(uint32_t app_ram_base, uint32_t app_ram_size);

int dtm_mpu_setup_app_isolation(uint32_t current_app_base, uint32_t current_app_size,
                                dtm_mpu_app_region_t *other_apps, int other_app_count);

void dtm_mpu_get_isolation_regions(uint32_t current_app_base, 
                                   uint32_t current_app_end,
                                   dtm_mpu_app_region_t *regions, int *count);

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
