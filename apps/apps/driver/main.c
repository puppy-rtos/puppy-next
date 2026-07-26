#include "libc.h"
#include "syscall.h"
#include "shared/pup_devfs_proto.h"
#include "shared/pup_driver_abi.h"
#include "driver_gpio.h"
#include "driver_of.h"
#ifndef DTM_NO_SPI_FLASH
#include "shared/pup_spi.h"
#include "driver_spi.h"
#include "driver_flash.h"
#endif

int g_server_channel = -1;
int g_key_irq_handle = -1;

static void gpio_service_loop(void)
{
    if (g_server_channel < 0) {
        return;
    }

    for (;;) {
        pup_channel_msg_t msg;
        int cookie = pup_channel_receive(g_server_channel, &msg);

        if (cookie < 0) {
            printf("[driver] channel receive failed, stop service\n");
            return;
        }

        if (msg.type == PUP_MSG_TYPE_PULSE) {
            gpio_handle_pulse(&msg);
            continue;
        }

        if (msg.type != PUP_MSG_TYPE_REQUEST || msg.buf == 0 || cookie <= 0) {
            if (cookie > 0) {
                pup_channel_reply(g_server_channel, cookie, -1);
            }
            continue;
        }

        {
            pup_devfs_proxy_payload_t* req = (pup_devfs_proxy_payload_t*)msg.buf;
            int ret;

#ifndef DTM_NO_SPI_FLASH
            const pup_dtm_spi_bus_t *bus = spi_bus();
            const pup_dtm_flash_device_t *flash = flash_device();

            if (bus && req->device_id == bus->device_id) {
                ret = spi_handle_request(req);
            } else if (flash && req->device_id == flash->device_id) {
                ret = flash_handle_request(req);
            } else
#endif
            {
                gpio_user_dev_t* dev = gpio_find_by_id(req->device_id);
                ret = gpio_handle_request(dev, req);
            }
            pup_channel_reply(g_server_channel, cookie, ret);
        }
    }
}

void app_main(const char* arg)
{
    const dtm_of_driver_t *of_drivers[] = {
        &g_gpio_of_driver,
#ifndef DTM_NO_SPI_FLASH
        &g_spi_of_driver,
        &g_flash_of_driver,
#endif
    };
    int i;

    (void)arg;

    printf("[driver] start\n");

    if (driver_of_init() != 0) {
        printf("[driver] OF init failed\n");
        pthread_exit((void*)-1);
    }

    gpio_resources_init();
#ifndef DTM_NO_SPI_FLASH
    spi_resources_init();
    flash_resources_init();
#endif
    if (driver_of_is_ready()) {
        dtm_of_probe_all(of_drivers, DTM_ARRAY_SIZE(of_drivers));
    }

    if (g_dev_count <= 0) {
        printf("[driver] no GPIO devices configured\n");
        pthread_exit((void*)-1);
    }
    printf("[driver] GPIO devices loaded: %d\n", g_dev_count);

    gpio_irq_config();

    g_server_channel = pup_channel_create();
    if (g_server_channel < 0) {
        printf("[driver] pup_channel_create failed\n");
        pthread_exit((void*)-1);
    }

    for (i = 0; i < g_dev_count; i++) {
        if (gpio_register_one(&g_devs[i]) != 0) {
            printf("[driver] failed at index=%d\n", i);
            pup_channel_destroy(g_server_channel);
            pthread_exit((void*)-1);
        }
    }

    {
        pup_user_irq_subscription_t sub;
        sub.irq_num = gpio_key_irq_num();
        sub.flags = 0;
        sub.channel_id = g_server_channel;
        sub.reserved1 = 0;
        if (sub.irq_num == 0U) {
            printf("[driver] no key IRQ configured\n");
        } else {
            g_key_irq_handle = irq_subscribe(&sub);
            if (g_key_irq_handle < 0) {
                printf("[driver] irq_subscribe failed for irq=%u\n", (unsigned int)sub.irq_num);
            } else {
                printf("[driver] irq subscribed: irq=%u handle=%d\n",
                       (unsigned int)sub.irq_num,
                       g_key_irq_handle);
            }
        }
    }

#ifndef DTM_NO_SPI_FLASH
    {
        if (spi_bus() == 0) {
            printf("[driver] no SPI bus configured\n");
        } else if (spi_bus_init() != 0 || spi_register_one() != 0) {
            printf("[driver] SPI registration failed, continuing without SPI\n");
        }
    }

    {
        int flash_ret = flash_init();
        if (flash_ret != 0) {
            printf("[driver] flash0 init failed (ret=%d), continuing without flash\n", flash_ret);
        } else {
            printf("[driver] flash0 initialized\n");
        }
    }
#endif

    printf("[driver] ready\n");
    gpio_service_loop();

    if (g_server_channel >= 0) {
        pup_channel_destroy(g_server_channel);
        g_server_channel = -1;
    }

    pthread_exit((void*)-1);
}
