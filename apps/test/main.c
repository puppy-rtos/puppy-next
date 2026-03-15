#include "libc.h"
#include "syscall.h"

#define O_RDONLY    0000000
#define O_WRONLY    0000001
#define O_RDWR      0000002
#define O_DIRECTORY 00200000

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

static char pth1_stack[4096];
static char pth2_stack[4096];
static char delay_thread_stack[4096];
static char sem_waiter_stack[2048];

static volatile int sem_waiter_done;
static int sem_test_handle = -1;

static void print_usage(void);
static int run_named_test(const char *name);

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

static int test_mpu(void)
{
    printf("\n=== MPU Test ===\n");
    printf("MPU configuration is kernel-only and cannot be executed from user mode\n");
    return -1;
}

static int test_devfs(void)
{
    int fd;
    int written;
    const char *msg = "Hello from user test app!\n";
    char val;
    char read_val = 0;

    printf("\n=== DevFS Test ===\n");

    printf("\n--- Test 1: Open UART device ---\n");
    fd = open("/dev/uart0", O_RDWR);
    if (fd < 0) {
        printf("Failed to open uart0, fd=%d\n", fd);
        return -1;
    }
    printf("Opened uart0, fd=%d\n", fd);

    printf("\n--- Test 2: Write to UART ---\n");
    written = (int)write(fd, msg, (long)strlen(msg));
    printf("Written %d bytes\n", written);

    printf("\n--- Test 3: Close UART ---\n");
    close(fd);
    printf("Closed uart0\n");

    printf("\n--- Test 4: Open GPIO device ---\n");
    fd = open("/dev/gpio0", O_RDWR);
    if (fd < 0) {
        printf("Failed to open gpio0, fd=%d\n", fd);
        return -1;
    }
    printf("Opened gpio0, fd=%d\n", fd);

    printf("\n--- Test 5: GPIO write/read ---\n");
    val = 1;
    write(fd, &val, 1);
    printf("Set GPIO high\n");

    val = 0;
    write(fd, &val, 1);
    printf("Set GPIO low\n");

    read(fd, &read_val, 1);
    printf("Read GPIO value: %d\n", read_val);

    close(fd);
    printf("Closed gpio0\n");

    printf("\nDevFS test completed\n");
    return 0;
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
    char high = 1;
    char low = 0;
    int cycles = 3;

    printf("\n=== LED Test (PF11) ===\n");
    printf("Blinking /dev/led0 at 1Hz for 3 seconds...\n");

    fd = open("/dev/led0", O_RDWR);
    if (fd < 0) {
        printf("Failed to open /dev/led0, fd=%d\n", fd);
        return -1;
    }

    for (int i = 0; i < cycles; i++) {
        write(fd, &high, 1);
        delay(500);
        write(fd, &low, 1);
        delay(500);
    }

    write(fd, &low, 1);
    close(fd);
    printf("LED test completed\n");
    return 0;
}

static int test_key0(void)
{
    int fd;
    int last_state = -1;
    int elapsed_ms = 0;
    char value = 0;

    printf("\n=== KEY0 Test (PC5) ===\n");
    printf("Reading /dev/key0 for 5 seconds...\n");

    fd = open("/dev/key0", O_RDONLY);
    if (fd < 0) {
        printf("Failed to open /dev/key0, fd=%d\n", fd);
        return -1;
    }

    while (elapsed_ms < 5000) {
        if (read(fd, &value, 1) > 0) {
            int state = value ? 1 : 0;
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

static const test_cmd_t test_cmds[] = {
    {"pthread", "Test pthread create and join", test_pthread},
    {"delay", "Test pthread_delay function", test_pthread_delay},
    {"mpu", "Show why MPU test stays in kernel", test_mpu},
    {"devfs", "Test device file system", test_devfs},
    {"romfs", "Test ROMFS file listing", test_romfs},
    {"led", "Blink PF11 at 500Hz for 3 seconds", test_led},
    {"key", "Read PC5 key0 status for 5 seconds", test_key0},
    {"sem", "Test named semaphore wait/post/close", test_sem},
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
        for (int i = 0; i < (int)TEST_CMD_NUM; i++) {
            int one_ret = test_cmds[i].func();
            printf("\nTest %s %s\n", test_cmds[i].name, one_ret == 0 ? "PASSED" : "FAILED");
            if (one_ret != 0) {
                ret = one_ret;
            }
        }
        exit(ret);
    }

    ret = run_named_test(test_name);
    exit(ret);
}
