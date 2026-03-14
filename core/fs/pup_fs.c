#include "pup_fs.h"
#include "puppy_log.h"
#include "kheap.h"
#include <libcm.h>

static const struct pup_fs_ops *g_pup_fs_ops_table[PUP_FS_TYPES_MAX];
static struct pup_filesystem g_pup_fs_table[PUP_FS_FILESYSTEMS_MAX];
static struct pup_fd *g_pup_fd_table[PUP_FS_FD_MAX];
static volatile pup_uint32_t g_pup_fs_lock = 0;
static volatile pup_uint32_t g_pup_fd_lock = 0;

static void pup_fs_lock(void)
{
    while (g_pup_fs_lock) {
    }
    g_pup_fs_lock = 1;
}

static void pup_fs_unlock(void)
{
    g_pup_fs_lock = 0;
}

static void pup_lock_fd_table(void)
{
    while (g_pup_fd_lock) {
    }
    g_pup_fd_lock = 1;
}

static void pup_unlock_fd_table(void)
{
    g_pup_fd_lock = 0;
}

int pup_fs_init(void)
{
    static int init_ok = 0;

    if (init_ok)
    {
        PUP_LOG_INFO(PUP_LOG_MODULE_FS, "pup fs already initialized");
        return 0;
    }

    libcm_memset((void *)g_pup_fs_ops_table, 0, sizeof(g_pup_fs_ops_table));
    libcm_memset(g_pup_fs_table, 0, sizeof(g_pup_fs_table));
    libcm_memset(g_pup_fd_table, 0, sizeof(g_pup_fd_table));

    g_pup_fs_lock = 0;
    g_pup_fd_lock = 0;
    init_ok = 1;

    PUP_LOG_INFO(PUP_LOG_MODULE_FS, "PUP FS initialized");
    return 0;
}

int pup_fs_register(const struct pup_fs_ops *ops)
{
    int ret = 0;
    const struct pup_fs_ops **empty = PUP_NULL;
    const struct pup_fs_ops **iter;

    if (ops == PUP_NULL || ops->name == PUP_NULL || ops->fops == PUP_NULL) {
        return PUP_FS_EIO;
    }

    pup_fs_lock();
    for (iter = &g_pup_fs_ops_table[0];
            iter < &g_pup_fs_ops_table[PUP_FS_TYPES_MAX]; iter++)
    {
        if (*iter == PUP_NULL)
            (empty == PUP_NULL) ? (empty = iter) : 0;
        else if (libcm_strcmp((*iter)->name, ops->name) == 0)
        {
            PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "Filesystem %s already registered", ops->name);
            ret = PUP_FS_EFULL;
            break;
        }
    }

    if (empty == PUP_NULL)
    {
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "No space to register filesystem (%s).", ops->name);
        ret = PUP_FS_EFULL;
    }
    else if (ret == 0)
    {
        *empty = ops;
        PUP_LOG_INFO(PUP_LOG_MODULE_FS, "Filesystem %s registered", ops->name);
    }

    pup_fs_unlock();
    return ret;
}

static struct pup_filesystem *pup_fs_lookup_by_path(const char *path)
{
    struct pup_filesystem *iter;
    struct pup_filesystem *fs = PUP_NULL;
    pup_uint32_t fspath, prefixlen;

    prefixlen = 0;

    if (path == PUP_NULL)
        return PUP_NULL;

    pup_fs_lock();

    for (iter = &g_pup_fs_table[0];
            iter < &g_pup_fs_table[PUP_FS_FILESYSTEMS_MAX]; iter++)
    {
        if ((iter->path == PUP_NULL) || (iter->ops == PUP_NULL))
            continue;

        fspath = libcm_strlen(iter->path);
        if ((fspath < prefixlen)
            || (libcm_strncmp(iter->path, path, fspath) != 0))
            continue;

        if (fspath > 1 && (libcm_strlen(path) > fspath) && (path[fspath] != '/'))
            continue;

        fs = iter;
        prefixlen = fspath;
    }

    pup_fs_unlock();

    return fs;
}

static char *pup_path_normalize(const char *directory, const char *filename)
{
    char *fullpath;
    char *dst0, *dst, *src;

    if (filename == PUP_NULL)
        return PUP_NULL;

    if (directory == PUP_NULL && filename[0] != '/')
    {
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "No working directory");
        return PUP_NULL;
    }

    if (filename[0] != '/')
    {
        fullpath = (char *)kheap_malloc(libcm_strlen(directory) + libcm_strlen(filename) + 2);

        if (fullpath == PUP_NULL)
            return PUP_NULL;

        libcm_snprintf(fullpath, libcm_strlen(directory) + libcm_strlen(filename) + 2,
                    "%s/%s", directory, filename);
    }
    else
    {
        fullpath = libcm_strdup(filename);

        if (fullpath == PUP_NULL)
            return PUP_NULL;
    }

    src = fullpath;
    dst = fullpath;

    dst0 = dst;
    while (1)
    {
        char c = *src;

        if (c == '.')
        {
            if (!src[1]) src++;
            else if (src[1] == '/')
            {
                src += 2;

                while ((*src == '/') && (*src != '\0'))
                    src++;
                continue;
            }
            else if (src[1] == '.')
            {
                if (!src[2])
                {
                    src += 2;
                    goto up_one;
                }
                else if (src[2] == '/')
                {
                    src += 3;

                    while ((*src == '/') && (*src != '\0'))
                        src++;
                    goto up_one;
                }
            }
        }

        while ((c = *src++) != '\0' && c != '/')
            *dst++ = c;

        if (c == '/')
        {
            *dst++ = '/';
            while (c == '/')
                c = *src++;

            src--;
        }
        else if (!c)
            break;

        continue;

up_one:
        dst--;
        if (dst < dst0)
        {
            kheap_free(fullpath);
            return PUP_NULL;
        }
        while (dst0 < dst && dst[-1] != '/')
            dst--;
    }

    *dst = '\0';

    dst--;
    if ((dst != fullpath) && (*dst == '/'))
        *dst = '\0';

    if ('\0' == fullpath[0])
    {
        fullpath[0] = '/';
        fullpath[1] = '\0';
    }

    return fullpath;
}

static const char *pup_path_subdir(const char *directory, const char *filename)
{
    const char *dir;

    if (libcm_strlen(directory) == libcm_strlen(filename))
        return PUP_NULL;

    dir = filename + libcm_strlen(directory);
    if ((*dir != '/') && (dir != filename))
    {
        dir--;
    }

    return dir;
}

int pup_fs_mount(const char *device_name,
              const char *path,
              const char *filesystemtype,
              unsigned long rwflag,
              const void *data)
{
    const struct pup_fs_ops **ops;
    struct pup_filesystem *iter;
    struct pup_filesystem *fs = PUP_NULL;
    char *fullpath = PUP_NULL;
    void *dev_id = PUP_NULL;

    PUP_LOG_INFO(PUP_LOG_MODULE_FS, "pup_fs_mount: %s -> %s (%s)", 
                 device_name ? device_name : "NULL", path, filesystemtype);

    if (device_name == PUP_NULL)
    {
        dev_id = PUP_NULL;
    }

    pup_fs_lock();

    for (ops = &g_pup_fs_ops_table[0];
         ops < &g_pup_fs_ops_table[PUP_FS_TYPES_MAX]; ops++)
        if ((*ops != PUP_NULL) && (libcm_strcmp((*ops)->name, filesystemtype) == 0))
            break;

    pup_fs_unlock();

    if (ops == &g_pup_fs_ops_table[PUP_FS_TYPES_MAX])
    {
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "Filesystem type %s not found", filesystemtype);
        return PUP_FS_ENOENT;
    }

    if ((*ops == PUP_NULL) || ((*ops)->mount == PUP_NULL))
    {
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "Filesystem %s has no mount function", filesystemtype);
        return PUP_FS_ENOSYS;
    }

    fullpath = pup_path_normalize(PUP_NULL, path);
    if (fullpath == PUP_NULL)
    {
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "Invalid path: %s", path);
        return PUP_FS_EIO;
    }

    pup_fs_lock();

    for (iter = &g_pup_fs_table[0];
         iter < &g_pup_fs_table[PUP_FS_FILESYSTEMS_MAX]; iter++)
    {
        if (iter->ops == PUP_NULL)
            (fs == PUP_NULL) ? (fs = iter) : 0;
        else if (libcm_strcmp(iter->path, fullpath) == 0)
        {
            PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "Path %s already mounted", path);
            kheap_free(fullpath);
            pup_fs_unlock();
            return PUP_FS_EFULL;
        }
    }

    if ((fs == PUP_NULL) && (iter == &g_pup_fs_table[PUP_FS_FILESYSTEMS_MAX]))
    {
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "No space to mount filesystem (%s).", filesystemtype);
        kheap_free(fullpath);
        pup_fs_unlock();
        return PUP_FS_EFULL;
    }

    fs->path   = fullpath;
    fs->ops    = *ops;
    fs->dev_id = dev_id;
    pup_fs_unlock();

    if ((*ops)->mount(fs, rwflag, data) < 0)
    {
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "Mount %s on %s failed", filesystemtype, path);
        
        pup_fs_lock();
        libcm_memset(fs, 0, sizeof(struct pup_filesystem));
        pup_fs_unlock();
        
        kheap_free(fullpath);
        return PUP_FS_EIO;
    }

    PUP_LOG_INFO(PUP_LOG_MODULE_FS, "Mounted %s on %s successfully", filesystemtype, path);
    return 0;
}

int pup_fs_unmount(const char *specialfile)
{
    char *fullpath;
    struct pup_filesystem *iter;
    struct pup_filesystem *fs = PUP_NULL;

    fullpath = pup_path_normalize(PUP_NULL, specialfile);
    if (fullpath == PUP_NULL)
    {
        return PUP_FS_EIO;
    }

    pup_fs_lock();

    for (iter = &g_pup_fs_table[0];
         iter < &g_pup_fs_table[PUP_FS_FILESYSTEMS_MAX]; iter++)
    {
        if ((iter->path != PUP_NULL) && (libcm_strcmp(iter->path, fullpath) == 0))
        {
            fs = iter;
            break;
        }
    }

    if (fs == PUP_NULL ||
        fs->ops->unmount == PUP_NULL ||
        fs->ops->unmount(fs) < 0)
    {
        pup_fs_unlock();
        kheap_free(fullpath);
        return PUP_FS_EIO;
    }

    if (fs->path != PUP_NULL)
        kheap_free(fs->path);

    libcm_memset(fs, 0, sizeof(struct pup_filesystem));

    pup_fs_unlock();
    kheap_free(fullpath);

    return 0;
}

int pup_fs_statfs(const char *path, struct statfs *buffer)
{
    struct pup_filesystem *fs;

    fs = pup_fs_lookup_by_path(path);
    if (fs != PUP_NULL)
    {
        if (fs->ops->statfs != PUP_NULL)
            return fs->ops->statfs(fs, buffer);
    }

    return PUP_FS_EIO;
}

static int pup_fd_alloc_slot(void)
{
    int idx;

    pup_lock_fd_table();

    for (idx = PUP_FS_STD_FD_COUNT; idx < PUP_FS_FD_MAX; idx++)
    {
        if (g_pup_fd_table[idx] == PUP_NULL)
            break;
    }

    if (idx < PUP_FS_FD_MAX)
    {
        g_pup_fd_table[idx] = (struct pup_fd *)kheap_calloc(1, sizeof(struct pup_fd));
        if (g_pup_fd_table[idx] != PUP_NULL)
        {
            g_pup_fd_table[idx]->magic = PUP_FD_MAGIC;
            g_pup_fd_table[idx]->ref_count = 1;
        }
        else
        {
            idx = -1;
        }
    }
    else
    {
        idx = -1;
    }

    pup_unlock_fd_table();
    return idx;
}

static void pup_fd_free_slot(int fd)
{
    if (fd < 0 || fd >= PUP_FS_FD_MAX)
        return;

    pup_lock_fd_table();

    if (g_pup_fd_table[fd] != PUP_NULL)
    {
        g_pup_fd_table[fd]->ref_count--;
        if (g_pup_fd_table[fd]->ref_count == 0)
        {
            kheap_free(g_pup_fd_table[fd]);
            g_pup_fd_table[fd] = PUP_NULL;
        }
    }

    pup_unlock_fd_table();
}

struct pup_fd *pup_fd_get(int fd)
{
    struct pup_fd *d;

    if (fd < 0 || fd >= PUP_FS_FD_MAX)
        return PUP_NULL;

    pup_lock_fd_table();
    d = g_pup_fd_table[fd];

    if ((d == PUP_NULL) || (d->magic != PUP_FD_MAGIC))
    {
        pup_unlock_fd_table();
        return PUP_NULL;
    }

    d->ref_count++;
    pup_unlock_fd_table();

    return d;
}

static void pup_fd_put(int fd, struct pup_fd *fd_obj)
{
    if (fd < 0 || fd >= PUP_FS_FD_MAX || fd_obj == PUP_NULL) {
        return;
    }

    pup_lock_fd_table();
    fd_obj->ref_count--;
    if (fd_obj->ref_count == 0) {
        kheap_free(fd_obj);
        g_pup_fd_table[fd] = PUP_NULL;
    }
    pup_unlock_fd_table();
}

int pup_open(const char *path, int flags)
{
    struct pup_filesystem *fs;
    struct pup_fd *fd;
    char *fullpath;
    int result;
    int fd_idx;

    fullpath = pup_path_normalize(PUP_NULL, path);
    if (fullpath == PUP_NULL)
    {
        return PUP_FS_ENOENT;
    }

    PUP_LOG_DEBUG(PUP_LOG_MODULE_FS, "open file:%s", fullpath);

    fs = pup_fs_lookup_by_path(fullpath);
    if (fs == PUP_NULL)
    {
        kheap_free(fullpath);
        return PUP_FS_ENOENT;
    }

    fd_idx = pup_fd_alloc_slot();
    if (fd_idx < 0)
    {
        kheap_free(fullpath);
        return PUP_FS_EFULL;
    }

    fd = g_pup_fd_table[fd_idx];

    PUP_LOG_DEBUG(PUP_LOG_MODULE_FS, "open in filesystem:%s", fs->ops->name);
    fd->fs    = fs;
    fd->fops  = fs->ops->fops;

    fd->type  = FT_REGULAR;
    fd->flags = flags;
    fd->size  = 0;
    fd->pos   = 0;
    fd->data  = fs;

    if (!(fs->ops->flags & PUP_FS_FLAG_FULLPATH))
    {
        if (pup_path_subdir(fs->path, fullpath) == PUP_NULL)
            fd->path = libcm_strdup("/");
        else
            fd->path = libcm_strdup(pup_path_subdir(fs->path, fullpath));
        kheap_free(fullpath);
        PUP_LOG_DEBUG(PUP_LOG_MODULE_FS, "Actual file path: %s", fd->path);
    }
    else
    {
        fd->path = fullpath;
    }

    if (fd->path == PUP_NULL) {
        pup_fd_free_slot(fd_idx);
        return PUP_FS_EIO;
    }

    if (fd->fops->open == PUP_NULL)
    {
        kheap_free(fd->path);
        fd->path = PUP_NULL;
        pup_fd_free_slot(fd_idx);
        return PUP_FS_ENOSYS;
    }

    if ((result = fd->fops->open(fd)) < 0)
    {
        kheap_free(fd->path);
        fd->path = PUP_NULL;
        pup_fd_free_slot(fd_idx);
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "open failed, result=%d", result);
        return result;
    }

    if (flags & O_DIRECTORY)
    {
        fd->type = FT_DIRECTORY;
    }

    PUP_LOG_DEBUG(PUP_LOG_MODULE_FS, "open successful, fd=%d", fd_idx);
    return fd_idx;
}

int pup_close(int fd)
{
    struct pup_fd *fd_obj;
    int result = 0;

    fd_obj = pup_fd_get(fd);
    if (fd_obj == PUP_NULL)
        return PUP_FS_EFULL;

    if (fd_obj->fops->close != PUP_NULL)
        result = fd_obj->fops->close(fd_obj);

    if (fd_obj->path != PUP_NULL)
    {
        kheap_free(fd_obj->path);
        fd_obj->path = PUP_NULL;
    }

    pup_fd_put(fd, fd_obj);

    return result;
}

int pup_read(int fd, void *buf, pup_size_t len)
{
    struct pup_fd *fd_obj;
    int result = 0;

    fd_obj = pup_fd_get(fd);
    if (fd_obj == PUP_NULL)
        return PUP_FS_EFULL;

    if (fd_obj->fops->read == PUP_NULL)
    {
        pup_fd_put(fd, fd_obj);
        return PUP_FS_ENOSYS;
    }

    result = fd_obj->fops->read(fd_obj, buf, len);
    pup_fd_put(fd, fd_obj);
    return result;
}

int pup_write(int fd, const void *buf, pup_size_t len)
{
    struct pup_fd *fd_obj;
    int result;

    fd_obj = pup_fd_get(fd);
    if (fd_obj == PUP_NULL)
        return PUP_FS_EFULL;

    if (fd_obj->fops->write == PUP_NULL)
    {
        pup_fd_put(fd, fd_obj);
        return PUP_FS_ENOSYS;
    }

    result = fd_obj->fops->write(fd_obj, buf, len);
    pup_fd_put(fd, fd_obj);
    return result;
}

int pup_ioctl(int fd, int cmd, void *args)
{
    struct pup_fd *fd_obj;
    int result;

    fd_obj = pup_fd_get(fd);
    if (fd_obj == PUP_NULL)
        return PUP_FS_EFULL;

    if (fd_obj->fops->ioctl == PUP_NULL) {
        pup_fd_put(fd, fd_obj);
        return PUP_FS_ENOSYS;
    }

    result = fd_obj->fops->ioctl(fd_obj, cmd, args);
    pup_fd_put(fd, fd_obj);
    return result;
}

long pup_lseek(int fd, long offset, int whence)
{
    struct pup_fd *fd_obj;
    long result;

    fd_obj = pup_fd_get(fd);
    if (fd_obj == PUP_NULL)
        return PUP_FS_EFULL;

    if (fd_obj->fops->lseek == PUP_NULL)
    {
        pup_fd_put(fd, fd_obj);
        return PUP_FS_ENOSYS;
    }

    long new_offset;
    switch (whence)
    {
    case SEEK_SET:
        new_offset = offset;
        break;
    case SEEK_CUR:
        new_offset = fd_obj->pos + offset;
        break;
    case SEEK_END:
        new_offset = fd_obj->size + offset;
        break;
    default:
        pup_fd_put(fd, fd_obj);
        return PUP_FS_EFULL;
    }

    result = fd_obj->fops->lseek(fd_obj, new_offset);

    if (result >= 0)
        fd_obj->pos = result;

    pup_fd_put(fd, fd_obj);
    return result;
}

int pup_fsync(int fd)
{
    struct pup_fd *fd_obj;
    int result;

    fd_obj = pup_fd_get(fd);
    if (fd_obj == PUP_NULL)
        return PUP_FS_EFULL;

    if (fd_obj->fops->flush == PUP_NULL)
    {
        pup_fd_put(fd, fd_obj);
        return PUP_FS_ENOSYS;
    }

    result = fd_obj->fops->flush(fd_obj);
    pup_fd_put(fd, fd_obj);
    return result;
}

int pup_getdents(int fd, struct dirent *dirp, pup_size_t nbytes)
{
    struct pup_fd *fd_obj;
    int result;

    fd_obj = pup_fd_get(fd);
    if (fd_obj == PUP_NULL || fd_obj->type != FT_DIRECTORY)
        return PUP_FS_EFULL;

    if (fd_obj->fops->getdents == PUP_NULL) {
        pup_fd_put(fd, fd_obj);
        return PUP_FS_ENOSYS;
    }

    result = fd_obj->fops->getdents(fd_obj, dirp, nbytes);
    pup_fd_put(fd, fd_obj);
    return result;
}

int pup_stat(const char *path, struct stat *buf)
{
    int result;
    char *fullpath;
    struct pup_filesystem *fs;

    fullpath = pup_path_normalize(PUP_NULL, path);
    if (fullpath == PUP_NULL)
    {
        return PUP_FS_EIO;
    }

    if ((fs = pup_fs_lookup_by_path(fullpath)) == PUP_NULL)
    {
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "can't find mounted filesystem on path:%s", fullpath);
        kheap_free(fullpath);
        return PUP_FS_ENOENT;
    }

    if ((fullpath[0] == '/' && fullpath[1] == '\0') ||
        (pup_path_subdir(fs->path, fullpath) == PUP_NULL))
    {
        buf->st_dev   = 0;

        buf->st_mode  = S_IRUSR | S_IRGRP | S_IROTH |
                        S_IWUSR | S_IWGRP | S_IWOTH;
        buf->st_mode |= S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;

        buf->st_size    = 0;
        buf->st_mtime   = 0;

        kheap_free(fullpath);
        return 0;
    }
    else
    {
        if (fs->ops->stat == PUP_NULL)
        {
            kheap_free(fullpath);
            PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "filesystem didn't implement stat");
            return PUP_FS_ENOSYS;
        }

        if (fs->ops->flags & PUP_FS_FLAG_FULLPATH)
            result = fs->ops->stat(fs, fullpath, buf);
        else
            result = fs->ops->stat(fs, pup_path_subdir(fs->path, fullpath), buf);
    }

    kheap_free(fullpath);

    return result;
}

int pup_unlink(const char *path)
{
    int result;
    char *fullpath;
    struct pup_filesystem *fs;

    fullpath = pup_path_normalize(PUP_NULL, path);
    if (fullpath == PUP_NULL)
    {
        return PUP_FS_EIO;
    }

    if ((fs = pup_fs_lookup_by_path(fullpath)) == PUP_NULL)
    {
        result = PUP_FS_ENOENT;
        goto __exit;
    }

    if (fs->ops->unlink != PUP_NULL)
    {
        if (!(fs->ops->flags & PUP_FS_FLAG_FULLPATH))
        {
            if (pup_path_subdir(fs->path, fullpath) == PUP_NULL)
                result = fs->ops->unlink(fs, "/");
            else
                result = fs->ops->unlink(fs, pup_path_subdir(fs->path, fullpath));
        }
        else
            result = fs->ops->unlink(fs, fullpath);
    }
    else result = PUP_FS_ENOSYS;

__exit:
    kheap_free(fullpath);
    return result;
}

int pup_rename(const char *oldpath, const char *newpath)
{
    int result;
    struct pup_filesystem *oldfs, *newfs;
    char *oldfullpath, *newfullpath;

    result = 0;
    newfullpath = PUP_NULL;
    oldfullpath = PUP_NULL;

    oldfullpath = pup_path_normalize(PUP_NULL, oldpath);
    if (oldfullpath == PUP_NULL)
    {
        result = PUP_FS_ENOENT;
        goto __exit;
    }

    newfullpath = pup_path_normalize(PUP_NULL, newpath);
    if (newfullpath == PUP_NULL)
    {
        result = PUP_FS_ENOENT;
        goto __exit;
    }

    oldfs = pup_fs_lookup_by_path(oldfullpath);
    newfs = pup_fs_lookup_by_path(newfullpath);

    if (oldfs == newfs)
    {
        if (oldfs->ops->rename == PUP_NULL)
        {
            result = PUP_FS_ENOSYS;
        }
        else
        {
            if (oldfs->ops->flags & PUP_FS_FLAG_FULLPATH)
                result = oldfs->ops->rename(oldfs, oldfullpath, newfullpath);
            else
                result = oldfs->ops->rename(oldfs,
                                            pup_path_subdir(oldfs->path, oldfullpath),
                                            pup_path_subdir(newfs->path, newfullpath));
        }
    }
    else
    {
        result = PUP_FS_EXDEV;
    }

__exit:
    kheap_free(oldfullpath);
    kheap_free(newfullpath);

    return result;
}
