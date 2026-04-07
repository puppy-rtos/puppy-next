/*
 * lfs utility functions
 *
 * Copyright (c) 2022, The littlefs authors.
 * Copyright (c) 2017, Arm Limited. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef LFS_UTIL_H
#define LFS_UTIL_H

#include "lfs_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef LFS_TRACE
#define LFS_TRACE(...)
#endif

#ifndef LFS_DEBUG
#define LFS_DEBUG(...)
#endif

#ifndef LFS_WARN
#define LFS_WARN(...)
#endif

#ifndef LFS_ERROR
#define LFS_ERROR(...)
#endif

static inline uint32_t lfs_max(uint32_t a, uint32_t b) {
    return (a > b) ? a : b;
}

static inline uint32_t lfs_min(uint32_t a, uint32_t b) {
    return (a < b) ? a : b;
}

static inline uint32_t lfs_aligndown(uint32_t a, uint32_t alignment) {
    return a - (a % alignment);
}

static inline uint32_t lfs_alignup(uint32_t a, uint32_t alignment) {
    return lfs_aligndown(a + alignment-1, alignment);
}

static inline uint32_t lfs_npw2(uint32_t a) {
    return 32 - __builtin_clz(a-1);
}

static inline uint32_t lfs_ctz(uint32_t a) {
    return __builtin_ctz(a);
}

static inline uint32_t lfs_popc(uint32_t a) {
    return __builtin_popcount(a);
}

static inline int lfs_scmp(uint32_t a, uint32_t b) {
    return (int)(unsigned)(a - b);
}

static inline uint32_t lfs_fromle32(uint32_t a) {
    return a;
}

static inline uint32_t lfs_tole32(uint32_t a) {
    return a;
}

static inline uint32_t lfs_frombe32(uint32_t a) {
    return __builtin_bswap32(a);
}

static inline uint32_t lfs_tobe32(uint32_t a) {
    return __builtin_bswap32(a);
}

uint32_t lfs_crc(uint32_t crc, const void *buffer, size_t size);

static inline void *lfs_malloc(size_t size) {
    return LFS_MALLOC(size);
}

static inline void lfs_free(void *p) {
    LFS_FREE(p);
}

#ifdef __cplusplus
}
#endif

#endif
