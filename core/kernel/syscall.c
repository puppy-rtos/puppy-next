#include "syscall.h"
#include "libcm.h"
#include <puppy_core.h>
#include "app_loader.h"
#include "app_manager.h"
#include "puppy_log.h"
#include "mpu.h"
#include "pup_fs.h"
#include "pup_devfs.h"
#include "pup_userfs.h"
#include "shared/pup_userfs_proto.h"
#include "pup_console.h"
#include "kheap.h"
#include "pup_sem.h"
#include "shared/pup_user_thread.h"
#include "stack_guard.h"
#include "dev_irq.h"

void syscall_uart_rx_init(void)
{
    pup_console_init();
}

#define MAX_USER_THREADS  4

typedef struct {
    pthread_t thread;
    int used;
    int exited;
    sem_t exit_sem;
    void* (*entry)(void*);
    void* arg;
    char* stack;
    const char* name;
    unsigned int got_base;
} user_thread_t;

static user_thread_t user_threads[MAX_USER_THREADS];

static void sys_exit_no_return(void) __attribute__((noreturn));
static void sys_stack_guard_terminate_current_thread(const char* stage) __attribute__((noreturn));
static long sys_exit(long arg1, long arg2, long arg3, long arg4);

static int sys_user_range_valid(app_process_t* app, const void* ptr, unsigned int size);
static int sys_user_string_valid(app_process_t* app, const char* str, unsigned int max_len);

static void sys_exit_no_return(void) {
    while (1) {
    }
}

static void sys_stack_guard_terminate_current_thread(const char* stage) {
    pthread_t current_thread = pthread_self();

    PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL,
                  "Stack overflow detected at syscall %s for thread '%s'",
                  stage,
                  current_thread ? pthread_getname(current_thread) : "unknown");

    (void)sys_exit(-1, 0, 0, 0);
    sys_exit_no_return();
}

static long sys_write(long arg1, long arg2, long arg3, long arg4) {
    int fd = (int)arg1;
    const char* buf = (const char*)arg2;
    int len = (int)arg3;

    if (!buf || len <= 0) {
        return 0;
    }

    if (fd == SYS_FD_STDOUT || fd == SYS_FD_STDERR) {
        return pup_console_write(buf, len);
    }

    if (fd < 0) {
        return -1;
    }

    return pup_write(fd, buf, len);
}

static long sys_read(long arg1, long arg2, long arg3, long arg4) {
    int fd = (int)arg1;
    char* buf = (char*)arg2;
    int len = (int)arg3;

    if (buf == NULL || len <= 0) {
        return -1;
    }

    if (fd == SYS_FD_STDIN) {
        return pup_console_read(buf, len);
    }

    if (fd < 0) {
        return -1;
    }

    return pup_read(fd, buf, len);
}

static long sys_exit(long arg1, long arg2, long arg3, long arg4) {
    int exit_code = (int)arg1;
    app_process_t* app;
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "sys_exit: code=%d", exit_code);
    
    pthread_t self = pthread_self();
    for (int i = 0; i < MAX_USER_THREADS; i++) {
        if (user_threads[i].thread == self) {
            user_threads[i].exited = 1;
            sem_post(&user_threads[i].exit_sem);
            break;
        }
    }
    
    app = app_find_by_thread(self);
    if (app && app->thread == self) {
        app->exit_code = exit_code;
        app->state = APP_STATE_EXITED;
        app->thread = PUP_NULL;
        PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "App '%s' (pid=%d) exiting with code %d",
                      app->name, app->pid, exit_code);
        
        /* Restore kernel MPU protection when app exits
         * This ensures the next app or shell runs with correct MPU config */
        pup_mpu_setup_kernel_protection();
    }
    
    pthread_exit((void*)exit_code);

    /*
     * User-thread exit must not return through the syscall trampoline.
     * If a dead thread falls back into user_thread_entry it will hit the
     * post-SVC spin loop and stall the system.
     */
    sys_exit_no_return();
}

static long sys_yield(long arg1, long arg2, long arg3, long arg4) {
    sched_yield();
    return 0;
}

extern void user_thread_exit(int code) __attribute__((noreturn));
extern void user_thread_entry(void* (*entry)(void*), void* arg, unsigned int got_base);

static void* user_thread_wrapper(void* arg) {
    user_thread_t* ut = (user_thread_t*)arg;
    
    void* (*entry)(void*) = ut->entry;
    void* thread_arg = ut->arg;
    unsigned int got_base = ut->got_base;
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "User thread started: entry=0x%08x, got_base=0x%08x", 
                  (unsigned int)entry, got_base);
    
    user_thread_entry(entry, thread_arg, got_base);
    
    return NULL;
}

static long sys_thread_create(long arg1, long arg2, long arg3, long arg4) {
    void* (*entry)(void*) = (void* (*)(void*))arg1;
    const pup_user_thread_create_t* params = (const pup_user_thread_create_t*)arg2;
    void* thread_arg;
    void* stackaddr;
    unsigned int stacksize;
    int priority;
    const char* thread_name = "pthread";
    app_process_t* app = app_get_current();

    if (!params) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_thread_create: missing params");
        return -1;
    }

    thread_arg = params->arg;
    stackaddr = params->stackaddr;
    stacksize = params->stacksize;
    priority = params->priority;
    if (params->name && params->name[0] != '\0') {
        thread_name = params->name;
    }
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL,
                  "sys_thread_create: entry=0x%08x, arg=0x%08x, stack=0x%08x, size=%d, prio=%d, name=%s",
                  (unsigned int)entry,
                  (unsigned int)thread_arg,
                  (unsigned int)stackaddr,
                  stacksize,
                  priority,
                  thread_name);
    
    if (!stackaddr || stacksize == 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_thread_create: invalid stack");
        return -1;
    }
    
    if ((unsigned int)stackaddr < PUP_MPU_USER_RAM_BASE || 
        (unsigned int)stackaddr + stacksize > PUP_MPU_USER_RAM_BASE + PUP_MPU_USER_RAM_SIZE) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_thread_create: stack not in user RAM");
        return -1;
    }
    
    int i;
    for (i = 0; i < MAX_USER_THREADS; i++) {
        if (!user_threads[i].used) {
            break;
        }
    }
    
    if (i >= MAX_USER_THREADS) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_thread_create: no free slot");
        return -1;
    }
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "sys_thread_create: using slot %d", i);
    
    user_threads[i].used = 1;
    user_threads[i].exited = 0;
    sem_init(&user_threads[i].exit_sem, 0, 0);
    user_threads[i].entry = entry;
    user_threads[i].arg = thread_arg;
    user_threads[i].stack = stackaddr;
    user_threads[i].name = thread_name;
    user_threads[i].got_base = app ? app->image.got_base : 0;
    
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstackaddr(&attr, user_threads[i].stack);
    pthread_attr_setstacksize(&attr, stacksize);
    pthread_attr_setpriority(&attr, priority);
    pthread_attr_setname(&attr, (char*)user_threads[i].name);
    pthread_attr_setuser(&attr, 1);
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "sys_thread_create: calling pthread_create");
    int ret = pthread_create(&user_threads[i].thread, &attr, user_thread_wrapper, &user_threads[i]);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "sys_thread_create: pthread_create returned %d", ret);
    
    if (ret != 0) {
        user_threads[i].used = 0;
        user_threads[i].exited = 0;
        sem_destroy(&user_threads[i].exit_sem);
        user_threads[i].thread = PUP_NULL;
        user_threads[i].name = PUP_NULL;
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_thread_create failed: %d", ret);
        return -1;
    }
    
    PUP_LOG_INFO(PUP_LOG_MODULE_SYSCALL, "sys_thread_create: thread=%d created", i);
    return i;
}

static long sys_thread_join(long arg1, long arg2, long arg3, long arg4) {
    int thread_id = (int)arg1;
    pthread_t thread;
    int ret;

    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "sys_thread_join: thread_id=%d", thread_id);

    if (thread_id < 0 || thread_id >= MAX_USER_THREADS) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_thread_join: invalid thread id %d", thread_id);
        return -1;
    }

    if (!user_threads[thread_id].used || user_threads[thread_id].thread == PUP_NULL) {
        PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "sys_thread_join: thread %d already reclaimed", thread_id);
        return 0;
    }

    thread = user_threads[thread_id].thread;

    if (!user_threads[thread_id].exited && !pup_thread_is_dead(thread)) {
        PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL,
                      "sys_thread_join: wait for thread %d to exit or be terminated",
                      thread_id);
        while (!user_threads[thread_id].exited && !pup_thread_is_dead(thread)) {
            sched_yield();
        }
    }

    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "sys_thread_join: reclaim thread %d", thread_id);
    ret = pthread_join(thread, NULL);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "sys_thread_join: pthread_join returned %d", ret);

    if (ret == 0) {
        user_threads[thread_id].used = 0;
        user_threads[thread_id].exited = 0;
        sem_destroy(&user_threads[thread_id].exit_sem);
        user_threads[thread_id].thread = PUP_NULL;
        user_threads[thread_id].entry = PUP_NULL;
        user_threads[thread_id].arg = PUP_NULL;
        user_threads[thread_id].stack = PUP_NULL;
        user_threads[thread_id].name = PUP_NULL;
        user_threads[thread_id].got_base = 0;
    }
    return ret;
}

static long sys_open(long arg1, long arg2, long arg3, long arg4) {
    const char* path = (const char*)arg1;
    int flags = (int)arg2;
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "sys_open: path=%s, flags=0x%x", path, flags);
    
    return pup_open(path, flags);
}

static long sys_close(long arg1, long arg2, long arg3, long arg4) {
    int fd = (int)arg1;
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "sys_close: fd=%d", fd);
    
    return pup_close(fd);
}

static long sys_ioctl(long arg1, long arg2, long arg3, long arg4) {
    int fd = (int)arg1;
    int cmd = (int)arg2;
    void* arg = (void*)arg3;
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "sys_ioctl: fd=%d, cmd=0x%x, arg=0x%x", fd, cmd, (unsigned int)arg);
    
    return pup_ioctl(fd, cmd, arg);
}

static long sys_getdents(long arg1, long arg2, long arg3, long arg4) {
    int fd = (int)arg1;
    struct dirent* dirp = (struct dirent*)arg2;
    unsigned int count = (unsigned int)arg3;
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "sys_getdents: fd=%d, count=%d", fd, count);
    
    return pup_getdents(fd, dirp, count);
}

extern void pup_pthread_list(void);

static long sys_ps(long arg1, long arg2, long arg3, long arg4) {
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "sys_ps: listing threads");
    pup_pthread_list();
    return 0;
}

static long sys_execve(long arg1, long arg2, long arg3, long arg4) {
    const char* path = (const char*)arg1;
    const char* name = (const char*)arg2;
    int wait = (int)arg3;
    const char* app_arg = (const char*)arg4;
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "sys_execve: path=%s, name=%s, wait=%d, has_arg=%d", 
                  path, name ? name : "NULL", wait, (app_arg && app_arg[0]) ? 1 : 0);
    
    if (!path) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_execve: path is NULL");
        return -1;
    }
    
    const char* app_name = name ? name : path;

    int result = app_exec(app_name, path,
                          (app_arg && app_arg[0] != '\0') ? app_arg : NULL,
                          wait);
    if (result < 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_execve: failed to exec app");
        return -1;
    }
    
    PUP_LOG_INFO(PUP_LOG_MODULE_SYSCALL, "sys_execve: app '%s' %s (result=%d)", 
                 app_name, wait ? "completed" : "started", result);
    
    return result;
}

static long sys_waitpid(long arg1, long arg2, long arg3, long arg4) {
    int pid = (int)arg1;
    int* status = (int*)arg2;
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "sys_waitpid: pid=%d", pid);
    
    app_process_t* app = app_get_by_pid(pid);
    if (!app) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_waitpid: invalid pid %d", pid);
        return -1;
    }
    
    app_wait(pid);
    
    if (status) {
        *status = app->exit_code;
    }
    
    int exit_code = app->exit_code;
    app_free_memory(pid);
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "sys_waitpid: app %d exited with code %d", pid, exit_code);
    
    return pid;
}

static long sys_getpid(long arg1, long arg2, long arg3, long arg4) {
    app_process_t* app = app_get_current();

    if (!app) {
        return 0;
    }

    return app->pid;
}

static long sys_delay(long arg1, long arg2, long arg3, long arg4) {
    int ticks = (int)arg1;
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "sys_delay: ticks=%d", ticks);
    
    return pthread_delay(ticks);
}

static long sys_apps(long arg1, long arg2, long arg3, long arg4) {
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "sys_apps: listing apps");
    app_list();
    return 0;
}

static long sys_tick_get(long arg1, long arg2, long arg3, long arg4) {
    return (long)pup_tick_get();
}

static long sys_free(long arg1, long arg2, long arg3, long arg4) {
    pup_heap_info_t* info = (pup_heap_info_t*)arg1;

    if (!info) {
        return -1;
    }

    info->kernel_total = (unsigned int)kheap_total_size();
    info->kernel_used = (unsigned int)kheap_used_size();
    info->kernel_free = (unsigned int)kheap_free_size();
    info->kernel_peak = (unsigned int)kheap_peak_used_size();
    info->user_total = (unsigned int)kheap_user_total_size();
    info->user_used = (unsigned int)kheap_user_used_size();
    info->user_free = (unsigned int)kheap_user_free_size();
    info->user_peak = (unsigned int)kheap_user_peak_used_size();
    info->shared_total = (unsigned int)kheap_shared_total_size();
    info->shared_used = (unsigned int)kheap_shared_used_size();
    info->shared_free = (unsigned int)kheap_shared_free_size();
    info->shared_peak = (unsigned int)kheap_shared_peak_used_size();

    return 0;
}

static long sys_sem_open(long arg1, long arg2, long arg3, long arg4) {
    const char *name  = (const char *)arg1;
    int         flags = (int)arg2;
    unsigned int value = (unsigned int)arg3;
    return (long)pup_sem_open(name, flags, value);
}

static long sys_sem_close(long arg1, long arg2, long arg3, long arg4) {
    return (long)pup_sem_close((int)arg1);
}

static long sys_sem_wait(long arg1, long arg2, long arg3, long arg4) {
    return (long)pup_sem_wait((int)arg1);
}

static long sys_sem_post(long arg1, long arg2, long arg3, long arg4) {
    return (long)pup_sem_post((int)arg1);
}

static long sys_shared_malloc(long arg1, long arg2, long arg3, long arg4) {
    size_t size = (size_t)arg1;
    void* ptr = kheap_shared_malloc(size);
    return (long)ptr;
}

static long sys_shared_free(long arg1, long arg2, long arg3, long arg4) {
    void* ptr = (void*)arg1;
    kheap_shared_free(ptr);
    return 0;
}

static long sys_shared_memalign(long arg1, long arg2, long arg3, long arg4) {
    size_t align = (size_t)arg1;
    size_t size = (size_t)arg2;
    void* ptr = kheap_shared_memalign(align, size);
    return (long)ptr;
}

static int sys_user_range_valid(app_process_t* app, const void* ptr, unsigned int size) {
    unsigned int app_start;
    unsigned int app_end;
    unsigned int ptr_start;
    unsigned int ptr_end;

    if (!app || !ptr || size == 0) {
        return 0;
    }

    app_start = app->memory_base;
    app_end = app->memory_base + app->memory_size;
    ptr_start = (unsigned int)ptr;
    ptr_end = ptr_start + size;

    if (app_end < app_start || ptr_end < ptr_start) {
        return 0;
    }

    if ((ptr_start >= app_start) && (ptr_end <= app_end)) {
        return 1;
    }

    if (app->flash_addr && app->flash_size > 0) {
        unsigned int flash_start = app->flash_addr;
        unsigned int flash_end = app->flash_addr + app->flash_size;
        if ((ptr_start >= flash_start) && (ptr_end <= flash_end)) {
            return 1;
        }
    }

    return 0;
}

static int sys_user_string_valid(app_process_t* app, const char* str, unsigned int max_len) {
    unsigned int i;

    if (!app || !str || max_len == 0) {
        return 0;
    }

    for (i = 0; i < max_len; i++) {
        if (!sys_user_range_valid(app, str + i, 1)) {
            return 0;
        }

        if (str[i] == '\0') {
            return 1;
        }
    }

    return 0;
}

static long sys_app_ram_range(long arg1, long arg2, long arg3, long arg4) {
    pup_app_ram_range_t* range = (pup_app_ram_range_t*)arg1;
    app_process_t* app = app_get_current();

    if (!range || !app) {
        return -1;
    }

    if (!sys_user_range_valid(app, range, sizeof(*range))) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_app_ram_range: invalid user buffer 0x%08x", (unsigned int)range);
        return -1;
    }

    range->ram_start = app->memory_base;
    range->ram_end = app->memory_base + app->memory_size;
    return 0;
}

static long sys_devfs_register(long arg1, long arg2, long arg3, long arg4) {
    const pup_user_devfs_registration_t* registration = (const pup_user_devfs_registration_t*)arg1;
    app_process_t* app = app_get_current();

    if (!app || !registration) {
        return -1;
    }

    if (!sys_user_range_valid(app, registration, sizeof(*registration))) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_devfs_register: invalid registration buffer");
        return -1;
    }

    if (!sys_user_string_valid(app, registration->name, PUP_DEVFS_MAX_NAME_LEN + 1U)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_devfs_register: invalid device name");
        return -1;
    }

    if (registration->channel_id < 0 || !pup_channel_is_owner(registration->channel_id, app->pid)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL,
                      "sys_devfs_register: invalid channel owner (pid=%d ch=%d)",
                      app->pid, registration->channel_id);
        return -1;
    }

    return pup_devfs_register_user_device(registration, app->pid);
}

static long sys_devfs_unregister(long arg1, long arg2, long arg3, long arg4) {
    const char* name = (const char*)arg1;
    app_process_t* app = app_get_current();

    if (!app || !name) {
        return -1;
    }

    if (!sys_user_string_valid(app, name, PUP_DEVFS_MAX_NAME_LEN + 1U)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_devfs_unregister: invalid device name");
        return -1;
    }

    return pup_devfs_unregister_user_device(name, app->pid);
}

static long sys_irq_subscribe(long arg1, long arg2, long arg3, long arg4) {
    const pup_user_irq_subscription_t* subscription = (const pup_user_irq_subscription_t*)arg1;
    app_process_t* app = app_get_current();

    if (!app || !subscription) {
        return -1;
    }

    if (!sys_user_range_valid(app, subscription, sizeof(*subscription))) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_irq_subscribe: invalid subscription buffer");
        return -1;
    }

    if (subscription->channel_id >= 0 &&
        !pup_channel_is_owner(subscription->channel_id, app->pid)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL,
                      "sys_irq_subscribe: invalid channel owner pid=%d ch=%d",
                      app->pid, subscription->channel_id);
        return -1;
    }

    return dev_irq_subscribe(subscription, app->pid);
}

static long sys_irq_unsubscribe(long arg1, long arg2, long arg3, long arg4) {
    int handle = (int)arg1;
    app_process_t* app = app_get_current();

    (void)arg2;
    (void)arg3;
    (void)arg4;

    if (!app) {
        return -1;
    }

    return dev_irq_unsubscribe(handle, app->pid);
}

static long sys_irq_wait(long arg1, long arg2, long arg3, long arg4) {
    int handle = (int)arg1;
    int timeout_ticks = (int)arg2;
    app_process_t* app = app_get_current();

    (void)arg3;
    (void)arg4;

    if (!app) {
        return -1;
    }

    return dev_irq_wait(handle, app->pid, timeout_ticks);
}

static long sys_irq_control(long arg1, long arg2, long arg3, long arg4)
{
    unsigned int irq_num = (unsigned int)arg1;
    int cmd = (int)arg2;
    int value = (int)arg3;
    app_process_t* app = app_get_current();

    (void)arg4;

    if (!app) {
        return -1;
    }

    return dev_irq_control(irq_num, cmd, value, app->pid);
}

void syscall_cleanup_owner_resources(int owner_pid)
{
    dev_irq_cleanup_owner(owner_pid);
}

/* -----------------------------------------------------------------------
 * Channel + Message IPC syscalls
 * --------------------------------------------------------------------- */

static long sys_channel_create(long arg1, long arg2, long arg3, long arg4)
{
    app_process_t *app = app_get_current();
    if (!app) {
        return -1;
    }
    return (long)pup_channel_create(app->pid);
}

static long sys_channel_destroy(long arg1, long arg2, long arg3, long arg4)
{
    int channel_id = (int)arg1;
    app_process_t *app = app_get_current();
    if (!app) {
        return -1;
    }
    /* Only the owner may destroy its channel */
    if (!pup_channel_is_owner(channel_id, app->pid)) {
        return -1;
    }
    return (long)pup_channel_destroy(channel_id);
}

static long sys_channel_call(long arg1, long arg2, long arg3, long arg4)
{
    int   channel_id = (int)arg1;
    void *buf        = (void *)arg2;
    int   buf_sz     = (int)arg3;
    app_process_t *app = app_get_current();

    if (!app || !buf || buf_sz < 0) {
        return -1;
    }
    /* buf must be in shared memory (accessible by both sides) */
    return pup_channel_send(channel_id, buf, buf_sz);
}

static long sys_channel_receive(long arg1, long arg2, long arg3, long arg4)
{
    int                channel_id = (int)arg1;
    pup_channel_msg_t *out_msg    = (pup_channel_msg_t *)arg2;
    app_process_t     *app        = app_get_current();
    pup_channel_msg_t  kernel_msg;
    int                cookie;

    if (!app || !out_msg) {
        return -1;
    }
    if (!pup_channel_is_owner(channel_id, app->pid)) {
        return -1;
    }
    if (!sys_user_range_valid(app, out_msg, sizeof(*out_msg))) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL,
                      "sys_channel_receive: invalid out_msg buffer");
        return -1;
    }

    cookie = pup_channel_receive(channel_id, &kernel_msg);
    if (cookie >= 0) {
        /* Copy message to user memory */
        *out_msg = kernel_msg;
    }
    return (long)cookie;
}

static long sys_channel_reply(long arg1, long arg2, long arg3, long arg4)
{
    int  channel_id = (int)arg1;
    int  cookie     = (int)arg2;
    long result     = arg3;

    app_process_t *app = app_get_current();
    if (!app || !pup_channel_is_owner(channel_id, app->pid)) {
        return -1;
    }

    return (long)pup_channel_reply(channel_id, cookie, result);
}

static long sys_userfs_register(long arg1, long arg2, long arg3, long arg4)
{
    const char *fs_type = (const char *)arg1;
    int channel_id = (int)arg2;
    app_process_t *app = app_get_current();

    if (!app || !fs_type) {
        return -1;
    }

    if (!sys_user_string_valid(app, fs_type, 32)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_userfs_register: invalid fs_type");
        return -1;
    }

    return (long)pup_userfs_register_type(fs_type, app->pid, channel_id);
}

static long sys_userfs_unregister(long arg1, long arg2, long arg3, long arg4)
{
    const char *fs_type = (const char *)arg1;
    app_process_t *app = app_get_current();

    if (!app || !fs_type) {
        return -1;
    }

    if (!sys_user_string_valid(app, fs_type, 32)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_userfs_unregister: invalid fs_type");
        return -1;
    }

    return (long)pup_userfs_unregister_type(fs_type, app->pid);
}

static long sys_userfs_mount(long arg1, long arg2, long arg3, long arg4)
{
    const char *fs_type = (const char *)arg1;
    const char *device = (const char *)arg2;
    const char *mount_path = (const char *)arg3;
    unsigned long flags = (unsigned long)arg4;
    app_process_t *app = app_get_current();
    int instance_id;
    int ret;

    if (!app || !fs_type || !mount_path) {
        return -1;
    }

    if (!sys_user_string_valid(app, fs_type, 32)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_userfs_mount: invalid fs_type");
        return -1;
    }

    if (!sys_user_string_valid(app, mount_path, PUP_FS_PATH_MAX)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_userfs_mount: invalid mount_path");
        return -1;
    }

    if (device && !sys_user_string_valid(app, device, PUP_FS_PATH_MAX)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_userfs_mount: invalid device");
        return -1;
    }

    instance_id = pup_userfs_create_instance(fs_type, device, mount_path, flags, PUP_NULL, app->pid);
    if (instance_id < 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_userfs_mount: create instance failed");
        return instance_id;
    }
    
    ret = pup_fs_mount(device, mount_path, "userfs", flags, PUP_NULL);
    if (ret < 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_userfs_mount: pup_fs_mount failed: %d", ret);
        pup_userfs_destroy_instance(instance_id, app->pid);
        return ret;
    }
    
    return instance_id;
}

static long sys_userfs_unmount(long arg1, long arg2, long arg3, long arg4)
{
    const char *mount_path = (const char *)arg1;
    app_process_t *app = app_get_current();

    if (!app || !mount_path) {
        return -1;
    }

    if (!sys_user_string_valid(app, mount_path, PUP_FS_PATH_MAX)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_userfs_unmount: invalid mount_path");
        return -1;
    }

    return (long)pup_fs_unmount(mount_path);
}

static long sys_userfs_mkfs(long arg1, long arg2, long arg3, long arg4)
{
    const char *fs_type = (const char *)arg1;
    const char *device = (const char *)arg2;
    app_process_t *app = app_get_current();
    int channel_id;
    pup_userfs_mkfs_req_t *req;
    long result;
    
    (void)arg3;
    (void)arg4;
    
    if (!app || !fs_type) {
        return -1;
    }
    
    if (!sys_user_string_valid(app, fs_type, 32)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_userfs_mkfs: invalid fs_type");
        return -1;
    }
    
    if (device && !sys_user_string_valid(app, device, PUP_FS_PATH_MAX)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_userfs_mkfs: invalid device");
        return -1;
    }
    
    channel_id = pup_userfs_find_channel_by_type(fs_type);
    if (channel_id < 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "sys_userfs_mkfs: fs_type '%s' not found", fs_type);
        return -1;
    }
    
    req = (pup_userfs_mkfs_req_t *)kheap_shared_malloc(sizeof(pup_userfs_mkfs_req_t));
    if (!req) {
        return -1;
    }
    
    libcm_memset(req, 0, sizeof(pup_userfs_mkfs_req_t));
    req->header.op = PUP_USERFS_OP_MKFS;
    req->header.instance_id = -1;
    req->header.fd = -1;
    if (device) {
        libcm_strncpy(req->device, device, PUP_USERFS_PATH_MAX - 1);
    }
    req->block_size = 0;
    req->block_count = 0;
    
    result = pup_channel_send(channel_id, req, sizeof(pup_userfs_mkfs_req_t));
    kheap_shared_free(req);
    
    return result;
}

static const syscall_func_t syscall_table[] = {
    [SYS_WRITE] = sys_write,
    [SYS_READ] = sys_read,
    [SYS_EXIT] = sys_exit,
    [SYS_YIELD] = sys_yield,
    [SYS_THREAD_CREATE] = sys_thread_create,
    [SYS_THREAD_JOIN] = sys_thread_join,
    [SYS_OPEN] = sys_open,
    [SYS_CLOSE] = sys_close,
    [SYS_IOCTL] = sys_ioctl,
    [SYS_GETDENTS] = sys_getdents,
    [SYS_PS] = sys_ps,
    [SYS_EXECVE] = sys_execve,
    [SYS_WAITPID] = sys_waitpid,
    [SYS_GETPID] = sys_getpid,
    [SYS_DELAY] = sys_delay,
    [SYS_APPS] = sys_apps,
    [SYS_TICK_GET] = sys_tick_get,
    [SYS_FREE] = sys_free,
    [SYS_SEM_OPEN]  = sys_sem_open,
    [SYS_SEM_CLOSE] = sys_sem_close,
    [SYS_SEM_WAIT]  = sys_sem_wait,
    [SYS_SEM_POST]  = sys_sem_post,
    [SYS_SHARED_MALLOC] = sys_shared_malloc,
    [SYS_SHARED_FREE] = sys_shared_free,
    [SYS_SHARED_MEMALIGN] = sys_shared_memalign,
    [SYS_APP_RAM_RANGE] = sys_app_ram_range,
    [SYS_DEVFS_REGISTER] = sys_devfs_register,
    [SYS_DEVFS_UNREGISTER] = sys_devfs_unregister,
    [SYS_IRQ_SUBSCRIBE] = sys_irq_subscribe,
    [SYS_IRQ_UNSUBSCRIBE] = sys_irq_unsubscribe,
    [SYS_IRQ_WAIT] = sys_irq_wait,
    [SYS_IRQ_CONTROL] = sys_irq_control,
    [SYS_CHANNEL_CREATE]  = sys_channel_create,
    [SYS_CHANNEL_DESTROY] = sys_channel_destroy,
    [SYS_CHANNEL_CALL]    = sys_channel_call,
    [SYS_CHANNEL_RECEIVE] = sys_channel_receive,
    [SYS_CHANNEL_REPLY]   = sys_channel_reply,
    [SYS_USERFS_REGISTER]   = sys_userfs_register,
    [SYS_USERFS_UNREGISTER] = sys_userfs_unregister,
    [SYS_USERFS_MOUNT]      = sys_userfs_mount,
    [SYS_USERFS_UNMOUNT]    = sys_userfs_unmount,
    [SYS_USERFS_MKFS]       = sys_userfs_mkfs,
};

#define SYSCALL_TABLE_SIZE (sizeof(syscall_table) / sizeof(syscall_func_t))

long syscall_handler(long syscall_num, long arg1, long arg2, long arg3, long arg4) {
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "syscall_handler: num=%d, args=%d,%d,%d,%d", 
               (int)syscall_num, (int)arg1, (int)arg2, (int)arg3, (int)arg4);
    
    /* Check stack at syscall entry */
    pthread_t current_thread = pthread_self();
    if (pup_stack_guard_check_syscall_context(current_thread) != 0) {
        sys_stack_guard_terminate_current_thread("entry");
    }
    
    if (arch_in_irq()) {
        PUP_LOG_WARNING(PUP_LOG_MODULE_SYSCALL, "Executing in interrupt context");
    }
    
    if (!arch_in_privileged()) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "CPU Mode: UNPRIVILEGED (user mode)!");
    }
    
    if (syscall_num < 0 || syscall_num >= SYSCALL_TABLE_SIZE) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "Invalid syscall number: %d", (int)syscall_num);
        return -1;
    }
    
    syscall_func_t func = syscall_table[syscall_num];
    if (!func) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_SYSCALL, "Syscall not implemented: %d", (int)syscall_num);
        return -1;
    }
    
    long result = func(arg1, arg2, arg3, arg4);
    
    /* Check stack at syscall exit */
    if (pup_stack_guard_check_syscall_context(current_thread) != 0) {
        sys_stack_guard_terminate_current_thread("exit");
    }
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_SYSCALL, "Syscall %d returned: %d", (int)syscall_num, (int)result);
    return result;
}
