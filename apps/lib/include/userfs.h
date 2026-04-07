#ifndef __USERFS_H__
#define __USERFS_H__

#include "pup_userfs_proto.h"

typedef struct {
    int (*mount)(const char *device, unsigned long flags, const void *data);
    int (*unmount)(void);
    int (*open)(const char *path, int flags);
    int (*close)(int fd);
    int (*read)(int fd, void *buf, uint32_t size);
    int (*write)(int fd, const void *buf, uint32_t size);
    long (*lseek)(int fd, long offset, int whence);
    int (*sync)(int fd);
    int (*stat)(const char *path, pup_userfs_stat_resp_t *st);
    int (*unlink)(const char *path);
    int (*rename)(const char *oldpath, const char *newpath);
    int (*mkdir)(const char *path);
    int (*getdents)(int fd, pup_userfs_dirent_t *dirp, uint32_t count, int *num_entries);
    int (*statfs)(pup_userfs_statfs_resp_t *buf);
    int (*mkfs)(const char *device, uint32_t block_size, uint32_t block_count);
} userfs_ops_t;

typedef struct {
    const char *fs_type;
    const userfs_ops_t *ops;
    void *private_data;
} userfs_config_t;

int userfs_service_start(const userfs_config_t *config);

void userfs_service_stop(void);

#endif
