#ifndef __PUP_DTM_H__
#define __PUP_DTM_H__

#include <stdint.h>

/*
 * Stable user/kernel ABI for device-tree access.
 *
 * The kernel owns the DTB blob and exposes its read-only flash address.
 * User-mode drivers parse that blob directly instead of depending on a
 * kernel-maintained driver-specific resource struct.
 */
#define PUP_DTM_QUERY_BLOB_INFO    1

#define PUP_DTM_SOURCE_FALLBACK    0U
#define PUP_DTM_SOURCE_DTB         1U

#define PUP_DTM_BLOB_FLAG_READABLE  0x00000001U

typedef struct {
    uint32_t source;
    uint32_t size;
    uintptr_t addr;
    uint32_t flags;
} pup_dtm_blob_info_t;

#endif
