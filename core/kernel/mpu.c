#include "mpu.h"
#include "dtm_mpu.h"
#include "puppy_log.h"
#include <string.h>

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

int pup_mpu_setup_app_protection(pup_uint32_t app_ram_base, pup_uint32_t app_ram_size) {
    return dtm_mpu_setup_app_protection(app_ram_base, app_ram_size);
}

int pup_mpu_isolate_app_region(pup_uint32_t app_ram_base, pup_uint32_t app_ram_size) {
    return dtm_mpu_isolate_app_region(app_ram_base, app_ram_size);
}

int pup_mpu_setup_app_isolation(pup_uint32_t current_app_base, pup_uint32_t current_app_size,
                                pup_mpu_app_region_t *other_apps, int other_app_count) {
    dtm_mpu_app_region_t dtm_apps[PUP_MPU_MAX_ISOLATED_APPS];
    int i;
    
    if (other_app_count > PUP_MPU_MAX_ISOLATED_APPS) {
        other_app_count = PUP_MPU_MAX_ISOLATED_APPS;
    }
    
    for (i = 0; i < other_app_count; i++) {
        dtm_apps[i].base = other_apps[i].base;
        dtm_apps[i].size = other_apps[i].size;
    }
    
    return dtm_mpu_setup_app_isolation(current_app_base, current_app_size, 
                                        dtm_apps, other_app_count);
}

void pup_mpu_get_isolation_regions(pup_uint32_t current_app_base, 
                                   pup_uint32_t current_app_end,
                                   pup_mpu_app_region_t *regions, int *count) {
    dtm_mpu_get_isolation_regions(current_app_base, current_app_end,
                                   (dtm_mpu_app_region_t *)regions, count);
}

pup_uint32_t pup_mpu_calc_min_align_size(pup_uint32_t size) {
    if (size == 0) return 32;
    
    pup_uint32_t aligned_size = 1;
    while (aligned_size < size) {
        aligned_size <<= 1;
    }
    
    if (aligned_size < 32) aligned_size = 32;
    
    return aligned_size;
}

pup_uint32_t pup_mpu_align_addr(pup_uint32_t addr, pup_uint32_t align_size) {
    pup_uint32_t mask = align_size - 1;
    return addr & ~mask;
}

int pup_mpu_dynamic_init(pup_mpu_dynamic_config_t *config, pup_uint32_t app_id) {
    if (!config) {
        return -1;
    }
    
    memset(config, 0, sizeof(pup_mpu_dynamic_config_t));
    config->app_id = app_id;
    config->region_count = 0;
    config->lock = 0;
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_MPU, "Dynamic MPU config initialized for app %d", app_id);
    return 0;
}

int pup_mpu_dynamic_alloc_region(pup_mpu_dynamic_config_t *config, 
                                  pup_uint32_t base_addr, pup_uint32_t size, 
                                  pup_uint32_t attributes) {
    pup_base_t key;
    int region_idx = -1;
    pup_uint32_t aligned_size;
    pup_uint32_t aligned_addr;
    
    if (!config || size == 0) {
        return -1;
    }
    
    key = arch_irq_lock();
    
    if (config->region_count >= PUP_MPU_MAX_DYNAMIC_REGIONS) {
        arch_irq_unlock(key);
        PUP_LOG_ERROR(PUP_LOG_MODULE_MPU, "No free dynamic region slots");
        return -1;
    }
    
    aligned_size = pup_mpu_calc_min_align_size(size);
    aligned_addr = pup_mpu_align_addr(base_addr, aligned_size);
    
    region_idx = config->region_count;
    config->regions[region_idx].region_num = PUP_MPU_REGION_OTHER_APP_0 + region_idx;
    config->regions[region_idx].base_addr = aligned_addr;
    config->regions[region_idx].size = aligned_size;
    config->regions[region_idx].attributes = attributes;
    config->regions[region_idx].valid = 1;
    config->region_count++;
    
    arch_irq_unlock(key);
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_MPU, "Allocated dynamic region %d: addr=0x%08x, size=%d",
                  region_idx, aligned_addr, aligned_size);
    return region_idx;
}

int pup_mpu_dynamic_free_region(pup_mpu_dynamic_config_t *config, pup_uint32_t region_num) {
    pup_base_t key;
    int i;
    int found = 0;
    
    if (!config) {
        return -1;
    }
    
    key = arch_irq_lock();
    
    for (i = 0; i < config->region_count; i++) {
        if (config->regions[i].region_num == region_num && config->regions[i].valid) {
            config->regions[i].valid = 0;
            found = 1;
            break;
        }
    }
    
    if (found) {
        for (i = 0; i < config->region_count - 1; i++) {
            if (!config->regions[i].valid && config->regions[i + 1].valid) {
                config->regions[i] = config->regions[i + 1];
                config->regions[i + 1].valid = 0;
            }
        }
        config->region_count--;
    }
    
    arch_irq_unlock(key);
    
    if (found) {
        PUP_LOG_DEBUG(PUP_LOG_MODULE_MPU, "Freed dynamic region %d", region_num);
        return 0;
    }
    
    return -1;
}

int pup_mpu_dynamic_update_region(pup_mpu_dynamic_config_t *config, pup_uint32_t region_num,
                                   pup_uint32_t base_addr, pup_uint32_t size,
                                   pup_uint32_t attributes) {
    pup_base_t key;
    int i;
    pup_uint32_t aligned_size;
    pup_uint32_t aligned_addr;
    
    if (!config || size == 0) {
        return -1;
    }
    
    aligned_size = pup_mpu_calc_min_align_size(size);
    aligned_addr = pup_mpu_align_addr(base_addr, aligned_size);
    
    key = arch_irq_lock();
    
    for (i = 0; i < config->region_count; i++) {
        if (config->regions[i].region_num == region_num && config->regions[i].valid) {
            config->regions[i].base_addr = aligned_addr;
            config->regions[i].size = aligned_size;
            config->regions[i].attributes = attributes;
            arch_irq_unlock(key);
            PUP_LOG_DEBUG(PUP_LOG_MODULE_MPU, "Updated dynamic region %d: addr=0x%08x, size=%d",
                          region_num, aligned_addr, aligned_size);
            return 0;
        }
    }
    
    arch_irq_unlock(key);
    return -1;
}

int pup_mpu_dynamic_apply(pup_mpu_dynamic_config_t *config) {
    int i;
    int ret = 0;
    
    if (!config) {
        return -1;
    }
    
    pup_mpu_disable();
    
    for (i = 0; i < PUP_MPU_MAX_ISOLATED_APPS; i++) {
        pup_mpu_region_clear(PUP_MPU_REGION_OTHER_APP_0 + i);
    }
    
    for (i = 0; i < config->region_count && i < PUP_MPU_MAX_ISOLATED_APPS; i++) {
        if (config->regions[i].valid) {
            ret = pup_mpu_region_set(config->regions[i].region_num,
                                      config->regions[i].base_addr,
                                      config->regions[i].size,
                                      config->regions[i].attributes);
            if (ret != 0) {
                PUP_LOG_ERROR(PUP_LOG_MODULE_MPU, "Failed to set region %d", 
                              config->regions[i].region_num);
                break;
            }
        }
    }
    
    pup_mpu_enable();
    
    PUP_LOG_INFO(PUP_LOG_MODULE_MPU, "Applied %d dynamic MPU regions", config->region_count);
    return ret;
}

void pup_mpu_dynamic_cleanup(pup_mpu_dynamic_config_t *config) {
    int i;
    
    if (!config) {
        return;
    }
    
    pup_mpu_disable();
    
    for (i = 0; i < config->region_count && i < PUP_MPU_MAX_ISOLATED_APPS; i++) {
        if (config->regions[i].valid) {
            pup_mpu_region_clear(config->regions[i].region_num);
        }
    }
    
    pup_mpu_enable();
    
    memset(config, 0, sizeof(pup_mpu_dynamic_config_t));
    PUP_LOG_DEBUG(PUP_LOG_MODULE_MPU, "Dynamic MPU config cleaned up");
}

int pup_mpu_multi_app_isolate(pup_uint32_t current_app_id,
                               pup_uint32_t current_base, pup_uint32_t current_size,
                               pup_mpu_app_region_t *all_apps, int app_count) {
    pup_mpu_app_region_t other_apps[PUP_MPU_MAX_ISOLATED_APPS];
    int other_count = 0;
    int i;
    
    if (!all_apps || app_count <= 0) {
        return pup_mpu_setup_app_protection(current_base, current_size);
    }
    
    for (i = 0; i < app_count && other_count < PUP_MPU_MAX_ISOLATED_APPS; i++) {
        if (i != (int)current_app_id) {
            other_apps[other_count].base = all_apps[i].base;
            other_apps[other_count].size = all_apps[i].size;
            other_count++;
        }
    }
    
    return pup_mpu_setup_app_isolation(current_base, current_size, other_apps, other_count);
}
