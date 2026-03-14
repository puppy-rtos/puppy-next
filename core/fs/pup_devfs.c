#include "pup_fs.h"
#include "pup_devfs.h"
#include "puppy_log.h"
#include "kheap.h"
#include <libcm.h>

#define PUP_DEVFS_MAX_DEVICES 16

static const pup_devfs_device_t *g_pup_devfs_device_table[PUP_DEVFS_MAX_DEVICES];
static int g_pup_devfs_device_count = 0;
static int g_pup_devfs_registry_initialized = 0;

struct device_dirent
{
    pup_uint16_t read_index;
    pup_uint16_t device_count;
};

int pup_devfs_registry_init(void)
{
    if (g_pup_devfs_registry_initialized) {
        return PUP_DEVFS_OK;
    }

    libcm_memset(g_pup_devfs_device_table, 0, sizeof(g_pup_devfs_device_table));
    g_pup_devfs_device_count = 0;
    g_pup_devfs_registry_initialized = 1;
    return PUP_DEVFS_OK;
}

int pup_devfs_register_device(const pup_devfs_device_t *device)
{
    int i;

    if (!device || !device->name || !device->ops) {
        return PUP_DEVFS_EINVAL;
    }

    if (!g_pup_devfs_registry_initialized) {
        return PUP_DEVFS_ERROR;
    }

    if (g_pup_devfs_device_count >= PUP_DEVFS_MAX_DEVICES) {
        return PUP_DEVFS_ENOMEM;
    }

    for (i = 0; i < g_pup_devfs_device_count; i++) {
        if (libcm_strcmp(g_pup_devfs_device_table[i]->name, device->name) == 0) {
            return PUP_DEVFS_EINVAL;
        }
    }

    g_pup_devfs_device_table[g_pup_devfs_device_count++] = device;
    return PUP_DEVFS_OK;
}

int pup_devfs_unregister_device(const char *name)
{
    int i;
    int j;

    if (!name) {
        return PUP_DEVFS_EINVAL;
    }

    for (i = 0; i < g_pup_devfs_device_count; i++) {
        if (libcm_strcmp(g_pup_devfs_device_table[i]->name, name) == 0) {
            for (j = i; j < g_pup_devfs_device_count - 1; j++) {
                g_pup_devfs_device_table[j] = g_pup_devfs_device_table[j + 1];
            }
            g_pup_devfs_device_count--;
            return PUP_DEVFS_OK;
        }
    }

    return PUP_DEVFS_ENOENT;
}

const pup_devfs_device_t *pup_devfs_find_device(const char *name)
{
    int i;
    const char *dev_name;

    if (!name) {
        return PUP_NULL;
    }

    dev_name = name;
    if (libcm_strncmp(name, "/dev/", 5) == 0) {
        dev_name = name + 5;
    }

    for (i = 0; i < g_pup_devfs_device_count; i++) {
        if (libcm_strcmp(g_pup_devfs_device_table[i]->name, dev_name) == 0) {
            return g_pup_devfs_device_table[i];
        }
    }

    return PUP_NULL;
}

int pup_devfs_get_device_count(void)
{
    return g_pup_devfs_device_count;
}

const char *pup_devfs_get_device_name(int index)
{
    if (index < 0 || index >= g_pup_devfs_device_count) {
        return PUP_NULL;
    }
    return g_pup_devfs_device_table[index]->name;
}

void pup_devfs_list_devices(void)
{
    int i;

    PUP_PRINTK("Device List:\n");
    PUP_PRINTK("  Type    Name\n");
    PUP_PRINTK("  ------  ------\n");

    for (i = 0; i < g_pup_devfs_device_count; i++) {
        PUP_PRINTK("  device  /dev/%s\n", g_pup_devfs_device_table[i]->name);
    }

    PUP_PRINTK("Total: %d device(s)\n", g_pup_devfs_device_count);
}

static int pup_devfs_mount(struct pup_filesystem *fs, unsigned long rwflag, const void *data)
{
    return 0;
}

static int pup_devfs_ioctl(struct pup_fd *file, int cmd, void *args)
{
    pup_devfs_file_t *pfile = (pup_devfs_file_t *)file->data;
    if (pfile && pfile->ops && pfile->ops->ioctl)
        return pfile->ops->ioctl(pfile, cmd, args);
    return -1;
}

static int pup_devfs_read(struct pup_fd *file, void *buf, pup_size_t count)
{
    pup_devfs_file_t *pfile = (pup_devfs_file_t *)file->data;
    if (pfile && pfile->ops && pfile->ops->read)
        return pfile->ops->read(pfile, (char *)buf, (int)count);
    return -1;
}

static int pup_devfs_write(struct pup_fd *file, const void *buf, pup_size_t count)
{
    pup_devfs_file_t *pfile = (pup_devfs_file_t *)file->data;
    if (pfile && pfile->ops && pfile->ops->write)
        return pfile->ops->write(pfile, (const char *)buf, (int)count);
    return -1;
}

static int pup_devfs_close(struct pup_fd *file)
{
    if (file->type == FT_DIRECTORY)
    {
        kheap_free(file->data);
        return 0;
    }
    
    pup_devfs_file_t *pfile = (pup_devfs_file_t *)file->data;
    if (pfile)
    {
        if (pfile->ops && pfile->ops->close)
            pfile->ops->close(pfile);
        kheap_free(pfile);
        file->data = PUP_NULL;
    }
    return 0;
}

static int pup_devfs_open(struct pup_fd *file)
{
    PUP_LOG_DEBUG(PUP_LOG_MODULE_CORE, "pup_devfs open: path=%s", file->path);

    if ((file->path[0] == '/') && (file->path[1] == '\0') &&
        (file->flags & O_DIRECTORY))
    {
        struct device_dirent *root_dirent;
        int count = pup_devfs_get_device_count();
        
        root_dirent = (struct device_dirent *)kheap_malloc(sizeof(struct device_dirent));
        if (root_dirent != PUP_NULL)
        {
            root_dirent->read_index = 0;
            root_dirent->device_count = (pup_uint16_t)count;
        }
        file->data = root_dirent;
        return 0;
    }

    const char *dev_name = file->path;
    if (dev_name[0] == '/')
        dev_name++;

    PUP_LOG_DEBUG(PUP_LOG_MODULE_CORE, "pup_devfs opening device: %s", dev_name);

    const pup_devfs_device_t *device = pup_devfs_find_device(dev_name);
    if (device == PUP_NULL)
    {
        PUP_LOG_ERROR(PUP_LOG_MODULE_CORE, "pup_devfs: device not found: %s", dev_name);
        return -2;
    }

    pup_devfs_file_t *pfile = (pup_devfs_file_t *)kheap_malloc(sizeof(pup_devfs_file_t));
    if (pfile == PUP_NULL)
        return -5;
    
    pfile->ops = device->ops;
    pfile->device = device;
    pfile->flags = file->flags;
    pfile->private_data = (void *)device->private_data;
    pfile->ref_count = 1;
    
    if (device->ops && device->ops->open)
    {
        int ret = device->ops->open(pfile);
        if (ret < 0)
        {
            kheap_free(pfile);
            PUP_LOG_ERROR(PUP_LOG_MODULE_CORE, "device open failed: %s", dev_name);
            return ret;
        }
    }

    file->data = pfile;
    file->type = FT_DEVICE;
    return 0;
}

static int pup_devfs_stat(struct pup_filesystem *fs, const char *path, struct stat *st)
{
    if ((path[0] == '/') && (path[1] == '\0'))
    {
        st->st_dev = 0;
        st->st_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH |
                      S_IWUSR | S_IWGRP | S_IWOTH |
                      S_IXUSR | S_IXGRP | S_IXOTH;
        st->st_size  = 0;
        st->st_mtime = 0;
        return 0;
    }
    else
    {
        const char *dev_name = path;
        if (dev_name[0] == '/')
            dev_name++;
        
        if (pup_devfs_find_device(dev_name) == PUP_NULL)
            return -2;
            
        st->st_dev = 0;
        st->st_mode = S_IFCHR | S_IRUSR | S_IRGRP | S_IROTH |
                      S_IWUSR | S_IWGRP | S_IWOTH;
        st->st_size  = 0;
        st->st_mtime = 0;
        return 0;
    }
}

static int pup_devfs_getdents(struct pup_fd *file, struct dirent *dirp, pup_uint32_t count)
{
    struct device_dirent *root_dirent;
    pup_uint32_t index;
    struct dirent *d;
    const char *name;

    root_dirent = (struct device_dirent *)file->data;
    if (root_dirent == PUP_NULL)
        return -1;

    count = (count / sizeof(struct dirent));
    if (count == 0)
        return -1;

    for (index = 0; index < count && root_dirent->read_index < root_dirent->device_count; index++)
    {
        name = pup_devfs_get_device_name(root_dirent->read_index);
        if (name == PUP_NULL)
            break;

        d = dirp + index;
        d->d_type = DT_CHR;
        d->d_namlen = (pup_uint8_t)libcm_strlen(name);
        d->d_reclen = (pup_uint16_t)sizeof(struct dirent);
        libcm_strncpy(d->d_name, name, d->d_namlen + 1);

        root_dirent->read_index++;
    }

    return index * sizeof(struct dirent);
}

static const struct pup_file_ops _device_fops =
{
    pup_devfs_open,
    pup_devfs_close,
    pup_devfs_ioctl,
    pup_devfs_read,
    pup_devfs_write,
    PUP_NULL,
    PUP_NULL,
    pup_devfs_getdents,
};

static const struct pup_fs_ops _device_fs =
{
    "pup_devfs",
    PUP_FS_FLAG_DEFAULT,
    &_device_fops,

    pup_devfs_mount,
    PUP_NULL,
    PUP_NULL,
    PUP_NULL,

    PUP_NULL,
    pup_devfs_stat,
    PUP_NULL,
};

int pup_devfs_init(void)
{
    pup_devfs_registry_init();

    int ret = pup_fs_register(&_device_fs);
    if (ret == 0)
    {
        PUP_LOG_INFO(PUP_LOG_MODULE_CORE, "pup_devfs initialized");
    }
    else
    {
        PUP_LOG_ERROR(PUP_LOG_MODULE_CORE, "pup_devfs init failed");
    }
    return ret;
}
