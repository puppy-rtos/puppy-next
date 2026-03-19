#include "dtm_bindings.h"

#ifdef DTM_BINFDING_MPU_ARM_M4_ST_GD
#include "dtm_mpu.h"
#include <default_config.h>

#define ARM_MPU_REGION_SIZE_32B      ((uint8_t)0x04U)
#define ARM_MPU_REGION_SIZE_64B      ((uint8_t)0x05U)
#define ARM_MPU_REGION_SIZE_128B     ((uint8_t)0x06U)
#define ARM_MPU_REGION_SIZE_256B     ((uint8_t)0x07U)
#define ARM_MPU_REGION_SIZE_512B     ((uint8_t)0x08U)
#define ARM_MPU_REGION_SIZE_1KB      ((uint8_t)0x09U)
#define ARM_MPU_REGION_SIZE_2KB      ((uint8_t)0x0AU)
#define ARM_MPU_REGION_SIZE_4KB      ((uint8_t)0x0BU)
#define ARM_MPU_REGION_SIZE_8KB      ((uint8_t)0x0CU)
#define ARM_MPU_REGION_SIZE_16KB     ((uint8_t)0x0DU)
#define ARM_MPU_REGION_SIZE_32KB     ((uint8_t)0x0EU)
#define ARM_MPU_REGION_SIZE_64KB     ((uint8_t)0x0FU)
#define ARM_MPU_REGION_SIZE_128KB    ((uint8_t)0x10U)
#define ARM_MPU_REGION_SIZE_256KB    ((uint8_t)0x11U)
#define ARM_MPU_REGION_SIZE_512KB    ((uint8_t)0x12U)
#define ARM_MPU_REGION_SIZE_1MB      ((uint8_t)0x13U)
#define ARM_MPU_REGION_SIZE_2MB      ((uint8_t)0x14U)
#define ARM_MPU_REGION_SIZE_4MB      ((uint8_t)0x15U)
#define ARM_MPU_REGION_SIZE_8MB      ((uint8_t)0x16U)
#define ARM_MPU_REGION_SIZE_16MB     ((uint8_t)0x17U)
#define ARM_MPU_REGION_SIZE_32MB     ((uint8_t)0x18U)
#define ARM_MPU_REGION_SIZE_64MB     ((uint8_t)0x19U)
#define ARM_MPU_REGION_SIZE_128MB    ((uint8_t)0x1AU)
#define ARM_MPU_REGION_SIZE_256MB    ((uint8_t)0x1BU)
#define ARM_MPU_REGION_SIZE_512MB    ((uint8_t)0x1CU)
#define ARM_MPU_REGION_SIZE_1GB      ((uint8_t)0x1DU)
#define ARM_MPU_REGION_SIZE_2GB      ((uint8_t)0x1EU)
#define ARM_MPU_REGION_SIZE_4GB      ((uint8_t)0x1FU)

#define ARM_MPU_AP_NONE 0U
#define ARM_MPU_AP_PRIV 1U
#define ARM_MPU_AP_URO  2U
#define ARM_MPU_AP_FULL 3U
#define ARM_MPU_AP_PRO  5U
#define ARM_MPU_AP_RO   6U

typedef struct {
    volatile uint32_t TYPE;
    volatile uint32_t CTRL;
    volatile uint32_t RNR;
    volatile uint32_t RBAR;
    volatile uint32_t RASR;
    volatile uint32_t RBAR_A1;
    volatile uint32_t RASR_A1;
    volatile uint32_t RBAR_A2;
    volatile uint32_t RASR_A2;
    volatile uint32_t RBAR_A3;
    volatile uint32_t RASR_A3;
} DTM_MPU_Type;

#define DTM_MPU_BASE            (0xE000ED90UL)
#define DTM_MPU                 ((DTM_MPU_Type *)DTM_MPU_BASE)

#define DTM_MPU_CTRL_ENABLE_Msk             (1UL << 0UL)
#define DTM_MPU_CTRL_HFNMIENA_Msk           (1UL << 1UL)
#define DTM_MPU_CTRL_PRIVDEFENA_Msk         (1UL << 2UL)

#define DTM_MPU_RBAR_REGION_Msk             (0xFUL << 0UL)
#define DTM_MPU_RBAR_VALID_Msk              (1UL << 4UL)
#define DTM_MPU_RBAR_ADDR_Msk               (0xFFFFFFE0UL)

#define DTM_MPU_RASR_ENABLE_Msk             (1UL << 0UL)
#define DTM_MPU_RASR_SIZE_Pos               1UL
#define DTM_MPU_RASR_SIZE_Msk               (0x1FUL << DTM_MPU_RASR_SIZE_Pos)
#define DTM_MPU_RASR_SRD_Pos                8UL
#define DTM_MPU_RASR_SRD_Msk                (0xFFUL << DTM_MPU_RASR_SRD_Pos)
#define DTM_MPU_RASR_B_Pos                  16UL
#define DTM_MPU_RASR_B_Msk                  (1UL << DTM_MPU_RASR_B_Pos)
#define DTM_MPU_RASR_C_Pos                  17UL
#define DTM_MPU_RASR_C_Msk                  (1UL << DTM_MPU_RASR_C_Pos)
#define DTM_MPU_RASR_S_Pos                  18UL
#define DTM_MPU_RASR_S_Msk                  (1UL << DTM_MPU_RASR_S_Pos)
#define DTM_MPU_RASR_TEX_Pos                19UL
#define DTM_MPU_RASR_TEX_Msk                (0x7UL << DTM_MPU_RASR_TEX_Pos)
#define DTM_MPU_RASR_AP_Pos                 24UL
#define DTM_MPU_RASR_AP_Msk                 (0x7UL << DTM_MPU_RASR_AP_Pos)
#define DTM_MPU_RASR_XN_Pos                 28UL
#define DTM_MPU_RASR_XN_Msk                 (1UL << DTM_MPU_RASR_XN_Pos)

typedef struct {
    uint32_t RBAR;
    uint32_t RASR;
} DTM_ARM_MPU_Region_t;

#define DTM_ARM_MPU_RBAR(Region, BaseAddress) \
    (((BaseAddress) & DTM_MPU_RBAR_ADDR_Msk) |  \
     ((Region) & DTM_MPU_RBAR_REGION_Msk) |  \
     (DTM_MPU_RBAR_VALID_Msk))

#define DTM_ARM_MPU_ACCESS_(TypeExtField, IsShareable, IsCacheable, IsBufferable)   \
    ((((TypeExtField) << DTM_MPU_RASR_TEX_Pos) & DTM_MPU_RASR_TEX_Msk)                  | \
     (((IsShareable)  << DTM_MPU_RASR_S_Pos)   & DTM_MPU_RASR_S_Msk)                    | \
     (((IsCacheable)  << DTM_MPU_RASR_C_Pos)   & DTM_MPU_RASR_C_Msk)                    | \
     (((IsBufferable) << DTM_MPU_RASR_B_Pos)   & DTM_MPU_RASR_B_Msk))

#define DTM_ARM_MPU_RASR_EX(DisableExec, AccessPermission, AccessAttributes, SubRegionDisable, Size)    \
    ((((DisableExec)      << DTM_MPU_RASR_XN_Pos)   & DTM_MPU_RASR_XN_Msk)                                  | \
     (((AccessPermission) << DTM_MPU_RASR_AP_Pos)   & DTM_MPU_RASR_AP_Msk)                                  | \
     (((AccessAttributes) & (DTM_MPU_RASR_TEX_Msk | DTM_MPU_RASR_S_Msk | DTM_MPU_RASR_C_Msk | DTM_MPU_RASR_B_Msk))) | \
     (((SubRegionDisable) << DTM_MPU_RASR_SRD_Pos)  & DTM_MPU_RASR_SRD_Msk)                                 | \
     (((Size)             << DTM_MPU_RASR_SIZE_Pos) & DTM_MPU_RASR_SIZE_Msk)                                | \
     (((DTM_MPU_RASR_ENABLE_Msk))))

#define DTM_ARM_MPU_RASR(DisableExec, AccessPermission, TypeExtField, IsShareable, IsCacheable, IsBufferable, SubRegionDisable, Size) \
    DTM_ARM_MPU_RASR_EX(DisableExec, AccessPermission, DTM_ARM_MPU_ACCESS_(TypeExtField, IsShareable, IsCacheable, IsBufferable), SubRegionDisable, Size)

typedef struct {
    volatile uint32_t CPUID;
    volatile uint32_t ICSR;
    volatile uint32_t VTOR;
    volatile uint32_t AIRCR;
    volatile uint32_t SCR;
    volatile uint32_t CCR;
    volatile uint32_t SHPR[3U];
    volatile uint32_t SHCSR;
    volatile uint32_t CFSR;
    volatile uint32_t HFSR;
    volatile uint32_t DFSR;
    volatile uint32_t MMFAR;
    volatile uint32_t BFAR;
} DTM_SCB_Type;

#define DTM_SCS_BASE            (0xE000E000UL)
#define DTM_SCB_BASE            (DTM_SCS_BASE +  0x0D00UL)
#define DTM_SCB                 ((DTM_SCB_Type       *)     DTM_SCB_BASE)

#define DTM_SCB_SHCSR_MEMFAULTENA_Msk      (1UL << 16)

static inline void __DSB(void) {
    __asm volatile("dsb" ::: "memory");
}

static inline void __ISB(void) {
    __asm volatile("isb" ::: "memory");
}

static inline void __DMB(void) {
    __asm volatile("dmb" ::: "memory");
}

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

static inline uint32_t dtm_ffs(uint32_t x) {
    if (x == 0) return 0;
    uint32_t pos = 0;
    while ((x & 1) == 0) {
        x >>= 1;
        pos++;
    }
    return pos;
}

static uint32_t mpu_align_min(uint32_t map_size) {
    if (map_size == 0) return 32;
    
    uint32_t size = 1;
    while (size < map_size) {
        size <<= 1;
    }
    
    if (size < 32) size = 32;
    
    return size;
}

int dtm_mpu_init(void) {
    uint32_t type = DTM_MPU->TYPE;
    uint8_t num_regions = (type >> 8) & 0xFF;
    
    if (num_regions == 0) {
        return -1;
    }
    
    dtm_mpu_disable();
    
    for (int i = 0; i < num_regions; i++) {
        ARM_MPU_ClrRegion(i);
    }
    
    dtm_mpu_enable();
    
    return 0;
}

void dtm_mpu_enable(void) {
    ARM_MPU_Enable(DTM_MPU_CTRL_PRIVDEFENA_Msk);
}

void dtm_mpu_disable(void) {
    ARM_MPU_Disable();
}

int dtm_mpu_region_set(uint32_t region_num, uint32_t base_addr, 
                       uint32_t size, uint32_t attributes) {
    uint32_t rasr;
    uint32_t size_encode;
    uint32_t ap;
    uint32_t xn;
    
    if (region_num >= DTM_MPU_MAX_REGIONS) {
        return -1;
    }
    
    uint32_t align_size = mpu_align_min(size);
    uint32_t aligned_addr = base_addr & ~(align_size - 1);
    
    size_encode = dtm_ffs(align_size) - 1;
    
    if (attributes & DTM_MPU_ATT_FULL) {
        ap = ARM_MPU_AP_FULL;
    } else if (attributes & DTM_MPU_ATT_WRITE) {
        ap = ARM_MPU_AP_FULL;
    } else if (attributes & DTM_MPU_ATT_READ) {
        ap = ARM_MPU_AP_URO;
    } else {
        ap = ARM_MPU_AP_NONE;
    }
    
    xn = 0;
    if (!(attributes & DTM_MPU_ATT_EXEC)) {
        xn = 1;
    }
    
    rasr = DTM_ARM_MPU_RASR(xn, ap, 0UL, 1UL, 1UL, 1UL, 0UL, size_encode);
    
    ARM_MPU_SetRegionEx(region_num, DTM_ARM_MPU_RBAR(region_num, aligned_addr), rasr);
    
    return 0;
}

void dtm_mpu_region_clear(uint32_t region_num) {
    if (region_num >= DTM_MPU_MAX_REGIONS) {
        return;
    }
    ARM_MPU_ClrRegion(region_num);
}

void dtm_mpu_load_regions(struct dtm_mpu_info *mpu_info) {
    int i;
    
    if (!mpu_info) return;
    
    dtm_mpu_disable();
    
    for (i = 0; i < DTM_MPU_MAX_REGIONS; i++) {
        struct dtm_mpu_region *region = &mpu_info->regions[i];
        if (region->valid == DTM_MPU_REGION_VALID && region->user_data) {
            DTM_ARM_MPU_Region_t *mpu_reg = (DTM_ARM_MPU_Region_t *)region->user_data;
            ARM_MPU_SetRegionEx(i, mpu_reg->RBAR, mpu_reg->RASR);
        } else {
            ARM_MPU_ClrRegion(i);
        }
    }
    
    dtm_mpu_enable();
}

void dtm_mpu_clear_regions(struct dtm_mpu_info *mpu_info) {
    int i;
    
    if (!mpu_info) return;
    
    dtm_mpu_disable();
    
    for (i = 0; i < DTM_MPU_MAX_REGIONS; i++) {
        if (mpu_info->regions[i].valid == DTM_MPU_REGION_VALID) {
            ARM_MPU_ClrRegion(i);
        }
    }
    
    dtm_mpu_enable();
}

void dtm_mpu_dump(void) {
    int i;
    
    for (i = 0; i < DTM_MPU_MAX_REGIONS; i++) {
        DTM_MPU->RNR = i;
        uint32_t rasr = DTM_MPU->RASR;
        
        if (rasr & DTM_MPU_RASR_ENABLE_Msk) {
            (void)i;
        }
    }
}

int dtm_mpu_setup_kernel_protection(void) {
    dtm_mpu_disable();
    
    dtm_mpu_region_set(0, DTM_MPU_KERNEL_FLASH_BASE, 1024 * 1024,
                       DTM_MPU_ATT_READ | DTM_MPU_ATT_EXEC);
    
    dtm_mpu_region_set(1, 0x20000000, 512 * 1024,
                       DTM_MPU_ATT_READ | DTM_MPU_ATT_WRITE | DTM_MPU_ATT_EXEC);
    
    dtm_mpu_enable();
    
    dtm_mpu_dump();
    
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
    
    // 配置共享堆区域
    dtm_mpu_region_set(3, PUP_SHARED_RAM_BASE, PUP_SHARED_RAM_SIZE,
                       DTM_MPU_ATT_READ | DTM_MPU_ATT_WRITE | DTM_MPU_ATT_EXEC);
    
    dtm_mpu_enable();
    
    dtm_mpu_dump();
    
    return 0;
}

#endif /* DTM_BINFDING_MPU_ARM_M4_ST_GD */
