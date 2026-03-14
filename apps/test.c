#include <puppy_core.h>
#include <stdio.h>
#include "mpu.h"
#include "pup_fs.h"
#include "pup_romfs.h"

static int end_exec;

static void *a_thread_func(void *arg)
{
    (void)arg;
    end_exec = 1;
    PUP_PRINTK("Wait for 3 seconds for thread to finish execution:\n");

    for (int i = 0; i < 3; i++)
    {
        sched_yield();
        PUP_PRINTK("Thread is running\n");
    }

    return NULL;
}

char pth1_stack[4096];
char pth2_stack[4096];

static int test_pthread(void)
{
    PUP_PRINTK("\n=== Pthread Test ===\n");
    
    pthread_t th1;
    pthread_t th2;
    end_exec = 0;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 4096);
    pthread_attr_setstackaddr(&attr, pth1_stack);
    pthread_attr_setname(&attr, "posix_t1");

    if (pthread_create(&th1, &attr, a_thread_func, NULL) != 0) {
        PUP_PRINTK("Error creating thread1\n");
        return -2;
    }
    pthread_attr_setstackaddr(&attr, pth2_stack);
    pthread_attr_setname(&attr, "posix_t2");
    if (pthread_create(&th2, &attr, a_thread_func, NULL) != 0) {
        PUP_PRINTK("Error creating thread2\n");
        return -2;
    }

    if (pthread_join(th1, NULL) != 0) {
        PUP_PRINTK("Error in pthread_join(thread1)\n");
        return -2;
    }

    if (pthread_join(th2, NULL) != 0) {
        PUP_PRINTK("Error in pthread_join(thread2)\n");
        return -2;
    }
    if (end_exec == 0) {
        PUP_PRINTK("Test FAILED: When using pthread_join(), "
               "main() did not wait for thread to finish "
               "execution before continuing.\n");
        return -1;
    }

    PUP_PRINTK("Test PASSED\n");
    return 0;
}

char delay_thread_stack[4096];
static volatile int delay_thread_done = 0;

static void *delay_thread_func(void *arg)
{
    PUP_PRINTK("[delay_test] Thread start, tick=%d\n", pup_tick_get());
    pthread_delay(10);
    PUP_PRINTK("[delay_test] Thread wake up after delay, tick=%d\n", pup_tick_get());
    delay_thread_done = 1;
    return NULL;
}

static int test_pthread_delay(void)
{
    PUP_PRINTK("\n=== pthread_delay Test ===\n");
    
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, sizeof(delay_thread_stack));
    pthread_attr_setstackaddr(&attr, delay_thread_stack);
    pthread_attr_setname(&attr, "delay_test");
    
    delay_thread_done = 0;
    pup_tick_t start_tick = pup_tick_get();
    PUP_PRINTK("[delay_test] Create thread at tick=%d\n", start_tick);
    
    if (pthread_create(&thread, &attr, delay_thread_func, NULL) != 0) {
        PUP_PRINTK("[delay_test] Error creating thread\n");
        return -1;
    }
    
    PUP_PRINTK("[delay_test] Main thread yield...\n");
    sched_yield();
    
    PUP_PRINTK("[delay_test] Main thread waiting (non-blocking)...\n");
    int wait_count = 0;
    while (!delay_thread_done) {
        sched_yield();
        wait_count++;
        if (wait_count % 100 == 0) {
            PUP_PRINTK("[delay_test] Main thread waiting... tick=%d\n", pup_tick_get());
        }
        if (wait_count > 10000) {
            PUP_PRINTK("[delay_test] TIMEOUT waiting for thread\n");
            break;
        }
    }
    
    pup_tick_t end_tick = pup_tick_get();
    PUP_PRINTK("[delay_test] Done, tick=%d, elapsed=%d\n", end_tick, end_tick - start_tick);
    
    if (end_tick - start_tick >= 10) {
        PUP_PRINTK("[delay_test] PASSED: delay worked correctly\n");
        return 0;
    } else {
        PUP_PRINTK("[delay_test] FAILED: delay did not work\n");
        return -1;
    }
}

static int test_mpu(void)
{
    PUP_PRINTK("\n=== MPU Test ===\n");
    
    if (pup_mpu_init() != 0) {
        PUP_PRINTK("MPU init failed\n");
        return -1;
    }
    
    pup_mpu_dump();
    
    PUP_PRINTK("\n--- Test 1: Set kernel flash region (512KB, read+exec) ---\n");
    pup_uint32_t flash_start = PUP_KERNEL_FLASH_BASE;
    pup_uint32_t flash_size = PUP_KERNEL_FLASH_SIZE;
    
    pup_mpu_region_set(0, flash_start, flash_size,
                       PUP_MPU_ATT_READ | PUP_MPU_ATT_EXEC);
    
    pup_mpu_dump();
    
    PUP_PRINTK("\n--- Test 2: Set all RAM region (read+write+exec) ---\n");
    pup_uint32_t ram_start = PUP_RAM_ADDRESS_START;
    pup_uint32_t ram_size = PUP_RAM_ADDRESS_END - PUP_RAM_ADDRESS_START;
    
    pup_mpu_region_set(1, ram_start, ram_size,
                       PUP_MPU_ATT_READ | PUP_MPU_ATT_WRITE | PUP_MPU_ATT_EXEC);
    
    pup_mpu_dump();
    
    PUP_PRINTK("\nMPU test completed\n");
    return 0;
}

static int test_devfs(void)
{
    PUP_PRINTK("\n=== DevFS Test ===\n");
    
    PUP_PRINTK("\n--- Test 1: Open UART device ---\n");
    int fd = pup_open("/dev/uart0", O_RDWR);
    if (fd < 0) {
        PUP_PRINTK("Failed to open uart0, fd=%d\n", fd);
        return -1;
    }
    PUP_PRINTK("Opened uart0, fd=%d\n", fd);
    
    PUP_PRINTK("\n--- Test 2: Write to UART ---\n");
    const char* msg = "Hello from kernel file system!\n";
    int written = pup_write(fd, msg, libcm_strlen(msg));
    PUP_PRINTK("Written %d bytes\n", written);
    
    PUP_PRINTK("\n--- Test 3: Close UART ---\n");
    pup_close(fd);
    PUP_PRINTK("Closed uart0\n");
    
    PUP_PRINTK("\n--- Test 4: Open GPIO device ---\n");
    fd = pup_open("/dev/gpio0", O_RDWR);
    if (fd < 0) {
        PUP_PRINTK("Failed to open gpio0, fd=%d\n", fd);
        return -1;
    }
    PUP_PRINTK("Opened gpio0, fd=%d\n", fd);
    
    PUP_PRINTK("\n--- Test 5: GPIO write/read ---\n");
    char val = 1;
    pup_write(fd, &val, 1);
    PUP_PRINTK("Set GPIO high\n");
    
    val = 0;
    pup_write(fd, &val, 1);
    PUP_PRINTK("Set GPIO low\n");
    
    char read_val;
    pup_read(fd, &read_val, 1);
    PUP_PRINTK("Read GPIO value: %d\n", read_val);
    
    pup_close(fd);
    PUP_PRINTK("Closed gpio0\n");
    
    PUP_PRINTK("\nDevFS test completed\n");
    return 0;
}

static int test_romfs(void)
{
    PUP_PRINTK("\n=== ROMFS Test ===\n");
    
    PUP_PRINTK("\n--- Test 1: Open /rom directory ---\n");
    int fd = pup_open("/rom", O_DIRECTORY);
    if (fd < 0) {
        PUP_PRINTK("Failed to open /rom, fd=%d\n", fd);
        return -1;
    }
    PUP_PRINTK("Opened /rom directory, fd=%d\n", fd);
    
    PUP_PRINTK("\n--- Test 2: List files in ROMFS ---\n");
    struct dirent entry;
    int file_count = 0;
    int ret;
    
    while ((ret = pup_getdents(fd, &entry, sizeof(entry))) > 0) {
        PUP_PRINTK("  [%d] %s (type=%d)\n", file_count, entry.d_name, entry.d_type);
        file_count++;
    }
    
    PUP_PRINTK("Total files found: %d\n", file_count);
    pup_close(fd);
    
    if (file_count == 0) {
        PUP_PRINTK("WARNING: No files found in ROMFS\n");
        return -1;
    }
    
    PUP_PRINTK("\n--- Test 3: Try to read first file ---\n");
    fd = pup_open("/rom", O_DIRECTORY);
    if (fd >= 0) {
        ret = pup_getdents(fd, &entry, sizeof(entry));
        if (ret > 0) {
            char filepath[64];
            libcm_snprintf(filepath, sizeof(filepath), "/rom/%s", entry.d_name);
            
            int file_fd = pup_open(filepath, O_RDONLY);
            if (file_fd >= 0) {
                PUP_PRINTK("Opened file: %s\n", filepath);
                
                char buf[32];
                int n = pup_read(file_fd, buf, sizeof(buf));
                if (n > 0) {
                    PUP_PRINTK("Read %d bytes from file\n", n);
                    PUP_PRINTK("First 16 bytes: ");
                    for (int i = 0; i < (n < 16 ? n : 16); i++) {
                        PUP_PRINTK("%02x ", (unsigned char)buf[i]);
                    }
                    PUP_PRINTK("\n");
                }
                pup_close(file_fd);
            } else {
                PUP_PRINTK("Failed to open file %s, fd=%d\n", filepath, file_fd);
            }
        }
        pup_close(fd);
    }
    
    PUP_PRINTK("\nROMFS test completed\n");
    return 0;
}

void test_romfs_auto_run(void)
{
    PUP_PRINTK("\n[ AUTO TEST ] Running ROMFS auto test using ls command...\n");
    
    extern int shell_cmd_ls(int argc, char **argv);
    
    char *argv1[] = {"ls", "/rom"};
    int ret = shell_cmd_ls(2, argv1);
    
    if (ret == 0) {
        PUP_PRINTK("[ AUTO TEST ] PASSED: ls /rom executed successfully\n");
    } else {
        PUP_PRINTK("[ AUTO TEST ] FAILED: ls /rom returned %d\n", ret);
    }
}

typedef struct {
    const char *name;
    const char *help;
    int (*func)(void);
} test_cmd_t;

static const test_cmd_t test_cmds[] = {
    {"pthread", "Test pthread create and join", test_pthread},
    {"delay",  "Test pthread_delay function", test_pthread_delay},
    {"mpu",    "Test MPU configuration", test_mpu},
    {"devfs",  "Test device file system", test_devfs},
    {"romfs",  "Test ROMFS file listing", test_romfs},
};

#define TEST_CMD_NUM (sizeof(test_cmds) / sizeof(test_cmds[0]))

int test_exec(int argc, char **argv)
{
    if (argc == 0) {
        PUP_PRINTK("Available tests:\n");
        for (int i = 0; i < TEST_CMD_NUM; i++) {
            PUP_PRINTK("  %-8s - %s\n", test_cmds[i].name, test_cmds[i].help);
        }
        return 0;
    }
    
    for (int i = 0; i < TEST_CMD_NUM; i++) {
        if (libcm_strcmp(argv[0], test_cmds[i].name) == 0) {
            return test_cmds[i].func();
        }
    }
    
    PUP_PRINTK("Unknown test: %s\n", argv[0]);
    PUP_PRINTK("Available tests: pthread, delay, mpu, devfs, romfs\n");
    return -1;
}
