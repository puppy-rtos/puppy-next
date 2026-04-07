#ifndef LFS_CONFIG_H
#define LFS_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "syscall.h"
#include "libc.h"

#define LFS_MALLOC(sz)      shared_malloc(sz)
#define LFS_FREE(p)         shared_free(p)

#define LFS_NO_ASSERT
#define LFS_ASSERT(test)    ((void)0)

#define LFS_NO_DEBUG
#define LFS_NO_WARN
#define LFS_NO_ERROR
#define LFS_TRACE(...)

#endif
