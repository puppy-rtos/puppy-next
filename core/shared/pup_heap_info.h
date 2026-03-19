#ifndef __PUP_HEAP_INFO_H__
#define __PUP_HEAP_INFO_H__

typedef struct {
    unsigned int kernel_total;
    unsigned int kernel_used;
    unsigned int kernel_free;
    unsigned int kernel_peak;
    unsigned int user_total;
    unsigned int user_used;
    unsigned int user_free;
    unsigned int user_peak;
    unsigned int shared_total;
    unsigned int shared_used;
    unsigned int shared_free;
    unsigned int shared_peak;
} pup_heap_info_t;

#endif
