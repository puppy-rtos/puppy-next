#include "mpu.h"
#include "dtm_mpu.h"
#include "puppy_log.h"

int pup_mpu_init(void) {
    PUP_LOG_INFO(PUP_LOG_MODULE_MPU, "MPU init via DTM");
    return dtm_mpu_init();
}

void pup_mpu_enable(void) {
    dtm_mpu_enable();
}

void pup_mpu_disable(void) {
    dtm_mpu_disable();
}

int pup_mpu_region_set(pup_uint32_t region_num, pup_uint32_t base_addr, 
                       pup_uint32_t size, pup_uint32_t attributes) {
    PUP_LOG_DEBUG(PUP_LOG_MODULE_MPU, "Set region %d: addr=0x%08x, size=%d, attr=0x%x",
               region_num, base_addr, size, attributes);
    return dtm_mpu_region_set(region_num, base_addr, size, attributes);
}

void pup_mpu_region_clear(pup_uint32_t region_num) {
    dtm_mpu_region_clear(region_num);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_MPU, "Cleared region %d", region_num);
}

void pup_mpu_load_regions(struct pup_mpu_info *mpu_info) {
    dtm_mpu_load_regions((struct dtm_mpu_info *)mpu_info);
}

void pup_mpu_clear_regions(struct pup_mpu_info *mpu_info) {
    dtm_mpu_clear_regions((struct dtm_mpu_info *)mpu_info);
}

void pup_mpu_dump(void) {
    PUP_LOG_INFO(PUP_LOG_MODULE_MPU, "=== MPU Status ===");
    dtm_mpu_dump();
    PUP_LOG_INFO(PUP_LOG_MODULE_MPU, "==================");
}

int pup_mpu_setup_kernel_protection(void) {
    int ret = dtm_mpu_setup_kernel_protection();
    PUP_LOG_INFO(PUP_LOG_MODULE_MPU, "Kernel MPU protection enabled");
    return ret;
}

int pup_mpu_setup_user_protection(void) {
    int ret = dtm_mpu_setup_user_protection();
    PUP_LOG_INFO(PUP_LOG_MODULE_MPU, "User MPU protection enabled");
    return ret;
}
