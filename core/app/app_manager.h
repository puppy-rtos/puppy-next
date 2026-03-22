#ifndef __APP_MANAGER_H__
#define __APP_MANAGER_H__

#include <puppy_core.h>
#include "app_loader.h"
#include "mpu.h"

#define APP_NAME_MAX_LEN        32
#define APP_MEMORY_ALIGN        4096  /* Align to 4KB for MPU subregion boundary */
#define APP_STACK_SIZE          (8 * 1024)
#define APP_EXEC_ARG_AREA_SIZE  256

#define APP_STATE_IDLE          0
#define APP_STATE_LOADING       1
#define APP_STATE_RUNNING       2
#define APP_STATE_EXITED        3

typedef struct app_process {
    char name[APP_NAME_MAX_LEN];
    int state;
    int pid;
    unsigned int memory_base;
    unsigned int memory_size;
    unsigned int stack_base;
    unsigned int stack_size;
    unsigned int stack_addr;
    unsigned int flash_addr;
    unsigned int flash_size;
    unsigned int entry_addr;
    unsigned int got_base;
    unsigned int app_arg_base;
    unsigned int got_start;
    unsigned int got_end;
    unsigned int text_start;
    pthread_t thread;
    int exit_code;
    user_app_t image;
    struct pup_mpu_info mpu_info;
    struct app_process* next;
} app_process_t;

typedef struct {
    int pid;
    const char* name;
    int argc;
    char** argv;
} app_exec_args_t;

int app_manager_init(void);
int app_alloc_memory(const char* name, unsigned int flash_addr, unsigned int flash_size, unsigned int memory_size);
void app_free_memory(int pid);
int app_find_by_name(const char* name);
app_process_t* app_get_by_pid(int pid);
app_process_t* app_get_current(void);
app_process_t* app_find_by_thread(pthread_t thread);
app_process_t* app_get_all_running(int *count);
int app_set_state(int pid, int state);
int app_get_state(int pid);
void app_list(void);
int app_exec(const char* name, const char* path, int wait);
int app_exec_with_arg(const char* name, const char* path, const char* arg, int wait);
int app_wait(int pid);
int app_get_process_exit_code(int pid);
void app_reap_exited(void);

extern app_process_t* current_app_process;

#endif
