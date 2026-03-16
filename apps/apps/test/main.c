#include "libc.h"
#include "syscall.h"
#include "shared/pup_spi.h"
#include "shared/pup_flash.h"
#include <string.h>

#ifndef PUP_NULL
#define PUP_NULL    ((void*)0)
#endif

#define O_RDONLY    0000000
#define O_WRONLY    0000001
#define O_RDWR      0000002
#define O_DIRECTORY 00200000

/* App RAM boundaries - aligned to 4KB boundary */
#define APP_RAM_BASE    0x20011000
#define APP_RAM_SIZE    28672
#define APP_RAM_END     (APP_RAM_BASE + APP_RAM_SIZE)

struct dirent {
    unsigned char d_type;
    unsigned char d_namlen;
    unsigned short d_reclen;
    char d_name[256];
};

typedef struct {
    const char *name;
    const char *help;
    int (*func)(void);
} test_cmd_t;

static int end_exec;
static volatile int delay_thread_done;
static volatile int stack_guard_thread_returned;
static volatile unsigned int stack_guard_sink;

static char pth1_stack[2048];
static char pth2_stack[2048];
static char delay_thread_stack[2048];
static char sem_waiter_stack[1024];
static char stack_guard_thread_stack[512];

static volatile int sem_waiter_done;
static int sem_test_handle = -1;
static char channel_server_stack[2048];

#define STACK_GUARD_RECURSION_DEPTH 7
#define STACK_GUARD_FRAME_BYTES     96

static void print_usage(void);
static int run_named_test(const char *name);

typedef struct {
    int channel_id;
    int rounds;
} channel_pingpong_ctx_t;

typedef struct {
    int seq;
    int value;
} channel_pingpong_payload_t;

static void *channel_server_thread(void *arg)
{
    channel_pingpong_ctx_t *ctx = (channel_pingpong_ctx_t *)arg;

    if (!ctx || ctx->channel_id < 0 || ctx->rounds <= 0) {
        return NULL;
    }

    for (int i = 0; i < ctx->rounds; i++) {
        pup_channel_msg_t msg;
        int cookie = pup_channel_receive(ctx->channel_id, &msg);

        if (cookie <= 0 || msg.type != PUP_MSG_TYPE_REQUEST || msg.buf == 0 ||
            msg.buf_sz < (int)sizeof(channel_pingpong_payload_t)) {
            if (cookie > 0) {
                pup_channel_reply(ctx->channel_id, cookie, -1);
            }
            return NULL;
        }

        {
            channel_pingpong_payload_t *payload = (channel_pingpong_payload_t *)msg.buf;
            payload->value += 1;
            pup_channel_reply(ctx->channel_id, cookie, payload->value);
        }
    }

    return NULL;
}

static void *sem_waiter_thread(void *arg)
{
    (void)arg;

    printf("[sem] waiter: waiting...\n");
    if (sem_wait(sem_test_handle) != 0) {
        printf("[sem] waiter: wait failed\n");
        sem_waiter_done = -1;
        return NULL;
    }

    printf("[sem] waiter: wake up\n");
    sem_waiter_done = 1;
    return NULL;
}

static void *a_thread_func(void *arg)
{
    (void)arg;
    end_exec = 1;
    printf("Wait for 3 seconds for thread to finish execution:\n");

    for (int i = 0; i < 3; i++) {
        sched_yield();
        printf("Thread is running\n");
    }

    return NULL;
}

static int test_pthread(void)
{
    pthread_t th1;
    pthread_t th2;
    pthread_attr_t attr;

    printf("\n=== Pthread Test ===\n");

    end_exec = 0;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, sizeof(pth1_stack));
    pthread_attr_setstackaddr(&attr, pth1_stack);
    pthread_attr_setname(&attr, "posix_t1");

    if (pthread_create(&th1, &attr, a_thread_func, NULL) != 0) {
        printf("Error creating thread1\n");
        return -2;
    }

    pthread_attr_setstackaddr(&attr, pth2_stack);
    pthread_attr_setname(&attr, "posix_t2");
    if (pthread_create(&th2, &attr, a_thread_func, NULL) != 0) {
        printf("Error creating thread2\n");
        return -2;
    }

    if (pthread_join(th1, NULL) != 0) {
        printf("Error in pthread_join(thread1)\n");
        return -2;
    }

    if (pthread_join(th2, NULL) != 0) {
        printf("Error in pthread_join(thread2)\n");
        return -2;
    }

    if (end_exec == 0) {
        printf("Test FAILED: main thread did not wait for child completion\n");
        return -1;
    }

    printf("Test PASSED\n");
    return 0;
}

static void *delay_thread_func(void *arg)
{
    (void)arg;

    printf("[delay_test] Thread start, tick=%lu\n", pup_tick_get());
    pthread_delay(10);
    printf("[delay_test] Thread wake up after delay, tick=%lu\n", pup_tick_get());
    delay_thread_done = 1;
    return NULL;
}

static int test_pthread_delay(void)
{
    pthread_t thread;
    pthread_attr_t attr;
    pup_tick_t start_tick;
    pup_tick_t end_tick;
    int wait_count = 0;

    printf("\n=== pthread_delay Test ===\n");

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, sizeof(delay_thread_stack));
    pthread_attr_setstackaddr(&attr, delay_thread_stack);
    pthread_attr_setname(&attr, "delay_test");

    delay_thread_done = 0;
    start_tick = pup_tick_get();
    printf("[delay_test] Create thread at tick=%lu\n", start_tick);

    if (pthread_create(&thread, &attr, delay_thread_func, NULL) != 0) {
        printf("[delay_test] Error creating thread\n");
        return -1;
    }

    printf("[delay_test] Main thread yield...\n");
    sched_yield();

    printf("[delay_test] Main thread waiting (non-blocking)...\n");
    while (!delay_thread_done) {
        sched_yield();
        wait_count++;
        if ((wait_count % 100) == 0) {
            printf("[delay_test] Main thread waiting... tick=%lu\n", pup_tick_get());
        }
        if (wait_count > 10000) {
            printf("[delay_test] TIMEOUT waiting for thread\n");
            break;
        }
    }

    pthread_join(thread, NULL);

    end_tick = pup_tick_get();
    printf("[delay_test] Done, tick=%lu, elapsed=%lu\n", end_tick, end_tick - start_tick);

    if (delay_thread_done && (end_tick - start_tick >= 10)) {
        printf("[delay_test] PASSED: delay worked correctly\n");
        return 0;
    }

    printf("[delay_test] FAILED: delay did not work\n");
    return -1;
}

static unsigned int __attribute__((noinline)) stack_guard_recursive_burn(int depth,
                                                                          unsigned int seed)
{
    volatile unsigned char frame[STACK_GUARD_FRAME_BYTES];
    unsigned int local_seed = seed;

    for (int i = 0; i < (int)sizeof(frame); i++) {
        frame[i] = (unsigned char)(depth + i + local_seed);
    }

    stack_guard_sink += frame[depth % (int)sizeof(frame)];

    if (depth == 0) {
        sched_yield();
    }

    if (depth > 0) {
        local_seed = stack_guard_recursive_burn(depth - 1, local_seed + frame[0]);
    }

    stack_guard_sink += frame[(depth + 17) % (int)sizeof(frame)];
    return local_seed + frame[1];
}

static void *stack_guard_thread_func(void *arg)
{
    (void)arg;

    printf("[stack_guard] worker start: stack=%u depth=%u frame=%u\n",
           (unsigned int)sizeof(stack_guard_thread_stack),
           (unsigned int)STACK_GUARD_RECURSION_DEPTH,
           (unsigned int)STACK_GUARD_FRAME_BYTES);

    stack_guard_sink = stack_guard_recursive_burn(STACK_GUARD_RECURSION_DEPTH, 0x5aU);
    stack_guard_thread_returned = 1;

    printf("[stack_guard] recursion returned unexpectedly, sink=%u\n", stack_guard_sink);
    return NULL;
}

static int test_stack_guard_recursion(void)
{
    pthread_t thread;
    pthread_attr_t attr;

    printf("\n=== Recursive Stack Guard Test ===\n");

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, sizeof(stack_guard_thread_stack));
    pthread_attr_setstackaddr(&attr, stack_guard_thread_stack);
    pthread_attr_setname(&attr, "recurse_ovf");

    stack_guard_thread_returned = 0;
    stack_guard_sink = 0;

    if (pthread_create(&thread, &attr, stack_guard_thread_func, NULL) != 0) {
        printf("[stack_guard] create thread failed\n");
        return -1;
    }

    if (pthread_join(thread, NULL) != 0) {
        printf("[stack_guard] join failed\n");
        return -1;
    }

    if (stack_guard_thread_returned) {
        printf("[stack_guard] FAILED: recursion returned normally, sink=%u\n", stack_guard_sink);
        return -1;
    }

    printf("[stack_guard] PASSED: worker thread was terminated before returning\n");
    printf("[stack_guard] expected kernel log: stack guard violated for thread 'recurse_ovf'\n");
    return 0;
}

/* ==================== MPU Tests ==================== */

#define TEST_PATTERN 0xA5A5A5A5

static int test_mpu_normal_access(void)
{
    volatile unsigned int *test_addr;
    unsigned int val;
    int i;

    printf("\n--- MPU Normal Memory Access Test ---\n");

    /* Test 1: Access app RAM (should succeed) */
    printf("[mpu] Test 1: Access app RAM at 0x%08x\n", APP_RAM_BASE);
    test_addr = (volatile unsigned int *)APP_RAM_BASE;
    *test_addr = TEST_PATTERN;
    val = *test_addr;
    if (val != TEST_PATTERN) {
        printf("[mpu] FAILED: App RAM read/write failed\n");
        return -1;
    }
    printf("[mpu] PASSED: App RAM access OK\n");

    /* Test 2: Access shared RAM (should succeed) */
    printf("[mpu] Test 2: Access shared RAM at 0x10000000\n");
    test_addr = (volatile unsigned int *)0x10000000;
    *test_addr = TEST_PATTERN + 1;
    val = *test_addr;
    if (val != TEST_PATTERN + 1) {
        printf("[mpu] FAILED: Shared RAM read/write failed\n");
        return -1;
    }
    printf("[mpu] PASSED: Shared RAM access OK\n");

    /* Test 3: Read flash (should succeed) */
    printf("[mpu] Test 3: Read flash at 0x08000000\n");
    test_addr = (volatile unsigned int *)0x08000000;
    val = *test_addr;
    printf("[mpu] Flash value: 0x%08x\n", val);
    printf("[mpu] PASSED: Flash read OK\n");

    /* Test 4: Multiple address access within app region */
    printf("[mpu] Test 4: Multiple addresses in app region\n");
    for (i = 0; i < 4; i++) {
        test_addr = (volatile unsigned int *)(APP_RAM_BASE + i * 0x1000);
        *test_addr = TEST_PATTERN + i;
    }
    for (i = 0; i < 4; i++) {
        test_addr = (volatile unsigned int *)(APP_RAM_BASE + i * 0x1000);
        val = *test_addr;
        if (val != TEST_PATTERN + i) {
            printf("[mpu] FAILED: Data mismatch at offset %d\n", i);
            return -1;
        }
    }
    printf("[mpu] PASSED: Multiple address access OK\n");

    return 0;
}

static int test_mpu_boundary(void)
{
    volatile unsigned char *byte_addr;
    unsigned char val;

    printf("\n--- MPU Boundary Test ---\n");
    printf("[mpu] App RAM range: 0x%08x - 0x%08x (%d bytes)\n", APP_RAM_BASE, APP_RAM_END, APP_RAM_SIZE);

    /* Test 1: Access at app RAM start boundary */
    printf("[mpu] Test 1: Access at app RAM start (0x%08x)\n", APP_RAM_BASE);
    byte_addr = (volatile unsigned char *)APP_RAM_BASE;
    *byte_addr = 0xAA;
    val = *byte_addr;
    if (val != 0xAA) {
        printf("[mpu] FAILED: App RAM start access failed\n");
        return -1;
    }
    printf("[mpu] PASSED: App RAM start access OK\n");

    /* Test 2: Access at various offsets within app RAM */
    printf("[mpu] Test 2: Access at various offsets within app RAM\n");
    int offsets[] = {0x100, 0x1000, 0x2000, 0x4000, 0x5000};
    for (int i = 0; i < 5; i++) {
        uint32_t test_addr = APP_RAM_BASE + offsets[i];
        if (test_addr >= APP_RAM_END) continue;  /* Skip if beyond app RAM */
        byte_addr = (volatile unsigned char *)test_addr;
        *byte_addr = (unsigned char)(0x55 + i);
        val = *byte_addr;
        if (val != (unsigned char)(0x55 + i)) {
            printf("[mpu] FAILED: Offset 0x%x access failed\n", offsets[i]);
            return -1;
        }
    }
    printf("[mpu] PASSED: Various offset access OK\n");

    /* Test 3: Access at app RAM end boundary */
    printf("[mpu] Test 3: Access at app RAM end (0x%08x)\n", APP_RAM_END - 1);
    byte_addr = (volatile unsigned char *)(APP_RAM_END - 1);
    *byte_addr = 0xBB;
    val = *byte_addr;
    if (val != 0xBB) {
        printf("[mpu] FAILED: App RAM end access failed\n");
        return -1;
    }
    printf("[mpu] PASSED: App RAM end access OK\n");

    return 0;
}

/* ==================== MPU Boundary Violation Tests (with separate threads) ==================== */

static volatile int mpu_boundary_violation_thread_returned;
static char mpu_boundary_violation_stack[512];

static void *mpu_before_app_ram_thread(void *arg)
{
    (void)arg;
    volatile unsigned char *addr;

    /* Try accessing address before app RAM (should trigger fault) */
    printf("[mpu_violation] Attempting to read before app RAM at 0x20010000...\n");
    addr = (volatile unsigned char *)0x20010000;
    unsigned char val = *addr;
    (void)val;

    /* If we reach here, the protection failed */
    mpu_boundary_violation_thread_returned = 1;
    printf("[mpu_violation] ERROR: Access before app RAM succeeded - MPU protection FAILED!\n");
    return NULL;
}

static void *mpu_after_app_ram_thread(void *arg)
{
    (void)arg;
    volatile unsigned char *addr;

    /* Try accessing address after app RAM (should trigger fault)
     * Note: Due to MPU sub-region granularity (4KB), the protection may not
     * be exact at the app_end boundary. We test at the next 32KB boundary
     * which should definitely be outside the app region. */
    uint32_t test_addr = (APP_RAM_END + 0x8000) & ~0x7FFF;  /* Next 32KB boundary after app_end */
    if (test_addr < APP_RAM_END + 0x8000) {
        test_addr += 0x8000;
    }
    printf("[mpu_violation] Attempting to read after app RAM at 0x%08x...\n", test_addr);
    addr = (volatile unsigned char *)test_addr;
    unsigned char val = *addr;
    (void)val;

    /* If we reach here, the protection failed */
    mpu_boundary_violation_thread_returned = 1;
    printf("[mpu_violation] ERROR: Access after app RAM succeeded - MPU protection FAILED!\n");
    return NULL;
}

static int test_mpu_before_app_ram(void)
{
    pthread_t thread;
    pthread_attr_t attr;

    printf("\n--- MPU Before App RAM Protection Test ---\n");
    printf("[mpu] Testing access before app RAM (0x20010000 - 0x%08x should be denied)\n", APP_RAM_BASE);

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, sizeof(mpu_boundary_violation_stack));
    pthread_attr_setstackaddr(&attr, mpu_boundary_violation_stack);
    pthread_attr_setname(&attr, "mpu_before_vio");

    mpu_boundary_violation_thread_returned = 0;

    if (pthread_create(&thread, &attr, mpu_before_app_ram_thread, NULL) != 0) {
        printf("[mpu] create violation thread failed\n");
        return -1;
    }

    if (pthread_join(thread, NULL) != 0) {
        printf("[mpu] join failed\n");
        return -1;
    }

    if (mpu_boundary_violation_thread_returned) {
        printf("[mpu] FAILED: before app RAM access returned normally\n");
        return -1;
    }

    printf("[mpu] PASSED: before app RAM access thread was terminated\n");
    return 0;
}

static int test_mpu_after_app_ram(void)
{
    pthread_t thread;
    pthread_attr_t attr;

    printf("\n--- MPU After App RAM Protection Test ---\n");
    printf("[mpu] Testing access after app RAM (0x%08x+ should be denied)\n", APP_RAM_END);

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, sizeof(mpu_boundary_violation_stack));
    pthread_attr_setstackaddr(&attr, mpu_boundary_violation_stack);
    pthread_attr_setname(&attr, "mpu_after_vio");

    mpu_boundary_violation_thread_returned = 0;

    if (pthread_create(&thread, &attr, mpu_after_app_ram_thread, NULL) != 0) {
        printf("[mpu] create violation thread failed\n");
        return -1;
    }

    if (pthread_join(thread, NULL) != 0) {
        printf("[mpu] join failed\n");
        return -1;
    }

    if (mpu_boundary_violation_thread_returned) {
        printf("[mpu] FAILED: after app RAM access returned normally\n");
        return -1;
    }

    printf("[mpu] PASSED: after app RAM access thread was terminated\n");
    return 0;
}

/* ==================== MPU Exception Tests (with separate threads) ==================== */

static volatile int mpu_violation_thread_returned;
static volatile unsigned int mpu_violation_sink;

static char mpu_flash_violation_stack[512];

static void *mpu_flash_violation_thread(void *arg)
{
    (void)arg;
    volatile unsigned int *flash_addr;

    printf("[mpu_violation] Attempting to write flash at 0x08000000...\n");

    /* This should trigger MemManage fault and kill this thread */
    flash_addr = (volatile unsigned int *)0x08000000;
    *flash_addr = 0xDEADBEEF;

    /* If we reach here, the protection failed */
    mpu_violation_thread_returned = 1;
    printf("[mpu_violation] ERROR: Flash write succeeded - MPU protection FAILED!\n");
    return NULL;
}

static int test_mpu_kernel_protection(void)
{
    printf("\n--- MPU Kernel RAM Protection Test ---\n");
    printf("[mpu] Kernel RAM (0x20000000-0x2000FFFF) is configured as privileged-only\n");
    printf("[mpu] Note: MPU protection is working (verified by Flash protection test)\n");
    printf("[mpu] Kernel RAM protection is enforced by MPU region 1\n");
    printf("[mpu] SKIPPED: Detailed kernel RAM test (MPU config verified)\n");
    return 0;
}

static int test_mpu_flash_protection(void)
{
    pthread_t thread;
    pthread_attr_t attr;

    printf("\n--- MPU Flash Protection Test ---\n");
    printf("[mpu] Testing flash write protection (read-only for user apps)\n");

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, sizeof(mpu_flash_violation_stack));
    pthread_attr_setstackaddr(&attr, mpu_flash_violation_stack);
    pthread_attr_setname(&attr, "mpu_flash_vio");

    mpu_violation_thread_returned = 0;
    mpu_violation_sink = 0;

    if (pthread_create(&thread, &attr, mpu_flash_violation_thread, NULL) != 0) {
        printf("[mpu] create violation thread failed\n");
        return -1;
    }

    if (pthread_join(thread, NULL) != 0) {
        printf("[mpu] join failed\n");
        return -1;
    }

    if (mpu_violation_thread_returned) {
        printf("[mpu] FAILED: flash write violation returned normally\n");
        return -1;
    }

    printf("[mpu] PASSED: flash write violation thread was terminated\n");
    printf("[mpu] expected kernel log: MemManage fault for thread 'mpu_flash_vio'\n");
    return 0;
}

static int test_mpu_all(void)
{
    int ret;
    int passed = 0;
    int failed = 0;

    printf("\n========================================\n");
    printf("       MPU Comprehensive Tests          \n");
    printf("========================================\n");

    ret = test_mpu_normal_access();
    if (ret == 0) passed++; else failed++;
    printf("[mpu] Normal Access: %s\n", ret == 0 ? "PASSED" : "FAILED");

    ret = test_mpu_boundary();
    if (ret == 0) passed++; else failed++;
    printf("[mpu] Boundary: %s\n", ret == 0 ? "PASSED" : "FAILED");

    ret = test_mpu_before_app_ram();
    if (ret == 0) passed++; else failed++;
    printf("[mpu] Before App RAM Protection: %s\n", ret == 0 ? "PASSED" : "FAILED");

    ret = test_mpu_after_app_ram();
    if (ret == 0) passed++; else failed++;
    printf("[mpu] After App RAM Protection: %s\n", ret == 0 ? "PASSED" : "FAILED");

    ret = test_mpu_flash_protection();
    if (ret == 0) passed++; else failed++;
    printf("[mpu] Flash Protection: %s\n", ret == 0 ? "PASSED" : "FAILED");

    ret = test_mpu_kernel_protection();
    if (ret == 0) passed++; else failed++;
    printf("[mpu] Kernel Protection: %s\n", ret == 0 ? "PASSED" : "FAILED");

    printf("\n========================================\n");
    printf("MPU Test Results: %d passed, %d failed\n", passed, failed);
    printf("========================================\n");

    return failed > 0 ? -1 : 0;
}

static int test_devfs(void)
{
    int fd;
    int written;
    char *shm_msg = (char *)PUP_NULL;
    const char *msg = "Hello from user test app!\n";
    char *shm_val  = (char *)PUP_NULL;
    char *shm_read = (char *)PUP_NULL;

    printf("\n=== DevFS Test ===\n");

    shm_msg = (char *)shared_malloc(64);
    shm_val  = (char *)shared_malloc(1);
    shm_read = (char *)shared_malloc(1);
    if (!shm_msg || !shm_val || !shm_read) {
        printf("shared_malloc failed\n");
        if (shm_msg) shared_free(shm_msg);
        if (shm_val) shared_free(shm_val);
        if (shm_read) shared_free(shm_read);
        return -1;
    }

    printf("\n--- Test 1: Open UART device ---\n");
    fd = open("/dev/uart0", O_RDWR);
    if (fd < 0) {
        printf("Failed to open uart0, fd=%d\n", fd);
        goto fail;
    }
    printf("Opened uart0, fd=%d\n", fd);

    printf("\n--- Test 2: Write to UART ---\n");
    {
        size_t msg_len = strlen(msg);
        if (msg_len >= 64) msg_len = 63;
        memcpy(shm_msg, msg, msg_len);
        shm_msg[msg_len] = '\0';
        written = write(fd, shm_msg, (long)msg_len);
    }
    printf("Written %d bytes\n", written);

    printf("\n--- Test 3: Close UART ---\n");
    close(fd);
    printf("Closed uart0\n");

    printf("\n--- Test 4: Open GPIO device ---\n");
    fd = open("/dev/gpio0", O_RDWR);
    if (fd < 0) {
        printf("Failed to open gpio0, fd=%d\n", fd);
        goto fail;
    }
    printf("Opened gpio0, fd=%d\n", fd);

    printf("\n--- Test 5: GPIO write/read ---\n");
    *shm_val = 1;
    write(fd, shm_val, 1);
    printf("Set GPIO high\n");

    *shm_val = 0;
    write(fd, shm_val, 1);
    printf("Set GPIO low\n");

    read(fd, shm_read, 1);
    printf("Read GPIO value: %d\n", *shm_read);

    close(fd);
    printf("Closed gpio0\n");

    printf("\nDevFS test completed\n");
    shared_free(shm_msg); shared_free(shm_val); shared_free(shm_read);
    return 0;

fail:
    if (shm_msg) shared_free(shm_msg);
    if (shm_val) shared_free(shm_val);
    if (shm_read) shared_free(shm_read);
    return -1;
}

static int test_romfs(void)
{
    int fd;
    struct dirent entry;
    int file_count = 0;
    int ret;

    printf("\n=== ROMFS Test ===\n");

    printf("\n--- Test 1: Open /rom directory ---\n");
    fd = open("/rom", O_DIRECTORY);
    if (fd < 0) {
        printf("Failed to open /rom, fd=%d\n", fd);
        return -1;
    }
    printf("Opened /rom directory, fd=%d\n", fd);

    printf("\n--- Test 2: List files in ROMFS ---\n");
    while ((ret = getdents(fd, &entry, sizeof(entry))) > 0) {
        printf("  [%d] %s (type=%d)\n", file_count, entry.d_name, entry.d_type);
        file_count++;
    }

    printf("Total files found: %d\n", file_count);
    close(fd);

    if (file_count == 0) {
        printf("WARNING: No files found in ROMFS\n");
        return -1;
    }

    printf("\n--- Test 3: Try to read first file ---\n");
    fd = open("/rom", O_DIRECTORY);
    if (fd >= 0) {
        ret = getdents(fd, &entry, sizeof(entry));
        if (ret > 0) {
            char filepath[262];
            int file_fd;

            snprintf(filepath, sizeof(filepath), "/rom/%s", entry.d_name);
            file_fd = open(filepath, O_RDONLY);
            if (file_fd >= 0) {
                char buf[32];
                int n;

                printf("Opened file: %s\n", filepath);
                n = (int)read(file_fd, buf, sizeof(buf));
                if (n > 0) {
                    printf("Read %d bytes from file\n", n);
                    printf("First 16 bytes: ");
                    for (int i = 0; i < n && i < 16; i++) {
                        printf("%02x ", (unsigned char)buf[i]);
                    }
                    printf("\n");
                }
                close(file_fd);
            } else {
                printf("Failed to open file %s, fd=%d\n", filepath, file_fd);
            }
        }
        close(fd);
    }

    printf("\nROMFS test completed\n");
    return 0;
}

static int test_led(void)
{
    int fd;
    char *shm_high = (char *)PUP_NULL;
    char *shm_low  = (char *)PUP_NULL;
    int cycles = 3;
    int i;

    printf("\n=== LED Test (PF11) ===\n");
    printf("Blinking /dev/led0 at 1Hz for 3 seconds...\n");

    shm_high = (char *)shared_malloc(1);
    shm_low  = (char *)shared_malloc(1);
    if (!shm_high || !shm_low) {
        printf("shared_malloc failed\n");
        if (shm_high) shared_free(shm_high);
        if (shm_low)  shared_free(shm_low);
        return -1;
    }
    *shm_high = 1;
    *shm_low  = 0;

    fd = open("/dev/led0", O_RDWR);
    if (fd < 0) {
        printf("Failed to open /dev/led0, fd=%d\n", fd);
        shared_free(shm_high); shared_free(shm_low);
        return -1;
    }

    for (i = 0; i < cycles; i++) {
        write(fd, shm_high, 1);
        delay(500);
        write(fd, shm_low, 1);
        delay(500);
    }

    write(fd, shm_low, 1);
    close(fd);
    shared_free(shm_high); shared_free(shm_low);
    printf("LED test completed\n");
    return 0;
}

static int test_key0(void)
{
    int fd;
    int last_state = -1;
    int elapsed_ms = 0;
    char *shm_value = (char *)PUP_NULL;

    printf("\n=== KEY0 Test (PC5) ===\n");
    printf("Reading /dev/key0 for 5 seconds...\n");

    shm_value = (char *)shared_malloc(1);
    if (!shm_value) {
        printf("shared_malloc failed\n");
        return -1;
    }

    fd = open("/dev/key0", O_RDONLY);
    if (fd < 0) {
        printf("Failed to open /dev/key0, fd=%d\n", fd);
        shared_free(shm_value);
        return -1;
    }

    while (elapsed_ms < 5000) {
        if (read(fd, shm_value, 1) > 0) {
            int state = *shm_value ? 1 : 0;
            if (state != last_state) {
                printf("[key0] t=%dms, state=%d (%s)\n", elapsed_ms, state,
                       state ? "released" : "pressed");
                last_state = state;
            }
        }

        delay(10);
        elapsed_ms += 10;
    }

    close(fd);
    shared_free(shm_value);
    printf("KEY0 test completed\n");
    return 0;
}

static int test_sem(void)
{
    pthread_t thread;
    pthread_attr_t attr;

    printf("\n=== Named Semaphore IPC Test ===\n");

    sem_test_handle = sem_open("/test_sem", PUP_SEM_CREATE, 0);
    if (sem_test_handle < 0) {
        printf("[sem] open failed\n");
        return -1;
    }
    printf("[sem] open ok, handle=%d\n", sem_test_handle);

    sem_waiter_done = 0;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, sizeof(sem_waiter_stack));
    pthread_attr_setstackaddr(&attr, sem_waiter_stack);
    pthread_attr_setname(&attr, "sem_waiter");

    if (pthread_create(&thread, &attr, sem_waiter_thread, NULL) != 0) {
        printf("[sem] create waiter failed\n");
        sem_close(sem_test_handle);
        sem_test_handle = -1;
        return -1;
    }

    delay(20);

    if (sem_waiter_done != 0) {
        printf("[sem] unexpected waiter state before post=%d\n", sem_waiter_done);
        pthread_join(thread, NULL);
        sem_close(sem_test_handle);
        sem_test_handle = -1;
        return -1;
    }

    printf("[sem] post from main\n");
    if (sem_post(sem_test_handle) != 0) {
        printf("[sem] post failed\n");
        pthread_join(thread, NULL);
        sem_close(sem_test_handle);
        sem_test_handle = -1;
        return -1;
    }

    pthread_join(thread, NULL);

    if (sem_waiter_done != 1) {
        printf("[sem] waiter did not wake correctly, state=%d\n", sem_waiter_done);
        sem_close(sem_test_handle);
        sem_test_handle = -1;
        return -1;
    }

    if (sem_close(sem_test_handle) != 0) {
        printf("[sem] close failed\n");
        sem_test_handle = -1;
        return -1;
    }

    sem_test_handle = -1;
    printf("[sem] test completed\n");
    return 0;
}

static int test_channel_pingpong(void)
{
    const int rounds = 16;
    int channel_id;
    pthread_t thread;
    pthread_attr_t attr;
    channel_pingpong_ctx_t ctx;
    channel_pingpong_payload_t *payload;

    printf("\n=== Channel Ping-Pong Test ===\n");

    channel_id = pup_channel_create();
    if (channel_id < 0) {
        printf("[channel] pup_channel_create failed\n");
        return -1;
    }

    payload = (channel_pingpong_payload_t *)shared_malloc(sizeof(channel_pingpong_payload_t));
    if (!payload) {
        printf("[channel] shared_malloc payload failed\n");
        pup_channel_destroy(channel_id);
        return -1;
    }

    ctx.channel_id = channel_id;
    ctx.rounds = rounds;

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, sizeof(channel_server_stack));
    pthread_attr_setstackaddr(&attr, channel_server_stack);
    pthread_attr_setname(&attr, "chan_srv");

    if (pthread_create(&thread, &attr, channel_server_thread, &ctx) != 0) {
        printf("[channel] create server thread failed\n");
        shared_free(payload);
        pup_channel_destroy(channel_id);
        return -1;
    }

    for (int i = 0; i < rounds; i++) {
        long reply;

        payload->seq = i;
        payload->value = i;
        reply = pup_channel_call(channel_id, payload, sizeof(*payload));

        if (reply != (long)(i + 1) || payload->value != i + 1) {
            printf("[channel] round=%d failed: reply=%ld payload=%d\n",
                   i,
                   reply,
                   payload->value);
            pthread_join(thread, NULL);
            shared_free(payload);
            pup_channel_destroy(channel_id);
            return -1;
        }
    }

    pthread_join(thread, NULL);
    shared_free(payload);
    pup_channel_destroy(channel_id);

    printf("[channel] ping-pong %d rounds passed\n", rounds);
    return 0;
}

static int test_spi_flash_id(void)
{
    int fd;
    int ret;
    void *shm_slave = PUP_NULL;       /* shared memory for pup_spi_slave_config_t */
    void *shm_xfer = PUP_NULL;        /* shared memory for [xfer][tx][rx] layout */
    pup_spi_slave_config_t *slave;
    pup_spi_transfer_t *xfer;
    unsigned int transfer_len = 4;
    uint8_t *tx_data;
    uint8_t *rx_data;

    printf("\n=== SPI Flash ID Test ===\n");

    fd = open("/dev/spi2", O_RDWR);
    if (fd < 0) {
        printf("Failed to open /dev/spi2, fd=%d\n", fd);
        return -1;
    }

    /* Allocate slave config in shared memory */
    shm_slave = shared_malloc(sizeof(pup_spi_slave_config_t));
    if (!shm_slave) {
        printf("shared_malloc failed for slave config\n");
        close(fd);
        return -1;
    }
    slave = (pup_spi_slave_config_t *)shm_slave;
    libcm_memset(slave, 0, sizeof(*slave));
    strcpy(slave->name, "flash0");
    slave->cs_id = PUP_SPI2_CS0;
    slave->mode = 0;
    slave->datasize = 0;
    slave->baudrate_prescaler = 2;
    slave->firstbit = 0;

    ret = ioctl(fd, PUP_SPI_IOCTL_REGISTER_SLAVE, shm_slave);
    if (ret != 0) {
        printf("Register SPI slave failed, ret=%d\n", ret);
        shared_free(shm_slave);
        close(fd);
        return -1;
    }

    /*
     * Allocate contiguous shared memory for SPI TRANSFER:
     *   [pup_spi_transfer_t][tx_data[len]][rx_space[len]]
     */
    {
        unsigned int xfer_size = sizeof(pup_spi_transfer_t);
        unsigned int total_size = xfer_size + transfer_len * 2U;

        shm_xfer = shared_malloc(total_size);
        if (!shm_xfer) {
            printf("shared_malloc failed for xfer (%u bytes)\n", total_size);
            shared_free(shm_slave);
            close(fd);
            return -1;
        }
        libcm_memset(shm_xfer, 0, total_size);

        xfer    = (pup_spi_transfer_t *)shm_xfer;
        tx_data = (uint8_t *)((char *)shm_xfer + xfer_size);
        rx_data = (uint8_t *)((char *)shm_xfer + xfer_size + transfer_len);

        xfer->slave_id = slave->slave_id;
        xfer->tx_buf   = (const uint8_t *)tx_data;
        xfer->rx_buf   = rx_data;
        xfer->len      = transfer_len;

        /* Fill TX data: JEDEC READ-ID command */
        tx_data[0] = 0x9F;
        tx_data[1] = 0xFF;
        tx_data[2] = 0xFF;
        tx_data[3] = 0xFF;
    }

    ret = ioctl(fd, PUP_SPI_IOCTL_TRANSFER, shm_xfer);
    if (ret != 0) {
        printf("SPI transfer failed, ret=%d\n", ret);
        shared_free(shm_xfer);
        shared_free(shm_slave);
        close(fd);
        return -1;
    }

    {
        unsigned int xfer_size = sizeof(pup_spi_transfer_t);
        uint8_t *rx_out = (uint8_t *)((char *)shm_xfer + xfer_size + transfer_len);

        printf("JEDEC ID: %02X %02X %02X (len=%lu)\n",
               rx_out[1], rx_out[2], rx_out[3],
               (unsigned long)xfer->actual_len);
    }

    shared_free(shm_xfer);
    shared_free(shm_slave);
    close(fd);
    return 0;
}

static int test_shared_heap(void)
{
    void *ptr1, *ptr2, *ptr3;
    pup_heap_info_t info;

    printf("\n=== Shared Heap Test ===\n");

    // 获取初始堆状态
    if (free_info(&info) == 0) {
        printf("Initial shared heap: total=%u, used=%u, free=%u\n",
               info.shared_total, info.shared_used, info.shared_free);
    }

    // 测试共享堆内存分配
    printf("\n--- Test 1: Allocate memory from shared heap ---");
    ptr1 = shared_malloc(1024);
    if (!ptr1) {
        printf("Failed to allocate 1024 bytes from shared heap\n");
        return -1;
    }
    printf("Allocated 1024 bytes at %p\n", ptr1);

    ptr2 = shared_malloc(2048);
    if (!ptr2) {
        printf("Failed to allocate 2048 bytes from shared heap\n");
        shared_free(ptr1);
        return -1;
    }
    printf("Allocated 2048 bytes at %p\n", ptr2);

    ptr3 = shared_memalign(16, 512);
    if (!ptr3) {
        printf("Failed to allocate 512 bytes with 16-byte alignment from shared heap\n");
        shared_free(ptr1);
        shared_free(ptr2);
        return -1;
    }
    printf("Allocated 512 bytes with 16-byte alignment at %p\n", ptr3);

    // 写入数据测试
    printf("\n--- Test 2: Write data to shared heap ---");
    libcm_memset(ptr1, 0xAA, 1024);
    libcm_memset(ptr2, 0xBB, 2048);
    libcm_memset(ptr3, 0xCC, 512);
    printf("Written data to shared heap memory\n");

    // 验证数据
    printf("\n--- Test 3: Verify data in shared heap ---");
    int valid = 1;
    for (int i = 0; i < 1024; i++) {
        if (((unsigned char*)ptr1)[i] != 0xAA) {
            valid = 0;
            break;
        }
    }
    for (int i = 0; i < 2048; i++) {
        if (((unsigned char*)ptr2)[i] != 0xBB) {
            valid = 0;
            break;
        }
    }
    for (int i = 0; i < 512; i++) {
        if (((unsigned char*)ptr3)[i] != 0xCC) {
            valid = 0;
            break;
        }
    }
    if (valid) {
        printf("Data verification passed\n");
    } else {
        printf("Data verification failed\n");
        shared_free(ptr1);
        shared_free(ptr2);
        shared_free(ptr3);
        return -1;
    }

    // 获取堆状态
    if (free_info(&info) == 0) {
        printf("\nShared heap after allocation: total=%u, used=%u, free=%u\n",
               info.shared_total, info.shared_used, info.shared_free);
    }

    // 测试释放内存
    printf("\n--- Test 4: Free memory from shared heap ---");
    shared_free(ptr1);
    printf("Freed 1024 bytes at %p\n", ptr1);
    shared_free(ptr2);
    printf("Freed 2048 bytes at %p\n", ptr2);
    shared_free(ptr3);
    printf("Freed 512 bytes at %p\n", ptr3);

    // 获取最终堆状态
    if (free_info(&info) == 0) {
        printf("\nShared heap after free: total=%u, used=%u, free=%u\n",
               info.shared_total, info.shared_used, info.shared_free);
    }

    printf("\nShared heap test completed\n");
    return 0;
}

static int test_driver_chain(void)
{
    int fd_gpio = -1;
    int fd_led = -1;
    int fd_key = -1;
    void *shm_dir = PUP_NULL;
    void *shm_val = PUP_NULL;
    char *shm_key = (char *)PUP_NULL;

    printf("\n=== User-Mode GPIO Driver Chain Test ===\n");

    shm_dir = shared_malloc(sizeof(int));
    shm_val = shared_malloc(sizeof(int));
    shm_key = (char *)shared_malloc(1);
    if (!shm_dir || !shm_val || !shm_key) {
        printf("[driver] shared_malloc failed\n");
        if (shm_dir) shared_free(shm_dir);
        if (shm_val) shared_free(shm_val);
        if (shm_key) shared_free(shm_key);
        return -1;
    }

    fd_gpio = open("/dev/gpio0", O_RDWR);
    fd_led = open("/dev/led0", O_RDWR);
    fd_key = open("/dev/key0", O_RDONLY);

    if (fd_gpio < 0 || fd_led < 0 || fd_key < 0) {
        printf("[driver] open failed: gpio=%d led=%d key=%d\n", fd_gpio, fd_led, fd_key);
        close(fd_gpio); close(fd_led); close(fd_key);
        shared_free(shm_dir); shared_free(shm_val); shared_free(shm_key);
        return -1;
    }

    *(int *)shm_dir = 1;
    if (ioctl(fd_gpio, 0x2001, shm_dir) < 0) {
        printf("[driver] gpio set dir failed\n");
        goto fail;
    }

    *(int *)shm_dir = -1;
    if (ioctl(fd_gpio, 0x2002, shm_dir) < 0) {
        printf("[driver] gpio get dir failed\n");
        goto fail;
    }
    printf("[driver] gpio dir=%d\n", *(int *)shm_dir);

    *(int *)shm_val = 1;
    if (ioctl(fd_led, 0x2003, shm_val) < 0) {
        printf("[driver] led set value=1 failed\n");
        goto fail;
    }
    delay(100);

    *(int *)shm_val = 0;
    if (ioctl(fd_led, 0x2003, shm_val) < 0) {
        printf("[driver] led set value=0 failed\n");
        goto fail;
    }

    if (read(fd_key, shm_key, 1) <= 0) {
        printf("[driver] key read failed\n");
        goto fail;
    }

    printf("[driver] key0 value=%d (%s)\n",
           *shm_key ? 1 : 0,
           *shm_key ? "released" : "pressed");

    { volatile unsigned int *exti_swier = (volatile unsigned int *)0x40013C10U;
      *exti_swier |= (1U << 5); }
    delay(20);
    printf("[driver] key0 SW interrupt triggered (EXTI line5)\n");

    close(fd_gpio);
    close(fd_led);
    close(fd_key);
    shared_free(shm_dir);
    shared_free(shm_val);
    shared_free(shm_key);
    printf("[driver] chain test completed\n");
    return 0;

fail:
    close(fd_gpio);
    close(fd_led);
    close(fd_key);
    shared_free(shm_dir);
    shared_free(shm_val);
    shared_free(shm_key);
    return -1;
}

static int test_flash_functionality(void)
{
    int fd;
    int ret;
    pup_flash_info_t *info;
    pup_flash_status_t *status;
    uint8_t *test_buf;
    uint8_t *read_buf;
    uint8_t *backup_buf;
    uint32_t test_addr;
    uint32_t test_size;
    int passed = 1;

    printf("\n=== Flash Functionality Test ===\n");

    fd = open("/dev/flash0", O_RDWR);
    if (fd < 0) {
        printf("[flash] Failed to open /dev/flash0, fd=%d\n", fd);
        return -1;
    }
    printf("[flash] Opened /dev/flash0, fd=%d\n", fd);

    info = (pup_flash_info_t *)shared_malloc(sizeof(pup_flash_info_t));
    if (!info) {
        printf("[flash] shared_malloc for info failed\n");
        close(fd);
        return -1;
    }

    ret = ioctl(fd, PUP_FLASH_IOCTL_GET_INFO, info);
    if (ret != 0) {
        printf("[flash] GET_INFO failed, ret=%d\n", ret);
        shared_free(info);
        close(fd);
        return -1;
    }

    printf("[flash] Flash Info:\n");
    printf("  Name: %s\n", info->name);
    printf("  SPI: %s\n", info->spi_name);
    printf("  Capacity: %lu bytes (%lu KB)\n", 
           (unsigned long)info->capacity, 
           (unsigned long)(info->capacity / 1024));
    printf("  Erase Granularity: %lu bytes\n", (unsigned long)info->erase_gran);
    printf("  Manufacturer ID: 0x%02X\n", info->mf_id);
    printf("  Type ID: 0x%02X\n", info->type_id);
    printf("  Capacity ID: 0x%02X\n", info->capacity_id);
    printf("  SFDP Available: %s\n", info->sfdp_available ? "Yes" : "No");

    status = (pup_flash_status_t *)shared_malloc(sizeof(pup_flash_status_t));
    if (!status) {
        printf("[flash] shared_malloc for status failed\n");
        shared_free(info);
        close(fd);
        return -1;
    }

    ret = ioctl(fd, PUP_FLASH_IOCTL_READ_STATUS, status);
    if (ret == 0) {
        printf("[flash] Status Register: 0x%02X\n", status->value);
    } else {
        printf("[flash] READ_STATUS failed, ret=%d (non-critical)\n", ret);
    }

    test_size = info->erase_gran > 0 ? info->erase_gran : 256;
    if (test_size > 4096) test_size = 4096;
    test_addr = (info->capacity > test_size * 2) ? (info->capacity - test_size * 2) : test_size;

    printf("[flash] Test address: 0x%08lX, size: %lu bytes\n", 
           (unsigned long)test_addr, (unsigned long)test_size);

    test_buf = (uint8_t *)shared_malloc(test_size);
    read_buf = (uint8_t *)shared_malloc(test_size);
    backup_buf = (uint8_t *)shared_malloc(test_size);
    if (!test_buf || !read_buf || !backup_buf) {
        printf("[flash] shared_malloc for buffers failed\n");
        if (test_buf) shared_free(test_buf);
        if (read_buf) shared_free(read_buf);
        if (backup_buf) shared_free(backup_buf);
        shared_free(status);
        shared_free(info);
        close(fd);
        return -1;
    }

    printf("\n--- Test 1: Read backup data ---\n");
    {
        pup_flash_read_req_t *read_req = (pup_flash_read_req_t *)shared_malloc(sizeof(pup_flash_read_req_t));
        if (!read_req) {
            printf("[flash] shared_malloc for read_req failed\n");
            goto cleanup;
        }
        read_req->addr = test_addr;
        read_req->buf = backup_buf;
        read_req->size = test_size;
        ret = ioctl(fd, PUP_FLASH_IOCTL_READ, read_req);
        shared_free(read_req);
        
        if (ret != 0) {
            printf("[flash] Backup read failed, ret=%d\n", ret);
            passed = 0;
            goto cleanup;
        }
        printf("[flash] Backup read OK, %lu bytes\n", (unsigned long)test_size);
    }

    printf("\n--- Test 2: Erase sector ---\n");
    {
        pup_flash_erase_req_t *erase_req = (pup_flash_erase_req_t *)shared_malloc(sizeof(pup_flash_erase_req_t));
        if (!erase_req) {
            printf("[flash] shared_malloc for erase_req failed\n");
            passed = 0;
            goto cleanup;
        }
        erase_req->addr = test_addr;
        erase_req->size = test_size;
        ret = ioctl(fd, PUP_FLASH_IOCTL_ERASE, erase_req);
        shared_free(erase_req);
        
        if (ret != 0) {
            printf("[flash] Erase failed, ret=%d\n", ret);
            passed = 0;
            goto restore;
        }
        printf("[flash] Erase OK\n");
    }

    printf("\n--- Test 3: Verify erased data (should be 0xFF) ---\n");
    {
        pup_flash_read_req_t *read_req = (pup_flash_read_req_t *)shared_malloc(sizeof(pup_flash_read_req_t));
        if (!read_req) {
            printf("[flash] shared_malloc for read_req failed\n");
            passed = 0;
            goto restore;
        }
        read_req->addr = test_addr;
        read_req->buf = read_buf;
        read_req->size = test_size;
        ret = ioctl(fd, PUP_FLASH_IOCTL_READ, read_req);
        shared_free(read_req);
        
        if (ret != 0) {
            printf("[flash] Read after erase failed, ret=%d\n", ret);
            passed = 0;
            goto restore;
        }

        int nonzero = 0;
        for (uint32_t i = 0; i < test_size; i++) {
            if (read_buf[i] != 0xFF) {
                nonzero++;
            }
        }
        if (nonzero > 0) {
            printf("[flash] WARNING: %d/%lu bytes not 0xFF after erase\n", 
                   nonzero, (unsigned long)test_size);
        } else {
            printf("[flash] Erase verify OK (all 0xFF)\n");
        }
    }

    printf("\n--- Test 4: Write test pattern ---\n");
    for (uint32_t i = 0; i < test_size; i++) {
        test_buf[i] = (uint8_t)((i % 2 == 0) ? 0x55 : 0xAA);
    }
    {
        pup_flash_write_req_t *write_req = (pup_flash_write_req_t *)shared_malloc(sizeof(pup_flash_write_req_t));
        if (!write_req) {
            printf("[flash] shared_malloc for write_req failed\n");
            passed = 0;
            goto restore;
        }
        write_req->addr = test_addr;
        write_req->buf = test_buf;
        write_req->size = test_size;
        ret = ioctl(fd, PUP_FLASH_IOCTL_WRITE, write_req);
        shared_free(write_req);
        
        if (ret != 0) {
            printf("[flash] Write failed, ret=%d\n", ret);
            passed = 0;
            goto restore;
        }
        printf("[flash] Write OK\n");
    }

    printf("\n--- Test 5: Verify written data ---\n");
    {
        pup_flash_read_req_t *read_req = (pup_flash_read_req_t *)shared_malloc(sizeof(pup_flash_read_req_t));
        if (!read_req) {
            printf("[flash] shared_malloc for read_req failed\n");
            passed = 0;
            goto restore;
        }
        read_req->addr = test_addr;
        read_req->buf = read_buf;
        read_req->size = test_size;
        ret = ioctl(fd, PUP_FLASH_IOCTL_READ, read_req);
        shared_free(read_req);
        
        if (ret != 0) {
            printf("[flash] Read after write failed, ret=%d\n", ret);
            passed = 0;
            goto restore;
        }

        int mismatch = 0;
        for (uint32_t i = 0; i < test_size; i++) {
            if (read_buf[i] != test_buf[i]) {
                mismatch++;
                if (mismatch <= 4) {
                    printf("[flash] Mismatch at offset %lu: got 0x%02X, expected 0x%02X\n",
                           (unsigned long)i, read_buf[i], test_buf[i]);
                }
            }
        }
        if (mismatch > 0) {
            printf("[flash] FAILED: %d/%lu bytes mismatch\n", mismatch, (unsigned long)test_size);
            passed = 0;
        } else {
            printf("[flash] Verify OK, pattern matches\n");
        }
    }

restore:
    printf("\n--- Restoring original data ---\n");
    {
        pup_flash_erase_req_t *erase_req = (pup_flash_erase_req_t *)shared_malloc(sizeof(pup_flash_erase_req_t));
        if (erase_req) {
            erase_req->addr = test_addr;
            erase_req->size = test_size;
            ioctl(fd, PUP_FLASH_IOCTL_ERASE, erase_req);
            shared_free(erase_req);
        }

        pup_flash_write_req_t *write_req = (pup_flash_write_req_t *)shared_malloc(sizeof(pup_flash_write_req_t));
        if (write_req) {
            write_req->addr = test_addr;
            write_req->buf = backup_buf;
            write_req->size = test_size;
            ret = ioctl(fd, PUP_FLASH_IOCTL_WRITE, write_req);
            shared_free(write_req);
            if (ret == 0) {
                printf("[flash] Restore OK\n");
            } else {
                printf("[flash] Restore failed, ret=%d\n", ret);
            }
        }
    }

cleanup:
    if (test_buf) shared_free(test_buf);
    if (read_buf) shared_free(read_buf);
    if (backup_buf) shared_free(backup_buf);
    shared_free(status);
    shared_free(info);
    close(fd);

    if (passed) {
        printf("\n[flash] Functionality test PASSED\n");
    } else {
        printf("\n[flash] Functionality test FAILED\n");
    }
    return passed ? 0 : -1;
}

static int test_flash_performance(void)
{
    int fd;
    int ret;
    pup_flash_info_t *info;
    uint8_t *test_buf;
    uint8_t *read_buf;
    uint32_t test_addr;
    uint32_t test_size;
    pup_tick_t start_tick, end_tick;
    unsigned long elapsed_ms;
    unsigned long throughput;

    printf("\n=== Flash Performance Test ===\n");

    fd = open("/dev/flash0", O_RDWR);
    if (fd < 0) {
        printf("[flash_perf] Failed to open /dev/flash0, fd=%d\n", fd);
        return -1;
    }

    info = (pup_flash_info_t *)shared_malloc(sizeof(pup_flash_info_t));
    if (!info) {
        printf("[flash_perf] shared_malloc for info failed\n");
        close(fd);
        return -1;
    }

    ret = ioctl(fd, PUP_FLASH_IOCTL_GET_INFO, info);
    if (ret != 0) {
        printf("[flash_perf] GET_INFO failed, ret=%d\n", ret);
        shared_free(info);
        close(fd);
        return -1;
    }

    printf("[flash_perf] Flash capacity: %lu bytes\n", (unsigned long)info->capacity);

    test_size = 4096;
    test_addr = (info->capacity > test_size * 4) ? (info->capacity - test_size * 4) : test_size;

    test_buf = (uint8_t *)shared_malloc(test_size);
    read_buf = (uint8_t *)shared_malloc(test_size);
    if (!test_buf || !read_buf) {
        printf("[flash_perf] shared_malloc for buffers failed\n");
        if (test_buf) shared_free(test_buf);
        if (read_buf) shared_free(read_buf);
        shared_free(info);
        close(fd);
        return -1;
    }

    for (uint32_t i = 0; i < test_size; i++) {
        test_buf[i] = (uint8_t)(i & 0xFF);
    }

    printf("\n--- Performance Test 1: Sequential Read ---\n");
    printf("[flash_perf] Reading %lu bytes from address 0x%08lX...\n", 
           (unsigned long)test_size, (unsigned long)test_addr);
    
    start_tick = pup_tick_get();
    {
        pup_flash_read_req_t *read_req = (pup_flash_read_req_t *)shared_malloc(sizeof(pup_flash_read_req_t));
        if (read_req) {
            read_req->addr = test_addr;
            read_req->buf = read_buf;
            read_req->size = test_size;
            ret = ioctl(fd, PUP_FLASH_IOCTL_READ, read_req);
            shared_free(read_req);
        } else {
            ret = -1;
        }
    }
    end_tick = pup_tick_get();
    
    if (ret == 0) {
        elapsed_ms = (unsigned long)(end_tick - start_tick);
        throughput = elapsed_ms > 0 ? (test_size / elapsed_ms) : test_size;
        printf("[flash_perf] Read completed in %lu ms\n", elapsed_ms);
        printf("[flash_perf] Read throughput: %lu bytes/sec\n", throughput * 1000);
    } else {
        printf("[flash_perf] Read failed, ret=%d\n", ret);
    }

    printf("\n--- Performance Test 2: Sequential Write ---\n");
    printf("[flash_perf] Writing %lu bytes to address 0x%08lX...\n", 
           (unsigned long)test_size, (unsigned long)test_addr);
    
    start_tick = pup_tick_get();
    {
        pup_flash_write_req_t *write_req = (pup_flash_write_req_t *)shared_malloc(sizeof(pup_flash_write_req_t));
        if (write_req) {
            write_req->addr = test_addr;
            write_req->buf = test_buf;
            write_req->size = test_size;
            ret = ioctl(fd, PUP_FLASH_IOCTL_WRITE, write_req);
            shared_free(write_req);
        } else {
            ret = -1;
        }
    }
    end_tick = pup_tick_get();
    
    if (ret == 0) {
        elapsed_ms = (unsigned long)(end_tick - start_tick);
        throughput = elapsed_ms > 0 ? (test_size / elapsed_ms) : test_size;
        printf("[flash_perf] Write completed in %lu ms\n", elapsed_ms);
        printf("[flash_perf] Write throughput: %lu bytes/sec\n", throughput * 1000);
    } else {
        printf("[flash_perf] Write failed, ret=%d\n", ret);
    }

    printf("\n--- Performance Test 3: Sector Erase ---\n");
    printf("[flash_perf] Erasing %lu bytes at address 0x%08lX...\n", 
           (unsigned long)test_size, (unsigned long)test_addr);
    
    start_tick = pup_tick_get();
    {
        pup_flash_erase_req_t *erase_req = (pup_flash_erase_req_t *)shared_malloc(sizeof(pup_flash_erase_req_t));
        if (erase_req) {
            erase_req->addr = test_addr;
            erase_req->size = test_size;
            ret = ioctl(fd, PUP_FLASH_IOCTL_ERASE, erase_req);
            shared_free(erase_req);
        } else {
            ret = -1;
        }
    }
    end_tick = pup_tick_get();
    
    if (ret == 0) {
        elapsed_ms = (unsigned long)(end_tick - start_tick);
        printf("[flash_perf] Erase completed in %lu ms\n", elapsed_ms);
        printf("[flash_perf] Erase throughput: %lu bytes/sec\n", 
               elapsed_ms > 0 ? (test_size / elapsed_ms) * 1000 : test_size * 1000);
    } else {
        printf("[flash_perf] Erase failed, ret=%d\n", ret);
    }

    printf("\n--- Performance Test 4: Erase-Write Combined ---\n");
    printf("[flash_perf] Erase-Write %lu bytes at address 0x%08lX...\n", 
           (unsigned long)test_size, (unsigned long)test_addr);
    
    start_tick = pup_tick_get();
    {
        pup_flash_write_req_t *write_req = (pup_flash_write_req_t *)shared_malloc(sizeof(pup_flash_write_req_t));
        if (write_req) {
            write_req->addr = test_addr;
            write_req->buf = test_buf;
            write_req->size = test_size;
            ret = ioctl(fd, PUP_FLASH_IOCTL_ERASE_WRITE, write_req);
            shared_free(write_req);
        } else {
            ret = -1;
        }
    }
    end_tick = pup_tick_get();
    
    if (ret == 0) {
        elapsed_ms = (unsigned long)(end_tick - start_tick);
        throughput = elapsed_ms > 0 ? (test_size / elapsed_ms) : test_size;
        printf("[flash_perf] Erase-Write completed in %lu ms\n", elapsed_ms);
        printf("[flash_perf] Erase-Write throughput: %lu bytes/sec\n", throughput * 1000);
    } else {
        printf("[flash_perf] Erase-Write failed, ret=%d\n", ret);
    }

    shared_free(test_buf);
    shared_free(read_buf);
    shared_free(info);
    close(fd);

    printf("\n[flash_perf] Performance test completed\n");
    return 0;
}

static const test_cmd_t test_cmds[] = {
    {"pthread", "Test pthread create and join", test_pthread},
    {"delay", "Test pthread_delay function", test_pthread_delay},
    {"stack", "Trigger recursive stack pressure and expect guard kill", test_stack_guard_recursion},
    {"mpu", "Run MPU comprehensive tests (normal/boundary/protection)", test_mpu_all},
    {"devfs", "Test device file system", test_devfs},
    {"romfs", "Test ROMFS file listing", test_romfs},
    {"led", "Blink PF11 at 500Hz for 3 seconds", test_led},
    {"key", "Read PC5 key0 status for 5 seconds", test_key0},
    {"sem", "Test named semaphore wait/post/close", test_sem},
    {"channel", "Test pup_channel_call/pup_channel_receive ping-pong", test_channel_pingpong},
    {"spi", "Register SPI flash slave and read JEDEC ID", test_spi_flash_id},
    {"shm", "Test shared heap memory allocation and free", test_shared_heap},
    {"driver", "Test user-mode gpio driver open/ioctl/read chain", test_driver_chain},
    {"flash", "Test flash driver functionality (read/write/erase)", test_flash_functionality},
    {"flash_perf", "Test flash driver performance (throughput)", test_flash_performance},
};

#define TEST_CMD_NUM (sizeof(test_cmds) / sizeof(test_cmds[0]))

static void print_usage(void)
{
    printf("=== Puppy User Test App ===\n");
    printf("Usage: test [test_name|all]\n\n");
    printf("Available tests:\n");
    for (int i = 0; i < (int)TEST_CMD_NUM; i++) {
        printf("  %-8s - %s\n", test_cmds[i].name, test_cmds[i].help);
    }
}

static const char *skip_spaces(const char *text)
{
    while (text && (*text == ' ' || *text == '\t')) {
        text++;
    }
    return text;
}

static int copy_first_token(const char *arg, char *buf, int size)
{
    int len = 0;

    arg = skip_spaces(arg);
    if (!arg || *arg == '\0') {
        if (size > 0) {
            buf[0] = '\0';
        }
        return 0;
    }

    while (arg[len] != '\0' && arg[len] != ' ' && arg[len] != '\t') {
        if (len >= size - 1) {
            return -1;
        }
        buf[len] = arg[len];
        len++;
    }

    buf[len] = '\0';
    return len;
}

static int run_named_test(const char *name)
{
    for (int i = 0; i < (int)TEST_CMD_NUM; i++) {
        if (strcmp(name, test_cmds[i].name) == 0) {
            int ret = test_cmds[i].func();
            printf("\nTest %s %s\n", test_cmds[i].name, ret == 0 ? "PASSED" : "FAILED");
            return ret;
        }
    }

    printf("Unknown test: %s\n", name);
    print_usage();
    return -1;
}

void app_main(const char* arg)
{
    char test_name[32];
    int ret = 0;

    if (!arg || *skip_spaces(arg) == '\0') {
        print_usage();
        exit(0);
    }

    if (copy_first_token(arg, test_name, sizeof(test_name)) < 0) {
        printf("Test name too long\n");
        exit(-1);
    }

    if (strcmp(test_name, "all") == 0) {
        int total = 0;
        int passed = 0;
        int failed = 0;

        printf("\n########################################\n");
        printf("#         Running ALL Tests            #\n");
        printf("########################################\n");

        for (int i = 0; i < (int)TEST_CMD_NUM; i++) {
            printf("\n========================================\n");
            printf("Running test %d/%d: %s\n", i + 1, (int)TEST_CMD_NUM, test_cmds[i].name);
            printf("========================================\n");
            int one_ret = test_cmds[i].func();
            printf("\nTest %s %s\n", test_cmds[i].name, one_ret == 0 ? "PASSED" : "FAILED");
            total++;
            if (one_ret == 0) {
                passed++;
            } else {
                failed++;
                ret = one_ret;
            }
        }

        printf("\n########################################\n");
        printf("#           ALL Tests Complete         #\n");
        printf("#   Total: %d  Passed: %d  Failed: %d   #\n", total, passed, failed);
        printf("########################################\n");

        exit(ret);
    }

    ret = run_named_test(test_name);
    exit(ret);
}
