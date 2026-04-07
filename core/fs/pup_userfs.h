#ifndef __PUP_USERFS_H__
#define __PUP_USERFS_H__

#include "pup_fs.h"

#define PUP_USERFS_MAX_TYPES       4
#define PUP_USERFS_MAX_INSTANCES   4

typedef struct pup_userfs_type {
    char name[32];
    int owner_pid;
    int channel_id;
    int used;
} pup_userfs_type_t;

typedef struct pup_userfs_instance {
    int instance_id;
    int type_index;
    int channel_id;
    int owner_pid;
    char mount_path[PUP_FS_PATH_MAX];
    char device[PUP_FS_PATH_MAX];
    void *private_data;
    int used;
} pup_userfs_instance_t;

int pup_userfs_init(void);

int pup_userfs_register_type(const char *fs_type, int owner_pid, int channel_id);

int pup_userfs_unregister_type(const char *fs_type, int owner_pid);

int pup_userfs_create_instance(const char *fs_type, const char *device,
                                const char *mount_path, unsigned long flags,
                                const void *data, int owner_pid);

int pup_userfs_destroy_instance(int instance_id, int owner_pid);

void pup_userfs_cleanup_owner(int owner_pid);

int pup_userfs_find_channel_by_type(const char *fs_type);

int pup_userfs_register_to_vfs(void);

#endif
