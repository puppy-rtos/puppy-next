#include "libc.h"
#include "syscall.h"
#include <string.h>

#define TEST_PATTERN 0xDEADBEEF

static volatile unsigned int test_data[256];
static volatile int test_result = 0;

static void safe_memory_test(void)
{
    unsigned int i;
    
    printf("[mpu_test] Starting safe memory test within app region...\n");
    
    for (i = 0; i < 256; i++) {
        test_data[i] = TEST_PATTERN + i;
    }
    
    for (i = 0; i < 256; i++) {
        if (test_data[i] != TEST_PATTERN + i) {
            printf("[mpu_test] Memory corruption detected at index %d!\n", i);
            test_result = -1;
            exit(-1);
        }
    }
    
    printf("[mpu_test] Memory test passed, data verified\n");
}

static void multi_region_access_test(void)
{
    volatile unsigned char *addr;
    unsigned char val;
    int i;
    
    printf("[mpu_test] Starting multi-region access test...\n");
    
    for (i = 0; i < 4; i++) {
        unsigned int test_addr = 0x20020000 + (i * 0x4000);
        addr = (volatile unsigned char *)test_addr;
        
        printf("[mpu_test] Testing access to region around 0x%08x...\n", test_addr);
        
        *addr = (unsigned char)(0xA5 + i);
        val = *addr;
        
        if (val != (unsigned char)(0xA5 + i)) {
            printf("[mpu_test] Region %d access failed!\n", i);
            exit(-1);
        }
        printf("[mpu_test] Region %d access OK\n", i);
    }
    
    printf("[mpu_test] Multi-region test passed\n");
}

static void read_only_test(void)
{
    volatile unsigned int *ro_addr;
    unsigned int val;
    
    printf("[mpu_test] Starting read-only memory test...\n");
    
    ro_addr = (volatile unsigned int *)0x08000000;
    printf("[mpu_test] Reading from flash at 0x%08x...\n", (unsigned int)ro_addr);
    val = *ro_addr;
    printf("[mpu_test] Read value: 0x%08x\n", val);
    
    printf("[mpu_test] Read-only test passed\n");
}

void app_main(const char* arg)
{
    if (!arg || *arg == '\0') {
        printf("Usage: mpu_test <safe|multi|readonly>\n");
        printf("  safe     - Test safe memory access within app region\n");
        printf("  multi    - Test access to multiple memory regions\n");
        printf("  readonly - Test read-only access to flash\n");
        exit(0);
    }
    
    printf("[mpu_test] MPU Test App Starting...\n");
    printf("[mpu_test] Test mode: %s\n", arg);
    
    if (strcmp(arg, "safe") == 0) {
        safe_memory_test();
    } else if (strcmp(arg, "multi") == 0) {
        multi_region_access_test();
    } else if (strcmp(arg, "readonly") == 0) {
        read_only_test();
    } else {
        printf("Unknown test mode: %s\n", arg);
        printf("Available modes: safe, multi, readonly\n");
        exit(-1);
    }
    
    printf("[mpu_test] Test completed successfully!\n");
    exit(0);
}
