#include "pup_fs.h"
#include "pup_devfs.h"
#include "puppy_log.h"
#include "kheap.h"
#include <libcm.h>
#include "pup_channel.h"

#define PUP_DEVFS_MAX_DEVICES 16

typedef struct {
    int used;
    char name[PUP_DEVFS_MAX_NAME_LEN + 1];
    pup_devfs_device_t device;
} pup_devfs_dynamic_device_t;

static const pup_devfs_device_t *g_pup_devfs_device_table[PUP_DEVFS_MAX_DEVICES];
static pup_devfs_dynamic_device_t g_pup_devfs_dynamic_devices[PUP_DEVFS_MAX_DEVICES];
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
    libcm_memset(g_pup_devfs_dynamic_devices, 0, sizeof(g_pup_devfs_dynamic_devices));
    g_pup_devfs_device_count = 0;
    g_pup_devfs_registry_initialized = 1;
    return PUP_DEVFS_OK;
}

static pup_devfs_dynamic_device_t* pup_devfs_find_free_dynamic_slot(void)
{
    int i;

    for (i = 0; i < PUP_DEVFS_MAX_DEVICES; i++) {
        if (!g_pup_devfs_dynamic_devices[i].used) {
            return &g_pup_devfs_dynamic_devices[i];
        }
    }

    return PUP_NULL;
}

static void pup_devfs_release_dynamic_device(const pup_devfs_device_t *device)
{
    int i;

    for (i = 0; i < PUP_DEVFS_MAX_DEVICES; i++) {
        if (g_pup_devfs_dynamic_devices[i].used &&
            &g_pup_devfs_dynamic_devices[i].device == device) {
            libcm_memset(&g_pup_devfs_dynamic_devices[i], 0, sizeof(g_pup_devfs_dynamic_devices[i]));
            return;
        }
    }
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
    const pup_devfs_device_t *device;

    if (!name) {
        return PUP_DEVFS_EINVAL;
    }

    for (i = 0; i < g_pup_devfs_device_count; i++) {
        if (libcm_strcmp(g_pup_devfs_device_table[i]->name, name) == 0) {
            device = g_pup_devfs_device_table[i];
            for (j = i; j < g_pup_devfs_device_count - 1; j++) {
                g_pup_devfs_device_table[j] = g_pup_devfs_device_table[j + 1];
            }
            g_pup_devfs_device_count--;
            g_pup_devfs_device_table[g_pup_devfs_device_count] = PUP_NULL;

            if (device->source_type == PUP_DEVFS_DEVICE_SOURCE_USER) {
                pup_devfs_release_dynamic_device(device);
            }

            return PUP_DEVFS_OK;
        }
    }

    return PUP_DEVFS_ENOENT;
}

int pup_devfs_register_user_device(const pup_user_devfs_registration_t *registration, int owner_pid)
{
    pup_devfs_dynamic_device_t *slot;
    int i;

    if (!registration || !registration->name || owner_pid <= 0) {
        return PUP_DEVFS_EINVAL;
    }

    if (!g_pup_devfs_registry_initialized) {
        return PUP_DEVFS_ERROR;
    }

    if (g_pup_devfs_device_count >= PUP_DEVFS_MAX_DEVICES) {
        return PUP_DEVFS_ENOMEM;
    }

    if (libcm_strlen(registration->name) == 0 ||
        libcm_strlen(registration->name) > PUP_DEVFS_MAX_NAME_LEN) {
        return PUP_DEVFS_EINVAL;
    }

    if (pup_devfs_find_device(registration->name) != PUP_NULL) {
        return PUP_DEVFS_EINVAL;
    }

    if (registration->device_id < 0) {
        return PUP_DEVFS_EINVAL;
    }

    for (i = 0; i < g_pup_devfs_device_count; i++) {
        const pup_devfs_device_t *dev = g_pup_devfs_device_table[i];
        if (dev && dev->source_type == PUP_DEVFS_DEVICE_SOURCE_USER
            && dev->owner_pid == owner_pid
            && dev->device_id == registration->device_id) {
            return PUP_DEVFS_EINVAL;
        }
    }

    slot = pup_devfs_find_free_dynamic_slot();
    if (!slot) {
        return PUP_DEVFS_ENOMEM;
    }

    libcm_memset(slot, 0, sizeof(*slot));
    slot->used = 1;
    libcm_strncpy(slot->name, registration->name, PUP_DEVFS_MAX_NAME_LEN);
    slot->name[PUP_DEVFS_MAX_NAME_LEN] = '\0';

    slot->device.name = slot->name;
    slot->device.ops = PUP_NULL;
    slot->device.private_data = PUP_NULL;
    slot->device.owner_pid = owner_pid;
    slot->device.channel_id = registration->channel_id;
    slot->device.device_id = registration->device_id;
    slot->device.source_type = PUP_DEVFS_DEVICE_SOURCE_USER;
    slot->device.flags = registration->flags | PUP_DRIVER_DEVICE_FLAG_DYNAMIC | PUP_DRIVER_DEVICE_FLAG_ONLINE;
    slot->device.capabilities = registration->capabilities;

    g_pup_devfs_device_table[g_pup_devfs_device_count++] = &slot->device;

    PUP_LOG_INFO(PUP_LOG_MODULE_CORE,
                 "pup_devfs: registered user device /dev/%s (owner pid=%d)",
                 slot->device.name,
                 owner_pid);
    return PUP_DEVFS_OK;
}

int pup_devfs_unregister_user_device(const char *name, int owner_pid)
{
    const pup_devfs_device_t *device = pup_devfs_find_device(name);

    if (!device || device->source_type != PUP_DEVFS_DEVICE_SOURCE_USER) {
        return PUP_DEVFS_ENOENT;
    }

    if (device->owner_pid != owner_pid) {
        return PUP_DEVFS_EINVAL;
    }

    return pup_devfs_unregister_device(device->name);
}

int pup_devfs_cleanup_owner_devices(int owner_pid)
{
    int removed = 0;
    int i = 0;

    if (owner_pid <= 0) {
        return 0;
    }

    while (i < g_pup_devfs_device_count) {
        const pup_devfs_device_t *device = g_pup_devfs_device_table[i];

        if (device && device->source_type == PUP_DEVFS_DEVICE_SOURCE_USER && device->owner_pid == owner_pid) {
            char dev_name[PUP_DEVFS_MAX_NAME_LEN + 1];

            libcm_strncpy(dev_name, device->name, PUP_DEVFS_MAX_NAME_LEN);
            dev_name[PUP_DEVFS_MAX_NAME_LEN] = '\0';
            pup_devfs_unregister_device(dev_name);
            removed++;
            continue;
        }

        i++;
    }

    if (removed > 0) {
        PUP_LOG_INFO(PUP_LOG_MODULE_CORE,
                     "pup_devfs: cleaned %d user device(s) for owner pid=%d",
                     removed,
                     owner_pid);
    }

    return removed;
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
        const pup_devfs_device_t *device = g_pup_devfs_device_table[i];
        const char *type = (device->source_type == PUP_DEVFS_DEVICE_SOURCE_USER) ? "user" : "device";
        PUP_PRINTK("  %-6s /dev/%s\n", type, device->name);
    }

    PUP_PRINTK("Total: %d device(s)\n", g_pup_devfs_device_count);
}

static int pup_devfs_mount(struct pup_filesystem *fs, unsigned long rwflag, const void *data)
{
    return 0;
}

/* -----------------------------------------------------------------------
 * Channel-based proxy ops – used for user-mode registered devices.
 * The kernel DevFS side allocates a pup_devfs_proxy_payload_t in shared
 * memory, sends it via pup_channel_send(), and waits for the driver to
 * call msg_reply() with the result.
 * --------------------------------------------------------------------- */

static int proxy_open(pup_devfs_file_t *file)
{
    int ch = file->device->channel_id;
    pup_devfs_proxy_payload_t *payload;
    long result;

    payload = (pup_devfs_proxy_payload_t *)kheap_shared_malloc(sizeof(*payload));
    if (!payload) {
        return -5;
    }
    payload->op       = DEVFS_PROXY_OP_OPEN;
    payload->device_id = file->device->device_id;
    payload->flags    = file->flags;
    payload->cmd      = 0;
    payload->data_buf = PUP_NULL;
    payload->data_len = 0;

    result = pup_channel_send(ch, payload, sizeof(*payload));
    kheap_shared_free(payload);
    return (int)result;
}

static int proxy_close(pup_devfs_file_t *file)
{
    int ch = file->device->channel_id;
    pup_devfs_proxy_payload_t *payload;

    if (!pup_channel_is_valid(ch)) {
        return 0;
    }
    payload = (pup_devfs_proxy_payload_t *)kheap_shared_malloc(sizeof(*payload));
    if (!payload) {
        return 0;
    }
    payload->op       = DEVFS_PROXY_OP_CLOSE;
    payload->device_id = file->device->device_id;
    payload->flags    = 0;
    payload->cmd      = 0;
    payload->data_buf = PUP_NULL;
    payload->data_len = 0;

    pup_channel_send(ch, payload, sizeof(*payload));
    kheap_shared_free(payload);
    return 0;
}

static int proxy_read(pup_devfs_file_t *file, char *buf, int len)
{
    int ch = file->device->channel_id;
    pup_devfs_proxy_payload_t *payload;
    void *data_buf = PUP_NULL;
    long result;

    if (len <= 0) {
        return 0;
    }
    payload = (pup_devfs_proxy_payload_t *)kheap_shared_malloc(sizeof(*payload));
    if (!payload) {
        return -5;
    }
    data_buf = kheap_shared_malloc((pup_size_t)len);
    if (!data_buf) {
        kheap_shared_free(payload);
        return -5;
    }

    payload->op       = DEVFS_PROXY_OP_READ;
    payload->device_id = file->device->device_id;
    payload->flags    = 0;
    payload->cmd      = 0;
    payload->data_buf = data_buf;
    payload->data_len = len;

    result = pup_channel_send(ch, payload, sizeof(*payload));

    if (result > 0 && buf) {
        libcm_memcpy(buf, data_buf, (int)result);
    }

    kheap_shared_free(data_buf);
    kheap_shared_free(payload);
    return (int)result;
}

static int proxy_write(pup_devfs_file_t *file, const char *buf, int len)
{
    int ch = file->device->channel_id;
    pup_devfs_proxy_payload_t *payload;
    void *data_buf = PUP_NULL;
    long result;

    if (len <= 0) {
        return 0;
    }
    payload = (pup_devfs_proxy_payload_t *)kheap_shared_malloc(sizeof(*payload));
    if (!payload) {
        return -5;
    }
    data_buf = kheap_shared_malloc((pup_size_t)len);
    if (!data_buf) {
        kheap_shared_free(payload);
        return -5;
    }
    if (buf) {
        libcm_memcpy(data_buf, buf, len);
    }

    payload->op       = DEVFS_PROXY_OP_WRITE;
    payload->device_id = file->device->device_id;
    payload->flags    = 0;
    payload->cmd      = 0;
    payload->data_buf = data_buf;
    payload->data_len = len;

    result = pup_channel_send(ch, payload, sizeof(*payload));

    kheap_shared_free(data_buf);
    kheap_shared_free(payload);
    return (int)result;
}

static int proxy_ioctl(pup_devfs_file_t *file, int cmd, void *arg)
{
    int ch = file->device->channel_id;
    pup_devfs_proxy_payload_t *payload;
    void *data_buf = PUP_NULL;
    long result;

    payload = (pup_devfs_proxy_payload_t *)kheap_shared_malloc(sizeof(*payload));
    if (!payload) {
        return -5;
    }

    /* Copy the ioctl argument (assumed to be ≤ 4 bytes, e.g. GPIO value) to
     * shared memory so the user-mode driver can access it safely. */
    if (arg) {
        data_buf = kheap_shared_malloc(sizeof(unsigned int));
        if (!data_buf) {
            kheap_shared_free(payload);
            return -5;
        }
        *(unsigned int *)data_buf = *(unsigned int *)arg;
    }

    payload->op       = DEVFS_PROXY_OP_IOCTL;
    payload->device_id = file->device->device_id;
    payload->flags    = 0;
    payload->cmd      = cmd;
    payload->data_buf = data_buf;
    payload->data_len = arg ? (int)sizeof(unsigned int) : 0;

    result = pup_channel_send(ch, payload, sizeof(*payload));

    /* Copy result back for GET-style ioctls */
    if (result >= 0 && data_buf && arg) {
        *(unsigned int *)arg = *(unsigned int *)data_buf;
    }

    if (data_buf) {
        kheap_shared_free(data_buf);
    }
    kheap_shared_free(payload);
    return (int)result;
}

static const pup_devfs_file_ops_t g_devfs_proxy_ops = {
    proxy_open,
    proxy_close,
    proxy_read,
    proxy_write,
    proxy_ioctl,
};

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

    if (device->source_type == PUP_DEVFS_DEVICE_SOURCE_USER) {
        if (!pup_channel_is_valid(device->channel_id)) {
            PUP_LOG_ERROR(PUP_LOG_MODULE_CORE,
                          "pup_devfs: user device '%s' has no valid channel", dev_name);
            return PUP_FS_ENOSYS;
        }

        pup_devfs_file_t *ufile = (pup_devfs_file_t *)kheap_malloc(sizeof(pup_devfs_file_t));
        if (ufile == PUP_NULL) {
            return -5;
        }
        ufile->ops          = &g_devfs_proxy_ops;
        ufile->device       = device;
        ufile->flags        = file->flags;
        ufile->private_data = PUP_NULL;
        ufile->ref_count    = 1;

        int uret = proxy_open(ufile);
        if (uret < 0) {
            kheap_free(ufile);
            PUP_LOG_ERROR(PUP_LOG_MODULE_CORE,
                          "pup_devfs: proxy open failed for '%s': %d", dev_name, uret);
            return uret;
        }

        file->data = ufile;
        file->type = FT_DEVICE;
        return 0;
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
