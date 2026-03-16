#include "libc.h"
#include "syscall.h"
#include "dtm_spi.h"
#include "dtm_gpio.h"
#include "dtm_bindings.h"
#include "shared/pup_devfs_proto.h"
#include "shared/pup_driver_abi.h"
#include "shared/pup_spi.h"
#include "driver_gpio.h"
#include "driver_spi.h"
#include "driver_flash.h"

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

            if (req->device_id == SPI_DEVICE_ID) {
                ret = spi_handle_request(req);
            } else if (req->device_id == FLASH_DEVICE_ID) {
                ret = flash_handle_request(req);
            } else {
                gpio_user_dev_t* dev = gpio_find_by_id(req->device_id);
                ret = gpio_handle_request(dev, req);
            }
            pup_channel_reply(g_server_channel, cookie, ret);
        }
    }
}

void app_main(const char* arg)
{
    int i;

    (void)arg;

    printf("[driver] start\n");

    gpio_irq_config();

    g_server_channel = pup_channel_create();
    if (g_server_channel < 0) {
        printf("[driver] pup_channel_create failed\n");
        pthread_exit((void*)-1);
    }

    for (i = 0; i < GPIO_DEV_COUNT; i++) {
        if (gpio_register_one(&g_devs[i]) != 0) {
            printf("[driver] failed at index=%d\n", i);
            pup_channel_destroy(g_server_channel);
            pthread_exit((void*)-1);
        }
    }

    {
        pup_user_irq_subscription_t sub;
        sub.irq_num = PUP_IRQ_EXTI9_5;
        sub.flags = 0;
        sub.channel_id = g_server_channel;
        sub.reserved1 = 0;
        g_key_irq_handle = irq_subscribe(&sub);
        if (g_key_irq_handle < 0) {
            printf("[driver] irq_subscribe failed for EXTI9_5\n");
        } else {
            printf("[driver] irq subscribed: EXTI9_5 handle=%d\n", g_key_irq_handle);
        }
    }

    /* Initialize SPI2 and register /dev/spi2 */
    {
        dtm_spi_config_t spi_cfg;

        dtm_gpio_set(GPIOC, DTM_GPIO_PIN2, DTM_GPIO_MODE_AF, DTM_GPIO_OTYPE_PP,
                     DTM_GPIO_SPEED_HIGH, DTM_GPIO_PUPD_PU);
        dtm_gpio_af_set(GPIOC, DTM_GPIO_PIN2, 5);

        dtm_gpio_set(GPIOC, DTM_GPIO_PIN3, DTM_GPIO_MODE_AF, DTM_GPIO_OTYPE_PP,
                     DTM_GPIO_SPEED_HIGH, DTM_GPIO_PUPD_PU);
        dtm_gpio_af_set(GPIOC, DTM_GPIO_PIN3, 5);

        dtm_gpio_set(GPIOB, DTM_GPIO_PIN13, DTM_GPIO_MODE_AF, DTM_GPIO_OTYPE_PP,
                     DTM_GPIO_SPEED_HIGH, DTM_GPIO_PUPD_PU);
        dtm_gpio_af_set(GPIOB, DTM_GPIO_PIN13, 5);

        spi_cfg.mode = DTM_SPI_MODE_0;
        spi_cfg.datasize = DTM_SPI_DATASIZE_8BIT;
        spi_cfg.baudrate_prescaler = DTM_SPI_BAUDRATE_PRESCALER_128;
        spi_cfg.firstbit = 0;
        spi_cfg.crc_polarity = 0;
        dtm_spi_init(DTM_SPI2_BASE, &spi_cfg);
        dtm_spi_enable(DTM_SPI2_BASE);
        printf("[driver] SPI2 initialized\n");

        if (spi_register_one() != 0) {
            printf("[driver] SPI2 registration failed, continuing without SPI\n");
        }
    }

    /* Initialize flash driver and register /dev/flash0 */
    {
        int flash_ret = flash_init();
        if (flash_ret != 0) {
            printf("[driver] flash0 init failed (ret=%d), continuing without flash\n", flash_ret);
        } else {
            printf("[driver] flash0 initialized\n");
        }
    }

    printf("[driver] ready\n");
    gpio_service_loop();

    if (g_server_channel >= 0) {
        pup_channel_destroy(g_server_channel);
        g_server_channel = -1;
    }

    pthread_exit((void*)-1);
}
