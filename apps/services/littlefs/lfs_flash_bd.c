#include "lfs_flash_bd.h"
#include "syscall.h"
#include "libc.h"
#include <stdint.h>

static int lfs_flash_read(const struct lfs_config *c, lfs_block_t block,
                          lfs_off_t off, void *buffer, lfs_size_t size)
{
    lfs_flash_bd_t *bd = (lfs_flash_bd_t *)c->context;
    uint32_t addr = bd->start_addr + (block * c->block_size) + off;
    int ret;
    
    pup_flash_read_req_t *req = (pup_flash_read_req_t *)shared_malloc(sizeof(pup_flash_read_req_t));
    if (!req) {
        return LFS_ERR_NOMEM;
    }
    
    req->addr = addr;
    req->buf = (uint8_t *)buffer;
    req->size = size;
    
    ret = ioctl(bd->fd, PUP_FLASH_IOCTL_READ, req);
    shared_free(req);
    
    if (ret != 0) {
        return LFS_ERR_IO;
    }
    
    return LFS_ERR_OK;
}

static int lfs_flash_prog(const struct lfs_config *c, lfs_block_t block,
                          lfs_off_t off, const void *buffer, lfs_size_t size)
{
    lfs_flash_bd_t *bd = (lfs_flash_bd_t *)c->context;
    uint32_t addr = bd->start_addr + (block * c->block_size) + off;
    int ret;
    
    pup_flash_write_req_t *req = (pup_flash_write_req_t *)shared_malloc(sizeof(pup_flash_write_req_t));
    if (!req) {
        return LFS_ERR_NOMEM;
    }
    
    req->addr = addr;
    req->buf = (const uint8_t *)buffer;
    req->size = size;
    
    ret = ioctl(bd->fd, PUP_FLASH_IOCTL_WRITE, req);
    shared_free(req);
    
    if (ret != 0) {
        return LFS_ERR_IO;
    }
    
    return LFS_ERR_OK;
}

static int lfs_flash_erase(const struct lfs_config *c, lfs_block_t block)
{
    lfs_flash_bd_t *bd = (lfs_flash_bd_t *)c->context;
    uint32_t addr = bd->start_addr + (block * c->block_size);
    int ret;
    
    pup_flash_erase_req_t *req = (pup_flash_erase_req_t *)shared_malloc(sizeof(pup_flash_erase_req_t));
    if (!req) {
        return LFS_ERR_NOMEM;
    }
    
    req->addr = addr;
    req->size = c->block_size;
    
    ret = ioctl(bd->fd, PUP_FLASH_IOCTL_ERASE, req);
    shared_free(req);
    
    if (ret != 0) {
        return LFS_ERR_IO;
    }
    
    return LFS_ERR_OK;
}

static int lfs_flash_sync(const struct lfs_config *c)
{
    (void)c;
    return LFS_ERR_OK;
}

int lfs_flash_bd_init(lfs_flash_bd_t *bd, int fd, pup_flash_info_t *info,
                      uint32_t start_block, uint32_t block_count)
{
    bd->fd = fd;
    bd->block_size = info->erase_gran;
    bd->block_count = block_count;
    bd->start_addr = start_block * info->erase_gran;
    return 0;
}

int lfs_flash_bd_configure(struct lfs_config *cfg, lfs_flash_bd_t *bd)
{
    cfg->context = bd;
    cfg->read = lfs_flash_read;
    cfg->prog = lfs_flash_prog;
    cfg->erase = lfs_flash_erase;
    cfg->sync = lfs_flash_sync;
    
    cfg->read_size = 1;
    cfg->prog_size = 1;
    cfg->block_size = bd->block_size;
    cfg->block_count = bd->block_count;
    cfg->block_cycles = 500;
    cfg->cache_size = bd->block_size > 256 ? 256 : bd->block_size;
    cfg->lookahead_size = 32;
    
    return 0;
}
