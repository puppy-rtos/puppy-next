#include "dtm_mpu.h"
#include "puppy_log.h"
#include <string.h>

/* ARM Cortex-M barrier instructions */
#define __DSB() __asm__ volatile ("dsb" ::: "memory")
#define __ISB() __asm__ volatile ("isb" ::: "memory")
#define __DMB() __asm__ volatile ("dmb" ::: "memory")

#define DTM_MPU ((DTM_MPU_Type *)DTM_MPU_BASE)
#define DTM_SCB ((DTM_SCB_Type *)DTM_SCB_BASE)

#define DTM_MPU_TYPE_SEPARATE_Msk           (1UL)

#define DTM_MPU_CTRL_ENABLE_Msk             (1UL)
#define DTM_MPU_CTRL_HFNMIENA_Msk           (1UL << 1UL)
#define DTM_MPU_CTRL_PRIVDEFENA_Msk         (1UL << 2UL)

#define DTM_MPU_RBAR_REGION_Msk             (0xFUL)
#define DTM_MPU_RBAR_REGION_Pos             (0U)
#define DTM_MPU_RBAR_VALID_Msk              (1UL << 4UL)
#define DTM_MPU_RBAR_VALID_Pos              (4U)
#define DTM_MPU_RBAR_ADDR_Msk               (0xFFFFFFE0UL)
#define DTM_MPU_RBAR_ADDR_Pos               (5U)

#define DTM_MPU_RASR_ENABLE_Msk             (1UL)

#define DTM_MPU_RASR_SIZE_Pos               (1U)
#define DTM_MPU_RASR_SIZE_Msk               (0x1EUL << DTM_MPU_RASR_SIZE_Pos)

#define DTM_MPU_RASR_SRD_Pos                (8U)
#define DTM_MPU_RASR_SRD_Msk                (0xFFUL << DTM_MPU_RASR_SRD_Pos)

#define DTM_MPU_RASR_ATTR_Pos               (16U)
#define DTM_MPU_RASR_ATTR_Msk               (0xFFFFUL << DTM_MPU_RASR_ATTR_Pos)

#define DTM_MPU_RASR_XN_Pos                 (28U)
#define DTM_MPU_RASR_XN_Msk                 (1UL << DTM_MPU_RASR_XN_Pos)

#define DTM_MPU_RASR_AP_Pos                 (24U)
#define DTM_MPU_RASR_AP_Msk                 (7UL << DTM_MPU_RASR_AP_Pos)

#define DTM_MPU_RASR_TEX_Pos                (19U)
#define DTM_MPU_RASR_TEX_Msk                (7UL << DTM_MPU_RASR_TEX_Pos)

#define DTM_MPU_RASR_S_Pos                  (18U)
#define DTM_MPU_RASR_S_Msk                  (1UL << DTM_MPU_RASR_S_Pos)

#define DTM_MPU_RASR_C_Pos                  (17U)
#define DTM_MPU_RASR_C_Msk                  (1UL << DTM_MPU_RASR_C_Pos)

#define DTM_MPU_RASR_B_Pos                  (16U)
#define DTM_MPU_RASR_B_Msk                  (1UL << DTM_MPU_RASR_B_Pos)

#define DTM_SCB_SHCSR_MEMFAULTENA_Msk       (1UL << 16UL)

#define DTM_MPU_SUB_REGION_CNT 8

#define DTM_ARM_MPU_RBAR(region, base) \
    (((base) & DTM_MPU_RBAR_ADDR_Msk) | \
     ((region) & DTM_MPU_RBAR_REGION_Msk) | \
     DTM_MPU_RBAR_VALID_Msk)

#define DTM_ARM_MPU_RASR(xn, ap, tex, s, c, b, srd, size) \
    ((srd << DTM_MPU_RASR_SRD_Pos) | \
     (size << DTM_MPU_RASR_SIZE_Pos) | \
     (ap << DTM_MPU_RASR_AP_Pos) | \
     (xn << DTM_MPU_RASR_XN_Pos) | \
     DTM_MPU_RASR_ENABLE_Msk)

static inline void ARM_MPU_Enable(uint32_t MPU_Control) {
    DTM_MPU->CTRL = MPU_Control | DTM_MPU_CTRL_ENABLE_Msk;
    DTM_SCB->SHCSR |= DTM_SCB_SHCSR_MEMFAULTENA_Msk;
    __DSB();
    __ISB();
}

static inline void ARM_MPU_Disable(void) {
    __DMB();
    DTM_SCB->SHCSR &= ~DTM_SCB_SHCSR_MEMFAULTENA_Msk;
    DTM_MPU->CTRL &= ~DTM_MPU_CTRL_ENABLE_Msk;
}

static inline void ARM_MPU_ClrRegion(uint32_t rnr) {
    DTM_MPU->RNR = rnr;
    DTM_MPU->RASR = 0U;
}

static inline void ARM_MPU_SetRegion(uint32_t rbar, uint32_t rasr) {
    DTM_MPU->RBAR = rbar;
    DTM_MPU->RASR = rasr;
}

static inline void ARM_MPU_SetRegionEx(uint32_t rnr, uint32_t rbar, uint32_t rasr) {
    DTM_MPU->RNR = rnr;
    DTM_MPU->RBAR = rbar;
    DTM_MPU->RASR = rasr;
}

static uint32_t dtm_ffs(uint32_t val) {
    uint32_t i = 0;
    while (i < 32 && !(val & (1 << i))) {
        i++;
    }
    return i;
}

int dtm_mpu_init(void) {
    return 0;
}

void dtm_mpu_enable(void) {
    ARM_MPU_Enable(DTM_MPU_CTRL_PRIVDEFENA_Msk);
}

void dtm_mpu_disable(void) {
    ARM_MPU_Disable();
}

static uint32_t dtm_mpu_size_to_encode(uint32_t size) {
    uint32_t encode = 4;
    while ((1U << (encode + 1)) < size && encode < 31) {
        encode++;
    }
    return encode;
}

static uint32_t dtm_mpu_align_addr(uint32_t addr, uint32_t size) {
    return addr & ~(size - 1);
}

int dtm_mpu_region_set(uint32_t region_num, uint32_t base_addr, uint32_t size, uint32_t attributes) {
    uint32_t rasr;
    uint32_t size_encode;
    uint32_t xn = 0;
    uint32_t ap = ARM_MPU_AP_FULL;
    uint32_t srd = 0;

    if (size < 32) {
        size = 32;
    }

    size_encode = dtm_mpu_size_to_encode(size);
    base_addr = dtm_mpu_align_addr(base_addr, 1 << (size_encode + 1));

    if (!(attributes & DTM_MPU_ATT_EXEC)) {
        xn = 1;
    }

    if (attributes == DTM_MPU_ATT_NONE) {
        ap = ARM_MPU_AP_NONE;
    } else if (attributes == (DTM_MPU_ATT_READ | DTM_MPU_ATT_EXEC)) {
        ap = ARM_MPU_AP_URO;
    }

    rasr = DTM_ARM_MPU_RASR(xn, ap, 0, 0, 1, 1, srd, size_encode);
    ARM_MPU_SetRegionEx(region_num, DTM_ARM_MPU_RBAR(region_num, base_addr), rasr);

    return 0;
}

void dtm_mpu_region_clear(uint32_t region_num) {
    ARM_MPU_ClrRegion(region_num);
}

void dtm_mpu_load_regions(struct dtm_mpu_info *mpu_info) {
    int i;
    for (i = 0; i < DTM_MPU_MAX_REGIONS; i++) {
        if (mpu_info->regions[i].valid == DTM_MPU_REGION_VALID) {
            dtm_mpu_region_set(i,
                               (uint32_t)mpu_info->regions[i].addr,
                               mpu_info->regions[i].size,
                               mpu_info->regions[i].attribute);
        }
    }
}

void dtm_mpu_clear_regions(struct dtm_mpu_info *mpu_info) {
    int i;
    for (i = 0; i < DTM_MPU_MAX_REGIONS; i++) {
        ARM_MPU_ClrRegion(i);
    }
}

void dtm_mpu_dump(void) {
    int i;
    PUP_LOG_INFO(PUP_LOG_MODULE_MPU, "MPU Configuration:");
    for (i = 0; i < DTM_MPU_MAX_REGIONS; i++) {
        DTM_MPU->RNR = i;
        uint32_t rbar = DTM_MPU->RBAR;
        uint32_t rasr = DTM_MPU->RASR;
        if (rasr & DTM_MPU_RASR_ENABLE_Msk) {
            uint32_t base = rbar & DTM_MPU_RBAR_ADDR_Msk;
            uint32_t size_encode = (rasr & DTM_MPU_RASR_SIZE_Msk) >> DTM_MPU_RASR_SIZE_Pos;
            uint32_t size = 1 << (size_encode + 1);
            uint32_t srd = (rasr & DTM_MPU_RASR_SRD_Msk) >> DTM_MPU_RASR_SRD_Pos;
            PUP_LOG_INFO(PUP_LOG_MODULE_MPU, "  Region %d: 0x%08x - 0x%08x (SRD=0x%02x)",
                         i, base, base + size, srd);
        }
    }
}

int dtm_mpu_setup_kernel_protection(void) {
    dtm_mpu_disable();

    dtm_mpu_region_set(0, DTM_MPU_KERNEL_FLASH_BASE, 1024 * 1024,
                       DTM_MPU_ATT_READ | DTM_MPU_ATT_EXEC);

    uint32_t rasr;
    uint32_t size_encode = ARM_MPU_REGION_SIZE_64KB;
    rasr = DTM_ARM_MPU_RASR(0UL, ARM_MPU_AP_FULL, 0UL, 1UL, 1UL, 1UL, 0UL, size_encode);
    ARM_MPU_SetRegionEx(1, DTM_ARM_MPU_RBAR(1, DTM_MPU_KERNEL_RAM_BASE), rasr);

    dtm_mpu_region_set(2, DTM_MPU_USER_RAM_BASE, DTM_MPU_USER_RAM_SIZE,
                       DTM_MPU_ATT_READ | DTM_MPU_ATT_WRITE | DTM_MPU_ATT_EXEC);

    dtm_mpu_region_set(3, PUP_SHARED_RAM_BASE, PUP_SHARED_RAM_SIZE,
                       DTM_MPU_ATT_READ | DTM_MPU_ATT_WRITE | DTM_MPU_ATT_EXEC);

    dtm_mpu_enable();

    return 0;
}

int dtm_mpu_setup_user_protection(void) {
    dtm_mpu_disable();

    dtm_mpu_region_set(0, DTM_MPU_KERNEL_FLASH_BASE, 1024 * 1024,
                       DTM_MPU_ATT_READ | DTM_MPU_ATT_EXEC);

    uint32_t rasr;
    uint32_t size_encode = ARM_MPU_REGION_SIZE_64KB;
    rasr = DTM_ARM_MPU_RASR(0UL, ARM_MPU_AP_PRIV, 0UL, 1UL, 1UL, 1UL, 0UL, size_encode);
    ARM_MPU_SetRegionEx(1, DTM_ARM_MPU_RBAR(1, DTM_MPU_KERNEL_RAM_BASE), rasr);

    dtm_mpu_region_set(2, DTM_MPU_USER_RAM_BASE, DTM_MPU_USER_RAM_SIZE,
                       DTM_MPU_ATT_READ | DTM_MPU_ATT_WRITE | DTM_MPU_ATT_EXEC);

    dtm_mpu_region_set(3, PUP_SHARED_RAM_BASE, PUP_SHARED_RAM_SIZE,
                       DTM_MPU_ATT_READ | DTM_MPU_ATT_WRITE | DTM_MPU_ATT_EXEC);

    dtm_mpu_enable();

    return 0;
}

/* Calculate minimum subregion alignment size for MPU
 * MPU region size must be power of 2, minimum 32 bytes
 * Subregion size = region_size / 8
 * This function returns the minimum subregion size that can cover the app
 */
static uint32_t dtm_mpu_calc_min_subregion_size(uint32_t app_size)
{
    uint32_t subregion_size = 32;  /* Minimum 32 bytes */
    
    /* Find the smallest subregion size that can cover the app
     * Region size = subregion_size * 8, must be >= app_size + alignment margin
     */
    while (subregion_size * 8 < app_size + subregion_size * 8) {
        subregion_size <<= 1;
        if (subregion_size >= 4096) break;  /* Max 4KB subregions (32KB region) */
    }
    
    return subregion_size;
}

/* Configure MPU region with sub-region masking
 * Uses two regions (front and back) to precisely match app RAM range
 * 
 * Strategy:
 * - Only align app_base to subregion boundary (4KB for 32KB region)
 * - app_end keeps actual value (not aligned) - this is the ACTUAL needed end
 * - Front region covers from aligned base
 * - Back region can start from anywhere (can overlap with front)
 * - Two regions overlap to precisely cover [app_base, app_end)
 * - Sub-regions disabled for areas outside app actual range
 */
static void dtm_mpu_config_precise_region(uint32_t app_base, uint32_t app_size,
                                           uint32_t *f_sub_mask, uint32_t *b_sub_mask,
                                           uint32_t *f_base, uint32_t *b_base,
                                           uint32_t *region_size)
{
    uint32_t subregion_size;
    uint32_t f_sub = 0, b_sub = 0;
    uint32_t app_end = app_base + app_size;  /* Actual end, NOT aligned */
    int i;

    /* Use fixed 32KB region size (4KB sub-regions) for good granularity
     * This gives us 4KB alignment requirement for app_base
     */
    *region_size = 32768;  /* 32KB */
    subregion_size = 4096; /* 4KB sub-regions */

    /* Front region: aligned to 32KB boundary, covers app start */
    *f_base = app_base & ~(*region_size - 1);
    
    /* Calculate front sub-region mask
     * Enable sub-regions that overlap with [app_base, app_end)
     * Disable sub-regions completely outside this range */
    for (i = 0; i < 8; i++) {
        uint32_t subregion_start = *f_base + i * subregion_size;
        uint32_t subregion_end = subregion_start + subregion_size;
        
        /* Enable only if sub-region overlaps with [app_base, app_end)
         * Overlap condition: subregion_start < app_end AND subregion_end > app_base */
        int overlaps = (subregion_start < app_end) && (subregion_end > app_base);
        if (!overlaps) {
            f_sub |= (1 << i);  /* Disable this sub-region */
        }
    }

    /* Back region: can overlap with front region
     * We place it to cover app_end precisely
     * Strategy: align app_end down to 32KB boundary for back region base
     * This allows overlap and ensures app_end is covered */
    *b_base = app_end & ~(*region_size - 1);
    
    /* If b_base is the same as f_base, shift it to create overlap coverage */
    if (*b_base == *f_base) {
        /* App fits within one 32KB region, but we use two overlapping regions
         * Back region starts at middle to extend coverage */
        *b_base = *f_base + subregion_size * 4;  /* 16KB offset, creates overlap */
    }
    
    /* Calculate back sub-region mask
     * Enable sub-regions that overlap with [app_base, app_end)
     * Disable sub-regions completely outside this range */
    for (i = 0; i < 8; i++) {
        uint32_t subregion_start = *b_base + i * subregion_size;
        uint32_t subregion_end = subregion_start + subregion_size;
        
        /* Enable only if sub-region overlaps with [app_base, app_end) */
        int overlaps = (subregion_start < app_end) && (subregion_end > app_base);
        if (!overlaps) {
            b_sub |= (1 << i);  /* Disable this sub-region */
        }
    }

    *f_sub_mask = f_sub;
    *b_sub_mask = b_sub;
}

int dtm_mpu_setup_app_protection(uint32_t app_ram_base, uint32_t app_ram_size) {
    uint32_t f_sub_mask, b_sub_mask, region_size;
    uint32_t f_base, b_base;
    uint32_t f_size_encode, b_size_encode;
    uint32_t f_rasr, b_rasr;

    dtm_mpu_disable();

    /* Region 0: Flash - Read/Execute for all */
    dtm_mpu_region_set(0, DTM_MPU_KERNEL_FLASH_BASE, 1024 * 1024,
                       DTM_MPU_ATT_READ | DTM_MPU_ATT_EXEC);

    /* Region 1: Kernel RAM (0x20000000-0x2000FFFF) - Privileged only, 64KB */
    uint32_t rasr;
    uint32_t size_encode = ARM_MPU_REGION_SIZE_64KB;
    rasr = DTM_ARM_MPU_RASR(0UL, ARM_MPU_AP_PRIV, 0UL, 1UL, 1UL, 1UL, 0UL, size_encode);
    ARM_MPU_SetRegionEx(1, DTM_ARM_MPU_RBAR(1, DTM_MPU_KERNEL_RAM_BASE), rasr);

    /* Calculate precise MPU configuration using two regions */
    dtm_mpu_config_precise_region(app_ram_base, app_ram_size,
                                   &f_sub_mask, &b_sub_mask, &f_base, &b_base, &region_size);

    /* Calculate size encodes */
    f_size_encode = dtm_ffs(region_size) - 1;
    b_size_encode = f_size_encode;

    /* Region 2: Front part of app RAM with sub-region masking */
    f_rasr = DTM_ARM_MPU_RASR(0UL, ARM_MPU_AP_FULL, 0UL, 1UL, 1UL, 1UL,
                               f_sub_mask, f_size_encode);
    ARM_MPU_SetRegionEx(2, DTM_ARM_MPU_RBAR(2, f_base), f_rasr);

    /* Region 3: Back part of app RAM with sub-region masking
     * Only enable if not all sub-regions are disabled */
    if (b_sub_mask != 0xFF) {
        b_rasr = DTM_ARM_MPU_RASR(0UL, ARM_MPU_AP_FULL, 0UL, 1UL, 1UL, 1UL,
                                   b_sub_mask, b_size_encode);
        ARM_MPU_SetRegionEx(3, DTM_ARM_MPU_RBAR(3, b_base), b_rasr);
    } else {
        /* All sub-regions disabled, clear this region */
        ARM_MPU_ClrRegion(3);
    }
    
    /* Region 4: Shared RAM - Full access (must be after app regions) */
    dtm_mpu_region_set(4, PUP_SHARED_RAM_BASE, PUP_SHARED_RAM_SIZE,
                       DTM_MPU_ATT_READ | DTM_MPU_ATT_WRITE | DTM_MPU_ATT_EXEC);

    /* Clear regions 5-7 (not used) */
    ARM_MPU_ClrRegion(5);
    ARM_MPU_ClrRegion(6);
    ARM_MPU_ClrRegion(7);

    dtm_mpu_enable();

    return 0;
}

int dtm_mpu_isolate_app_region(uint32_t app_ram_base, uint32_t app_ram_size) {
    return dtm_mpu_setup_app_protection(app_ram_base, app_ram_size);
}

int dtm_mpu_setup_app_isolation(uint32_t current_app_base, uint32_t current_app_size,
                                 dtm_mpu_app_region_t *other_apps, int other_app_count) {
    return dtm_mpu_setup_app_protection(current_app_base, current_app_size);
}

void dtm_mpu_get_isolation_regions(uint32_t current_app_base, uint32_t current_app_end,
                                    dtm_mpu_app_region_t *regions, int *count) {
    *count = 0;
}
