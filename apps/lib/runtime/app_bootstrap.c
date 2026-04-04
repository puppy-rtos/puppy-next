#include "app_header.h"

extern void app_main(void);
extern char _sstack;
extern int __text_start__;
extern int __got_start__;
extern int __got_end__;
extern int __rel_start__;
extern int __rel_end__;
extern int __data_start__;
extern int __data_end__;
extern int __data_load__;
extern int __bss_start__;
extern int __bss_end__;
extern int __user_app_stack_size;
extern int __app_arg_start__;
extern int __app_arg_end__;

__attribute__((section(".app_header")))
app_header_t app_header = {
    .magic = APP_HEADER_MAGIC,
    .load_addr = 0x00000000,
    .entry_addr = (unsigned int)app_main,
    .app_size = 0,
    .version = 0x00020000,
    .stack_addr = (unsigned int)&_sstack,
    .stack_size = (unsigned int)&__user_app_stack_size,
    .arg_start = (unsigned int)&__app_arg_start__,
    .arg_end = (unsigned int)&__app_arg_end__,
    .got_start = (unsigned int)&__got_start__,
    .got_end = (unsigned int)&__got_end__,
    .text_start = (unsigned int)&__text_start__,
    .rel_start = (unsigned int)&__rel_start__,
    .rel_end = (unsigned int)&__rel_end__,
    .data_start = (unsigned int)&__data_start__,
    .data_end = (unsigned int)&__data_end__,
    .data_load = (unsigned int)&__data_load__,
    .bss_start = (unsigned int)&__bss_start__,
    .bss_end = (unsigned int)&__bss_end__,
};
