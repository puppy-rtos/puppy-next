#ifndef __PUPPY_MPU_H__
#define __PUPPY_MPU_H__

#include <puppy_core.h>

#define PUP_MPU_REGION_INVALID      0x00
#define PUP_MPU_REGION_VALID        0x01

#define PUP_MPU_ATT_READ     0x01
#define PUP_MPU_ATT_WRITE    0x02
#define PUP_MPU_ATT_EXEC     0x04
#define PUP_MPU_ATT_FULL     0x08
#define PUP_MPU_ATT_NONE     0x00

#define PUP_MPU_MAX_REGIONS  8

#define PUP_MPU_REGION_FLASH        0
#define PUP_MPU_REGION_KERNEL_RAM   1
#define PUP_MPU_REGION_APP_RAM      2
#define PUP_MPU_REGION_SHARED_RAM   3
#define PUP_MPU_REGION_OTHER_APP_0  4
#define PUP_MPU_REGION_OTHER_APP_1  5
#define PUP_MPU_REGION_OTHER_APP_2  6
#define PUP_MPU_REGION_OTHER_APP_3  7

#define PUP_MPU_MAX_ISOLATED_APPS   4

#define PUP_MPU_MAX_DYNAMIC_REGIONS 6

struct pup_mpu_region {
    pup_int32_t valid;
    void *addr;
    pup_size_t size;
    pup_uint32_t attribute;
    void *user_data;
};

struct pup_mpu_info {
    struct pup_mpu_region regions[PUP_MPU_MAX_REGIONS];
};

typedef struct {
    pup_uint32_t base;
    pup_uint32_t size;
} pup_mpu_app_region_t;

typedef struct {
    pup_uint32_t region_num;
    pup_uint32_t base_addr;
    pup_uint32_t size;
    pup_uint32_t attributes;
    pup_int32_t valid;
} pup_mpu_dynamic_region_t;

typedef struct {
    pup_mpu_dynamic_region_t regions[PUP_MPU_MAX_DYNAMIC_REGIONS];
    pup_int32_t region_count;
    pup_uint32_t app_id;
    pup_base_t lock;
} pup_mpu_dynamic_config_t;

int pup_mpu_init(void);
void pup_mpu_enable(void);
void pup_mpu_disable(void);

int pup_mpu_region_set(pup_uint32_t region_num, pup_uint32_t base_addr, 
                       pup_uint32_t size, pup_uint32_t attributes);
void pup_mpu_region_clear(pup_uint32_t region_num);

void pup_mpu_load_regions(struct pup_mpu_info *mpu_info);
void pup_mpu_clear_regions(struct pup_mpu_info *mpu_info);

void pup_mpu_dump(void);

#define PUP_MPU_KERNEL_FLASH_BASE   PUP_KERNEL_FLASH_BASE
#define PUP_MPU_KERNEL_FLASH_SIZE   PUP_KERNEL_FLASH_SIZE
#define PUP_MPU_KERNEL_RAM_BASE     PUP_KERNEL_RAM_BASE
#define PUP_MPU_KERNEL_RAM_SIZE     PUP_KERNEL_RAM_SIZE
#define PUP_MPU_USER_RAM_BASE       PUP_APP_RAM_BASE
#define PUP_MPU_USER_RAM_SIZE       PUP_APP_RAM_SIZE

int pup_mpu_setup_user_protection(void);
int pup_mpu_setup_kernel_protection(void);

int pup_mpu_setup_app_protection(pup_uint32_t app_ram_base, pup_uint32_t app_ram_size);
int pup_mpu_apply_cached_app_profile(pup_uint32_t region_size,
                                     pup_uint32_t front_base,
                                     pup_uint32_t back_base,
                                     pup_uint32_t front_sub_mask,
                                     pup_uint32_t back_sub_mask);
int pup_mpu_isolate_app_region(pup_uint32_t app_ram_base, pup_uint32_t app_ram_size);
int pup_mpu_app_isolation_enabled(void);

int pup_mpu_setup_app_isolation(pup_uint32_t current_app_base, pup_uint32_t current_app_size,
                                pup_mpu_app_region_t *other_apps, int other_app_count);

void pup_mpu_get_isolation_regions(pup_uint32_t current_app_base, 
                                   pup_uint32_t current_app_end,
                                   pup_mpu_app_region_t *regions, int *count);

int pup_mpu_dynamic_init(pup_mpu_dynamic_config_t *config, pup_uint32_t app_id);
int pup_mpu_dynamic_alloc_region(pup_mpu_dynamic_config_t *config, 
                                  pup_uint32_t base_addr, pup_uint32_t size, 
                                  pup_uint32_t attributes);
int pup_mpu_dynamic_free_region(pup_mpu_dynamic_config_t *config, pup_uint32_t region_num);
int pup_mpu_dynamic_update_region(pup_mpu_dynamic_config_t *config, pup_uint32_t region_num,
                                   pup_uint32_t base_addr, pup_uint32_t size,
                                   pup_uint32_t attributes);
int pup_mpu_dynamic_apply(pup_mpu_dynamic_config_t *config);
void pup_mpu_dynamic_cleanup(pup_mpu_dynamic_config_t *config);

int pup_mpu_multi_app_isolate(pup_uint32_t current_app_id,
                               pup_uint32_t current_base, pup_uint32_t current_size,
                               pup_mpu_app_region_t *all_apps, int app_count);

pup_uint32_t pup_mpu_calc_min_align_size(pup_uint32_t size);
pup_uint32_t pup_mpu_align_addr(pup_uint32_t addr, pup_uint32_t align_size);

#endif
