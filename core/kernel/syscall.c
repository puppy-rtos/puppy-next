#include "syscall.h"
#include "libcm.h"
#include <puppy_core.h>
#include "app_loader.h"
#include "app_manager.h"
#include "puppy_log.h"
#include "mpu.h"
#include "pup_fs.h"
#include "pup_console.h"
#include "kheap.h"
#include "pup_sem.h"
#include "shared/pup_user_thread.h"
#include "stack_guard.h"

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

    int result;
    if (app_arg && app_arg[0] != '\0') {
        result = app_exec_with_arg(app_name, path, app_arg, wait);
    } else {
        result = app_exec(app_name, path, wait);
    }
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
