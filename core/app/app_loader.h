#ifndef __APP_LOADER_H__
#define __APP_LOADER_H__

#include <puppy_core.h>

#define USER_APP_STACK_SIZE 0x2000
#define USER_APP_ARG_AREA_SIZE 0x100

typedef struct {
    void* entry;
    void* entry_arg;
    void* stack_addr;
    pup_size_t stack_size;
    pup_size_t arg_size;
    pup_size_t size;
    unsigned int pid;
    unsigned int flash_addr;
    unsigned int ram_base;
    unsigned int ram_size;
    unsigned int compiled_ram_start;
    unsigned int compiled_ram_end;
    unsigned int got_start;
    unsigned int got_end;
    unsigned int text_start;
    unsigned int got_base;
} user_app_t;

typedef struct {
    unsigned int magic;
    unsigned int load_addr;
    unsigned int entry_addr;
    unsigned int app_size;
    unsigned int version;
    unsigned int stack_addr;
    unsigned int stack_size;
    unsigned int arg_start;
    unsigned int arg_end;
    unsigned int got_start;
    unsigned int got_end;
    unsigned int text_start;
    unsigned int rel_start;
    unsigned int rel_end;
    unsigned int data_start;
    unsigned int data_end;
    unsigned int data_load;
    unsigned int bss_start;
    unsigned int bss_end;
} app_header_t;

#define APP_HEADER_MAGIC 0x50555050

int app_loader_init(void);
int app_calc_memory_requirement(void* flash_addr, pup_size_t size, unsigned int* required_size);
int app_load_into_memory(void* flash_addr, pup_size_t size,
                         unsigned int ram_base, unsigned int ram_size,
                         user_app_t* app);
int app_run(user_app_t* app, const char* thread_name, pthread_t* thread_out);

void kernel_reloc(unsigned int *got_base, unsigned int text_base,
                  unsigned int gs, unsigned int ge, unsigned int ts,
                  unsigned int *rel_start, unsigned int *rel_end);

#endif
