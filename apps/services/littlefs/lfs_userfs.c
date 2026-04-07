#include "lfs_userfs.h"
#include "lfs.h"
#include "lfs_flash_bd.h"
#include "pup_userfs_proto.h"
#include "pup_flash.h"
#include "libc.h"
#include "syscall.h"
#include <stdint.h>
#include <stddef.h>

#define LFS_USERFS_MAX_INSTANCES   4
#define LFS_USERFS_MAX_FDS         8
#define LFS_USERFS_MAX_DIRS        4

#define LFS_RAMBD_BLOCK_COUNT      16
#define LFS_RAMBD_BLOCK_SIZE       512
#define LFS_RAMBD_SIZE             (LFS_RAMBD_BLOCK_COUNT * LFS_RAMBD_BLOCK_SIZE)

typedef struct {
    lfs_t lfs;
    struct lfs_config cfg;
    int mounted;
    int flash_fd;
    int use_rambd;
    lfs_flash_bd_t bd;
    char device[PUP_USERFS_PATH_MAX];
    
    uint8_t *rambd_buffer;
    uint8_t read_buf[256];
    uint8_t prog_buf[256];
    uint8_t lookahead_buf[32];
    
    lfs_file_t files[LFS_USERFS_MAX_FDS];
    int fds[LFS_USERFS_MAX_FDS];
    int fd_used[LFS_USERFS_MAX_FDS];
    
    lfs_dir_t dirs[LFS_USERFS_MAX_DIRS];
    int dir_fds[LFS_USERFS_MAX_DIRS];
    int dir_used[LFS_USERFS_MAX_DIRS];
    
    int next_fd;
    int next_dir_fd;
} lfs_instance_t;

static lfs_instance_t g_instances[LFS_USERFS_MAX_INSTANCES];
static int g_instance_used[LFS_USERFS_MAX_INSTANCES];

static int alloc_instance(void)
{
    int i;
    for (i = 0; i < LFS_USERFS_MAX_INSTANCES; i++) {
        if (!g_instance_used[i]) {
            g_instance_used[i] = 1;
            return i;
        }
    }
    return -1;
}

static int find_instance_by_device(const char *device)
{
    int i;
    for (i = 0; i < LFS_USERFS_MAX_INSTANCES; i++) {
        if (g_instance_used[i] && g_instances[i].mounted) {
            if (device == NULL || device[0] == '\0') {
                if (g_instances[i].use_rambd) {
                    return i;
                }
            } else {
                if (!g_instances[i].use_rambd && 
                    strcmp(g_instances[i].device, device) == 0) {
                    return i;
                }
            }
        }
    }
    return -1;
}

static lfs_instance_t *get_instance(int idx)
{
    (void)idx;
    return NULL;
}

static void free_instance(int idx)
{
    lfs_instance_t *inst = &g_instances[idx];
    
    if (inst->rambd_buffer) {
        shared_free(inst->rambd_buffer);
        inst->rambd_buffer = NULL;
    }
    
    if (inst->flash_fd >= 0) {
        close(inst->flash_fd);
        inst->flash_fd = -1;
    }
    
    memset(inst, 0, sizeof(lfs_instance_t));
    g_instance_used[idx] = 0;
}

static int lfs_rambd_read(const struct lfs_config *c, lfs_block_t block,
                          lfs_off_t off, void *buffer, lfs_size_t size)
{
    lfs_instance_t *inst = (lfs_instance_t *)c->context;
    uint32_t addr = block * c->block_size + off;
    
    if (!inst || !inst->rambd_buffer) {
        return LFS_ERR_IO;
    }
    if (addr + size > LFS_RAMBD_SIZE) {
        return LFS_ERR_INVAL;
    }
    memcpy(buffer, inst->rambd_buffer + addr, size);
    return LFS_ERR_OK;
}

static int lfs_rambd_prog(const struct lfs_config *c, lfs_block_t block,
                          lfs_off_t off, const void *buffer, lfs_size_t size)
{
    lfs_instance_t *inst = (lfs_instance_t *)c->context;
    uint32_t addr = block * c->block_size + off;
    
    if (!inst || !inst->rambd_buffer) {
        return LFS_ERR_IO;
    }
    if (addr + size > LFS_RAMBD_SIZE) {
        return LFS_ERR_INVAL;
    }
    memcpy(inst->rambd_buffer + addr, buffer, size);
    return LFS_ERR_OK;
}

static int lfs_rambd_erase(const struct lfs_config *c, lfs_block_t block)
{
    lfs_instance_t *inst = (lfs_instance_t *)c->context;
    uint32_t addr = block * c->block_size;
    
    if (!inst || !inst->rambd_buffer) {
        return LFS_ERR_IO;
    }
    if (addr + c->block_size > LFS_RAMBD_SIZE) {
        return LFS_ERR_INVAL;
    }
    memset(inst->rambd_buffer + addr, 0xFF, c->block_size);
    return LFS_ERR_OK;
}

static int lfs_rambd_sync(const struct lfs_config *c)
{
    (void)c;
    return LFS_ERR_OK;
}

static void configure_rambd(lfs_instance_t *inst, struct lfs_config *cfg)
{
    cfg->context = inst;
    cfg->read = lfs_rambd_read;
    cfg->prog = lfs_rambd_prog;
    cfg->erase = lfs_rambd_erase;
    cfg->sync = lfs_rambd_sync;
    cfg->read_size = 1;
    cfg->prog_size = 1;
    cfg->block_size = LFS_RAMBD_BLOCK_SIZE;
    cfg->block_count = LFS_RAMBD_BLOCK_COUNT;
    cfg->block_cycles = 500;
    cfg->cache_size = 256;
    cfg->lookahead_size = 32;
    cfg->read_buffer = inst->read_buf;
    cfg->prog_buffer = inst->prog_buf;
    cfg->lookahead_buffer = inst->lookahead_buf;
}

static int alloc_fd(lfs_instance_t *inst)
{
    int i;
    for (i = 0; i < LFS_USERFS_MAX_FDS; i++) {
        if (!inst->fd_used[i]) {
            inst->fd_used[i] = 1;
            inst->fds[i] = inst->next_fd++;
            return inst->fds[i];
        }
    }
    return -1;
}

static int find_fd_index(lfs_instance_t *inst, int fd)
{
    int i;
    for (i = 0; i < LFS_USERFS_MAX_FDS; i++) {
        if (inst->fd_used[i] && inst->fds[i] == fd) {
            return i;
        }
    }
    return -1;
}

static void free_fd(lfs_instance_t *inst, int fd)
{
    int idx = find_fd_index(inst, fd);
    if (idx >= 0) {
        inst->fd_used[idx] = 0;
    }
}

static int alloc_dir_fd(lfs_instance_t *inst)
{
    int i;
    for (i = 0; i < LFS_USERFS_MAX_DIRS; i++) {
        if (!inst->dir_used[i]) {
            inst->dir_used[i] = 1;
            inst->dir_fds[i] = inst->next_dir_fd++;
            return inst->dir_fds[i];
        }
    }
    return -1;
}

static int find_dir_index(lfs_instance_t *inst, int fd)
{
    int i;
    for (i = 0; i < LFS_USERFS_MAX_DIRS; i++) {
        if (inst->dir_used[i] && inst->dir_fds[i] == fd) {
            return i;
        }
    }
    return -1;
}

static void free_dir_fd(lfs_instance_t *inst, int fd)
{
    int idx = find_dir_index(inst, fd);
    if (idx >= 0) {
        inst->dir_used[idx] = 0;
    }
}

static int lfs_userfs_mount(const char *device, unsigned long flags, const void *data)
{
    int inst_idx;
    lfs_instance_t *inst;
    int ret;
    pup_flash_info_t flash_info;
    
    (void)flags;
    (void)data;
    
    if (device && device[0] != '\0') {
        int existing = find_instance_by_device(device);
        if (existing >= 0) {
            printf("[lfs] device %s already mounted\n", device);
            return 0;
        }
    } else {
        int existing = find_instance_by_device(NULL);
        if (existing >= 0) {
            printf("[lfs] RAM device already mounted\n");
            return 0;
        }
    }
    
    inst_idx = alloc_instance();
    if (inst_idx < 0) {
        printf("[lfs] no free instance slot\n");
        return -1;
    }
    
    inst = &g_instances[inst_idx];
    memset(inst, 0, sizeof(lfs_instance_t));
    inst->flash_fd = -1;
    inst->next_fd = 1;
    inst->next_dir_fd = 100;
    
    if (device && device[0] != '\0' && strcmp(device, "none") != 0) {
        strncpy(inst->device, device, PUP_USERFS_PATH_MAX - 1);
        inst->use_rambd = 0;
        
        inst->flash_fd = open(device, 0);
        if (inst->flash_fd < 0) {
            printf("[lfs] cannot open device %s, using RAM\n", device);
            inst->use_rambd = 1;
        } else {
            ret = ioctl(inst->flash_fd, PUP_FLASH_IOCTL_GET_INFO, &flash_info);
            if (ret < 0) {
                printf("[lfs] failed to get flash info for %s, using RAM\n", device);
                close(inst->flash_fd);
                inst->flash_fd = -1;
                inst->use_rambd = 1;
            } else {
                printf("[lfs] device %s: %lu bytes, erase_gran: %lu\n",
                       device, (unsigned long)flash_info.capacity, 
                       (unsigned long)flash_info.erase_gran);
            }
        }
    } else {
        inst->use_rambd = 1;
        inst->device[0] = '\0';
    }
    
    if (inst->use_rambd) {
        printf("[lfs] using RAM block device: %d blocks x %d bytes\n",
               LFS_RAMBD_BLOCK_COUNT, LFS_RAMBD_BLOCK_SIZE);
        
        inst->rambd_buffer = (uint8_t *)shared_malloc(LFS_RAMBD_SIZE);
        if (!inst->rambd_buffer) {
            printf("[lfs] failed to allocate RAM buffer\n");
            free_instance(inst_idx);
            return -1;
        }
        memset(inst->rambd_buffer, 0xFF, LFS_RAMBD_SIZE);
        configure_rambd(inst, &inst->cfg);
    } else {
        uint32_t total_blocks = flash_info.capacity / flash_info.erase_gran;
        uint32_t start_block = 0;
        uint32_t block_count = total_blocks;
        
        printf("[lfs] using entire flash: %lu blocks (%lu bytes)\n",
               (unsigned long)block_count,
               (unsigned long)(block_count * flash_info.erase_gran));
        
        ret = lfs_flash_bd_init(&inst->bd, inst->flash_fd, &flash_info, start_block, block_count);
        if (ret < 0) {
            printf("[lfs] flash bd init failed: %d\n", ret);
            free_instance(inst_idx);
            return -1;
        }
        
        lfs_flash_bd_configure(&inst->cfg, &inst->bd);
        inst->cfg.read_buffer = inst->read_buf;
        inst->cfg.prog_buffer = inst->prog_buf;
        inst->cfg.lookahead_buffer = inst->lookahead_buf;
    }
    
    ret = lfs_mount(&inst->lfs, &inst->cfg);
    if (ret < 0) {
        printf("[lfs] mount failed: %d, formatting...\n", ret);
        ret = lfs_format(&inst->lfs, &inst->cfg);
        if (ret < 0) {
            printf("[lfs] format failed: %d\n", ret);
            free_instance(inst_idx);
            return -1;
        }
        ret = lfs_mount(&inst->lfs, &inst->cfg);
        if (ret < 0) {
            printf("[lfs] mount after format failed: %d\n", ret);
            free_instance(inst_idx);
            return -1;
        }
    }
    
    inst->mounted = 1;
    printf("[lfs] mounted successfully (inst=%d, %s)\n", 
           inst_idx, inst->use_rambd ? "RAM" : inst->device);
    
    return inst_idx;
}

static int lfs_userfs_unmount(void)
{
    int i;
    
    for (i = 0; i < LFS_USERFS_MAX_INSTANCES; i++) {
        if (g_instance_used[i] && g_instances[i].mounted) {
            lfs_instance_t *inst = &g_instances[i];
            
            lfs_unmount(&inst->lfs);
            
            printf("[lfs] unmounted instance %d (%s)\n", 
                   i, inst->use_rambd ? "RAM" : inst->device);
            
            free_instance(i);
        }
    }
    
    return 0;
}

static lfs_instance_t *fd_to_instance(int fd)
{
    int i;
    for (i = 0; i < LFS_USERFS_MAX_INSTANCES; i++) {
        if (g_instance_used[i] && g_instances[i].mounted) {
            if (find_fd_index(&g_instances[i], fd) >= 0 ||
                find_dir_index(&g_instances[i], fd) >= 0) {
                return &g_instances[i];
            }
        }
    }
    return NULL;
}

static int lfs_userfs_open(const char *path, int flags)
{
    int i;
    lfs_instance_t *inst = NULL;
    int fd;
    int idx;
    int lfs_flags = 0;
    int ret;
    
    for (i = 0; i < LFS_USERFS_MAX_INSTANCES; i++) {
        if (g_instance_used[i] && g_instances[i].mounted) {
            inst = &g_instances[i];
            break;
        }
    }
    
    if (!inst) {
        return -1;
    }
    
    if (flags & PUP_USERFS_O_DIRECTORY) {
        fd = alloc_dir_fd(inst);
        if (fd < 0) {
            return -1;
        }
        
        idx = find_dir_index(inst, fd);
        ret = lfs_dir_open(&inst->lfs, &inst->dirs[idx], path);
        if (ret < 0) {
            free_dir_fd(inst, fd);
            return -1;
        }
        
        return fd;
    }
    
    fd = alloc_fd(inst);
    if (fd < 0) {
        return -1;
    }
    
    idx = find_fd_index(inst, fd);
    
    if (flags & PUP_USERFS_O_WRONLY) {
        lfs_flags |= LFS_O_WRONLY;
    } else if (flags & PUP_USERFS_O_RDWR) {
        lfs_flags |= LFS_O_RDWR;
    } else {
        lfs_flags |= LFS_O_RDONLY;
    }
    
    if (flags & PUP_USERFS_O_CREAT) {
        lfs_flags |= LFS_O_CREAT;
    }
    if (flags & PUP_USERFS_O_EXCL) {
        lfs_flags |= LFS_O_EXCL;
    }
    if (flags & PUP_USERFS_O_TRUNC) {
        lfs_flags |= LFS_O_TRUNC;
    }
    if (flags & PUP_USERFS_O_APPEND) {
        lfs_flags |= LFS_O_APPEND;
    }
    
    ret = lfs_file_open(&inst->lfs, &inst->files[idx], path, lfs_flags);
    if (ret < 0) {
        free_fd(inst, fd);
        return -1;
    }
    
    return fd;
}

static int lfs_userfs_close(int fd)
{
    lfs_instance_t *inst = fd_to_instance(fd);
    int idx;
    
    if (!inst) {
        return -1;
    }
    
    idx = find_fd_index(inst, fd);
    if (idx < 0) {
        idx = find_dir_index(inst, fd);
        if (idx >= 0) {
            lfs_dir_close(&inst->lfs, &inst->dirs[idx]);
            free_dir_fd(inst, fd);
            return 0;
        }
        return -1;
    }
    
    lfs_file_close(&inst->lfs, &inst->files[idx]);
    free_fd(inst, fd);
    return 0;
}

static int lfs_userfs_read(int fd, void *buf, uint32_t size)
{
    lfs_instance_t *inst = fd_to_instance(fd);
    int idx;
    lfs_ssize_t ret;
    
    if (!inst) {
        return -1;
    }
    
    idx = find_fd_index(inst, fd);
    if (idx < 0) {
        return -1;
    }
    
    ret = lfs_file_read(&inst->lfs, &inst->files[idx], buf, size);
    return (int)ret;
}

static int lfs_userfs_write(int fd, const void *buf, uint32_t size)
{
    lfs_instance_t *inst = fd_to_instance(fd);
    int idx;
    lfs_ssize_t ret;
    
    if (!inst) {
        return -1;
    }
    
    idx = find_fd_index(inst, fd);
    if (idx < 0) {
        return -1;
    }
    
    ret = lfs_file_write(&inst->lfs, &inst->files[idx], buf, size);
    return (int)ret;
}

static long lfs_userfs_lseek(int fd, long offset, int whence)
{
    lfs_instance_t *inst = fd_to_instance(fd);
    int idx;
    lfs_soff_t ret;
    int lfs_whence;
    
    if (!inst) {
        return -1;
    }
    
    idx = find_fd_index(inst, fd);
    if (idx < 0) {
        return -1;
    }
    
    switch (whence) {
    case PUP_USERFS_SEEK_SET:
        lfs_whence = LFS_SEEK_SET;
        break;
    case PUP_USERFS_SEEK_CUR:
        lfs_whence = LFS_SEEK_CUR;
        break;
    case PUP_USERFS_SEEK_END:
        lfs_whence = LFS_SEEK_END;
        break;
    default:
        return -1;
    }
    
    ret = lfs_file_seek(&inst->lfs, &inst->files[idx], offset, lfs_whence);
    return (long)ret;
}

static int lfs_userfs_sync(int fd)
{
    lfs_instance_t *inst = fd_to_instance(fd);
    int idx;
    
    if (!inst) {
        return -1;
    }
    
    idx = find_fd_index(inst, fd);
    if (idx < 0) {
        return -1;
    }
    
    return lfs_file_sync(&inst->lfs, &inst->files[idx]);
}

static int lfs_userfs_stat(const char *path, pup_userfs_stat_resp_t *st)
{
    int i;
    lfs_instance_t *inst = NULL;
    struct lfs_info info;
    int ret;
    
    for (i = 0; i < LFS_USERFS_MAX_INSTANCES; i++) {
        if (g_instance_used[i] && g_instances[i].mounted) {
            inst = &g_instances[i];
            break;
        }
    }
    
    if (!inst) {
        return -1;
    }
    
    ret = lfs_stat(&inst->lfs, path, &info);
    if (ret < 0) {
        return -1;
    }
    
    st->st_dev = 0;
    st->st_ino = 0;
    st->st_mode = (info.type == LFS_TYPE_DIR) ? 0040777 : 0100777;
    st->st_nlink = 1;
    st->st_size = info.size;
    st->st_mtime = 0;
    
    return 0;
}

static int lfs_userfs_unlink(const char *path)
{
    int i;
    lfs_instance_t *inst = NULL;
    
    for (i = 0; i < LFS_USERFS_MAX_INSTANCES; i++) {
        if (g_instance_used[i] && g_instances[i].mounted) {
            inst = &g_instances[i];
            break;
        }
    }
    
    if (!inst) {
        return -1;
    }
    
    return lfs_remove(&inst->lfs, path);
}

static int lfs_userfs_rename(const char *oldpath, const char *newpath)
{
    int i;
    lfs_instance_t *inst = NULL;
    
    for (i = 0; i < LFS_USERFS_MAX_INSTANCES; i++) {
        if (g_instance_used[i] && g_instances[i].mounted) {
            inst = &g_instances[i];
            break;
        }
    }
    
    if (!inst) {
        return -1;
    }
    
    return lfs_rename(&inst->lfs, oldpath, newpath);
}

static int lfs_userfs_mkdir(const char *path)
{
    int i;
    lfs_instance_t *inst = NULL;
    
    for (i = 0; i < LFS_USERFS_MAX_INSTANCES; i++) {
        if (g_instance_used[i] && g_instances[i].mounted) {
            inst = &g_instances[i];
            break;
        }
    }
    
    if (!inst) {
        return -1;
    }
    
    return lfs_mkdir(&inst->lfs, path);
}

static int lfs_userfs_getdents(int fd, pup_userfs_dirent_t *dirp, uint32_t count, int *num_entries)
{
    lfs_instance_t *inst = fd_to_instance(fd);
    int idx;
    struct lfs_info info;
    int ret;
    int entries = 0;
    
    if (!inst) {
        return -1;
    }
    
    idx = find_dir_index(inst, fd);
    if (idx < 0) {
        int dir_fd = alloc_dir_fd(inst);
        if (dir_fd < 0) {
            return -1;
        }
        idx = find_dir_index(inst, dir_fd);
        ret = lfs_dir_open(&inst->lfs, &inst->dirs[idx], "/");
        if (ret < 0) {
            free_dir_fd(inst, dir_fd);
            return -1;
        }
    }
    
    while (entries < (int)count) {
        ret = lfs_dir_read(&inst->lfs, &inst->dirs[idx], &info);
        if (ret <= 0) {
            break;
        }
        
        if (info.name[0] == '\0') {
            continue;
        }
        
        if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0) {
            continue;
        }
        
        dirp[entries].d_type = (info.type == LFS_TYPE_DIR) ? PUP_USERFS_DT_DIR : PUP_USERFS_DT_REG;
        dirp[entries].d_namlen = (uint8_t)strlen(info.name);
        dirp[entries].d_reclen = sizeof(pup_userfs_dirent_t);
        strcpy(dirp[entries].d_name, info.name);
        dirp[entries].d_name[PUP_USERFS_NAME_MAX] = '\0';
        
        entries++;
    }
    
    *num_entries = entries;
    return 0;
}

static int lfs_userfs_mkfs(const char *device, uint32_t block_size, uint32_t block_count)
{
    lfs_t lfs;
    int ret;
    pup_flash_info_t flash_info;
    struct lfs_config cfg;
    lfs_flash_bd_t bd;
    uint8_t read_buf[256];
    uint8_t prog_buf[256];
    uint8_t lookahead_buf[32];
    int flash_fd = -1;
    int use_rambd = 0;
    uint8_t *rambd_buffer = NULL;
    
    (void)block_size;
    (void)block_count;
    
    printf("[lfs] mkfs on device %s\n", device ? device : "RAM");
    
    memset(&cfg, 0, sizeof(cfg));
    memset(&lfs, 0, sizeof(lfs));
    
    if (device && device[0] != '\0' && strcmp(device, "none") != 0) {
        flash_fd = open(device, 0);
        if (flash_fd < 0) {
            printf("[lfs] cannot open device %s\n", device);
            return -1;
        }
        
        ret = ioctl(flash_fd, PUP_FLASH_IOCTL_GET_INFO, &flash_info);
        if (ret < 0) {
            printf("[lfs] failed to get flash info for %s\n", device);
            close(flash_fd);
            return -1;
        }
        
        printf("[lfs] device %s: %lu bytes, erase_gran: %lu\n",
               device, (unsigned long)flash_info.capacity, 
               (unsigned long)flash_info.erase_gran);
        
        uint32_t total_blocks = flash_info.capacity / flash_info.erase_gran;
        uint32_t start_block = 0;
        uint32_t block_count = total_blocks;
        
        printf("[lfs] using entire flash: %lu blocks (%lu bytes)\n",
               (unsigned long)block_count,
               (unsigned long)(block_count * flash_info.erase_gran));
        
        ret = lfs_flash_bd_init(&bd, flash_fd, &flash_info, start_block, block_count);
        if (ret < 0) {
            printf("[lfs] flash bd init failed: %d\n", ret);
            close(flash_fd);
            return -1;
        }
        
        lfs_flash_bd_configure(&cfg, &bd);
        cfg.read_buffer = read_buf;
        cfg.prog_buffer = prog_buf;
        cfg.lookahead_buffer = lookahead_buf;
    } else {
        use_rambd = 1;
        
        rambd_buffer = (uint8_t *)shared_malloc(LFS_RAMBD_SIZE);
        if (!rambd_buffer) {
            printf("[lfs] failed to allocate RAM buffer\n");
            return -1;
        }
        memset(rambd_buffer, 0xFF, LFS_RAMBD_SIZE);
        
        cfg.context = rambd_buffer;
        cfg.read = lfs_rambd_read;
        cfg.prog = lfs_rambd_prog;
        cfg.erase = lfs_rambd_erase;
        cfg.sync = lfs_rambd_sync;
        cfg.read_size = 1;
        cfg.prog_size = 1;
        cfg.block_size = LFS_RAMBD_BLOCK_SIZE;
        cfg.block_count = LFS_RAMBD_BLOCK_COUNT;
        cfg.block_cycles = 500;
        cfg.cache_size = 256;
        cfg.lookahead_size = 32;
        cfg.read_buffer = read_buf;
        cfg.prog_buffer = prog_buf;
        cfg.lookahead_buffer = lookahead_buf;
    }
    
    ret = lfs_format(&lfs, &cfg);
    
    if (use_rambd && rambd_buffer) {
        shared_free(rambd_buffer);
    }
    if (flash_fd >= 0) {
        close(flash_fd);
    }
    
    if (ret < 0) {
        printf("[lfs] mkfs failed: %d\n", ret);
        return -1;
    }
    
    printf("[lfs] mkfs completed successfully\n");
    return 0;
}

const userfs_ops_t lfs_userfs_ops = {
    .mount = lfs_userfs_mount,
    .unmount = lfs_userfs_unmount,
    .open = lfs_userfs_open,
    .close = lfs_userfs_close,
    .read = lfs_userfs_read,
    .write = lfs_userfs_write,
    .lseek = lfs_userfs_lseek,
    .sync = lfs_userfs_sync,
    .stat = lfs_userfs_stat,
    .unlink = lfs_userfs_unlink,
    .rename = lfs_userfs_rename,
    .mkdir = lfs_userfs_mkdir,
    .getdents = lfs_userfs_getdents,
    .statfs = NULL,
    .mkfs = lfs_userfs_mkfs,
};
