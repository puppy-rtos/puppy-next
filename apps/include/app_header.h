#ifndef __APP_HEADER_H__
#define __APP_HEADER_H__

#define USER_APP_STACK_SIZE 0x2000

typedef struct {
    unsigned int magic;
    unsigned int load_addr;
    unsigned int entry_addr;
    unsigned int app_size;
    unsigned int version;
    unsigned int stack_addr;
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

#endif
