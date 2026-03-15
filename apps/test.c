#include <puppy_core.h>

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
