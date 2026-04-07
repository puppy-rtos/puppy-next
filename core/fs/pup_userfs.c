#include "pup_userfs.h"
#include "pup_fs.h"
#include "puppy_log.h"
#include "kheap.h"
#include "pup_channel.h"
#include "shared/pup_userfs_proto.h"
#include <libcm.h>

static pup_userfs_type_t g_userfs_types[PUP_USERFS_MAX_TYPES];
static pup_userfs_instance_t g_userfs_instances[PUP_USERFS_MAX_INSTANCES];
static volatile pup_uint32_t g_userfs_lock = 0;
static int g_userfs_next_instance_id = 1;

static void pup_userfs_lock(void)
{
    while (g_userfs_lock) {
    }
    g_userfs_lock = 1;
}

static void pup_userfs_unlock(void)
{
    g_userfs_lock = 0;
}

int pup_userfs_init(void)
{
    libcm_memset(g_userfs_types, 0, sizeof(g_userfs_types));
    libcm_memset(g_userfs_instances, 0, sizeof(g_userfs_instances));
    g_userfs_lock = 0;
    g_userfs_next_instance_id = 1;
    
    PUP_LOG_INFO(PUP_LOG_MODULE_FS, "userfs framework initialized");
    return 0;
}

int pup_userfs_register_type(const char *fs_type, int owner_pid, int channel_id)
{
    int i;
    int free_slot = -1;
    
    if (fs_type == PUP_NULL || channel_id < 0) {
        return PUP_FS_EIO;
    }
    
    pup_userfs_lock();
    
    for (i = 0; i < PUP_USERFS_MAX_TYPES; i++) {
        if (!g_userfs_types[i].used) {
            if (free_slot < 0) free_slot = i;
        } else if (libcm_strcmp(g_userfs_types[i].name, fs_type) == 0) {
            pup_userfs_unlock();
            PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "userfs type %s already registered", fs_type);
            return PUP_FS_EFULL;
        }
    }
    
    if (free_slot < 0) {
        pup_userfs_unlock();
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "no free slot for userfs type %s", fs_type);
        return PUP_FS_EFULL;
    }
    
    g_userfs_types[free_slot].used = 1;
    libcm_strncpy(g_userfs_types[free_slot].name, fs_type, sizeof(g_userfs_types[free_slot].name) - 1);
    g_userfs_types[free_slot].name[sizeof(g_userfs_types[free_slot].name) - 1] = '\0';
    g_userfs_types[free_slot].owner_pid = owner_pid;
    g_userfs_types[free_slot].channel_id = channel_id;
    
    pup_userfs_unlock();
    
    PUP_LOG_INFO(PUP_LOG_MODULE_FS, "userfs type '%s' registered (owner=%d, ch=%d)", 
                 fs_type, owner_pid, channel_id);
    return 0;
}

int pup_userfs_unregister_type(const char *fs_type, int owner_pid)
{
    int i;
    
    if (fs_type == PUP_NULL) {
        return PUP_FS_EIO;
    }
    
    pup_userfs_lock();
    
    for (i = 0; i < PUP_USERFS_MAX_TYPES; i++) {
        if (g_userfs_types[i].used && 
            libcm_strcmp(g_userfs_types[i].name, fs_type) == 0 &&
            g_userfs_types[i].owner_pid == owner_pid) {
            libcm_memset(&g_userfs_types[i], 0, sizeof(pup_userfs_type_t));
            pup_userfs_unlock();
            PUP_LOG_INFO(PUP_LOG_MODULE_FS, "userfs type '%s' unregistered", fs_type);
            return 0;
        }
    }
    
    pup_userfs_unlock();
    return PUP_FS_ENOENT;
}

int pup_userfs_create_instance(const char *fs_type, const char *device,
                                const char *mount_path, unsigned long flags,
                                const void *data, int owner_pid)
{
    int i;
    int type_idx = -1;
    int free_slot = -1;
    int instance_id;
    pup_userfs_mount_req_t *req;
    long result;
    
    if (fs_type == PUP_NULL || mount_path == PUP_NULL) {
        return PUP_FS_EIO;
    }
    
    pup_userfs_lock();
    
    for (i = 0; i < PUP_USERFS_MAX_TYPES; i++) {
        if (g_userfs_types[i].used && 
            libcm_strcmp(g_userfs_types[i].name, fs_type) == 0) {
            type_idx = i;
            break;
        }
    }
    
    if (type_idx < 0) {
        pup_userfs_unlock();
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "userfs type '%s' not found", fs_type);
        return PUP_FS_ENOENT;
    }
    
    for (i = 0; i < PUP_USERFS_MAX_INSTANCES; i++) {
        if (!g_userfs_instances[i].used) {
            if (free_slot < 0) free_slot = i;
        } else if (libcm_strcmp(g_userfs_instances[i].mount_path, mount_path) == 0) {
            pup_userfs_unlock();
            PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "mount path '%s' already in use", mount_path);
            return PUP_FS_EFULL;
        }
    }
    
    if (free_slot < 0) {
        pup_userfs_unlock();
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "no free slot for userfs instance");
        return PUP_FS_EFULL;
    }
    
    instance_id = g_userfs_next_instance_id++;
    
    g_userfs_instances[free_slot].used = 1;
    g_userfs_instances[free_slot].instance_id = instance_id;
    g_userfs_instances[free_slot].type_index = type_idx;
    g_userfs_instances[free_slot].channel_id = g_userfs_types[type_idx].channel_id;
    g_userfs_instances[free_slot].owner_pid = owner_pid;
    libcm_strncpy(g_userfs_instances[free_slot].mount_path, mount_path, PUP_FS_PATH_MAX - 1);
    if (device) {
        libcm_strncpy(g_userfs_instances[free_slot].device, device, PUP_FS_PATH_MAX - 1);
    }
    
    pup_userfs_unlock();
    
    req = (pup_userfs_mount_req_t *)kheap_shared_malloc(sizeof(pup_userfs_mount_req_t));
    if (req == PUP_NULL) {
        pup_userfs_lock();
        libcm_memset(&g_userfs_instances[free_slot], 0, sizeof(pup_userfs_instance_t));
        pup_userfs_unlock();
        return PUP_FS_EIO;
    }
    
    libcm_memset(req, 0, sizeof(pup_userfs_mount_req_t));
    req->header.op = PUP_USERFS_OP_MOUNT;
    req->header.instance_id = instance_id;
    req->header.fd = -1;
    if (device) {
        libcm_strncpy(req->device, device, PUP_USERFS_PATH_MAX - 1);
    }
    req->mount_flags = flags;
    req->data_len = 0;
    
    result = pup_channel_send(g_userfs_types[type_idx].channel_id, 
                               req, sizeof(pup_userfs_mount_req_t));
    kheap_shared_free(req);
    
    if (result < 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "userfs mount request failed: %d", (int)result);
        pup_userfs_lock();
        libcm_memset(&g_userfs_instances[free_slot], 0, sizeof(pup_userfs_instance_t));
        pup_userfs_unlock();
        return PUP_FS_EIO;
    }
    
    PUP_LOG_INFO(PUP_LOG_MODULE_FS, "userfs instance %d created at '%s'", 
                 instance_id, mount_path);
    return instance_id;
}

int pup_userfs_destroy_instance(int instance_id, int owner_pid)
{
    int i;
    pup_userfs_instance_t *inst = PUP_NULL;
    pup_userfs_req_header_t *req;
    long result;
    
    pup_userfs_lock();
    
    for (i = 0; i < PUP_USERFS_MAX_INSTANCES; i++) {
        if (g_userfs_instances[i].used && 
            g_userfs_instances[i].instance_id == instance_id) {
            inst = &g_userfs_instances[i];
            break;
        }
    }
    
    if (inst == PUP_NULL) {
        pup_userfs_unlock();
        return PUP_FS_ENOENT;
    }
    
    if (inst->owner_pid != owner_pid) {
        pup_userfs_unlock();
        return PUP_FS_EIO;
    }
    
    int channel_id = inst->channel_id;
    
    pup_userfs_unlock();
    
    req = (pup_userfs_req_header_t *)kheap_shared_malloc(sizeof(pup_userfs_req_header_t));
    if (req != PUP_NULL) {
        req->op = PUP_USERFS_OP_UNMOUNT;
        req->instance_id = instance_id;
        req->fd = -1;
        pup_channel_send(channel_id, req, sizeof(pup_userfs_req_header_t));
        kheap_shared_free(req);
    }
    
    pup_userfs_lock();
    libcm_memset(inst, 0, sizeof(pup_userfs_instance_t));
    pup_userfs_unlock();
    
    PUP_LOG_INFO(PUP_LOG_MODULE_FS, "userfs instance %d destroyed", instance_id);
    return 0;
}

void pup_userfs_cleanup_owner(int owner_pid)
{
    int i;
    
    pup_userfs_lock();
    
    for (i = 0; i < PUP_USERFS_MAX_TYPES; i++) {
        if (g_userfs_types[i].used && g_userfs_types[i].owner_pid == owner_pid) {
            PUP_LOG_INFO(PUP_LOG_MODULE_FS, "cleanup userfs type '%s' (owner=%d)", 
                         g_userfs_types[i].name, owner_pid);
            libcm_memset(&g_userfs_types[i], 0, sizeof(pup_userfs_type_t));
        }
    }
    
    for (i = 0; i < PUP_USERFS_MAX_INSTANCES; i++) {
        if (g_userfs_instances[i].used && g_userfs_instances[i].owner_pid == owner_pid) {
            PUP_LOG_INFO(PUP_LOG_MODULE_FS, "cleanup userfs instance %d (owner=%d)", 
                         g_userfs_instances[i].instance_id, owner_pid);
            libcm_memset(&g_userfs_instances[i], 0, sizeof(pup_userfs_instance_t));
        }
    }
    
    pup_userfs_unlock();
}

static pup_userfs_instance_t *find_instance_by_path(const char *path)
{
    int i;
    size_t max_len = 0;
    pup_userfs_instance_t *best = PUP_NULL;
    
    for (i = 0; i < PUP_USERFS_MAX_INSTANCES; i++) {
        if (!g_userfs_instances[i].used) continue;
        
        size_t mount_len = libcm_strlen(g_userfs_instances[i].mount_path);
        if (libcm_strncmp(g_userfs_instances[i].mount_path, path, mount_len) == 0) {
            if (mount_len > max_len) {
                max_len = mount_len;
                best = &g_userfs_instances[i];
            }
        }
    }
    
    return best;
}

static const char *get_relative_path(const char *mount_path, const char *full_path)
{
    size_t mount_len = libcm_strlen(mount_path);
    const char *rel = full_path + mount_len;
    
    if (*rel == '/') rel++;
    if (*rel == '\0') return "/";
    return rel;
}

static int userfs_open(struct pup_fd *fd)
{
    pup_userfs_instance_t *inst;
    pup_userfs_open_req_t *req;
    long result;
    
    inst = find_instance_by_path(fd->path);
    if (inst == PUP_NULL) {
        return PUP_FS_ENOENT;
    }
    
    const char *rel_path = get_relative_path(inst->mount_path, fd->path);
    
    req = (pup_userfs_open_req_t *)kheap_shared_malloc(sizeof(pup_userfs_open_req_t));
    if (req == PUP_NULL) {
        return PUP_FS_EIO;
    }
    
    libcm_memset(req, 0, sizeof(pup_userfs_open_req_t));
    req->header.op = PUP_USERFS_OP_OPEN;
    req->header.instance_id = inst->instance_id;
    req->header.fd = -1;
    libcm_strncpy(req->path, rel_path, PUP_USERFS_PATH_MAX - 1);
    req->flags = fd->flags;
    
    result = pup_channel_send(inst->channel_id, req, sizeof(pup_userfs_open_req_t));
    kheap_shared_free(req);
    
    if (result < 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "userfs open failed: %d", (int)result);
        return PUP_FS_EIO;
    }
    
    fd->data = (void *)(long)result;
    
    return 0;
}

static int userfs_close(struct pup_fd *fd)
{
    pup_userfs_instance_t *inst;
    pup_userfs_req_header_t *req;
    long result;
    
    inst = find_instance_by_path(fd->path);
    if (inst == PUP_NULL) {
        return 0;
    }
    
    req = (pup_userfs_req_header_t *)kheap_shared_malloc(sizeof(pup_userfs_req_header_t));
    if (req == PUP_NULL) {
        return 0;
    }
    
    req->op = PUP_USERFS_OP_CLOSE;
    req->instance_id = inst->instance_id;
    req->fd = (int)(long)fd->data;
    
    result = pup_channel_send(inst->channel_id, req, sizeof(pup_userfs_req_header_t));
    kheap_shared_free(req);
    
    return 0;
}

static int userfs_read(struct pup_fd *fd, void *buf, pup_size_t count)
{
    pup_userfs_instance_t *inst;
    pup_userfs_io_req_t *req;
    long result;
    size_t req_size;
    
    inst = find_instance_by_path(fd->path);
    if (inst == PUP_NULL) {
        return PUP_FS_ENOENT;
    }
    
    req_size = sizeof(pup_userfs_io_req_t) + count;
    req = (pup_userfs_io_req_t *)kheap_shared_malloc(req_size);
    if (req == PUP_NULL) {
        return PUP_FS_EIO;
    }
    
    req->header.op = PUP_USERFS_OP_READ;
    req->header.instance_id = inst->instance_id;
    req->header.fd = (int)(long)fd->data;
    req->size = count;
    
    result = pup_channel_send(inst->channel_id, req, req_size);
    
    if (result >= 0) {
        int read_len = (int)result;
        if (read_len > 0 && read_len <= (int)count) {
            libcm_memcpy(buf, req->data_buf, read_len);
        }
        kheap_shared_free(req);
        return read_len;
    }
    
    kheap_shared_free(req);
    return PUP_FS_EIO;
}

static int userfs_write(struct pup_fd *fd, const void *buf, pup_size_t count)
{
    pup_userfs_instance_t *inst;
    pup_userfs_io_req_t *req;
    long result;
    size_t req_size;
    
    inst = find_instance_by_path(fd->path);
    if (inst == PUP_NULL) {
        return PUP_FS_ENOENT;
    }
    
    req_size = sizeof(pup_userfs_io_req_t) + count;
    req = (pup_userfs_io_req_t *)kheap_shared_malloc(req_size);
    if (req == PUP_NULL) {
        return PUP_FS_EIO;
    }
    
    req->header.op = PUP_USERFS_OP_WRITE;
    req->header.instance_id = inst->instance_id;
    req->header.fd = (int)(long)fd->data;
    req->size = count;
    libcm_memcpy(req->data_buf, buf, count);
    
    result = pup_channel_send(inst->channel_id, req, req_size);
    kheap_shared_free(req);
    
    if (result >= 0) {
        return (int)result;
    }
    
    return PUP_FS_EIO;
}

static int userfs_lseek(struct pup_fd *fd, long offset)
{
    pup_userfs_instance_t *inst;
    pup_userfs_lseek_req_t *req;
    long result;
    
    inst = find_instance_by_path(fd->path);
    if (inst == PUP_NULL) {
        return PUP_FS_ENOENT;
    }
    
    req = (pup_userfs_lseek_req_t *)kheap_shared_malloc(sizeof(pup_userfs_lseek_req_t));
    if (req == PUP_NULL) {
        return PUP_FS_EIO;
    }
    
    req->header.op = PUP_USERFS_OP_LSEEK;
    req->header.instance_id = inst->instance_id;
    req->header.fd = (int)(long)fd->data;
    req->offset = offset;
    req->whence = PUP_USERFS_SEEK_SET;
    
    result = pup_channel_send(inst->channel_id, req, sizeof(pup_userfs_lseek_req_t));
    kheap_shared_free(req);
    
    if (result >= 0) {
        return (int)result;
    }
    
    return PUP_FS_EIO;
}

static int userfs_ioctl(struct pup_fd *fd, int cmd, void *args)
{
    return PUP_FS_ENOSYS;
}

static int userfs_flush(struct pup_fd *fd)
{
    pup_userfs_instance_t *inst;
    pup_userfs_req_header_t *req;
    long result;
    
    inst = find_instance_by_path(fd->path);
    if (inst == PUP_NULL) {
        return PUP_FS_ENOENT;
    }
    
    req = (pup_userfs_req_header_t *)kheap_shared_malloc(sizeof(pup_userfs_req_header_t));
    if (req == PUP_NULL) {
        return PUP_FS_EIO;
    }
    
    req->op = PUP_USERFS_OP_SYNC;
    req->instance_id = inst->instance_id;
    req->fd = (int)(long)fd->data;
    
    result = pup_channel_send(inst->channel_id, req, sizeof(pup_userfs_req_header_t));
    kheap_shared_free(req);
    
    if (result >= 0) {
        return (int)result;
    }
    
    return PUP_FS_EIO;
}

static int userfs_getdents(struct pup_fd *fd, struct dirent *dirp, pup_uint32_t count)
{
    pup_userfs_instance_t *inst;
    pup_userfs_getdents_req_t *req;
    pup_userfs_dirent_t *userfs_entries;
    long result;
    char *req_buf;
    size_t req_size;
    int num_entries;
    int i;
    
    inst = find_instance_by_path(fd->path);
    if (inst == PUP_NULL) {
        return PUP_FS_ENOENT;
    }
    
    req_size = sizeof(pup_userfs_getdents_req_t) + sizeof(pup_userfs_dirent_t) * 16;
    req_buf = (char *)kheap_shared_malloc(req_size);
    if (req_buf == PUP_NULL) {
        return PUP_FS_EIO;
    }
    
    req = (pup_userfs_getdents_req_t *)req_buf;
    req->header.op = PUP_USERFS_OP_GETDENTS;
    req->header.instance_id = inst->instance_id;
    req->header.fd = (int)(long)fd->data;
    req->count = count / sizeof(struct dirent);
    
    result = pup_channel_send(inst->channel_id, req_buf, req_size);
    
    if (result >= 0) {
        pup_userfs_resp_header_t *hdr = (pup_userfs_resp_header_t *)req_buf;
        if (hdr->result >= 0) {
            num_entries = hdr->result;
            userfs_entries = (pup_userfs_dirent_t *)(req_buf + sizeof(pup_userfs_resp_header_t));
            
            for (i = 0; i < num_entries && i < (int)(count / sizeof(struct dirent)); i++) {
                dirp[i].d_type = userfs_entries[i].d_type;
                dirp[i].d_namlen = userfs_entries[i].d_namlen;
                dirp[i].d_reclen = sizeof(struct dirent);
                libcm_strncpy(dirp[i].d_name, userfs_entries[i].d_name, PUP_FS_PATH_MAX - 1);
            }
            
            kheap_shared_free(req_buf);
            return i * sizeof(struct dirent);
        }
    }
    
    kheap_shared_free(req_buf);
    return PUP_FS_EIO;
}

static int userfs_stat(struct pup_filesystem *fs, const char *path, struct stat *st)
{
    pup_userfs_instance_t *inst;
    pup_userfs_stat_req_t *req;
    pup_userfs_stat_resp_t *resp;
    long result;
    char *req_buf;
    size_t req_size;
    
    inst = find_instance_by_path(path);
    if (inst == PUP_NULL) {
        return PUP_FS_ENOENT;
    }
    
    const char *rel_path = get_relative_path(inst->mount_path, path);
    
    req_size = sizeof(pup_userfs_stat_req_t) + sizeof(pup_userfs_stat_resp_t);
    req_buf = (char *)kheap_shared_malloc(req_size);
    if (req_buf == PUP_NULL) {
        return PUP_FS_EIO;
    }
    
    req = (pup_userfs_stat_req_t *)req_buf;
    libcm_memset(req, 0, sizeof(pup_userfs_stat_req_t));
    req->header.op = PUP_USERFS_OP_STAT;
    req->header.instance_id = inst->instance_id;
    req->header.fd = -1;
    libcm_strncpy(req->path, rel_path, PUP_USERFS_PATH_MAX - 1);
    
    result = pup_channel_send(inst->channel_id, req_buf, req_size);
    
    if (result >= 0) {
        resp = (pup_userfs_stat_resp_t *)req_buf;
        if (resp->header.result >= 0) {
            st->st_dev = resp->st_dev;
            st->st_ino = resp->st_ino;
            st->st_mode = resp->st_mode;
            st->st_nlink = resp->st_nlink;
            st->st_size = resp->st_size;
            st->st_mtime = resp->st_mtime;
            kheap_shared_free(req_buf);
            return 0;
        }
    }
    
    kheap_shared_free(req_buf);
    return PUP_FS_EIO;
}

static int userfs_unlink(struct pup_filesystem *fs, const char *path)
{
    pup_userfs_instance_t *inst;
    pup_userfs_unlink_req_t *req;
    long result;
    
    inst = find_instance_by_path(path);
    if (inst == PUP_NULL) {
        return PUP_FS_ENOENT;
    }
    
    const char *rel_path = get_relative_path(inst->mount_path, path);
    
    req = (pup_userfs_unlink_req_t *)kheap_shared_malloc(sizeof(pup_userfs_unlink_req_t));
    if (req == PUP_NULL) {
        return PUP_FS_EIO;
    }
    
    libcm_memset(req, 0, sizeof(pup_userfs_unlink_req_t));
    req->header.op = PUP_USERFS_OP_UNLINK;
    req->header.instance_id = inst->instance_id;
    req->header.fd = -1;
    libcm_strncpy(req->path, rel_path, PUP_USERFS_PATH_MAX - 1);
    
    result = pup_channel_send(inst->channel_id, req, sizeof(pup_userfs_unlink_req_t));
    kheap_shared_free(req);
    
    if (result >= 0) {
        return (int)result;
    }
    
    return PUP_FS_EIO;
}

static int userfs_rename(struct pup_filesystem *fs, const char *oldpath, const char *newpath)
{
    pup_userfs_instance_t *inst;
    pup_userfs_rename_req_t *req;
    long result;
    
    inst = find_instance_by_path(oldpath);
    if (inst == PUP_NULL) {
        return PUP_FS_ENOENT;
    }
    
    const char *rel_old = get_relative_path(inst->mount_path, oldpath);
    const char *rel_new = get_relative_path(inst->mount_path, newpath);
    
    req = (pup_userfs_rename_req_t *)kheap_shared_malloc(sizeof(pup_userfs_rename_req_t));
    if (req == PUP_NULL) {
        return PUP_FS_EIO;
    }
    
    libcm_memset(req, 0, sizeof(pup_userfs_rename_req_t));
    req->header.op = PUP_USERFS_OP_RENAME;
    req->header.instance_id = inst->instance_id;
    req->header.fd = -1;
    libcm_strncpy(req->oldpath, rel_old, PUP_USERFS_PATH_MAX - 1);
    libcm_strncpy(req->newpath, rel_new, PUP_USERFS_PATH_MAX - 1);
    
    result = pup_channel_send(inst->channel_id, req, sizeof(pup_userfs_rename_req_t));
    kheap_shared_free(req);
    
    if (result >= 0) {
        return (int)result;
    }
    
    return PUP_FS_EIO;
}

static const struct pup_file_ops _userfs_fops = {
    userfs_open,
    userfs_close,
    userfs_ioctl,
    userfs_read,
    userfs_write,
    userfs_flush,
    userfs_lseek,
    userfs_getdents,
};

static int userfs_mount(struct pup_filesystem *fs, unsigned long rwflag, const void *data)
{
    fs->data = PUP_NULL;
    return 0;
}

static int userfs_unmount(struct pup_filesystem *fs)
{
    pup_userfs_instance_t *inst;
    int i;
    
    pup_userfs_lock();
    
    for (i = 0; i < PUP_USERFS_MAX_INSTANCES; i++) {
        if (g_userfs_instances[i].used && 
            libcm_strcmp(g_userfs_instances[i].mount_path, fs->path) == 0) {
            inst = &g_userfs_instances[i];
            break;
        }
    }
    
    pup_userfs_unlock();
    
    if (inst == PUP_NULL) {
        return PUP_FS_ENOENT;
    }
    
    int channel_id = inst->channel_id;
    int instance_id = inst->instance_id;
    
    pup_userfs_req_header_t *req;
    long result;
    
    req = (pup_userfs_req_header_t *)kheap_shared_malloc(sizeof(pup_userfs_req_header_t));
    if (req == PUP_NULL) {
        return PUP_FS_EIO;
    }
    
    req->op = PUP_USERFS_OP_UNMOUNT;
    req->instance_id = instance_id;
    req->fd = -1;
    
    result = pup_channel_send(channel_id, req, sizeof(pup_userfs_req_header_t));
    kheap_shared_free(req);
    
    if (result < 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "userfs unmount request failed: %d", (int)result);
    }
    
    pup_userfs_lock();
    libcm_memset(inst, 0, sizeof(pup_userfs_instance_t));
    pup_userfs_unlock();
    
    PUP_LOG_INFO(PUP_LOG_MODULE_FS, "userfs instance %d destroyed at '%s'", instance_id, fs->path);
    
    return 0;
}

static const struct pup_fs_ops _userfs = {
    "userfs",
    PUP_FS_FLAG_FULLPATH,
    &_userfs_fops,
    userfs_mount,
    userfs_unmount,
    PUP_NULL,
    PUP_NULL,
    userfs_unlink,
    userfs_stat,
    userfs_rename,
};

int pup_userfs_register_to_vfs(void)
{
    int ret = pup_fs_register(&_userfs);
    if (ret == 0) {
        PUP_LOG_INFO(PUP_LOG_MODULE_FS, "userfs registered to VFS");
    } else {
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "userfs register to VFS failed");
    }
    return ret;
}

int pup_userfs_find_channel_by_type(const char *fs_type)
{
    int i;
    
    if (fs_type == PUP_NULL) {
        return -1;
    }
    
    pup_userfs_lock();
    
    for (i = 0; i < PUP_USERFS_MAX_TYPES; i++) {
        if (g_userfs_types[i].used && 
            libcm_strcmp(g_userfs_types[i].name, fs_type) == 0) {
            int channel_id = g_userfs_types[i].channel_id;
            pup_userfs_unlock();
            return channel_id;
        }
    }
    
    pup_userfs_unlock();
    return -1;
}
