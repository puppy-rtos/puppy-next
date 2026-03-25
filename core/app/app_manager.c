#include "app_manager.h"
#include "app_loader.h"
#include "libcm.h"
#include "puppy_log.h"
#include "mpu.h"
#include "pup_romfs.h"
#include "pup_devfs.h"
#include "kheap.h"
#include "syscall.h"
#include "pup_channel.h"
#include <puppy_core.h>
#include <string.h>
#include <stdio.h>

static app_process_t* g_app_list = NULL;
static int next_pid = 1;
app_process_t* current_app_process = NULL;
static unsigned int g_mpu_cache_version = 1;

static int app_exec_internal(const char* name, const char* path, const char* arg, int wait);

static unsigned int app_mpu_choose_region_size(unsigned int app_base, unsigned int app_size) {
    unsigned int region_size = 256;
    unsigned int app_end = app_base + app_size;

    if (app_size == 0 || app_end < app_base) {
        return 256;
    }

    while (region_size <= (1024U * 1024U)) {
        unsigned int front_base = app_base & ~(region_size - 1U);
        unsigned long long coverage_end = (unsigned long long)front_base + ((unsigned long long)region_size * 2ULL);

        if ((unsigned long long)app_end <= coverage_end) {
            return region_size;
        }

        region_size <<= 1U;
    }

    return 1024U * 1024U;
}

static void app_mpu_prepare_cache(app_process_t* app) {
    unsigned int protected_end;
    unsigned int got_size;
    unsigned int region_size;
    unsigned int subregion_size;
    unsigned int f_base;
    unsigned int b_base;
    unsigned int f_mask = 0;
    unsigned int b_mask = 0xFF;

    if (!app) {
        return;
    }

    protected_end = app->memory_base + (app->image.compiled_ram_end - app->image.compiled_ram_start);
    got_size = (app->got_end > app->got_start) ? (app->got_end - app->got_start) : 0;

    if (app->got_base != 0 && got_size > 0) {
        unsigned int got_end = app->got_base + got_size;
        if (got_end > protected_end) {
            protected_end = got_end;
        }
    }

    if (app->entry_addr != 0 && app->image.arg_size > 0) {
        unsigned int arg_end = app->app_arg_base + app->image.arg_size;
        if (arg_end > protected_end) {
            protected_end = arg_end;
        }
    }

    region_size = app_mpu_choose_region_size(app->memory_base, protected_end - app->memory_base);
    subregion_size = region_size / 8U;
    f_base = app->memory_base & ~(region_size - 1U);
    b_base = (protected_end - 1U) & ~(region_size - 1U);

    for (int i = 0; i < 8; i++) {
        unsigned int s = f_base + i * subregion_size;
        unsigned int e = s + subregion_size;
        if (!(s < protected_end && e > app->memory_base)) {
            f_mask |= (1U << i);
        }
    }

    if (b_base != f_base) {
        b_mask = 0;
        for (int i = 0; i < 8; i++) {
            unsigned int s = b_base + i * subregion_size;
            unsigned int e = s + subregion_size;
            if (!(s < protected_end && e > app->memory_base)) {
                b_mask |= (1U << i);
            }
        }
    }

    app->mpu_cached_valid = 1;
    app->mpu_cached_region_size = region_size;
    app->mpu_cached_subregion_size = subregion_size;
    app->mpu_cached_front_base = f_base;
    app->mpu_cached_back_base = b_base;
    app->mpu_cached_front_mask = f_mask;
    app->mpu_cached_back_mask = b_mask;
    app->mpu_cached_protected_end = protected_end;
    app->mpu_cached_version = g_mpu_cache_version++;

    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP,
                 "MPU cache prepared: pid=%d ver=%u range=[0x%08x,0x%08x) region=%uB sub=%uB",
                 app->pid,
                 app->mpu_cached_version,
                 app->memory_base,
                 app->mpu_cached_protected_end,
                 app->mpu_cached_region_size,
                 app->mpu_cached_subregion_size);
}

int app_manager_init(void) {
    g_app_list = NULL;
    next_pid = 1;
    current_app_process = NULL;
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "App manager initialized (dynamic process table)");
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "App memory uses dynamic user heap (aligned=%u)", APP_MEMORY_ALIGN);
    
    return 0;
}

int app_alloc_memory(const char* name, unsigned int flash_addr, unsigned int flash_size, unsigned int memory_size) {
    app_process_t* app;
    void* mem;
    size_t free_bytes;
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Allocating memory for app '%s'", name);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Flash addr: 0x%08x, size: %d", flash_addr, flash_size);
    
    if (memory_size == 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Invalid app memory size: %u", memory_size);
        return -1;
    }

    free_bytes = kheap_user_free_size();
    if (memory_size > free_bytes) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Not enough user heap: need=%u free=%u",
                      memory_size, (unsigned int)free_bytes);
        return -1;
    }

    app = (app_process_t*)kheap_calloc(1, sizeof(app_process_t));
    if (!app) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Failed to allocate app process node");
        return -1;
    }
    
    mem = kheap_user_memalign(APP_MEMORY_ALIGN, memory_size);
    if (!mem) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "No dynamic app memory available (%u bytes)", memory_size);
        kheap_free(app);
        return -1;
    }
    
    app->pid = next_pid++;
    app->state = APP_STATE_LOADING;
    
    strncpy(app->name, name, APP_NAME_MAX_LEN - 1);
    app->name[APP_NAME_MAX_LEN - 1] = '\0';
    
    app->memory_base = (unsigned int)mem;
    app->memory_size = memory_size;
    app->stack_base = (memory_size >= APP_STACK_SIZE)
        ? (app->memory_base + memory_size - APP_STACK_SIZE)
        : app->memory_base;
    app->stack_size = APP_STACK_SIZE;
    app->flash_addr = flash_addr;
    app->flash_size = flash_size;
    app->exit_code = 0;
    app->thread = PUP_NULL;
    app->next = g_app_list;
    g_app_list = app;
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Allocated app '%s' (pid=%d)", name, app->pid);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Memory: 0x%08x - 0x%08x", 
                  app->memory_base, app->memory_base + app->memory_size);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Stack: 0x%08x (%d bytes)", 
                  app->stack_base, app->stack_size);
    
    return app->pid;
}

void app_free_memory(int pid) {
    app_process_t* app = g_app_list;
    app_process_t* prev = NULL;
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Freeing memory for pid=%d", pid);
    
    while (app) {
        if (app->pid == pid) {
            break;
        }
        prev = app;
        app = app->next;
    }
    
    if (!app) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "App not found: pid=%d", pid);
        return;
    }
    
    if (app->memory_base != 0) {
        kheap_user_free((void*)app->memory_base);
    }

    syscall_cleanup_owner_resources(app->pid);
    pup_devfs_cleanup_owner_devices(app->pid);
    pup_channel_cleanup_owner(app->pid);

    if (prev) {
        prev->next = app->next;
    } else {
        g_app_list = app->next;
    }

    if (current_app_process == app) {
        current_app_process = NULL;
    }
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Freed app '%s' (pid=%d)", 
                 app->name, pid);

    kheap_free(app);
}

int app_find_by_name(const char* name) {
    app_process_t* app = g_app_list;

    while (app) {
        if (strcmp(app->name, name) == 0) {
            return app->pid;
        }
        app = app->next;
    }
    
    return -1;
}

app_process_t* app_get_by_pid(int pid) {
    app_process_t* app = g_app_list;

    while (app) {
        if (app->pid == pid) {
            return app;
        }
        app = app->next;
    }
    
    return NULL;
}

app_process_t* app_get_current(void) {
    return app_find_by_thread(pthread_self());
}

app_process_t* app_find_by_thread(pthread_t thread) {
    void* stackaddr;
    pup_size_t stacksize;
    unsigned int stack_start;
    unsigned int stack_end;

    if (!thread) {
        return NULL;
    }

    stackaddr = pup_thread_get_stackaddr(thread);
    stacksize = pup_thread_get_stacksize(thread);
    stack_start = (unsigned int)stackaddr;
    stack_end = stack_start + stacksize;

    app_process_t* app = g_app_list;

    while (app) {
        unsigned int app_start;
        unsigned int app_end;

        if (app->thread == thread) {
            return app;
        }

        app_start = app->memory_base;
        app_end = app_start + app->memory_size;

        if ((stack_start >= app_start && stack_start < app_end) ||
            (stack_end > app_start && stack_end <= app_end)) {
            return app;
        }

        app = app->next;
    }

    return NULL;
}

static app_process_t g_running_apps_buffer[16];

app_process_t* app_get_all_running(int *count) {
    int running_count = 0;
    app_process_t* app = g_app_list;
    
    while (app && running_count < 16) {
        if (app->state == APP_STATE_RUNNING && app->memory_base != 0) {
            g_running_apps_buffer[running_count] = *app;
            running_count++;
        }
        app = app->next;
    }
    
    *count = running_count;
    return g_running_apps_buffer;
}

int app_set_state(int pid, int state) {
    app_process_t* app = app_get_by_pid(pid);
    
    if (!app) {
        return -1;
    }
    
    app->state = state;
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "App '%s' (pid=%d) state changed to %d", 
                  app->name, pid, state);
    return 0;
}

int app_get_state(int pid) {
    app_process_t* app = app_get_by_pid(pid);
    
    if (!app) {
        return -1;
    }
    
    return app->state;
}

void app_list(void) {
    int count = 0;
    app_process_t* app = g_app_list;
    
    PUP_PRINTK("\nPID  State    Name             Memory               Size\n");
    PUP_PRINTK("---- -------- ---------------- -------------------- ------\n");
    
    while (app) {
        const char* state_str;
        
        switch (app->state) {
            case APP_STATE_IDLE:    state_str = "IDLE"; break;
            case APP_STATE_LOADING: state_str = "LOADING"; break;
            case APP_STATE_RUNNING: state_str = "RUNNING"; break;
            case APP_STATE_EXITED:  state_str = "EXITED"; break;
            default:                state_str = "UNKNOWN"; break;
        }

        PUP_PRINTK("%-4d %-8s %-16.16s 0x%08x-0x%08x %6u\n",
                   app->pid, state_str, app->name,
                   app->memory_base,
                   app->memory_base + app->memory_size,
                   app->memory_size);
        count++;
        app = app->next;
    }
    
    if (count == 0) {
        PUP_PRINTK("No running apps\n");
    }
}

int app_exec(const char* name, const char* path, const char* arg, int wait) {
    return app_exec_internal(name, path, arg, wait);
}

static int app_exec_internal(const char* name, const char* path, const char* arg, int wait) {
    void* file_data = NULL;
    unsigned int file_size = 0;
    unsigned int app_mem_size = 0;
    int pid;
    int exit_code;
    app_process_t* app;
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "app_exec: name=%s, path=%s, wait=%d", name, path, wait);
    
    if (pup_romfs_get_file_data(path, &file_data, &file_size) != 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "App not found: %s", path);
        return -1;
    }
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Loading app '%s' from %s (%d bytes)", name, path, file_size);

    if (app_calc_memory_requirement(file_data, file_size, &app_mem_size) != 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Failed to calculate app memory requirement: %s", name);
        return -1;
    }

    app_mem_size = (unsigned int)((app_mem_size + (APP_MEMORY_ALIGN - 1)) & ~(APP_MEMORY_ALIGN - 1));

    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "App '%s' memory requirement: %u bytes", name, app_mem_size);

    pid = app_alloc_memory(name, (unsigned int)file_data, file_size, app_mem_size);
    if (pid < 0) {
        return -1;
    }

    app = app_get_by_pid(pid);
    if (!app) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Failed to resolve pid=%d after allocation", pid);
        app_free_memory(pid);
        return -1;
    }

    if (app_load_into_memory(file_data, file_size,
                             app->memory_base, app->memory_size,
                             &app->image) != 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Failed to load app: %s", name);
        app_free_memory(pid);
        return -1;
    }

    app->image.pid = pid;
    app->stack_addr = (unsigned int)app->image.stack_addr;
    app->stack_size = app->image.stack_size;
    app->entry_addr = (unsigned int)app->image.entry;
    app->got_base = app->image.got_base;
    app->got_start = app->image.got_start;
    app->got_end = app->image.got_end;
    app->text_start = app->image.text_start;

    app->app_arg_base = (unsigned int)app->image.entry_arg;
    if (app->image.arg_size == 0 || app->app_arg_base == 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Invalid app arg area for '%s'", name);
        app_free_memory(pid);
        return -1;
    }

    libcm_memset((void*)app->app_arg_base, 0, app->image.arg_size);
    if (arg) {
        libcm_strncpy((char*)app->app_arg_base, arg, app->image.arg_size - 1);
    }

    app_mpu_prepare_cache(app);

    {
        unsigned int used_size = app->mpu_cached_protected_end - app->memory_base;
        unsigned int alloc_slack = (app->memory_size > used_size) ? (app->memory_size - used_size) : 0;
        PUP_LOG_DEBUG(PUP_LOG_MODULE_APP,
                     "App memory summary: name=%s pid=%d alloc=%u used=%u protected=%u slack=%u",
                     app->name,
                     app->pid,
                     app->memory_size,
                     used_size,
                     app->mpu_cached_protected_end - app->memory_base,
                     alloc_slack);
    }

    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Running app '%s'", name);

    if (app_run(&app->image, name, &app->thread) != 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Failed to start app thread: %s", name);
        app_free_memory(pid);
        return -1;
    }

    app->state = APP_STATE_RUNNING;
    
    if (!wait) {
        return pid;
    }

    exit_code = app_wait(pid);
    app_free_memory(pid);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "App '%s' completed with code %d", name, exit_code);
    return exit_code;
}

int app_wait(int pid) {
    app_process_t* app = app_get_by_pid(pid);
    int exit_code;

    if (!app) {
        return -1;
    }

    while (app->state != APP_STATE_EXITED) {
        pthread_delay(1);

        app = app_get_by_pid(pid);
        if (!app) {
            return -1;
        }
    }

    if (app->thread) {
        pthread_join(app->thread, NULL);
        app->thread = PUP_NULL;
    }

    exit_code = app->exit_code;

    pup_reap_zombies();

    return exit_code;
}

int app_get_process_exit_code(int pid) {
    app_process_t* app = app_get_by_pid(pid);
    
    if (!app) {
        return -1;
    }
    
    return app->exit_code;
}

void app_reap_exited(void) {
    app_process_t* app = g_app_list;

    while (app) {
        app_process_t* next = app->next;

        if (app->state == APP_STATE_EXITED) {
            if (app->thread) {
                pthread_join(app->thread, NULL);
                app->thread = PUP_NULL;
            }

            app_free_memory(app->pid);
        }

        app = next;
    }
}
