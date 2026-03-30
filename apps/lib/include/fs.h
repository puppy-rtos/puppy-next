#ifndef __APP_FS_H__
#define __APP_FS_H__

#include "syscall.h"

#define FS_O_RDONLY    0x0001
#define FS_O_WRONLY    0x0002
#define FS_O_RDWR      0x0003

#define FS_OK          0
#define FS_ERROR      -1

static inline int fs_open(const char* path, int flags) {
    return open(path, flags);
}

static inline int fs_close(int fd) {
    return close(fd);
}

static inline int fs_read(int fd, char* buf, int len) {
    return (int)read(fd, buf, len);
}

static inline int fs_write(int fd, const char* buf, int len) {
    return (int)write(fd, buf, len);
}

static inline int fs_ioctl(int fd, int cmd, void* arg) {
    return ioctl(fd, cmd, arg);
}

#endif
