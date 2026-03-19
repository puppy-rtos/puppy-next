#include "kheap.h"
#include "tlsf.h"
#include "puppy_log.h"
#include "mpu.h"
#include <puppy_core.h>
#include "default_config.h"
#include <string.h>

#ifndef ALIGN_UP
#define ALIGN_UP(size, align)   (((size) + (align) - 1) & ~((align) - 1))
#endif
#ifndef ALIGN_DOWN
#define ALIGN_DOWN(size, align) ((size) & ~((align) - 1))
#endif

extern char _end;
extern char _heap_start;
extern char _heap_end;

static tlsf_t g_kheap_tlsf = NULL;
static pool_t g_kheap_pool = NULL;
static size_t g_kheap_total_size = 0;
static size_t g_kheap_current_used = 0;
static size_t g_kheap_peak_used = 0;
static tlsf_t g_kheap_user_tlsf = NULL;
static pool_t g_kheap_user_pool = NULL;
static size_t g_kheap_user_total_size = 0;
static size_t g_kheap_user_current_used = 0;
static size_t g_kheap_user_peak_used = 0;
static tlsf_t g_kheap_shared_tlsf = NULL;
static pool_t g_kheap_shared_pool = NULL;
static size_t g_kheap_shared_total_size = 0;
static size_t g_kheap_shared_current_used = 0;
static size_t g_kheap_shared_peak_used = 0;

static inline pup_base_t kheap_lock(void)
{
    return arch_irq_lock();
}

static inline void kheap_unlock(pup_base_t key)
{
    arch_irq_unlock(key);
}

static void kheap_refresh_peak(void);
static void kheap_user_refresh_peak(void);
static void kheap_shared_refresh_peak(void);

void kheap_init(void)
{
    size_t heap_size;
    void* heap_start;
    void* user_heap_start;
    size_t user_heap_size;
    
    heap_start = (void*)ALIGN_UP((size_t)&_end, 8);
    
    heap_size = (size_t)&_heap_end - (size_t)heap_start;
    heap_size = ALIGN_DOWN(heap_size, 8);
    
    if (heap_size < tlsf_size() + tlsf_pool_overhead() + tlsf_block_size_min()) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_KHEAP, "kheap: not enough memory for heap");
        return;
    }
    
    g_kheap_tlsf = tlsf_create_with_pool(heap_start, heap_size);
    if (g_kheap_tlsf == NULL) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_KHEAP, "kheap: tlsf_create_with_pool failed");
        return;
    }
    
    g_kheap_pool = tlsf_get_pool(g_kheap_tlsf);
    g_kheap_total_size = heap_size;
    g_kheap_current_used = 0;
    g_kheap_peak_used = 0;

    user_heap_start = (void*)ALIGN_UP((size_t)PUP_MPU_USER_RAM_BASE, 8);
    user_heap_size = ALIGN_DOWN((size_t)PUP_MPU_USER_RAM_SIZE, 8);

    if (user_heap_size >= tlsf_size() + tlsf_pool_overhead() + tlsf_block_size_min()) {
        g_kheap_user_tlsf = tlsf_create_with_pool(user_heap_start, user_heap_size);
        if (g_kheap_user_tlsf != NULL) {
            g_kheap_user_pool = tlsf_get_pool(g_kheap_user_tlsf);
            g_kheap_user_total_size = user_heap_size;
            g_kheap_user_current_used = 0;
            g_kheap_user_peak_used = 0;
            PUP_LOG_INFO(PUP_LOG_MODULE_KHEAP, "kheap user pool: 0x%08x, size: %u bytes",
                         (unsigned int)user_heap_start, (unsigned int)user_heap_size);
        } else {
            PUP_LOG_WARNING(PUP_LOG_MODULE_KHEAP, "kheap user pool init failed");
        }
    } else {
        PUP_LOG_WARNING(PUP_LOG_MODULE_KHEAP, "kheap user pool too small");
    }
    
    PUP_LOG_INFO(PUP_LOG_MODULE_KHEAP, "kheap: initialized at 0x%08x, size: %u bytes",
                 (unsigned int)heap_start, (unsigned int)heap_size);

    kheap_refresh_peak();
    kheap_user_refresh_peak();
    
    // 初始化共享堆
    void* shared_heap_start = (void*)ALIGN_UP((size_t)PUP_SHARED_RAM_BASE, 8);
    size_t shared_heap_size = ALIGN_DOWN((size_t)PUP_SHARED_RAM_SIZE, 8);
    
    if (shared_heap_size >= tlsf_size() + tlsf_pool_overhead() + tlsf_block_size_min()) {
        g_kheap_shared_tlsf = tlsf_create_with_pool(shared_heap_start, shared_heap_size);
        if (g_kheap_shared_tlsf != NULL) {
            g_kheap_shared_pool = tlsf_get_pool(g_kheap_shared_tlsf);
            g_kheap_shared_total_size = shared_heap_size;
            g_kheap_shared_current_used = 0;
            g_kheap_shared_peak_used = 0;
            PUP_LOG_INFO(PUP_LOG_MODULE_KHEAP, "kheap shared pool: 0x%08x, size: %u bytes",
                         (unsigned int)shared_heap_start, (unsigned int)shared_heap_size);
        } else {
            PUP_LOG_WARNING(PUP_LOG_MODULE_KHEAP, "kheap shared pool init failed");
        }
    } else {
        PUP_LOG_WARNING(PUP_LOG_MODULE_KHEAP, "kheap shared pool too small");
    }
    
    kheap_shared_refresh_peak();
}

void* kheap_malloc(size_t size)
{
    void* ptr;
    size_t used;
    pup_base_t key;
    
    if (g_kheap_tlsf == NULL) {
        return NULL;
    }
    
    key = kheap_lock();
    ptr = tlsf_malloc(g_kheap_tlsf, size);
    if (ptr != NULL) {
        used = tlsf_block_size(ptr);
        g_kheap_current_used += used;
        kheap_refresh_peak();
    }
    kheap_unlock(key);
    
    return ptr;
}

void kheap_free(void* ptr)
{
    pup_base_t key;
    size_t used;

    if (ptr == NULL || g_kheap_tlsf == NULL) {
        return;
    }

    key = kheap_lock();
    used = tlsf_block_size(ptr);
    tlsf_free(g_kheap_tlsf, ptr);
    if (g_kheap_current_used >= used) {
        g_kheap_current_used -= used;
    } else {
        g_kheap_current_used = 0;
    }
    kheap_unlock(key);
}

void* kheap_calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size_t total;
    
    total = nmemb * size;
    ptr = kheap_malloc(total);
    
    if (ptr != NULL) {
        memset(ptr, 0, total);
    }
    
    return ptr;
}

void* kheap_realloc(void* ptr, size_t size)
{
    void* new_ptr;
    size_t old_used = 0;
    size_t new_used = 0;
    pup_base_t key;

    if (g_kheap_tlsf == NULL) {
        return NULL;
    }
    
    key = kheap_lock();
    if (ptr != NULL) {
        old_used = tlsf_block_size(ptr);
    }
    new_ptr = tlsf_realloc(g_kheap_tlsf, ptr, size);

    if (new_ptr != NULL) {
        new_used = tlsf_block_size(new_ptr);
        if (g_kheap_current_used >= old_used) {
            g_kheap_current_used -= old_used;
        } else {
            g_kheap_current_used = 0;
        }
        g_kheap_current_used += new_used;
        kheap_refresh_peak();
    } else if (size == 0 && ptr != NULL) {
        if (g_kheap_current_used >= old_used) {
            g_kheap_current_used -= old_used;
        } else {
            g_kheap_current_used = 0;
        }
    }

    kheap_unlock(key);

    return new_ptr;
}

void* kheap_memalign(size_t align, size_t size)
{
    void* ptr;
    size_t used;
    pup_base_t key;

    if (g_kheap_tlsf == NULL) {
        return NULL;
    }
    
    key = kheap_lock();
    ptr = tlsf_memalign(g_kheap_tlsf, align, size);
    if (ptr != NULL) {
        used = tlsf_block_size(ptr);
        g_kheap_current_used += used;
        kheap_refresh_peak();
    }
    kheap_unlock(key);

    return ptr;
}

void* kheap_user_malloc(size_t size)
{
    void* ptr;
    size_t used;
    pup_base_t key;

    if (g_kheap_user_tlsf == NULL) {
        return NULL;
    }

    key = kheap_lock();
    ptr = tlsf_malloc(g_kheap_user_tlsf, size);
    if (ptr != NULL) {
        used = tlsf_block_size(ptr);
        g_kheap_user_current_used += used;
        kheap_user_refresh_peak();
    }
    kheap_unlock(key);

    return ptr;
}

void* kheap_user_memalign(size_t align, size_t size)
{
    void* ptr;
    size_t used;
    pup_base_t key;

    if (g_kheap_user_tlsf == NULL) {
        return NULL;
    }

    key = kheap_lock();
    ptr = tlsf_memalign(g_kheap_user_tlsf, align, size);
    if (ptr != NULL) {
        used = tlsf_block_size(ptr);
        g_kheap_user_current_used += used;
        kheap_user_refresh_peak();
    }
    kheap_unlock(key);

    return ptr;
}

void kheap_user_free(void* ptr)
{
    pup_base_t key;
    size_t used;

    if (ptr == NULL || g_kheap_user_tlsf == NULL) {
        return;
    }

    key = kheap_lock();
    used = tlsf_block_size(ptr);
    tlsf_free(g_kheap_user_tlsf, ptr);
    if (g_kheap_user_current_used >= used) {
        g_kheap_user_current_used -= used;
    } else {
        g_kheap_user_current_used = 0;
    }
    kheap_unlock(key);
}

void* kheap_shared_malloc(size_t size)
{
    void* ptr;
    size_t used;
    pup_base_t key;

    if (g_kheap_shared_tlsf == NULL) {
        return NULL;
    }

    key = kheap_lock();
    ptr = tlsf_malloc(g_kheap_shared_tlsf, size);
    if (ptr != NULL) {
        used = tlsf_block_size(ptr);
        g_kheap_shared_current_used += used;
        kheap_shared_refresh_peak();
    }
    kheap_unlock(key);

    return ptr;
}

void* kheap_shared_memalign(size_t align, size_t size)
{
    void* ptr;
    size_t used;
    pup_base_t key;

    if (g_kheap_shared_tlsf == NULL) {
        return NULL;
    }

    key = kheap_lock();
    ptr = tlsf_memalign(g_kheap_shared_tlsf, align, size);
    if (ptr != NULL) {
        used = tlsf_block_size(ptr);
        g_kheap_shared_current_used += used;
        kheap_shared_refresh_peak();
    }
    kheap_unlock(key);

    return ptr;
}

void kheap_shared_free(void* ptr)
{
    pup_base_t key;
    size_t used;

    if (ptr == NULL || g_kheap_shared_tlsf == NULL) {
        return;
    }

    key = kheap_lock();
    used = tlsf_block_size(ptr);
    tlsf_free(g_kheap_shared_tlsf, ptr);
    if (g_kheap_shared_current_used >= used) {
        g_kheap_shared_current_used -= used;
    } else {
        g_kheap_shared_current_used = 0;
    }
    kheap_unlock(key);
}

size_t kheap_user_total_size(void)
{
    return g_kheap_user_total_size;
}

size_t kheap_user_used_size(void)
{
    pup_base_t key;
    size_t total_used;

    key = kheap_lock();
    total_used = g_kheap_user_current_used;
    kheap_unlock(key);

    return total_used;
}

size_t kheap_user_free_size(void)
{
    pup_base_t key;
    size_t free_size;

    key = kheap_lock();
    free_size = (g_kheap_user_total_size >= g_kheap_user_current_used)
        ? (g_kheap_user_total_size - g_kheap_user_current_used)
        : 0;
    kheap_unlock(key);

    return free_size;
}

size_t kheap_user_peak_used_size(void)
{
    return g_kheap_user_peak_used;
}

size_t kheap_total_size(void)
{
    return g_kheap_total_size;
}

size_t kheap_used_size(void)
{
    pup_base_t key;
    size_t total_used;

    key = kheap_lock();
    total_used = g_kheap_current_used;
    kheap_unlock(key);

    return total_used;
}

size_t kheap_free_size(void)
{
    pup_base_t key;
    size_t free_size;

    key = kheap_lock();
    free_size = (g_kheap_total_size >= g_kheap_current_used)
        ? (g_kheap_total_size - g_kheap_current_used)
        : 0;
    kheap_unlock(key);

    return free_size;
}

size_t kheap_peak_used_size(void)
{
    return g_kheap_peak_used;
}

size_t kheap_shared_total_size(void)
{
    return g_kheap_shared_total_size;
}

size_t kheap_shared_used_size(void)
{
    pup_base_t key;
    size_t total_used;

    key = kheap_lock();
    total_used = g_kheap_shared_current_used;
    kheap_unlock(key);

    return total_used;
}

size_t kheap_shared_free_size(void)
{
    pup_base_t key;
    size_t free_size;

    key = kheap_lock();
    free_size = (g_kheap_shared_total_size >= g_kheap_shared_current_used)
        ? (g_kheap_shared_total_size - g_kheap_shared_current_used)
        : 0;
    kheap_unlock(key);

    return free_size;
}

size_t kheap_shared_peak_used_size(void)
{
    return g_kheap_shared_peak_used;
}

static void kheap_refresh_peak(void)
{
    if (g_kheap_current_used > g_kheap_peak_used) {
        g_kheap_peak_used = g_kheap_current_used;
    }
}

static void kheap_user_refresh_peak(void)
{
    if (g_kheap_user_current_used > g_kheap_user_peak_used) {
        g_kheap_user_peak_used = g_kheap_user_current_used;
    }
}

static void kheap_shared_refresh_peak(void)
{
    if (g_kheap_shared_current_used > g_kheap_shared_peak_used) {
        g_kheap_shared_peak_used = g_kheap_shared_current_used;
    }
}

void kheap_dump(void)
{
    pup_base_t key;
    size_t total_used;
    
    PUP_PRINTK("Kernel Heap Dump:\n");
    PUP_PRINTK("  Total: %u bytes\n", (unsigned int)g_kheap_total_size);
    
    key = kheap_lock();
    if (g_kheap_tlsf != NULL && g_kheap_pool != NULL) {
        total_used = g_kheap_current_used;
        PUP_PRINTK("  Used: %u bytes\n", (unsigned int)total_used);
        PUP_PRINTK("  Free: %u bytes\n", (unsigned int)(g_kheap_total_size - total_used));
        PUP_PRINTK("  Peak: %u bytes\n", (unsigned int)g_kheap_peak_used);
    }
    kheap_unlock(key);
    
    PUP_PRINTK("User Heap Dump:\n");
    PUP_PRINTK("  Total: %u bytes\n", (unsigned int)g_kheap_user_total_size);
    
    key = kheap_lock();
    if (g_kheap_user_tlsf != NULL && g_kheap_user_pool != NULL) {
        total_used = g_kheap_user_current_used;
        PUP_PRINTK("  Used: %u bytes\n", (unsigned int)total_used);
        PUP_PRINTK("  Free: %u bytes\n", (unsigned int)(g_kheap_user_total_size - total_used));
        PUP_PRINTK("  Peak: %u bytes\n", (unsigned int)g_kheap_user_peak_used);
    }
    kheap_unlock(key);
    
    PUP_PRINTK("Shared Heap Dump:\n");
    PUP_PRINTK("  Total: %u bytes\n", (unsigned int)g_kheap_shared_total_size);
    
    key = kheap_lock();
    if (g_kheap_shared_tlsf != NULL && g_kheap_shared_pool != NULL) {
        total_used = g_kheap_shared_current_used;
        PUP_PRINTK("  Used: %u bytes\n", (unsigned int)total_used);
        PUP_PRINTK("  Free: %u bytes\n", (unsigned int)(g_kheap_shared_total_size - total_used));
        PUP_PRINTK("  Peak: %u bytes\n", (unsigned int)g_kheap_shared_peak_used);
    }
    kheap_unlock(key);
}
