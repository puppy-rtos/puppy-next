#ifndef __PUP_DEVFS_H__
#define __PUP_DEVFS_H__

#include <puppy_core.h>

#define PUP_DEVFS_MAX_OPEN_FILES    8
#define PUP_DEVFS_MAX_NAME_LEN      16

#define PUP_DEVFS_OK          0
#define PUP_DEVFS_ERROR      -1
#define PUP_DEVFS_ENOMEM     -2
#define PUP_DEVFS_ENOENT     -3
#define PUP_DEVFS_EINVAL     -4
#define PUP_DEVFS_EMFILE     -5

typedef struct pup_devfs_file pup_devfs_file_t;
typedef struct pup_devfs_file_ops pup_devfs_file_ops_t;
typedef struct pup_devfs_device pup_devfs_device_t;

struct pup_devfs_file_ops {
	int (*open)(pup_devfs_file_t *file);
	int (*close)(pup_devfs_file_t *file);
	int (*read)(pup_devfs_file_t *file, char *buf, int len);
	int (*write)(pup_devfs_file_t *file, const char *buf, int len);
	int (*ioctl)(pup_devfs_file_t *file, int cmd, void *arg);
};

struct pup_devfs_file {
	const pup_devfs_file_ops_t *ops;
	const pup_devfs_device_t *device;
	int flags;
	int ref_count;
	void *private_data;
};

struct pup_devfs_device {
	const char *name;
	const pup_devfs_file_ops_t *ops;
	void *private_data;
};

int pup_devfs_registry_init(void);
int pup_devfs_register_device(const pup_devfs_device_t *device);
int pup_devfs_unregister_device(const char *name);
const pup_devfs_device_t *pup_devfs_find_device(const char *name);
int pup_devfs_get_device_count(void);
const char *pup_devfs_get_device_name(int index);
void pup_devfs_list_devices(void);

int pup_devfs_init(void);

#endif
