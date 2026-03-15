#include "libc.h"
#include "syscall.h"

static inline int get_cpu_mode(void) {
    volatile int tmp = 0;
    __asm volatile("mrs %0, CONTROL;"
    : "=r" (tmp)
        :
        : "memory");
    return tmp & 0x1;
}

static void* thread1_func(void* arg) {
    int id = *(int*)arg;
    for (int i = 0; i < 3; i++) {
        printf("[Thread%d] Running, i=%d\n", id, i);
        sched_yield();
    }
    printf("[Thread%d] Exiting\n", id);
    return (void*)0;
}

static void* thread2_func(void* arg) {
    int id = *(int*)arg;
    for (int i = 0; i < 3; i++) {
        printf("[Thread%d] Running, i=%d\n", id, i);
        sched_yield();
    }
    printf("[Thread%d] Exiting\n", id);
    return (void*)1;
}

#define USER_THREAD_STACK_SIZE  1024

static char thread1_stack[USER_THREAD_STACK_SIZE];
static char thread2_stack[USER_THREAD_STACK_SIZE];

static __attribute__((unused)) void test_invalid_memory_access(void) {
    printf("\n=== MPU Protection Test ===\n");
    
    volatile int* kernel_ram = (volatile int*)0x20000000;

    printf("[MPU Test] Testing invalid kernel RAM access...\n");
    printf("[MPU Test] This should trigger a MemManage fault\n");
    *kernel_ram = 0xDEADBEEF;
    printf("[MPU Test] ERROR: Should not reach here!\n");
}

void app_main(const char* arg) {
    const char* app_arg = (arg && arg[0]) ? arg : "(no-arg)";

    if (get_cpu_mode()) {
        printf("[Hello App] Running in USER mode (unprivileged)\n");
    } else {
        printf("[Hello App] Running in PRIVILEGED mode\n");
    }

    printf("[Hello App] arg: %s\n", app_arg);

    printf("=== Thread Test Start ===\n");
    
    int id1 = 1, id2 = 2;
    pthread_t tid1, tid2;
    
    pthread_attr_t attr1, attr2;
    pthread_attr_init(&attr1);
    pthread_attr_setstackaddr(&attr1, thread1_stack);
    pthread_attr_setstacksize(&attr1, USER_THREAD_STACK_SIZE);
    pthread_attr_setname(&attr1, "user_t1");
    
    pthread_attr_init(&attr2);
    pthread_attr_setstackaddr(&attr2, thread2_stack);
    pthread_attr_setstacksize(&attr2, USER_THREAD_STACK_SIZE);
    pthread_attr_setname(&attr2, "user_t2");
    
    int t1_created = 0;
    int t2_created = 0;

    printf("[Hello App] Creating thread1...\n");
    int ret = pthread_create(&tid1, &attr1, thread1_func, &id1);
    if (ret == 0) {
        t1_created = 1;
        printf("[Hello App] Thread1 created, id=%d\n", (int)(intptr_t)tid1);
    } else {
        printf("[Hello App] Thread1 create failed\n");
    }
    
    printf("[Hello App] Creating thread2...\n");
    ret = pthread_create(&tid2, &attr2, thread2_func, &id2);
    if (ret == 0) {
        t2_created = 1;
        printf("[Hello App] Thread2 created, id=%d\n", (int)(intptr_t)tid2);
    } else {
        printf("[Hello App] Thread2 create failed\n");
    }
    
    printf("[Hello App] Yielding to let threads run...\n");
    for (int i = 0; i < 8; i++) {
        printf("[Hello App] yield %d\n", i);    
        sched_yield();
    }

    printf("[Hello App] Joining child threads...\n");
    if (t1_created) {
        pthread_join(tid1, NULL);
    }
    if (t2_created) {
        pthread_join(tid2, NULL);
    }

    printf("=== Thread Test Complete ===\n");
    
    test_invalid_memory_access();
    
    pthread_exit((void*)0);
}
