#include <puppy_core.h>

#include <stdio.h>
#include "dtm_clk.h"
#include "dtm_gpio.h"
#include "dtm_uart.h"
#include "dtm_delay.h"
#include "libcm.h"
#include "app_loader.h"
#include "app_manager.h"
#include "mpu.h"
#include "pup_fs.h"
#include "pup_devfs.h"
#include "pup_romfs.h"
#include "dev_uart.h"
#include "dev_gpio.h"
#include "dev_spi.h"
#include "dtm_pmctrl.h"
#include "dtm_flash.h"
#include "app_loader.h"
#include "puppy_log.h"
#include "syscall.h"
#include "pup_console.h"

int libcm_console_output(const char *str, int len)
{
    return pup_console_write(str, len);
}

int puppy_board_init(void)
{
    dtm_pmctrl_init();
    dtm_flash_init();
    dtm_clk_init();
    dtm_uart_init(115200);
    pup_console_init();
    dtm_delay_init();
    dtm_delay_set_periodic(pup_tick_inc, 1000);

    return 0;
}

int main( void )
{
    puppy_board_init();
    puppy_init();
    return 0;
}

void *puppy_main_thread(void *arg) {
    PUP_LOG_INFO(PUP_LOG_MODULE_MPU, "Initializing MPU");

    if (pup_mpu_init() != 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_MPU, "MPU init failed");
        return (void*)-1;
    }

    pup_mpu_setup_kernel_protection();

    PUP_LOG_INFO(PUP_LOG_MODULE_APP, "Initializing app manager");
    app_manager_init();

    PUP_LOG_INFO(PUP_LOG_MODULE_FS, "Initializing PUP FS");
    pup_fs_init();

    PUP_LOG_INFO(PUP_LOG_MODULE_FS, "Registering pup_devfs");
    pup_devfs_init();
        
    PUP_LOG_INFO(PUP_LOG_MODULE_DEV, "Registering devices");
    dev_uart_init();
    dev_gpio_init();
    dev_spi_init();

    PUP_LOG_INFO(PUP_LOG_MODULE_FS, "Registering romfs");
    pup_romfs_init();
    
    PUP_LOG_INFO(PUP_LOG_MODULE_FS, "Mounting pup_devfs at /dev");
    if (pup_fs_mount(PUP_NULL, "/dev", "pup_devfs", 0, PUP_NULL) == 0) {
        PUP_LOG_INFO(PUP_LOG_MODULE_FS, "pup_devfs mounted successfully");
    } else {
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "pup_devfs mount failed");
    }
    
    PUP_LOG_INFO(PUP_LOG_MODULE_FS, "Mounting romfs at /rom");
    if (pup_fs_mount(PUP_NULL, "/rom", "rom", 0, (const void *)PUP_MOUNT_ROMFS_BASE) == 0) {
        PUP_LOG_INFO(PUP_LOG_MODULE_FS, "romfs mounted successfully");
    } else {
        PUP_LOG_ERROR(PUP_LOG_MODULE_FS, "romfs mount failed");
    }
    
    PUP_LOG_INFO(PUP_LOG_MODULE_FS, "File system initialized");

    PUP_LOG_INFO(PUP_LOG_MODULE_APP, "Starting user shell");
    if (app_exec("shell", "/rom/shell", 0) < 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Failed to start user shell");
    }

    /* Keep main thread alive and periodically reap zombie threads. */
    while (1) {
        app_reap_exited();
        pup_reap_zombies();
        pthread_delay(100);
    }

    return 0;
}
