#include "userfs.h"
#include "lfs_userfs.h"
#include "syscall.h"
#include "libc.h"
#include <stddef.h>

void app_main(const char *arg)
{
    printf("[lfs_service] starting...\n");
    
    userfs_config_t config = {
        .fs_type = "lfs",
        .ops = &lfs_userfs_ops,
        .private_data = NULL,
    };
    
    int ret = userfs_service_start(&config);
    if (ret < 0) {
        printf("[lfs_service] failed to start service\n");
        return;
    }
    
    printf("[lfs_service] service stopped\n");
}
