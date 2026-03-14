#ifndef __KHEAP_H__
#define __KHEAP_H__

#include <stddef.h>

void kheap_init(void);

void* kheap_malloc(size_t size);
void kheap_free(void* ptr);
void* kheap_calloc(size_t nmemb, size_t size);
void* kheap_realloc(void* ptr, size_t size);
void* kheap_memalign(size_t align, size_t size);

void* kheap_user_malloc(size_t size);
void* kheap_user_memalign(size_t align, size_t size);
void kheap_user_free(void* ptr);
size_t kheap_user_total_size(void);
size_t kheap_user_used_size(void);
size_t kheap_user_free_size(void);
size_t kheap_user_peak_used_size(void);

size_t kheap_total_size(void);
size_t kheap_used_size(void);
size_t kheap_free_size(void);
size_t kheap_peak_used_size(void);

void kheap_dump(void);

#endif
