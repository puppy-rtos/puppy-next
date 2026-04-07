#ifndef __PUP_USERFS_PROTO_H__
#define __PUP_USERFS_PROTO_H__

#include <stdint.h>
#include <stddef.h>

#define PUP_USERFS_PATH_MAX     256
#define PUP_USERFS_NAME_MAX     255
#define PUP_USERFS_MAX_FDS      8

typedef enum {
    PUP_USERFS_OP_MOUNT     = 1,
    PUP_USERFS_OP_UNMOUNT   = 2,
    PUP_USERFS_OP_OPEN      = 3,
    PUP_USERFS_OP_CLOSE     = 4,
    PUP_USERFS_OP_READ      = 5,
    PUP_USERFS_OP_WRITE     = 6,
    PUP_USERFS_OP_LSEEK     = 7,
    PUP_USERFS_OP_IOCTL     = 8,
    PUP_USERFS_OP_SYNC      = 9,
    PUP_USERFS_OP_GETDENTS  = 10,
    PUP_USERFS_OP_STAT      = 11,
    PUP_USERFS_OP_UNLINK    = 12,
    PUP_USERFS_OP_RENAME    = 13,
    PUP_USERFS_OP_MKDIR     = 14,
    PUP_USERFS_OP_STATFS    = 15,
    PUP_USERFS_OP_MKFS      = 16,
} pup_userfs_op_t;

#define PUP_USERFS_FT_REGULAR    0
#define PUP_USERFS_FT_DIRECTORY  2
#define PUP_USERFS_FT_DEVICE     4

#define PUP_USERFS_O_RDONLY      0000000
#define PUP_USERFS_O_WRONLY      0000001
#define PUP_USERFS_O_RDWR        0000002
#define PUP_USERFS_O_CREAT       0000100
#define PUP_USERFS_O_EXCL        0000200
#define PUP_USERFS_O_TRUNC       0001000
#define PUP_USERFS_O_APPEND      0002000
#define PUP_USERFS_O_DIRECTORY   00200000

#define PUP_USERFS_SEEK_SET      0
#define PUP_USERFS_SEEK_CUR      1
#define PUP_USERFS_SEEK_END      2

#define PUP_USERFS_DT_UNKNOWN    0
#define PUP_USERFS_DT_FIFO       1
#define PUP_USERFS_DT_CHR        2
#define PUP_USERFS_DT_DIR        4
#define PUP_USERFS_DT_BLK        6
#define PUP_USERFS_DT_REG        8
#define PUP_USERFS_DT_LNK        10

typedef struct {
    pup_userfs_op_t op;
    int instance_id;
    int fd;
} pup_userfs_req_header_t;

typedef struct {
    pup_userfs_req_header_t header;
    char device[PUP_USERFS_PATH_MAX];
    unsigned long mount_flags;
    int data_len;
} pup_userfs_mount_req_t;

typedef struct {
    pup_userfs_req_header_t header;
    char path[PUP_USERFS_PATH_MAX];
    int flags;
} pup_userfs_open_req_t;

typedef struct {
    pup_userfs_req_header_t header;
    uint32_t size;
    char data_buf[0];
} pup_userfs_io_req_t;

typedef struct {
    pup_userfs_req_header_t header;
    long offset;
    int whence;
} pup_userfs_lseek_req_t;

typedef struct {
    pup_userfs_req_header_t header;
    int cmd;
    int arg_len;
    char arg_data[0];
} pup_userfs_ioctl_req_t;

typedef struct {
    pup_userfs_req_header_t header;
    uint32_t count;
} pup_userfs_getdents_req_t;

typedef struct {
    pup_userfs_req_header_t header;
    char path[PUP_USERFS_PATH_MAX];
} pup_userfs_stat_req_t;

typedef struct {
    pup_userfs_req_header_t header;
    char path[PUP_USERFS_PATH_MAX];
} pup_userfs_unlink_req_t;

typedef struct {
    pup_userfs_req_header_t header;
    char oldpath[PUP_USERFS_PATH_MAX];
    char newpath[PUP_USERFS_PATH_MAX];
} pup_userfs_rename_req_t;

typedef struct {
    pup_userfs_req_header_t header;
    char path[PUP_USERFS_PATH_MAX];
} pup_userfs_mkdir_req_t;

typedef struct {
    pup_userfs_req_header_t header;
    char device[PUP_USERFS_PATH_MAX];
    uint32_t block_size;
    uint32_t block_count;
} pup_userfs_mkfs_req_t;

typedef struct {
    int result;
    uint32_t size;
} pup_userfs_resp_header_t;

typedef struct {
    pup_userfs_resp_header_t header;
    uint32_t st_dev;
    uint32_t st_ino;
    uint16_t st_mode;
    uint16_t st_nlink;
    uint32_t st_size;
    uint32_t st_mtime;
} pup_userfs_stat_resp_t;

typedef struct {
    uint8_t d_type;
    uint8_t d_namlen;
    uint16_t d_reclen;
    char d_name[PUP_USERFS_NAME_MAX + 1];
} pup_userfs_dirent_t;

typedef struct {
    pup_userfs_resp_header_t header;
    uint32_t f_bsize;
    uint32_t f_blocks;
    uint32_t f_bfree;
} pup_userfs_statfs_resp_t;

typedef struct {
    pup_userfs_resp_header_t header;
    int fd;
    uint32_t file_size;
    int file_type;
} pup_userfs_open_resp_t;

typedef struct {
    pup_userfs_resp_header_t header;
    long new_offset;
} pup_userfs_lseek_resp_t;

#endif
