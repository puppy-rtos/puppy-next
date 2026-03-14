#include "user_libc.h"
#include "user_syscall.h"
#include "user_fs.h"
#include "app_header.h"

extern void fs_test_app_main(void);
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

__attribute__((section(".app_header")))
app_header_t fs_test_app_header = {
    .magic = APP_HEADER_MAGIC,
    .load_addr = 0x08080000,
    .entry_addr = (unsigned int)fs_test_app_main,
    .app_size = 0,
    .version = 0x00010000,
    .stack_addr = (unsigned int)&_sstack,
    .got_start = (unsigned int)&__got_start__,
    .got_end = (unsigned int)&__got_end__,
    .text_start = (unsigned int)&__text_start__,
    .rel_start = (unsigned int)&__rel_start__,
    .rel_end = (unsigned int)&__rel_end__,
    .data_start = (unsigned int)&__data_start__,
    .data_end = (unsigned int)&__data_end__,
    .data_load = (unsigned int)&__data_load__,
    .bss_start = (unsigned int)&__bss_start__,
    .bss_end = (unsigned int)&__bss_end__
};

static void test_uart_file(void) {
    printf("\n=== UART File Test ===\n");
    
    int fd = user_fs_open("/dev/uart0", USER_FS_O_RDWR);
    if (fd < 0) {
        printf("[UART Test] Failed to open uart0, fd=%d\n", fd);
        return;
    }
    printf("[UART Test] Opened uart0, fd=%d\n", fd);
    
    const char* msg = "Hello from file interface!\n";
    int written = user_fs_write(fd, msg, strlen(msg));
    printf("[UART Test] Written %d bytes\n", written);
    
    user_fs_close(fd);
    printf("[UART Test] Closed uart0\n");
}

static void test_gpio_file(void) {
    printf("\n=== GPIO File Test ===\n");
    
    int fd = user_fs_open("/dev/gpio0", USER_FS_O_RDWR);
    if (fd < 0) {
        printf("[GPIO Test] Failed to open gpio0, fd=%d\n", fd);
        return;
    }
    printf("[GPIO Test] Opened gpio0, fd=%d\n", fd);
    
    for (int i = 0; i < 5; i++) {
        char value = 1;
        user_fs_write(fd, &value, 1);
        printf("[GPIO Test] Set GPIO high\n");
        
        for (volatile int j = 0; j < 100000; j++);
        
        value = 0;
        user_fs_write(fd, &value, 1);
        printf("[GPIO Test] Set GPIO low\n");
        
        for (volatile int j = 0; j < 100000; j++);
    }
    
    char read_val;
    user_fs_read(fd, &read_val, 1);
    printf("[GPIO Test] Read value: %d\n", read_val);
    
    user_fs_close(fd);
    printf("[GPIO Test] Closed gpio0\n");
}

void fs_test_app_main(void) {
    printf("\n=== File System Test Application ===\n");
    
    test_uart_file();
    test_gpio_file();
    
    printf("\n=== All File System Tests Complete ===\n");
    
    user_exit(0);
}
