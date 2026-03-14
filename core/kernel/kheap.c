#include "kheap.h"
#include "tlsf.h"
#include "puppy_log.h"
#include "mpu.h"
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
static size_t g_kheap_peak_used = 0;
static tlsf_t g_kheap_user_tlsf = NULL;
static pool_t g_kheap_user_pool = NULL;
static size_t g_kheap_user_total_size = 0;
static size_t g_kheap_user_peak_used = 0;

static void kheap_walker(void* ptr, size_t size, int used, void* user);
static void kheap_refresh_peak(void);
static void kheap_user_refresh_peak(void);

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
    g_kheap_peak_used = 0;

    user_heap_start = (void*)ALIGN_UP((size_t)PUP_MPU_USER_RAM_BASE, 8);
    user_heap_size = ALIGN_DOWN((size_t)PUP_MPU_USER_RAM_SIZE, 8);

    if (user_heap_size >= tlsf_size() + tlsf_pool_overhead() + tlsf_block_size_min()) {
        g_kheap_user_tlsf = tlsf_create_with_pool(user_heap_start, user_heap_size);
        if (g_kheap_user_tlsf != NULL) {
            g_kheap_user_pool = tlsf_get_pool(g_kheap_user_tlsf);
            g_kheap_user_total_size = user_heap_size;
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
}

void* kheap_malloc(size_t size)
{
    void* ptr;
    
    if (g_kheap_tlsf == NULL) {
        return NULL;
    }
    
    ptr = tlsf_malloc(g_kheap_tlsf, size);
    if (ptr != NULL) {
        kheap_refresh_peak();
    }
    
    return ptr;
}

void kheap_free(void* ptr)
{
    if (ptr == NULL || g_kheap_tlsf == NULL) {
        return;
    }
    
    tlsf_free(g_kheap_tlsf, ptr);
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

    if (g_kheap_tlsf == NULL) {
        return NULL;
    }
    
    new_ptr = tlsf_realloc(g_kheap_tlsf, ptr, size);
    if (new_ptr != NULL || size == 0) {
        kheap_refresh_peak();
    }

    return new_ptr;
}

void* kheap_memalign(size_t align, size_t size)
{
    void* ptr;

    if (g_kheap_tlsf == NULL) {
        return NULL;
    }
    
    ptr = tlsf_memalign(g_kheap_tlsf, align, size);
    if (ptr != NULL) {
        kheap_refresh_peak();
    }

    return ptr;
}

void* kheap_user_malloc(size_t size)
{
    void* ptr;

    if (g_kheap_user_tlsf == NULL) {
        return NULL;
    }

    ptr = tlsf_malloc(g_kheap_user_tlsf, size);
    if (ptr != NULL) {
        kheap_user_refresh_peak();
    }

    return ptr;
}

void* kheap_user_memalign(size_t align, size_t size)
{
    void* ptr;

    if (g_kheap_user_tlsf == NULL) {
        return NULL;
    }

    ptr = tlsf_memalign(g_kheap_user_tlsf, align, size);
    if (ptr != NULL) {
        kheap_user_refresh_peak();
    }

    return ptr;
}

void kheap_user_free(void* ptr)
{
    if (ptr == NULL || g_kheap_user_tlsf == NULL) {
        return;
    }

    tlsf_free(g_kheap_user_tlsf, ptr);
}

size_t kheap_user_total_size(void)
{
    return g_kheap_user_total_size;
}

size_t kheap_user_used_size(void)
{
    size_t total_used = 0;

    if (g_kheap_user_tlsf != NULL && g_kheap_user_pool != NULL) {
        tlsf_walk_pool(g_kheap_user_pool, kheap_walker, &total_used);
    }

    return total_used;
}

size_t kheap_user_free_size(void)
{
    return g_kheap_user_total_size - kheap_user_used_size();
}

size_t kheap_user_peak_used_size(void)
{
    return g_kheap_user_peak_used;
}

size_t kheap_total_size(void)
{
    return g_kheap_total_size;
}

static void kheap_walker(void* ptr, size_t size, int used, void* user)
{
    size_t* total_used = (size_t*)user;
    
    if (used) {
        *total_used += size;
    }
}

size_t kheap_used_size(void)
{
    size_t total_used = 0;
    
    if (g_kheap_tlsf != NULL && g_kheap_pool != NULL) {
        tlsf_walk_pool(g_kheap_pool, kheap_walker, &total_used);
    }
    
    return total_used;
}

size_t kheap_free_size(void)
{
    return g_kheap_total_size - kheap_used_size();
}

size_t kheap_peak_used_size(void)
{
    return g_kheap_peak_used;
}

static void kheap_refresh_peak(void)
{
    size_t used = kheap_used_size();

    if (used > g_kheap_peak_used) {
        g_kheap_peak_used = used;
    }
}

static void kheap_user_refresh_peak(void)
{
    size_t used = kheap_user_used_size();

    if (used > g_kheap_user_peak_used) {
        g_kheap_user_peak_used = used;
    }
}

void kheap_dump(void)
{
    size_t total_used = 0;
    
    PUP_PRINTK("Kernel Heap Dump:\n");
    PUP_PRINTK("  Total: %u bytes\n", (unsigned int)g_kheap_total_size);
    
    if (g_kheap_tlsf != NULL && g_kheap_pool != NULL) {
        tlsf_walk_pool(g_kheap_pool, kheap_walker, &total_used);
        PUP_PRINTK("  Used: %u bytes\n", (unsigned int)total_used);
        PUP_PRINTK("  Free: %u bytes\n", (unsigned int)(g_kheap_total_size - total_used));
        PUP_PRINTK("  Peak: %u bytes\n", (unsigned int)g_kheap_peak_used);
    }
}
