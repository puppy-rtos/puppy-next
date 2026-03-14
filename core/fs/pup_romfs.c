#include "pup_fs.h"
#include "pup_romfs.h"
#include "puppy_log.h"
#include <libcm.h>

static const romfs_header_t *g_romfs_header = PUP_NULL;

static romfs_file_ctx_t g_file_contexts[PUP_FS_FD_MAX];

static int pup_romfs_mount(struct pup_filesystem *fs, unsigned long rwflag, const void *data)
{
    const romfs_header_t *header = (const romfs_header_t *)data;

    if (data == PUP_NULL)
        return PUP_FS_EIO;

    if (header->magic != ROMFS_MAGIC) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "ROMFS: Invalid magic 0x%08x, expected 0x%08x", 
                      header->magic, ROMFS_MAGIC);
        return PUP_FS_EIO;
    }

    g_romfs_header = header;
    fs->data = (void *)header;

    PUP_LOG_INFO(PUP_LOG_MODULE_FS, "ROMFS mounted at %p", data);
    PUP_LOG_INFO(PUP_LOG_MODULE_FS, "ROMFS: %d files, total size %d bytes", 
                 header->file_count, header->total_size);
    
    for (int i = 0; i < header->file_count && i < ROMFS_MAX_FILES; i++) {
        PUP_LOG_DEBUG(PUP_LOG_MODULE_FS, "  [%d] %s: %d bytes", 
                     i, header->files[i].name, header->files[i].size);
    }
    
    return 0;
}

static int pup_romfs_unmount(struct pup_filesystem *fs)
{
    g_romfs_header = PUP_NULL;
    return 0;
}

static int pup_romfs_find_file_index(const char *name)
{
    if (g_romfs_header == PUP_NULL || name == PUP_NULL)
        return PUP_FS_ENOENT;
    
    const char *filename = name;
    if (name[0] == '/')
        filename = name + 1;
    
    for (int i = 0; i < g_romfs_header->file_count; i++) {
        if (libcm_strcmp(g_romfs_header->files[i].name, filename) == 0) {
            return i;
        }
    }
    
    return PUP_FS_ENOENT;
}

int pup_romfs_get_file_data(const char *path, void **data, unsigned int *size)
{
    if (g_romfs_header == PUP_NULL || path == PUP_NULL) {
        return PUP_FS_ENOENT;
    }
    
    const char *filename = path;
    if (libcm_strncmp(path, "/rom/", 5) == 0) {
        filename = path + 5;
    } else if (path[0] == '/') {
        filename = path + 1;
    }
    
    for (int i = 0; i < g_romfs_header->file_count; i++) {
        if (libcm_strcmp(g_romfs_header->files[i].name, filename) == 0) {
            const unsigned char *data_start = (const unsigned char *)g_romfs_header + g_romfs_header->data_offset;
            
            if (data) {
                *data = (void *)(data_start + g_romfs_header->files[i].data_offset);
            }
            if (size) {
                *size = g_romfs_header->files[i].size;
            }
            
            return 0;
        }
    }
    
    return PUP_FS_ENOENT;
}

static int pup_romfs_alloc_context(void)
{
    for (int i = 0; i < PUP_FS_FD_MAX; i++) {
        if (g_file_contexts[i].header == PUP_NULL) {
            return i;
        }
    }
    return PUP_FS_EFULL;
}

static int pup_romfs_open(struct pup_fd *file)
{
    struct pup_filesystem *fs;
    const romfs_header_t *header;
    int file_idx;
    int ctx_idx;

    fs = (struct pup_filesystem *)file->data;
    header = (const romfs_header_t *)fs->data;

    PUP_LOG_DEBUG(PUP_LOG_MODULE_FS, "ROMFS open: file->data=%p, fs=%p, fs->data=%p, header=%p", 
                  file->data, fs, fs->data, header);

    if (header == PUP_NULL) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "ROMFS: header is NULL");
           return PUP_FS_EIO;
    }
    
    if (header->magic != ROMFS_MAGIC) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "ROMFS: Invalid magic 0x%08x, expected 0x%08x", 
                      header->magic, ROMFS_MAGIC);
           return PUP_FS_EIO;
    }

    if (file->flags & (O_CREAT | O_WRONLY | O_APPEND | O_TRUNC | O_RDWR))
        return PUP_FS_ENOSYS;

    if (libcm_strcmp(file->path, "/") == 0) {
        file->size = header->file_count;
        file->pos = 0;
        file->data = (void *)header;
        PUP_LOG_DEBUG(PUP_LOG_MODULE_FS, "ROMFS: Opened root directory");
        return 0;
    }

    file_idx = pup_romfs_find_file_index(file->path);
    if (file_idx < 0) {
        PUP_LOG_DEBUG(PUP_LOG_MODULE_FS, "ROMFS: File not found: %s", file->path);
        return PUP_FS_ENOENT;
    }

    ctx_idx = pup_romfs_alloc_context();
    if (ctx_idx < 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "ROMFS: No free context slot");
        return PUP_FS_EFULL;
    }

    g_file_contexts[ctx_idx].header = header;
    g_file_contexts[ctx_idx].file_index = file_idx;
    g_file_contexts[ctx_idx].position = 0;

    file->data = &g_file_contexts[ctx_idx];
    file->size = header->files[file_idx].size;
    file->pos = 0;

    PUP_LOG_DEBUG(PUP_LOG_MODULE_FS, "ROMFS: Opened file %s, size=%d", 
                  header->files[file_idx].name, file->size);
    return 0;
}

static int pup_romfs_close(struct pup_fd *file)
{
    romfs_file_ctx_t *ctx;

    if (file->data != PUP_NULL) {
        if (file->type == FT_DIRECTORY) {
            file->data = PUP_NULL;
        } else {
            ctx = (romfs_file_ctx_t *)file->data;
            ctx->header = PUP_NULL;
            ctx->file_index = -1;
            ctx->position = 0;
        }
    }
    return 0;
}

static int pup_romfs_read(struct pup_fd *file, void *buf, pup_size_t count)
{
    romfs_file_ctx_t *ctx;
    const romfs_header_t *header;
    const romfs_file_header_t *file_header;
    const unsigned char *data_start;
    const unsigned char *file_data;
    pup_size_t length;

    ctx = (romfs_file_ctx_t *)file->data;
    if (ctx == PUP_NULL || ctx->header == PUP_NULL) {
        return PUP_FS_EFULL;
    }

    header = ctx->header;
    file_header = &header->files[ctx->file_index];
    
    data_start = (const unsigned char *)header + header->data_offset;
    file_data = data_start + file_header->data_offset;

    if (count < file_header->size - ctx->position)
        length = count;
    else
        length = file_header->size - ctx->position;

    if (length > 0)
        libcm_memcpy(buf, file_data + ctx->position, length);

    ctx->position += length;
    file->pos = ctx->position;

    return length;
}

static int pup_romfs_lseek(struct pup_fd *file, long offset)
{
    romfs_file_ctx_t *ctx;
    const romfs_header_t *header;
    const romfs_file_header_t *file_header;

    ctx = (romfs_file_ctx_t *)file->data;
    if (ctx == PUP_NULL || ctx->header == PUP_NULL) {
        return PUP_FS_EFULL;
    }

    header = ctx->header;
    file_header = &header->files[ctx->file_index];

    if (offset < 0)
        offset = 0;
    else if (offset > (long)file_header->size)
        offset = file_header->size;

    ctx->position = offset;
    file->pos = offset;

    return offset;
}

static int pup_romfs_stat(struct pup_filesystem *fs, const char *path, struct stat *st)
{
    const romfs_header_t *header;
    int file_idx;

    header = (const romfs_header_t *)fs->data;
    if (header == PUP_NULL)
        return PUP_FS_EFULL;

    if (libcm_strcmp(path, "/") == 0) {
        st->st_dev = 0;
        st->st_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH |
                      S_IXUSR | S_IXGRP | S_IXOTH;
        st->st_size = 0;
        st->st_mtime = 0;
        return 0;
    }

    file_idx = pup_romfs_find_file_index(path);
    if (file_idx < 0)
        return PUP_FS_ENOENT;

    st->st_dev = 0;
    st->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
    st->st_size = header->files[file_idx].size;
    st->st_mtime = 0;

    return 0;
}

static int pup_romfs_getdents(struct pup_fd *file, struct dirent *dirp, pup_uint32_t count)
{
    const romfs_header_t *header;
    struct dirent *d;
    pup_size_t index;
    pup_size_t num_entries;

    header = (const romfs_header_t *)file->data;
    if (header == PUP_NULL || header->magic != ROMFS_MAGIC) {
        return PUP_FS_EFULL;
    }

    num_entries = count / sizeof(struct dirent);
    if (num_entries == 0)
        return PUP_FS_EFULL;

    for (index = 0; index < num_entries && file->pos < header->file_count; index++) {
        d = dirp + index;
        
        d->d_type = DT_REG;
        d->d_namlen = libcm_strlen(header->files[file->pos].name);
        d->d_reclen = (pup_uint16_t)sizeof(struct dirent);
        libcm_strncpy(d->d_name, header->files[file->pos].name, d->d_namlen + 1);
        
        ++file->pos;
    }

    return index * sizeof(struct dirent);
}

static const struct pup_file_ops _rom_fops =
{
    pup_romfs_open,
    pup_romfs_close,
    PUP_NULL,
    pup_romfs_read,
    PUP_NULL,
    PUP_NULL,
    pup_romfs_lseek,
    pup_romfs_getdents,
};

static const struct pup_fs_ops _romfs =
{
    "rom",
    PUP_FS_FLAG_DEFAULT,
    &_rom_fops,

    pup_romfs_mount,
    pup_romfs_unmount,
    PUP_NULL,
    PUP_NULL,

    PUP_NULL,
    pup_romfs_stat,
    PUP_NULL,
};

int pup_romfs_init(void)
{
    for (int i = 0; i < PUP_FS_FD_MAX; i++) {
        g_file_contexts[i].header = PUP_NULL;
        g_file_contexts[i].file_index = -1;
        g_file_contexts[i].position = 0;
    }
    
    int ret = pup_fs_register(&_romfs);
    if (ret == 0)
    {
        PUP_LOG_INFO(PUP_LOG_MODULE_FS, "romfs initialized");
    }
    else
    {
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "romfs init failed");
    }
    return ret;
}
