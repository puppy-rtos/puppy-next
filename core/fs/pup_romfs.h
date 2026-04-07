#ifndef __PUP_ROMFS_H__
#define __PUP_ROMFS_H__

#include <puppy_core.h>

#define ROMFS_MAGIC        0x524F4D46
#define ROMFS_MAX_FILES    32
#define ROMFS_MAX_NAME_LEN 32

typedef struct {
	unsigned int size;
	unsigned int data_offset;
	char name[ROMFS_MAX_NAME_LEN];
} romfs_file_header_t;

typedef struct {
	unsigned int magic;
	unsigned int file_count;
	unsigned int total_size;
	unsigned int data_offset;
	romfs_file_header_t files[ROMFS_MAX_FILES];
} romfs_header_t;

typedef struct {
	const romfs_header_t *header;
	int file_index;
	unsigned int position;
} romfs_file_ctx_t;

int pup_romfs_init(void);
int pup_romfs_get_file_data(const char *path, void **data, unsigned int *size);

#endif
