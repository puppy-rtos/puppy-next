#ifndef __PUPPY_MPU_H__
#define __PUPPY_MPU_H__

#include <puppy_core.h>

#define PUP_MPU_REGION_INVALID      0x00
#define PUP_MPU_REGION_VALID        0x01

#define PUP_MPU_ATT_READ     0x01
#define PUP_MPU_ATT_WRITE    0x02
#define PUP_MPU_ATT_EXEC     0x04
#define PUP_MPU_ATT_FULL     0x08

#define PUP_MPU_MAX_REGIONS  8

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

#endif
