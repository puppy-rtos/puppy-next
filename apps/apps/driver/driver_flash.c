#include "libc.h"
#include "syscall.h"
#include "dtm_bindings.h"
#include "dtm_spi.h"
#include "dtm_gpio.h"
#include "shared/pup_devfs_proto.h"
#include "shared/pup_driver_abi.h"
#include "shared/pup_flash.h"
#include "shared/pup_spi.h"
#include "driver_flash.h"
#include <sfud.h>
#include <stdarg.h>

#define DEV_FLASH_DEV_PATH           "/dev/flash0"
#define DEV_FLASH_TEST_READ_SIZE     16

#define FLASH_CS_GPIOX    GPIOB
#define FLASH_CS_PIN      DTM_GPIO_PIN12

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef struct {
    int sfud_ready;
    sfud_flash *flash;
} dev_flash_ctx_t;

static dev_flash_ctx_t g_dev_flash_ctx;

static void sfud_spi_lock(const sfud_spi *spi)
{
    (void)spi;
}

static void sfud_spi_unlock(const sfud_spi *spi)
{
    (void)spi;
}

static void flash_cs_select(void)
{
    dtm_gpio_pin_set(FLASH_CS_GPIOX, FLASH_CS_PIN, 0);
}

static void flash_cs_deselect(void)
{
    dtm_gpio_pin_set(FLASH_CS_GPIOX, FLASH_CS_PIN, 1);
}

static sfud_err dev_flash_do_transfer(const uint8_t *write_buf,
                                      size_t write_size,
                                      uint8_t *read_buf,
                                      size_t read_size)
{
    if ((write_size == 0 && read_size == 0) || (write_buf == NULL && write_size != 0)) {
        printf("[driver] dev_flash_do_transfer: invalid params write_size=%u read_size=%u\n",
               (unsigned)write_size, (unsigned)read_size);
        return SFUD_ERR_WRITE;
    }

    flash_cs_select();

    for (size_t i = 0; i < write_size; i++) {
        dtm_spi_transmit_byte(DTM_SPI2_BASE, write_buf[i]);
    }

    for (size_t i = 0; i < read_size; i++) {
        read_buf[i] = dtm_spi_transmit_byte(DTM_SPI2_BASE, 0xFF);
    }

    flash_cs_deselect();

    return SFUD_SUCCESS;
}

static sfud_err spi_write_read(const sfud_spi *spi,
                               const uint8_t *write_buf,
                               size_t write_size,
                               uint8_t *read_buf,
                               size_t read_size)
{
    (void)spi;
    return dev_flash_do_transfer(write_buf, write_size, read_buf, read_size);
}

sfud_err sfud_spi_port_init(sfud_flash *flash)
{

    if (flash == NULL) {
        return SFUD_ERR_NOT_FOUND;
    }

    dtm_gpio_set(FLASH_CS_GPIOX, FLASH_CS_PIN, DTM_GPIO_MODE_OUT, DTM_GPIO_OTYPE_PP,
                 DTM_GPIO_SPEED_HIGH, DTM_GPIO_PUPD_PU);
    flash_cs_deselect();

    {
        dtm_spi_config_t spi_cfg;
        spi_cfg.mode = DTM_SPI_MODE_0;
        spi_cfg.datasize = DTM_SPI_DATASIZE_8BIT;
        spi_cfg.baudrate_prescaler = DTM_SPI_BAUDRATE_PRESCALER_8;
        spi_cfg.firstbit = 0;
        spi_cfg.crc_polarity = 0;
        dtm_spi_init(DTM_SPI2_BASE, &spi_cfg);
        dtm_spi_enable(DTM_SPI2_BASE);
    }

    flash->spi.wr = spi_write_read;
    flash->spi.lock = sfud_spi_lock;
    flash->spi.unlock = sfud_spi_unlock;
    flash->spi.user_data = &g_dev_flash_ctx;
    flash->retry.delay = NULL;
    flash->retry.times = 60000;
    flash->user_data = &g_dev_flash_ctx;

    printf("[driver] sfud_spi_port_init: done\n");

    return SFUD_SUCCESS;
}

void sfud_log_debug(const char *file, const long line, const char *format, ...)
{
    char log_buf[192];
    va_list args;

    va_start(args, format);
    vsnprintf(log_buf, sizeof(log_buf), format, args);
    va_end(args);
    printf("[SFUD DBG] %s:%ld %s\n", file, line, log_buf);
}

void sfud_log_info(const char *format, ...)
{
    char log_buf[192];
    va_list args;

    va_start(args, format);
    vsnprintf(log_buf, sizeof(log_buf), format, args);
    va_end(args);
    printf("[SFUD] %s\n", log_buf);
}

int flash_handle_request(pup_devfs_proxy_payload_t *req)
{
    sfud_err err;
    uint8_t status = 0;

    if (!req) {
        return -1;
    }

    if (!g_dev_flash_ctx.sfud_ready || g_dev_flash_ctx.flash == NULL) {
        return -1;
    }

    switch (req->op) {
    case DEVFS_PROXY_OP_OPEN:
        return 0;

    case DEVFS_PROXY_OP_CLOSE:
        return 0;

    case DEVFS_PROXY_OP_READ:
        return -1;

    case DEVFS_PROXY_OP_WRITE:
        return -1;

    case DEVFS_PROXY_OP_IOCTL:
        switch (req->cmd) {
        case PUP_FLASH_IOCTL_GET_INFO:
            {
                pup_flash_info_t *info = (pup_flash_info_t *)req->data_buf;
                if (info == NULL) {
                    return -1;
                }
                libcm_memset(info, 0, sizeof(*info));
                strcpy(info->name, g_dev_flash_ctx.flash->name);
                strcpy(info->spi_name, g_dev_flash_ctx.flash->spi.name);
                info->capacity = g_dev_flash_ctx.flash->chip.capacity;
                info->erase_gran = g_dev_flash_ctx.flash->chip.erase_gran;
                info->erase_cmd = g_dev_flash_ctx.flash->chip.erase_gran_cmd;
                info->init_ok = g_dev_flash_ctx.flash->init_ok ? 1 : 0;
                info->sfdp_available = g_dev_flash_ctx.flash->sfdp.available ? 1 : 0;
                info->mf_id = g_dev_flash_ctx.flash->chip.mf_id;
                info->type_id = g_dev_flash_ctx.flash->chip.type_id;
                info->capacity_id = g_dev_flash_ctx.flash->chip.capacity_id;
                return 0;
            }
        case PUP_FLASH_IOCTL_READ:
            {
                pup_flash_read_req_t *rreq = (pup_flash_read_req_t *)req->data_buf;
                if (rreq == NULL || rreq->buf == NULL || rreq->size == 0) {
                    return -1;
                }
                err = sfud_read(g_dev_flash_ctx.flash, rreq->addr, rreq->size, rreq->buf);
                return err == SFUD_SUCCESS ? 0 : -1;
            }
        case PUP_FLASH_IOCTL_WRITE:
            {
                pup_flash_write_req_t *wreq = (pup_flash_write_req_t *)req->data_buf;
                if (wreq == NULL || wreq->buf == NULL || wreq->size == 0) {
                    return -1;
                }
                err = sfud_write(g_dev_flash_ctx.flash, wreq->addr, wreq->size, wreq->buf);
                return err == SFUD_SUCCESS ? 0 : -1;
            }
        case PUP_FLASH_IOCTL_ERASE:
            {
                pup_flash_erase_req_t *ereq = (pup_flash_erase_req_t *)req->data_buf;
                if (ereq == NULL || ereq->size == 0) {
                    return -1;
                }
                err = sfud_erase(g_dev_flash_ctx.flash, ereq->addr, ereq->size);
                return err == SFUD_SUCCESS ? 0 : -1;
            }
        case PUP_FLASH_IOCTL_ERASE_WRITE:
            {
                pup_flash_write_req_t *wreq = (pup_flash_write_req_t *)req->data_buf;
                if (wreq == NULL || wreq->buf == NULL || wreq->size == 0) {
                    return -1;
                }
                err = sfud_erase_write(g_dev_flash_ctx.flash, wreq->addr, wreq->size, wreq->buf);
                return err == SFUD_SUCCESS ? 0 : -1;
            }
        case PUP_FLASH_IOCTL_READ_STATUS:
            {
                pup_flash_status_t *flash_status = (pup_flash_status_t *)req->data_buf;
                if (flash_status == NULL) {
                    return -1;
                }
                err = sfud_read_status(g_dev_flash_ctx.flash, &status);
                if (err != SFUD_SUCCESS) {
                    return -1;
                }
                flash_status->value = status;
                return 0;
            }
        default:
            return -1;
        }

    default:
        return -1;
    }
}

int flash_register_one(void)
{
    extern int g_server_channel;
    pup_user_devfs_registration_t reg;
    char name_buf[17];

    name_buf[0] = 'f'; name_buf[1] = 'l'; name_buf[2] = 'a'; name_buf[3] = 's';
    name_buf[4] = 'h'; name_buf[5] = '0'; name_buf[6] = '\0';

    reg.name = name_buf;
    reg.flags = PUP_DRIVER_DEVICE_FLAG_NONE;
    reg.capabilities = PUP_DRIVER_CAP_NONE;
    reg.channel_id = g_server_channel;
    reg.device_id = FLASH_DEVICE_ID;

    if (devfs_register(&reg) != 0) {
        printf("[driver] devfs_register failed for flash0\n");
        return -1;
    }

    printf("[driver] registered /dev/flash0 channel=%d devid=%d\n",
           g_server_channel, FLASH_DEVICE_ID);
    return 0;
}

static void dev_flash_dump_bytes(const uint8_t *buf, int size)
{
    char line[96];
    int pos = 0;
    int i;

    for (i = 0; i < size; i++) {
        pos += snprintf(line + pos, sizeof(line) - pos, "%02x", buf[i]);
        if (i != size - 1) {
            pos += snprintf(line + pos, sizeof(line) - pos, " ");
        }
        if (pos >= (int)sizeof(line) - 4) {
            break;
        }
    }

    printf("[driver] flash0 sample: %s\n", line);
}

int flash_init(void)
{
    sfud_err sfud_ret;
    uint8_t jedec_id[3];
    sfud_flash *flash;
    int retry;

    libcm_memset(&g_dev_flash_ctx, 0, sizeof(g_dev_flash_ctx));
    dtm_gpio_set(FLASH_CS_GPIOX, FLASH_CS_PIN, DTM_GPIO_MODE_OUT, DTM_GPIO_OTYPE_PP,
                 DTM_GPIO_SPEED_HIGH, DTM_GPIO_PUPD_PU);
    flash_cs_deselect();

    {
        dtm_spi_config_t spi_cfg;
        spi_cfg.mode = DTM_SPI_MODE_0;
        spi_cfg.datasize = DTM_SPI_DATASIZE_8BIT;
        spi_cfg.baudrate_prescaler = DTM_SPI_BAUDRATE_PRESCALER_8;
        spi_cfg.firstbit = 0;
        spi_cfg.crc_polarity = 0;
        dtm_spi_init(DTM_SPI2_BASE, &spi_cfg);
        dtm_spi_enable(DTM_SPI2_BASE);
    }

    for (retry = 0; retry < 3; retry++) {
        flash_cs_select();
        dtm_spi_transmit_byte(DTM_SPI2_BASE, 0x9F);
        jedec_id[0] = dtm_spi_transmit_byte(DTM_SPI2_BASE, 0xFF);
        jedec_id[1] = dtm_spi_transmit_byte(DTM_SPI2_BASE, 0xFF);
        jedec_id[2] = dtm_spi_transmit_byte(DTM_SPI2_BASE, 0xFF);
        flash_cs_deselect();

        printf("[driver] flash0: JEDEC ID read attempt %d: %02x %02x %02x\n",
               retry + 1, jedec_id[0], jedec_id[1], jedec_id[2]);

        if (jedec_id[0] != 0x00 && jedec_id[0] != 0xFF) {
            break;
        }
    }

    if (jedec_id[0] == 0x00 || jedec_id[0] == 0xFF) {
        printf("[driver] flash0: no SPI flash detected (JEDEC ID: %02x %02x %02x)\n",
               jedec_id[0], jedec_id[1], jedec_id[2]);
        return -1;
    }

    printf("[driver] flash0: SPI flash detected, initializing SFUD...\n");
    sfud_ret = sfud_init();
    if (sfud_ret != SFUD_SUCCESS) {
        printf("[driver] sfud_init failed: %d\n", (int)sfud_ret);
        return -1;
    }

    flash = sfud_get_device(SFUD_FLASH0_DEVICE_INDEX);
    if (flash == NULL || !flash->init_ok) {
        printf("[driver] flash0 init not ready\n");
        return -1;
    }

    g_dev_flash_ctx.flash = flash;
    g_dev_flash_ctx.sfud_ready = 1;
    if (flash_register_one() != 0) {
        printf("[driver] register flash0 failed\n");
        return -1;
    }

    printf("[driver] flash0 registered to devfs, capacity=%lu erase_gran=%lu\n",
           (unsigned long)flash->chip.capacity,
           (unsigned long)flash->chip.erase_gran);
    return 0;
}

int flash_selftest(void)
{
    pup_flash_info_t info;
    uint8_t status = 0;
    sfud_err err;
    uint8_t *test_buf = NULL;
    uint8_t *backup_buf = NULL;
    uint8_t *read_buf = NULL;
    uint32_t test_addr;
    uint32_t test_size;
    int passed = 1;

    if (!g_dev_flash_ctx.sfud_ready || g_dev_flash_ctx.flash == NULL) {
        printf("[driver] selftest: flash not ready\n");
        return -1;
    }

    libcm_memset(&info, 0, sizeof(info));
    strcpy(info.name, g_dev_flash_ctx.flash->name);
    strcpy(info.spi_name, g_dev_flash_ctx.flash->spi.name);
    info.capacity = g_dev_flash_ctx.flash->chip.capacity;
    info.erase_gran = g_dev_flash_ctx.flash->chip.erase_gran;
    info.erase_cmd = g_dev_flash_ctx.flash->chip.erase_gran_cmd;
    info.init_ok = g_dev_flash_ctx.flash->init_ok ? 1 : 0;
    info.sfdp_available = g_dev_flash_ctx.flash->sfdp.available ? 1 : 0;
    info.mf_id = g_dev_flash_ctx.flash->chip.mf_id;
    info.type_id = g_dev_flash_ctx.flash->chip.type_id;
    info.capacity_id = g_dev_flash_ctx.flash->chip.capacity_id;

    printf("[driver] flash0 info: mf=0x%02x type=0x%02x cap_id=0x%02x cap=%lu erase=%lu sfdp=%d\n",
           info.mf_id,
           info.type_id,
           info.capacity_id,
           (unsigned long)info.capacity,
           (unsigned long)info.erase_gran,
           info.sfdp_available);

    err = sfud_read_status(g_dev_flash_ctx.flash, &status);
    if (err == SFUD_SUCCESS) {
        printf("[driver] flash0 status=0x%02x\n", status);
    }

    test_size = info.erase_gran > 0 ? info.erase_gran : 256;
    test_addr = (info.capacity > test_size * 2) ? (info.capacity - test_size * 2) : test_size;

    test_buf = (uint8_t *)shared_malloc(test_size);
    backup_buf = (uint8_t *)shared_malloc(test_size);
    read_buf = (uint8_t *)shared_malloc(test_size);
    if (!test_buf || !backup_buf || !read_buf) {
        printf("[driver] selftest: buffer malloc failed\n");
        goto cleanup;
    }

    printf("[driver] selftest: test addr=0x%08lx size=%lu\n", (unsigned long)test_addr, (unsigned long)test_size);

    err = sfud_read(g_dev_flash_ctx.flash, test_addr, test_size, backup_buf);
    if (err != SFUD_SUCCESS) {
        printf("[driver] selftest: backup read failed %d\n", err);
        goto cleanup;
    }
    printf("[driver] selftest: backed up %lu bytes\n", (unsigned long)test_size);

    printf("[driver] selftest: erasing sector at 0x%08lx...\n", (unsigned long)test_addr);
    err = sfud_erase(g_dev_flash_ctx.flash, test_addr, test_size);
    if (err != SFUD_SUCCESS) {
        printf("[driver] selftest: erase failed %d\n", err);
        passed = 0;
        goto restore;
    }
    printf("[driver] selftest: erase OK\n");

    err = sfud_read(g_dev_flash_ctx.flash, test_addr, test_size, read_buf);
    if (err != SFUD_SUCCESS) {
        printf("[driver] selftest: read after erase failed %d\n", err);
        passed = 0;
        goto restore;
    }
    {
        int i;
        int nonzero = 0;
        for (i = 0; i < (int)test_size; i++) {
            if (read_buf[i] != 0xFF) {
                nonzero++;
                if (nonzero <= 4) {
                    printf("[driver] selftest: erase verify byte[%d]=0x%02x (expected 0xFF)\n", i, read_buf[i]);
                }
            }
        }
        if (nonzero > 0) {
            printf("[driver] selftest: erase verify warning, %d/%lu bytes not 0xFF\n", nonzero, (unsigned long)test_size);
        } else {
            printf("[driver] selftest: erase verify OK (all 0xFF)\n");
        }
    }
    dev_flash_dump_bytes(read_buf, (int)test_size);

    for (uint32_t i = 0; i < test_size; i++) {
        test_buf[i] = (uint8_t)((i % 2 == 0) ? 0x55 : 0xAA);
    }

    printf("[driver] selftest: writing pattern 0x55AA...\n");
    err = sfud_write(g_dev_flash_ctx.flash, test_addr, test_size, test_buf);
    if (err != SFUD_SUCCESS) {
        printf("[driver] selftest: write failed %d\n", err);
        passed = 0;
        goto restore;
    }
    printf("[driver] selftest: write OK\n");

    err = sfud_read(g_dev_flash_ctx.flash, test_addr, test_size, read_buf);
    if (err != SFUD_SUCCESS) {
        printf("[driver] selftest: read after write failed %d\n", err);
        passed = 0;
        goto restore;
    }

    {
        int mismatch = 0;
        for (uint32_t i = 0; i < test_size; i++) {
            if (read_buf[i] != test_buf[i]) {
                mismatch++;
                if (mismatch <= 4) {
                    printf("[driver] selftest: verify byte[%lu]=0x%02x expected 0x%02x\n", 
                           (unsigned long)i, read_buf[i], test_buf[i]);
                }
            }
        }
        if (mismatch > 0) {
            printf("[driver] selftest: FAILED verify mismatch %d/%lu bytes\n", mismatch, (unsigned long)test_size);
            passed = 0;
        } else {
            printf("[driver] selftest: verify OK pattern match\n");
        }
    }
    dev_flash_dump_bytes(read_buf, (int)test_size);

restore:
    printf("[driver] selftest: restoring original data...\n");
    err = sfud_write(g_dev_flash_ctx.flash, test_addr, test_size, backup_buf);
    if (err == SFUD_SUCCESS) {
        printf("[driver] selftest: restore OK\n");
    } else {
        printf("[driver] selftest: restore failed %d\n", err);
    }

cleanup:
    if (test_buf) shared_free(test_buf);
    if (backup_buf) shared_free(backup_buf);
    if (read_buf) shared_free(read_buf);

    if (passed) {
        printf("[driver] selftest: PASSED\n");
    } else {
        printf("[driver] selftest: FAILED\n");
    }
    return passed ? 0 : -1;
}
