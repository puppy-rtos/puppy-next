
#include <puppy_core.h>
#include <stdio.h>
#include <string.h>
#include "pup_console.h"
#include "app_loader.h"
#include "app_manager.h"
#include "pup_fs.h"
#include "pup_romfs.h"
#include "puppy_log.h"
#include "kheap.h"

#ifndef PUP_SHELL_ENABLED
#define PUP_SHELL_ENABLED 1
#endif

#if PUP_SHELL_ENABLED

int shell_cmd_ls(int argc, char **argv);

#define PUP_SHELL_MAX_CMD_LEN  128
#define PUP_SHELL_MAX_ARGS     8
#define PUP_SHELL_PROMPT       "puppy> "

typedef int (*shell_cmd_func)(int argc, char **argv);

typedef struct {
    const char *name;
    const char *help;
    shell_cmd_func func;
} shell_cmd_t;

static int shell_cmd_help(int argc, char **argv);
static int shell_cmd_ps(int argc, char **argv);
static int shell_cmd_apps(int argc, char **argv);
static int shell_cmd_run(int argc, char **argv);
static int shell_cmd_clear(int argc, char **argv);
static int shell_cmd_test(int argc, char **argv);
int shell_cmd_ls(int argc, char **argv);
static int shell_cmd_cat(int argc, char **argv);
static int shell_cmd_echo(int argc, char **argv);
static int shell_cmd_ushell(int argc, char **argv);
static int shell_cmd_free(int argc, char **argv);

static const shell_cmd_t shell_cmds[] = {
    {"help",  "Show available commands", shell_cmd_help},
    {"ps",    "List all threads", shell_cmd_ps},
    {"apps",  "List running apps", shell_cmd_apps},
    {"run",   "Run app in background (usage: run <app_name>)", shell_cmd_run},
    {"clear", "Clear screen", shell_cmd_clear},
    {"test",  "Run test cases (usage: test [name])", shell_cmd_test},
    {"ls",    "List directory (usage: ls [path])", shell_cmd_ls},
    {"cat",   "Read from device (usage: cat <device>)", shell_cmd_cat},
    {"echo",  "Write to device (usage: echo <text> > <device>)", shell_cmd_echo},
    {"ushell", "Enter user shell (exit to return)", shell_cmd_ushell},
    {"free",  "Show kernel heap usage", shell_cmd_free},
};

#define SHELL_CMD_NUM (sizeof(shell_cmds) / sizeof(shell_cmds[0]))

static char shell_buf[PUP_SHELL_MAX_CMD_LEN];
static int shell_buf_pos = 0;
void puppy_shell_thread(void *arg);
static pthread_t g_kshell_thread = PUP_NULL;
static int g_kshell_started = 0;
static pup_uint8_t g_kshell_stack[PUP_KSHELL_THREAD_STACK_SIZE];

static void* puppy_shell_thread_entry(void* arg)
{
    puppy_shell_thread(arg);
    return PUP_NULL;
}

static void shell_putchar(char c)
{
    pup_console_putchar(c);
}

static void shell_puts(const char *str)
{
    pup_console_write(str, libcm_strlen(str));
}

static int shell_getchar(void)
{
    return pup_console_getchar();
}

static void shell_show_prompt(void)
{
    shell_puts(PUP_SHELL_PROMPT);
}

static int shell_cmd_help(int argc, char **argv)
{
    PUP_PRINTK("Available commands:\n");
    for (int i = 0; i < SHELL_CMD_NUM; i++) {
        PUP_PRINTK("  %-8s - %s\n", shell_cmds[i].name, shell_cmds[i].help);
    }
    return 0;
}

static int shell_cmd_ps(int argc, char **argv)
{
    extern void pup_pthread_list(void);
    pup_pthread_list();
    return 0;
}

static int shell_cmd_apps(int argc, char **argv)
{
    app_list();
    return 0;
}

static int shell_cmd_run(int argc, char **argv)
{
    const char *app_name;
    char path[64];
    int ret;

    if (argc < 2) {
        PUP_PRINTK("Usage: run <app_name>\n");
        PUP_PRINTK("Example: run hello_app\n");
        PUP_PRINTK("Use 'ls /rom' to see available apps\n");
        return -1;
    }

    app_name = argv[1];
    libcm_snprintf(path, sizeof(path), "/rom/%s", app_name);

    PUP_PRINTK("Running app: %s\n", app_name);
    PUP_PRINTK("Path: %s\n", path);

    ret = app_exec(app_name, path, 0);
    if (ret < 0) {
        PUP_PRINTK("Error: Failed to run app '%s' (ret=%d)\n", app_name, ret);
        return -1;
    }

    PUP_PRINTK("App '%s' started with pid=%d (running in background)\n", app_name, ret);
    return 0;
}

static int shell_cmd_clear(int argc, char **argv)
{
    shell_puts("\033[2J\033[H");
    return 0;
}

static int shell_cmd_test(int argc, char **argv)
{
    char arg_buf[APP_EXEC_ARG_AREA_SIZE];
    int offset = 0;

    libcm_memset(arg_buf, 0, sizeof(arg_buf));

    for (int i = 1; i < argc; i++) {
        int arg_len = (int)libcm_strlen(argv[i]);

        if (offset != 0) {
            if (offset >= (int)sizeof(arg_buf) - 1) {
                PUP_PRINTK("Error: test arguments too long\n");
                return -1;
            }
            arg_buf[offset++] = ' ';
        }

        if (offset + arg_len >= (int)sizeof(arg_buf)) {
            PUP_PRINTK("Error: test arguments too long\n");
            return -1;
        }

        libcm_memcpy(&arg_buf[offset], argv[i], arg_len);
        offset += arg_len;
    }

    return app_exec_with_arg("test_app", "/rom/test_app",
                             offset > 0 ? arg_buf : PUP_NULL, 1);
}

int shell_cmd_ls(int argc, char **argv)
{
    const char *path = "/";
    
    if (argc > 1) {
        path = argv[1];
    }
    
    int fd;
    struct dirent entry;
    int ret;
    
    fd = pup_open(path, O_DIRECTORY);
    if (fd < 0) {
        PUP_PRINTK("Error: Cannot open %s (ret=%d)\n", path, fd);
        return -1;
    }
    
    PUP_PRINTK("Directory: %s\n", path);
    PUP_PRINTK("  Type  Name\n");
    PUP_PRINTK("  ----  ----\n");
    
    while (1) {
        ret = pup_getdents(fd, &entry, sizeof(entry));
        if (ret <= 0) break;
        
        const char *type_str = "file";
        if (entry.d_type == DT_DIR) type_str = "dir ";
        else if (entry.d_type == DT_CHR) type_str = "char";
        
        PUP_PRINTK("  %s  %s\n", type_str, entry.d_name);
    }
    
    pup_close(fd);
    return 0;
}

static int shell_cmd_cat(int argc, char **argv)
{
    if (argc < 2) {
        PUP_PRINTK("Usage: cat <device>\n");
        PUP_PRINTK("Example: cat /dev/uart0\n");
        return -1;
    }
    
    int fd = pup_open(argv[1], O_RDONLY);
    if (fd < 0) {
        PUP_PRINTK("Error: Cannot open %s (fd=%d)\n", argv[1], fd);
        return -1;
    }
    
    char buf[64];
    int n;
    PUP_PRINTK("Reading from %s:\n", argv[1]);
    
    int total = 0;
    int max_read = 100;
    
    while ((n = pup_read(fd, buf, sizeof(buf))) > 0 && total < max_read) {
        for (int i = 0; i < n; i++) {
            if (buf[i] >= 0x20 && buf[i] < 0x7F) {
                PUP_PRINTK("%c", buf[i]);
            } else if (buf[i] == '\n' || buf[i] == '\r') {
                PUP_PRINTK("\n");
            } else {
                PUP_PRINTK("\\x%02x", (unsigned char)buf[i]);
            }
        }
        total += n;
    }
    
    PUP_PRINTK("\n[Total: %d bytes]\n", total);
    pup_close(fd);
    return 0;
}

static int shell_cmd_echo(int argc, char **argv)
{
    if (argc < 4) {
        PUP_PRINTK("Usage: echo <text> > <device>\n");
        PUP_PRINTK("Example: echo hello > /dev/uart0\n");
        return -1;
    }
    
    int redirect_pos = -1;
    for (int i = 1; i < argc; i++) {
        if (libcm_strcmp(argv[i], ">") == 0) {
            redirect_pos = i;
            break;
        }
    }
    
    if (redirect_pos < 0 || redirect_pos >= argc - 1) {
        PUP_PRINTK("Error: Missing redirect target\n");
        PUP_PRINTK("Usage: echo <text> > <device>\n");
        return -1;
    }
    
    const char *device = argv[redirect_pos + 1];
    
    int fd = pup_open(device, O_WRONLY);
    if (fd < 0) {
        PUP_PRINTK("Error: Cannot open %s (fd=%d)\n", device, fd);
        return -1;
    }
    
    for (int i = 1; i < redirect_pos; i++) {
        pup_write(fd, argv[i], libcm_strlen(argv[i]));
        if (i < redirect_pos - 1) {
            pup_write(fd, " ", 1);
        }
    }
    pup_write(fd, "\n", 1);
    
    PUP_PRINTK("Written to %s\n", device);
    pup_close(fd);
    return 0;
}

static int shell_cmd_ushell(int argc, char **argv)
{
    int exit_code;

    PUP_PRINTK("Starting user shell...\n");
    exit_code = app_exec("shell_app", "/rom/shell_app", 1);
    if (exit_code < 0) {
        PUP_PRINTK("Failed to start user shell\n");
        return -1;
    }

    PUP_PRINTK("User shell exited (code=%d), back to kernel shell\n", exit_code);
    pup_reap_zombies();
    return 0;
}

static int shell_cmd_free(int argc, char **argv)
{
    size_t total, used, free;
    size_t peak;
    size_t u_total, u_used, u_free;
    size_t u_peak;
    
    total = kheap_total_size();
    used = kheap_used_size();
    free = kheap_free_size();
    peak = kheap_peak_used_size();
    u_total = kheap_user_total_size();
    u_used = kheap_user_used_size();
    u_free = kheap_user_free_size();
    u_peak = kheap_user_peak_used_size();
    
    PUP_PRINTK("Kernel Heap Usage:\n");
    PUP_PRINTK("  Total: %u bytes (%u KB)\n", (unsigned int)total, (unsigned int)(total / 1024));
    PUP_PRINTK("  Used:  %u bytes (%u KB)\n", (unsigned int)used, (unsigned int)(used / 1024));
    PUP_PRINTK("  Free:  %u bytes (%u KB)\n", (unsigned int)free, (unsigned int)(free / 1024));
    PUP_PRINTK("  Peak:  %u bytes (%u KB)\n", (unsigned int)peak, (unsigned int)(peak / 1024));
    PUP_PRINTK("User App Heap Usage:\n");
    PUP_PRINTK("  Total: %u bytes (%u KB)\n", (unsigned int)u_total, (unsigned int)(u_total / 1024));
    PUP_PRINTK("  Used:  %u bytes (%u KB)\n", (unsigned int)u_used, (unsigned int)(u_used / 1024));
    PUP_PRINTK("  Free:  %u bytes (%u KB)\n", (unsigned int)u_free, (unsigned int)(u_free / 1024));
    PUP_PRINTK("  Peak:  %u bytes (%u KB)\n", (unsigned int)u_peak, (unsigned int)(u_peak / 1024));
    
    return 0;
}

static int shell_parse_args(char *buf, char **argv, int max_args)
{
    int argc = 0;
    char *p = buf;
    
    while (*p && argc < max_args) {
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') {
            p++;
        }
        if (*p) {
            *p++ = '\0';
        }
    }
    
    return argc;
}

static int shell_exec_cmd(int argc, char **argv)
{
    if (argc == 0) {
        return 0;
    }
    
    for (int i = 0; i < SHELL_CMD_NUM; i++) {
        if (libcm_strcmp(argv[0], shell_cmds[i].name) == 0) {
            return shell_cmds[i].func(argc, argv);
        }
    }
    
    if (argv[0][0] == '/') {
        int fd = pup_open(argv[0], O_RDONLY);
        if (fd >= 0) {
            pup_close(fd);
            
            void *app_data = NULL;
            unsigned int app_size = 0;
            
            int ret = pup_romfs_get_file_data(argv[0], &app_data, &app_size);
            if (ret == 0 && app_data != NULL && app_size > 0) {
                PUP_PRINTK("Loading app from %s (%d bytes)...\n", argv[0], app_size);
                
                ret = app_load_from_flash(app_data, app_size);
                if (ret == 0) {
                    extern user_app_t loaded_app;
                    PUP_PRINTK("Running app...\n");
                    app_run_legacy(&loaded_app);
                    PUP_PRINTK("App returned\n");
                    return 0;
                } else {
                    PUP_PRINTK("Failed to load app\n");
                    return -1;
                }
            } else {
                PUP_PRINTK("File not found in ROMFS: %s\n", argv[0]);
                return -1;
            }
        }
    }
    
    PUP_PRINTK("Unknown command: %s\n", argv[0]);
    PUP_PRINTK("Type 'help' for available commands\n");
    return -1;
}

static void shell_process_line(void)
{
    char *argv[PUP_SHELL_MAX_ARGS];
    int argc;
    
    shell_buf[shell_buf_pos] = '\0';
    shell_puts("\n");
    
    argc = shell_parse_args(shell_buf, argv, PUP_SHELL_MAX_ARGS);
    shell_exec_cmd(argc, argv);
    
    shell_buf_pos = 0;
    shell_show_prompt();
}

void puppy_shell_thread(void *arg)
{
    int ch;
    
    PUP_PRINTK("\n=== Puppy Shell ===\n");
    PUP_PRINTK("Type 'help' for available commands\n\n");
    shell_show_prompt();
    
    while (1) {
        ch = shell_getchar();
        
        if (ch == '\r' || ch == '\n') {
            shell_process_line();
        }
        else if (ch == 0x7F || ch == '\b') {
            if (shell_buf_pos > 0) {
                shell_buf_pos--;
                shell_puts("\b \b");
            }
        }
        else if (ch >= 0x20 && ch < 0x7F) {
            if (shell_buf_pos < PUP_SHELL_MAX_CMD_LEN - 1) {
                shell_buf[shell_buf_pos++] = (char)ch;
                shell_putchar((char)ch);
            }
        }
    }
}

int puppy_shell_start(void)
{
    pthread_attr_t attr;
    int ret;

    if (g_kshell_started) {
        return 0;
    }

    pthread_attr_init(&attr);
    pthread_attr_setname(&attr, "kshell");
    pthread_attr_setstackaddr(&attr, g_kshell_stack);
    pthread_attr_setstacksize(&attr, sizeof(g_kshell_stack));

    ret = pthread_create(&g_kshell_thread, &attr, puppy_shell_thread_entry, PUP_NULL);
    if (ret != 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Failed to create kshell thread: %d", ret);
        return -1;
    }

    g_kshell_started = 1;
    return 0;
}

#else

void puppy_shell_thread(void *arg)
{
}

int puppy_shell_start(void)
{
    return 0;
}

#endif
