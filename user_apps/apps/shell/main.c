#include "user_libc.h"
#include "user_syscall.h"
#include "user_fs.h"
#include "app_header.h"

#define PUP_SHELL_MAX_CMD_LEN  128
#define PUP_SHELL_MAX_ARGS     8
#define PUP_SHELL_PROMPT       "puppy> "

#define DT_UNKNOWN              0
#define DT_DIR                  4
#define DT_CHR                  2

#define O_RDONLY                0000000
#define O_DIRECTORY             00200000

struct dirent {
    unsigned char d_type;
    unsigned char d_namlen;
    unsigned short d_reclen;
    char d_name[256];
};

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

__attribute__((section(".app_header")))
app_header_t app_header = {
    .magic = APP_HEADER_MAGIC,
    .load_addr = 0x08080000,
    .entry_addr = (unsigned int)app_main,
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
    .bss_end = (unsigned int)&__bss_end__,
};

typedef int (*shell_cmd_func)(int argc, char **argv);

typedef struct {
    const char *name;
    const char *help;
    shell_cmd_func func;
} shell_cmd_t;

static int shell_cmd_help(int argc, char **argv);
static int shell_cmd_ps(int argc, char **argv);
static int shell_cmd_ls(int argc, char **argv);
static int shell_cmd_cat(int argc, char **argv);
static int shell_cmd_echo(int argc, char **argv);
static int shell_cmd_clear(int argc, char **argv);
static int shell_cmd_run(int argc, char **argv);
static int shell_cmd_runw(int argc, char **argv);
static int shell_cmd_apps(int argc, char **argv);
static int shell_cmd_exit(int argc, char **argv);

static const shell_cmd_t shell_cmds[] = {
    {"help",  "Show available commands", shell_cmd_help},
    {"ps",    "List all threads", shell_cmd_ps},
    {"ls",    "List directory (usage: ls [path])", shell_cmd_ls},
    {"cat",   "Read from device (usage: cat <device>)", shell_cmd_cat},
    {"echo",  "Write to device (usage: echo <text> > <device>)", shell_cmd_echo},
    {"clear", "Clear screen", shell_cmd_clear},
    {"run",   "Run app in background (usage: run <app_name>)", shell_cmd_run},
    {"runw",  "Run app and wait (usage: runw <app_name>)", shell_cmd_runw},
    {"apps",  "List running apps", shell_cmd_apps},
    {"exit",  "Exit user shell and return to kernel shell", shell_cmd_exit},
};

#define SHELL_CMD_NUM (sizeof(shell_cmds) / sizeof(shell_cmds[0]))

static char shell_buf[PUP_SHELL_MAX_CMD_LEN];
static int shell_buf_pos = 0;

static void shell_putchar(char c)
{
    user_write(USER_FD_STDOUT, &c, 1);
}

static void shell_puts(const char *str)
{
    while (*str) {
        if (*str == '\n') {
            shell_putchar('\r');
        }
        shell_putchar(*str++);
    }
}

static int shell_getchar(void)
{
    char c;
    int ret = user_read(USER_FD_STDIN, &c, 1);

    if (ret > 0) {
        return (int)(unsigned char)c;
    }
    return -1;
}

static void shell_show_prompt(void)
{
    shell_puts(PUP_SHELL_PROMPT);
}

static int shell_cmd_help(int argc, char **argv)
{
    printf("Available commands:\n");
    for (unsigned int i = 0; i < SHELL_CMD_NUM; i++) {
        printf("  %-8s - %s\n", shell_cmds[i].name, shell_cmds[i].help);
    }
    return 0;
}

static int shell_cmd_ps(int argc, char **argv)
{
    user_ps();
    return 0;
}

static int shell_cmd_ls(int argc, char **argv)
{
    const char *path = "/";
    
    if (argc > 1) {
        path = argv[1];
    }
    
    int fd;
    struct dirent entry;
    int ret;
    
    fd = user_open(path, O_DIRECTORY);
    if (fd < 0) {
        printf("Error: Cannot open %s (ret=%d)\n", path, fd);
        return -1;
    }
    
    printf("Directory: %s\n", path);
    printf("  Type  Name\n");
    printf("  ----  ----\n");
    
    while (1) {
        ret = user_getdents(fd, &entry, sizeof(entry));
        if (ret <= 0) break;
        
        const char *type_str = "file";
        if (entry.d_type == DT_DIR) type_str = "dir ";
        else if (entry.d_type == DT_CHR) type_str = "char";
        
        printf("  %s  %s\n", type_str, entry.d_name);
    }
    
    user_close(fd);
    return 0;
}

static int shell_cmd_cat(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: cat <device>\n");
        printf("Example: cat /dev/uart0\n");
        return -1;
    }
    
    int fd = user_open(argv[1], O_RDONLY);
    if (fd < 0) {
        printf("Error: Cannot open %s (fd=%d)\n", argv[1], fd);
        return -1;
    }
    
    char buf[64];
    int n;
    printf("Reading from %s:\n", argv[1]);
    
    int total = 0;
    int max_read = 100;
    
    while ((n = user_fs_read(fd, buf, sizeof(buf))) > 0 && total < max_read) {
        for (int i = 0; i < n; i++) {
            if (buf[i] >= 0x20 && buf[i] < 0x7F) {
                printf("%c", buf[i]);
            } else if (buf[i] == '\n' || buf[i] == '\r') {
                printf("\n");
            } else {
                printf("\\x%02x", (unsigned char)buf[i]);
            }
        }
        total += n;
    }
    
    printf("\n[Total: %d bytes]\n", total);
    user_close(fd);
    return 0;
}

static int shell_cmd_echo(int argc, char **argv)
{
    if (argc < 4) {
        printf("Usage: echo <text> > <device>\n");
        printf("Example: echo hello > /dev/uart0\n");
        return -1;
    }
    
    int redirect_pos = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0) {
            redirect_pos = i;
            break;
        }
    }
    
    if (redirect_pos < 0 || redirect_pos >= argc - 1) {
        printf("Error: Missing redirect target\n");
        printf("Usage: echo <text> > <device>\n");
        return -1;
    }
    
    const char *device = argv[redirect_pos + 1];
    
    int fd = user_open(device, 0x0002);
    if (fd < 0) {
        printf("Error: Cannot open %s (fd=%d)\n", device, fd);
        return -1;
    }
    
    for (int i = 1; i < redirect_pos; i++) {
        user_fs_write(fd, argv[i], strlen(argv[i]));
        if (i < redirect_pos - 1) {
            user_fs_write(fd, " ", 1);
        }
    }
    user_fs_write(fd, "\n", 1);
    
    printf("Written to %s\n", device);
    user_close(fd);
    return 0;
}

static int shell_cmd_clear(int argc, char **argv)
{
    shell_puts("\033[2J\033[H");
    return 0;
}

static int shell_cmd_run(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: run <app_name>\n");
        printf("Example: run hello_app\n");
        printf("Use 'ls /rom' to see available apps\n");
        return -1;
    }
    
    const char* app_name = argv[1];
    char path[64];
    
    snprintf(path, sizeof(path), "/rom/%s", app_name);
    
    printf("Running app: %s\n", app_name);
    printf("Path: %s\n", path);
    
    int pid = user_execve(path, app_name, 0);
    if (pid < 0) {
        printf("Error: Failed to run app '%s' (ret=%d)\n", app_name, pid);
        return -1;
    }
    
    printf("App '%s' started with pid=%d (running in background)\n", app_name, pid);
    return 0;
}

static int shell_cmd_runw(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: runw <app_name>\n");
        printf("Example: runw hello_app\n");
        printf("Use 'ls /rom' to see available apps\n");
        return -1;
    }
    
    const char* app_name = argv[1];
    char path[64];
    
    snprintf(path, sizeof(path), "/rom/%s", app_name);
    
    printf("Running app: %s (waiting for completion)\n", app_name);
    printf("Path: %s\n", path);
    
    int exit_code = user_execve(path, app_name, 1);
    if (exit_code < 0) {
        printf("Error: Failed to run app '%s' (ret=%d)\n", app_name, exit_code);
        return -1;
    }
    
    printf("App '%s' exited with code: %d\n", app_name, exit_code);
    return exit_code;
}

static int shell_cmd_apps(int argc, char **argv)
{
    return user_apps();
}

static int shell_cmd_exit(int argc, char **argv)
{
    int exit_code = 0;

    if (argc > 1) {
        printf("Ignoring argument '%s', exit code defaults to 0\n", argv[1]);
    }

    printf("Exiting user shell...\n");
    user_exit(exit_code);
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
    
    for (unsigned int i = 0; i < SHELL_CMD_NUM; i++) {
        if (strcmp(argv[0], shell_cmds[i].name) == 0) {
            return shell_cmds[i].func(argc, argv);
        }
    }
    
    printf("Unknown command: %s\n", argv[0]);
    printf("Type 'help' for available commands\n");
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

void app_main(void)
{
    printf("=== Puppy Shell (User Mode) ===\n");
    printf("Type 'help' for available commands\n");
    printf("New commands: run, runw, apps\n\n");
    
    int ch;
    shell_show_prompt();
    
    while (1) {
        ch = shell_getchar();
        
        if (ch < 0) {
            sched_yield();
            continue;
        }
        
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
