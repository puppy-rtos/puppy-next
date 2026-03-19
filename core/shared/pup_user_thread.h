#ifndef __PUP_USER_THREAD_H__
#define __PUP_USER_THREAD_H__

typedef struct {
    void* arg;
    void* stackaddr;
    unsigned int stacksize;
    int priority;
    const char* name;
} pup_user_thread_create_t;

#endif