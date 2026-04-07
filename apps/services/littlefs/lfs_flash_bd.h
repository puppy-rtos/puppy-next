#ifndef __LFS_FLASH_BD_H__
#define __LFS_FLASH_BD_H__

#include "lfs.h"
#include "pup_flash.h"
#include <stdint.h>

typedef struct {
    int fd;
    uint32_t block_size;
    uint32_t block_count;
    uint32_t start_addr;
} lfs_flash_bd_t;

int lfs_flash_bd_init(lfs_flash_bd_t *bd, int fd, pup_flash_info_t *info,
                      uint32_t start_block, uint32_t block_count);

int lfs_flash_bd_configure(struct lfs_config *cfg, lfs_flash_bd_t *bd);

#endif
