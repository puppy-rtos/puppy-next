#ifndef __USER_FS_H__
#define __USER_FS_H__

#include "user_syscall.h"

#define USER_FS_O_RDONLY    0x0001
#define USER_FS_O_WRONLY    0x0002
#define USER_FS_O_RDWR      0x0003

#define USER_FS_OK          0
#define USER_FS_ERROR      -1

static inline int user_fs_open(const char* path, int flags) {
    return user_open(path, flags);
}

static inline int user_fs_close(int fd) {
    return user_close(fd);
}

static inline int user_fs_read(int fd, char* buf, int len) {
    return (int)user_read(fd, buf, len);
}

static inline int user_fs_write(int fd, const char* buf, int len) {
    return (int)user_write(fd, buf, len);
}

static inline int user_fs_ioctl(int fd, int cmd, void* arg) {
    return user_ioctl(fd, cmd, arg);
}

#endif
