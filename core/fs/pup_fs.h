#ifndef __PUP_FS_H__
#define __PUP_FS_H__

#include <puppy_core.h>

#define PUP_FS_FILESYSTEMS_MAX     4
#define PUP_FS_TYPES_MAX 4
#define PUP_FS_FD_MAX              8
#define PUP_FS_STD_FD_COUNT        3
#define PUP_FS_PATH_MAX            256

#define PUP_FS_FLAG_DEFAULT     0x00
#define PUP_FS_FLAG_FULLPATH    0x01

/* DFS return codes (negative values are error codes). */
#define PUP_FS_EOK                 0
#define PUP_FS_EFULL               -1
#define PUP_FS_ENOENT              -2
#define PUP_FS_ENOSYS              -4
#define PUP_FS_EIO                 -5
#define PUP_FS_EXDEV               -18

#define DT_UNKNOWN              0
#define DT_FIFO                 1
#define DT_CHR                  2
#define DT_DIR                  4
#define DT_BLK                  6
#define DT_REG                  8
#define DT_LNK                  10
#define DT_SOCK                 12

#define O_RDONLY                0000000
#define O_WRONLY                0000001
#define O_RDWR                  0000002
#define O_CREAT                 0000100
#define O_EXCL                  0000200
#define O_TRUNC                 0001000
#define O_APPEND                0002000
#define O_NONBLOCK              0004000
#define O_DIRECTORY             00200000

#define SEEK_SET                0
#define SEEK_CUR                1
#define SEEK_END                2

#define S_IFMT                  00170000
#define S_IFSOCK                0140000
#define S_IFLNK                 0120000
#define S_IFREG                 0100000
#define S_IFBLK                 0060000
#define S_IFDIR                 0040000
#define S_IFCHR                 0020000
#define S_IFIFO                 0010000

#define S_ISDIR(m)              (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)              (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)              (((m) & S_IFMT) == S_IFBLK)
#define S_ISREG(m)              (((m) & S_IFMT) == S_IFREG)
#define S_ISFIFO(m)             (((m) & S_IFMT) == S_IFIFO)
#define S_ISLNK(m)              (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m)             (((m) & S_IFMT) == S_IFSOCK)

#define S_IRUSR                 00400
#define S_IWUSR                 00200
#define S_IXUSR                 00100
#define S_IRGRP                 00040
#define S_IWGRP                 00020
#define S_IXGRP                 00010
#define S_IROTH                 00004
#define S_IWOTH                 00002
#define S_IXOTH                 00001

struct statfs
{
    pup_size_t f_bsize;
    pup_size_t f_blocks;
    pup_size_t f_bfree;
};

struct stat
{
    pup_uint32_t st_dev;
    pup_uint32_t st_ino;
    pup_uint16_t st_mode;
    pup_uint16_t st_nlink;
    pup_uint16_t st_uid;
    pup_uint16_t st_gid;
    pup_uint32_t st_rdev;
    pup_size_t   st_size;
    pup_size_t   st_blksize;
    pup_size_t   st_blocks;
    pup_uint32_t st_atime;
    pup_uint32_t st_mtime;
    pup_uint32_t st_ctime;
};

struct dirent
{
    pup_uint8_t d_type;
    pup_uint8_t d_namlen;
    pup_uint16_t d_reclen;
    char d_name[PUP_FS_PATH_MAX];
};

struct pup_filesystem;
struct pup_fd;

struct pup_file_ops
{
    int (*open)     (struct pup_fd *fd);
    int (*close)    (struct pup_fd *fd);
    int (*ioctl)    (struct pup_fd *fd, int cmd, void *args);
    int (*read)     (struct pup_fd *fd, void *buf, pup_size_t count);
    int (*write)    (struct pup_fd *fd, const void *buf, pup_size_t count);
    int (*flush)    (struct pup_fd *fd);
    int (*lseek)    (struct pup_fd *fd, long offset);
    int (*getdents) (struct pup_fd *fd, struct dirent *dirp, pup_uint32_t count);
};

struct pup_fs_ops
{
    char *name;
    pup_uint32_t flags;
    const struct pup_file_ops *fops;
    int (*mount)    (struct pup_filesystem *fs, unsigned long rwflag, const void *data);
    int (*unmount)  (struct pup_filesystem *fs);
    int (*mkfs)     (void *devid);
    int (*statfs)   (struct pup_filesystem *fs, struct statfs *buf);
    int (*unlink)   (struct pup_filesystem *fs, const char *pathname);
    int (*stat)     (struct pup_filesystem *fs, const char *filename, struct stat *buf);
    int (*rename)   (struct pup_filesystem *fs, const char *oldpath, const char *newpath);
};

struct pup_filesystem
{
    void *dev_id;
    char *path;
    const struct pup_fs_ops *ops;
    void *data;
};

struct pup_fd
{
    pup_uint16_t magic;
    pup_uint16_t type;
    char *path;
    int ref_count;
    struct pup_filesystem *fs;
    const struct pup_file_ops *fops;
    pup_uint32_t flags;
    pup_size_t   size;
    long    pos;
    void *data;
};

#define PUP_FD_MAGIC            0xfdfd

#define FT_REGULAR              0
#define FT_DIRECTORY            2
#define FT_DEVICE               4

int pup_fs_init(void);

int pup_fs_register(const struct pup_fs_ops *ops);
int pup_fs_mount(const char *device_name, const char *path,
              const char *filesystemtype, unsigned long rwflag, const void *data);
int pup_fs_unmount(const char *specialfile);
int pup_fs_statfs(const char *path, struct statfs *buffer);

int pup_open(const char *path, int flags);
int pup_close(int fd);
int pup_read(int fd, void *buf, pup_size_t len);
int pup_write(int fd, const void *buf, pup_size_t len);
int pup_ioctl(int fd, int cmd, void *args);
long pup_lseek(int fd, long offset, int whence);
int pup_fsync(int fd);
int pup_stat(const char *path, struct stat *buf);
int pup_unlink(const char *path);
int pup_rename(const char *oldpath, const char *newpath);

int pup_getdents(int fd, struct dirent *dirp, pup_size_t nbytes);

struct pup_fd *pup_fd_get(int fd);

#endif
