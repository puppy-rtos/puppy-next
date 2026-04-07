#include "libc.h"
#include "syscall.h"
#include "fs.h"

#define PUP_SHELL_MAX_CMD_LEN  128
#define PUP_SHELL_MAX_ARGS     8
#define PUP_SHELL_PROMPT       "puppy> "
#define PUP_SHELL_APP_ARG_LEN  256
#define PUP_SHELL_HISTORY_MAX  5

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
static int shell_cmd_apps(int argc, char **argv);
static int shell_cmd_free(int argc, char **argv);
static int shell_cmd_exit(int argc, char **argv);
static int shell_cmd_mount(int argc, char **argv);
static int shell_cmd_mkfs(int argc, char **argv);
static int shell_cmd_umount(int argc, char **argv);
static int shell_build_exec_arg(char **argv, int start, int argc, char *buf, int buf_len);

static const shell_cmd_t shell_cmds[] = {
    {"help",   "Show available commands", shell_cmd_help},
    {"ps",     "List all threads", shell_cmd_ps},
    {"ls",     "List directory (usage: ls [path])", shell_cmd_ls},
    {"cat",    "Read from file (usage: cat <file>)", shell_cmd_cat},
    {"echo",   "Write to file (usage: echo <text> > <file>)", shell_cmd_echo},
    {"clear",  "Clear screen", shell_cmd_clear},
    {"apps",   "List running apps", shell_cmd_apps},
    {"free",   "Show heap usage", shell_cmd_free},
    {"exit",   "Exit user shell", shell_cmd_exit},
    {"mount",  "Mount filesystem (usage: mount [-t type] <device> <path>)", shell_cmd_mount},
    {"mkfs",   "Format filesystem (usage: mkfs [-t type] <device>)", shell_cmd_mkfs},
    {"umount", "Unmount filesystem (usage: umount <path>)", shell_cmd_umount},
};

#define SHELL_CMD_NUM (sizeof(shell_cmds) / sizeof(shell_cmds[0]))

static char shell_buf[PUP_SHELL_MAX_CMD_LEN];
static int shell_buf_pos = 0;
static int shell_cursor_pos = 0;
static char shell_history[PUP_SHELL_HISTORY_MAX][PUP_SHELL_MAX_CMD_LEN];
static int shell_history_count = 0;
static int shell_history_head = 0;
static int shell_history_view = -1;
static char shell_saved_buf[PUP_SHELL_MAX_CMD_LEN];
static int shell_saved_buf_pos = 0;

static void shell_putchar(char c)
{
    write(STDOUT_FILENO, &c, 1);
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
    int ret = read(STDIN_FILENO, &c, 1);

    if (ret > 0) {
        return (int)(unsigned char)c;
    }
    return -1;
}

static void shell_show_prompt(void)
{
    shell_puts(PUP_SHELL_PROMPT);
}

static void shell_refresh_input(void)
{
    shell_puts("\r\033[K");
    shell_show_prompt();
    shell_puts(shell_buf);

    for (int i = shell_cursor_pos; i < shell_buf_pos; i++) {
        shell_putchar('\b');
    }
}

static void shell_set_buffer(const char *text)
{
    int len = (int)strlen(text);

    if (len >= PUP_SHELL_MAX_CMD_LEN) {
        len = PUP_SHELL_MAX_CMD_LEN - 1;
    }

    memcpy(shell_buf, text, (size_t)len);
    shell_buf[len] = '\0';
    shell_buf_pos = len;
    shell_cursor_pos = len;
    shell_refresh_input();
}

static void shell_leave_history_view(void)
{
    shell_history_view = -1;
    shell_saved_buf[0] = '\0';
    shell_saved_buf_pos = 0;
}

static void shell_history_add(const char *line)
{
    int len;

    if (!line || line[0] == '\0') {
        return;
    }

    if (shell_history_count > 0) {
        int last_index = (shell_history_head + PUP_SHELL_HISTORY_MAX - 1) % PUP_SHELL_HISTORY_MAX;
        if (strcmp(shell_history[last_index], line) == 0) {
            return;
        }
    }

    len = (int)strlen(line);
    if (len >= PUP_SHELL_MAX_CMD_LEN) {
        len = PUP_SHELL_MAX_CMD_LEN - 1;
    }

    memcpy(shell_history[shell_history_head], line, (size_t)len);
    shell_history[shell_history_head][len] = '\0';
    shell_history_head = (shell_history_head + 1) % PUP_SHELL_HISTORY_MAX;
    if (shell_history_count < PUP_SHELL_HISTORY_MAX) {
        shell_history_count++;
    }
}

static void shell_history_prev(void)
{
    int oldest;

    if (shell_history_count == 0) {
        return;
    }

    if (shell_history_view < 0) {
        memcpy(shell_saved_buf, shell_buf, (size_t)shell_buf_pos);
        shell_saved_buf[shell_buf_pos] = '\0';
        shell_saved_buf_pos = shell_buf_pos;
        shell_history_view = (shell_history_head + PUP_SHELL_HISTORY_MAX - 1) % PUP_SHELL_HISTORY_MAX;
    } else {
        oldest = (shell_history_head + PUP_SHELL_HISTORY_MAX - shell_history_count) % PUP_SHELL_HISTORY_MAX;
        if (shell_history_view != oldest) {
            shell_history_view = (shell_history_view + PUP_SHELL_HISTORY_MAX - 1) % PUP_SHELL_HISTORY_MAX;
        }
    }

    shell_set_buffer(shell_history[shell_history_view]);
}

static void shell_history_browse_next(void)
{
    int newest;

    if (shell_history_view < 0) {
        return;
    }

    newest = (shell_history_head + PUP_SHELL_HISTORY_MAX - 1) % PUP_SHELL_HISTORY_MAX;
    if (shell_history_view == newest) {
        shell_history_view = -1;
        shell_set_buffer(shell_saved_buf);
        shell_buf_pos = shell_saved_buf_pos;
        shell_cursor_pos = shell_saved_buf_pos;
        return;
    }

    shell_history_view = (shell_history_view + 1) % PUP_SHELL_HISTORY_MAX;
    shell_set_buffer(shell_history[shell_history_view]);
}

static void shell_insert_char(char ch)
{
    if (shell_buf_pos >= PUP_SHELL_MAX_CMD_LEN - 1) {
        return;
    }

    if (shell_cursor_pos < shell_buf_pos) {
        for (int i = shell_buf_pos; i >= shell_cursor_pos; i--) {
            shell_buf[i + 1] = shell_buf[i];
        }
    }

    shell_buf[shell_cursor_pos] = ch;
    shell_buf_pos++;
    shell_cursor_pos++;
    shell_buf[shell_buf_pos] = '\0';
    shell_refresh_input();
}

static void shell_backspace(void)
{
    if (shell_cursor_pos <= 0) {
        return;
    }

    for (int i = shell_cursor_pos; i <= shell_buf_pos; i++) {
        shell_buf[i - 1] = shell_buf[i];
    }
    shell_buf_pos--;
    shell_cursor_pos--;
    shell_refresh_input();
}

static void shell_move_cursor_left(void)
{
    if (shell_cursor_pos > 0) {
        shell_cursor_pos--;
        shell_refresh_input();
    }
}

static void shell_move_cursor_right(void)
{
    if (shell_cursor_pos < shell_buf_pos) {
        shell_cursor_pos++;
        shell_refresh_input();
    }
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
    ps();
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
    
    fd = open(path, O_DIRECTORY);
    if (fd < 0) {
        printf("Error: Cannot open %s (ret=%d)\n", path, fd);
        return -1;
    }
    
    printf("Directory: %s\n", path);
    printf("  Type  Name\n");
    printf("  ----  ----\n");
    
    while (1) {
        ret = getdents(fd, &entry, sizeof(entry));
        if (ret <= 0) break;
        
        const char *type_str = "file";
        if (entry.d_type == DT_DIR) type_str = "dir ";
        else if (entry.d_type == DT_CHR) type_str = "char";
        
        printf("  %s  %s\n", type_str, entry.d_name);
    }
    
    close(fd);
    return 0;
}

static int shell_cmd_cat(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: cat <file>\n");
        printf("Example: cat /etc/config.txt\n");
        return -1;
    }
    
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        printf("Error: Cannot open %s (fd=%d)\n", argv[1], fd);
        return -1;
    }
    
    char buf[64];
    int n;
    printf("Reading from %s:\n", argv[1]);
    
    int total = 0;
    int max_read = 100;
    
    while ((n = fs_read(fd, buf, sizeof(buf))) > 0 && total < max_read) {
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
    close(fd);
    return 0;
}

static int shell_cmd_echo(int argc, char **argv)
{
    if (argc < 4) {
        printf("Usage: echo <text> > <file>\n");
        printf("Example: echo hello > /etc/config.txt\n");
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
        printf("Usage: echo <text> > <file>\n");
        return -1;
    }
    
    const char *file = argv[redirect_pos + 1];
    
    int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        printf("Error: Cannot open %s (fd=%d)\n", file, fd);
        return -1;
    }
    
    for (int i = 1; i < redirect_pos; i++) {
        fs_write(fd, argv[i], strlen(argv[i]));
        if (i < redirect_pos - 1) {
            fs_write(fd, " ", 1);
        }
    }
    fs_write(fd, "\n", 1);
    
    printf("Written to %s\n", file);
    close(fd);
    return 0;
}

static int shell_cmd_clear(int argc, char **argv)
{
    shell_puts("\033[2J\033[H");
    return 0;
}

static int shell_cmd_apps(int argc, char **argv)
{
    return apps();
}

static int shell_cmd_free(int argc, char **argv)
{
    pup_heap_info_t info;

    if (free_info(&info) < 0) {
        printf("Error: failed to query heap usage\n");
        return -1;
    }

    printf("Kernel Heap Usage:\n");
    printf("  Total: %u bytes (%u KB)\n", info.kernel_total, info.kernel_total / 1024);
    printf("  Used:  %u bytes (%u KB)\n", info.kernel_used, info.kernel_used / 1024);
    printf("  Free:  %u bytes (%u KB)\n", info.kernel_free, info.kernel_free / 1024);
    printf("  Peak:  %u bytes (%u KB)\n", info.kernel_peak, info.kernel_peak / 1024);
    printf("User App Heap Usage:\n");
    printf("  Total: %u bytes (%u KB)\n", info.user_total, info.user_total / 1024);
    printf("  Used:  %u bytes (%u KB)\n", info.user_used, info.user_used / 1024);
    printf("  Free:  %u bytes (%u KB)\n", info.user_free, info.user_free / 1024);
    printf("  Peak:  %u bytes (%u KB)\n", info.user_peak, info.user_peak / 1024);
    printf("Shared Heap Usage:\n");
    printf("  Total: %u bytes (%u KB)\n", info.shared_total, info.shared_total / 1024);
    printf("  Used:  %u bytes (%u KB)\n", info.shared_used, info.shared_used / 1024);
    printf("  Free:  %u bytes (%u KB)\n", info.shared_free, info.shared_free / 1024);
    printf("  Peak:  %u bytes (%u KB)\n", info.shared_peak, info.shared_peak / 1024);

    return 0;
}

static int shell_cmd_exit(int argc, char **argv)
{
    int exit_code = 0;

    if (argc > 1) {
        printf("Ignoring argument '%s', exit code defaults to 0\n", argv[1]);
    }

    printf("Exiting user shell...\n");
    exit(exit_code);
    return 0;
}

static int shell_cmd_mount(int argc, char **argv)
{
    const char *fs_type = "lfs";
    const char *device = NULL;
    const char *mount_path = NULL;
    int arg_pos = 1;
    
    if (argc == 1) {
        printf("Usage: mount [-t type] <device> <path>\n");
        printf("       mount none <path>  (for RAM-based filesystem)\n");
        printf("Example:\n");
        printf("  mount /dev/flash0 /data\n");
        printf("  mount -t lfs /dev/flash0 /data\n");
        printf("  mount none /tmp\n");
        return 0;
    }
    
    if (argc > 1 && strcmp(argv[1], "-t") == 0) {
        if (argc < 5) {
            printf("Error: -t requires filesystem type\n");
            printf("Usage: mount -t <type> <device> <path>\n");
            return -1;
        }
        fs_type = argv[2];
        arg_pos = 3;
    }
    
    if (arg_pos < argc) {
        device = argv[arg_pos];
        arg_pos++;
    }
    
    if (arg_pos < argc) {
        mount_path = argv[arg_pos];
    }
    
    if (!mount_path) {
        printf("Error: missing mount path\n");
        return -1;
    }
    
    int ret = userfs_mount(fs_type, device, mount_path, 0);
    if (ret < 0) {
        printf("mount failed: %d\n", ret);
        return -1;
    }
    
    printf("Mounted %s at %s (instance=%d)\n", 
           device ? device : "RAM", mount_path, ret);
    return 0;
}

static int shell_cmd_mkfs(int argc, char **argv)
{
    const char *fs_type = "littlefs";
    const char *device = NULL;
    int arg_pos = 1;
    
    if (argc == 1) {
        printf("Usage: mkfs [-t type] <device>\n");
        printf("Example:\n");
        printf("  mkfs /dev/flash0\n");
        printf("  mkfs -t littlefs /dev/flash0\n");
        return 0;
    }
    
    if (argc > 1 && strcmp(argv[1], "-t") == 0) {
        if (argc < 4) {
            printf("Error: -t requires filesystem type\n");
            printf("Usage: mkfs -t <type> <device>\n");
            return -1;
        }
        fs_type = argv[2];
        arg_pos = 3;
    }
    
    if (arg_pos < argc) {
        device = argv[arg_pos];
    }
    
    if (!device) {
        printf("Error: missing device\n");
        return -1;
    }
    
    printf("Formatting %s...\n", device);
    
    int ret = userfs_mkfs(fs_type, device);
    if (ret < 0) {
        printf("mkfs failed: %d\n", ret);
        return -1;
    }
    
    printf("mkfs completed successfully\n");
    return 0;
}

static int shell_cmd_umount(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: umount <path>\n");
        printf("Example: umount /data\n");
        return -1;
    }
    
    const char *mount_path = argv[1];
    
    int ret = userfs_unmount(mount_path);
    if (ret < 0) {
        printf("umount failed: %d\n", ret);
        return -1;
    }
    
    printf("Unmounted %s\n", mount_path);
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

static int shell_build_exec_arg(char **argv, int start, int argc, char *buf, int buf_len)
{
    int offset = 0;

    if (buf_len <= 0) {
        return -1;
    }

    buf[0] = '\0';
    for (int i = start; i < argc; i++) {
        int len = (int)strlen(argv[i]);

        if (i > start) {
            if (offset >= buf_len - 1) {
                return -1;
            }
            buf[offset++] = ' ';
        }

        if (offset + len >= buf_len) {
            return -1;
        }

        memcpy(&buf[offset], argv[i], (size_t)len);
        offset += len;
        buf[offset] = '\0';
    }

    return 0;
}

static const char* shell_path_basename(const char *path)
{
    const char *base = path;

    while (*path) {
        if (*path == '/') {
            base = path + 1;
        }
        path++;
    }

    return base;
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

    if (argv[0][0] == '/') {
        const char *path = argv[0];
        const char *app_name = shell_path_basename(path);
        char app_arg[PUP_SHELL_APP_ARG_LEN];
        const char *arg_ptr = 0;
        int wait = 1;
        int arg_end = argc;
        int result;

        if (*app_name == '\0') {
            printf("Invalid executable path: %s\n", path);
            return -1;
        }

        if (argc > 1 && strcmp(argv[argc - 1], "&") == 0) {
            wait = 0;
            arg_end = argc - 1;
        }

        if (arg_end > 1) {
            if (shell_build_exec_arg(argv, 1, arg_end, app_arg, sizeof(app_arg)) != 0) {
                printf("Error: app arguments too long\n");
                return -1;
            }
            arg_ptr = app_arg;
        }

        result = execve_with_arg(path, app_name, arg_ptr, wait);
        if (result >= 0) {
            if (wait) {
                return result;
            }
            printf("App '%s' started with pid=%d (running in background)\n", app_name, result);
            return 0;
        }

        /*
         * Absolute path means the user explicitly tried to execute an app.
         * Do not fall through to "Unknown command" on runtime failures.
         */
        printf("App execution failed: %s (ret=%d)\n", path, result);
        return result;
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

    shell_history_add(shell_buf);
    shell_leave_history_view();
    
    argc = shell_parse_args(shell_buf, argv, PUP_SHELL_MAX_ARGS);
    shell_exec_cmd(argc, argv);
    
    shell_buf_pos = 0;
    shell_cursor_pos = 0;
    shell_buf[0] = '\0';
    shell_show_prompt();
}

void app_main(void)
{
    printf("=== Puppy Shell (User Mode) ===\n");
    printf("Type 'help' for available commands\n");
    printf("Use absolute path to run apps, append '&' for background\n\n");
    
    int ch;
    shell_show_prompt();
    
    while (1) {
        ch = shell_getchar();
        
        if (ch < 0) {
            sched_yield();
            continue;
        }

        if (ch == 0x1B) {
            int ch1 = shell_getchar();
            int ch2 = shell_getchar();

            if (ch1 == '[') {
                if (ch2 == 'A') {
                    shell_history_prev();
                    continue;
                }
                if (ch2 == 'B') {
                    shell_history_browse_next();
                    continue;
                }
                if (ch2 == 'C') {
                    if (shell_history_view >= 0) {
                        shell_leave_history_view();
                    }
                    shell_move_cursor_right();
                    continue;
                }
                if (ch2 == 'D') {
                    if (shell_history_view >= 0) {
                        shell_leave_history_view();
                    }
                    shell_move_cursor_left();
                    continue;
                }
            }
            continue;
        }
        
        if (ch == '\r' || ch == '\n') {
            shell_process_line();
        }
        else if (ch == 0x7F || ch == '\b') {
            if (shell_cursor_pos > 0) {
                if (shell_history_view >= 0) {
                    shell_leave_history_view();
                }
                shell_backspace();
            }
        }
        else if (ch >= 0x20 && ch < 0x7F) {
            if (shell_buf_pos < PUP_SHELL_MAX_CMD_LEN - 1) {
                if (shell_history_view >= 0) {
                    shell_leave_history_view();
                }
                shell_insert_char((char)ch);
            }
        }
    }
}
