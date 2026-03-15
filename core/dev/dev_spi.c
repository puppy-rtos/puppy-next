#include "dev_spi.h"
#include "shared/pup_spi.h"
#include "puppy_log.h"
#include "dtm_bindings.h"
#include "dtm_spi.h"
#include "dtm_gpio.h"
#include <libcm.h>

#define SPI2_MAX_SLAVES  4

typedef struct {
    int used;
    int id;
    char name[PUP_SPI_NAME_MAX];
    uint32_t gpiox;
    uint16_t pinx;
    uint8_t mode;
    uint8_t baudrate_prescaler;
    uint8_t datasize;
    uint8_t firstbit;
} spi2_slave_t;

typedef struct {
    int initialized;
    int next_slave_id;
    spi2_slave_t slaves[SPI2_MAX_SLAVES];
} spi2_bus_ctx_t;

static spi2_bus_ctx_t g_spi2_ctx;

static int spi2_open(pup_devfs_file_t *file)
{
    (void)file;

#if defined(DTM_BINFDING_SPI_ARM_M4_ST_GD) && defined(DTM_SPI2_BASE)
    dtm_spi_config_t spi_config;

    if (g_spi2_ctx.initialized) {
        return PUP_DEVFS_OK;
    }

    dtm_gpio_set(GPIOC, DTM_GPIO_PIN2, DTM_GPIO_MODE_AF, DTM_GPIO_OTYPE_PP,
                 DTM_GPIO_SPEED_HIGH, DTM_GPIO_PUPD_PU);
    dtm_gpio_af_set(GPIOC, DTM_GPIO_PIN2, 5);

    dtm_gpio_set(GPIOC, DTM_GPIO_PIN3, DTM_GPIO_MODE_AF, DTM_GPIO_OTYPE_PP,
                 DTM_GPIO_SPEED_HIGH, DTM_GPIO_PUPD_PU);
    dtm_gpio_af_set(GPIOC, DTM_GPIO_PIN3, 5);

    dtm_gpio_set(GPIOB, DTM_GPIO_PIN13, DTM_GPIO_MODE_AF, DTM_GPIO_OTYPE_PP,
                 DTM_GPIO_SPEED_HIGH, DTM_GPIO_PUPD_PU);
    dtm_gpio_af_set(GPIOB, DTM_GPIO_PIN13, 5);

    spi_config.mode = DTM_SPI_MODE_0;
    spi_config.datasize = DTM_SPI_DATASIZE_8BIT;
    spi_config.baudrate_prescaler = DTM_SPI_BAUDRATE_PRESCALER_8;
    spi_config.firstbit = 0;
    spi_config.crc_polarity = 0;

    dtm_spi_init(DTM_SPI2_BASE, &spi_config);
    dtm_spi_enable(DTM_SPI2_BASE);

    g_spi2_ctx.initialized = 1;
    PUP_LOG_INFO(PUP_LOG_MODULE_DEV, "SPI2 initialized");
    return PUP_DEVFS_OK;
#else
    PUP_LOG_ERROR(PUP_LOG_MODULE_DEV, "SPI2 is not supported on current chip bindings");
    return PUP_DEVFS_ERROR;
#endif
}

static int spi2_close(pup_devfs_file_t *file)
{
    (void)file;
    return PUP_DEVFS_OK;
}

static int spi2_read(pup_devfs_file_t *file, char *buf, int len)
{
    (void)file;
    (void)buf;
    (void)len;
    return PUP_DEVFS_EINVAL;
}

static int spi2_write(pup_devfs_file_t *file, const char *buf, int len)
{
    (void)file;
    (void)buf;
    (void)len;
    return PUP_DEVFS_EINVAL;
}

static int spi2_find_slave_by_name(const char *name)
{
    int i;

    for (i = 0; i < SPI2_MAX_SLAVES; i++) {
        if (g_spi2_ctx.slaves[i].used && libcm_strcmp(g_spi2_ctx.slaves[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}

static int spi2_find_slave_by_id(int id)
{
    int i;

    for (i = 0; i < SPI2_MAX_SLAVES; i++) {
        if (g_spi2_ctx.slaves[i].used && g_spi2_ctx.slaves[i].id == id) {
            return i;
        }
    }

    return -1;
}

static int spi2_find_free_slot(void)
{
    int i;

    for (i = 0; i < SPI2_MAX_SLAVES; i++) {
        if (!g_spi2_ctx.slaves[i].used) {
            return i;
        }
    }

    return -1;
}

static int spi2_cs_from_id(uint8_t cs_id, uint32_t *gpiox, uint16_t *pinx)
{
#if defined(DTM_BINFDING_SPI_ARM_M4_ST_GD)
    if (cs_id == PUP_SPI2_CS0) {
        *gpiox = GPIOB;
        *pinx = DTM_GPIO_PIN12;
        return 0;
    }
#else
    (void)cs_id;
    (void)gpiox;
    (void)pinx;
#endif

    return -1;
}

static int spi2_register_slave(pup_spi_slave_config_t *cfg)
{
    int slot;
    uint32_t gpiox;
    uint16_t pinx;

    if (cfg == PUP_NULL || cfg->name[0] == '\0') {
        return PUP_DEVFS_EINVAL;
    }

    slot = spi2_find_slave_by_name(cfg->name);
    if (slot >= 0) {
        cfg->slave_id = g_spi2_ctx.slaves[slot].id;
        return PUP_DEVFS_OK;
    }

    slot = spi2_find_free_slot();
    if (slot < 0) {
        return PUP_DEVFS_ENOMEM;
    }

    if (spi2_cs_from_id(cfg->cs_id, &gpiox, &pinx) != 0) {
        return PUP_DEVFS_EINVAL;
    }

    g_spi2_ctx.slaves[slot].used = 1;
    g_spi2_ctx.slaves[slot].id = g_spi2_ctx.next_slave_id++;
    g_spi2_ctx.slaves[slot].gpiox = gpiox;
    g_spi2_ctx.slaves[slot].pinx = pinx;
    g_spi2_ctx.slaves[slot].mode = cfg->mode;
    g_spi2_ctx.slaves[slot].baudrate_prescaler = cfg->baudrate_prescaler;
    g_spi2_ctx.slaves[slot].datasize = cfg->datasize;
    g_spi2_ctx.slaves[slot].firstbit = cfg->firstbit;
    libcm_strncpy(g_spi2_ctx.slaves[slot].name, cfg->name, PUP_SPI_NAME_MAX - 1);
    g_spi2_ctx.slaves[slot].name[PUP_SPI_NAME_MAX - 1] = '\0';

    dtm_gpio_set(gpiox, pinx, DTM_GPIO_MODE_OUT, DTM_GPIO_OTYPE_PP,
                 DTM_GPIO_SPEED_HIGH, DTM_GPIO_PUPD_PU);
    dtm_gpio_pin_set(gpiox, pinx, 1);

    cfg->slave_id = g_spi2_ctx.slaves[slot].id;

    PUP_LOG_INFO(PUP_LOG_MODULE_DEV, "SPI2 slave registered: %s id=%d", cfg->name, cfg->slave_id);
    return PUP_DEVFS_OK;
}

static int spi2_transfer(pup_spi_transfer_t *xfer)
{
    int slot;
    spi2_slave_t *slave;
    dtm_spi_config_t spi_config;
    uint32_t i;

    if (xfer == PUP_NULL || xfer->len == 0 || (!xfer->tx_buf && !xfer->rx_buf)) {
        return PUP_DEVFS_EINVAL;
    }

    slot = spi2_find_slave_by_id(xfer->slave_id);
    if (slot < 0) {
        return PUP_DEVFS_ENOENT;
    }

    slave = &g_spi2_ctx.slaves[slot];

    spi_config.mode = slave->mode;
    spi_config.datasize = slave->datasize;
    spi_config.baudrate_prescaler = slave->baudrate_prescaler;
    spi_config.firstbit = slave->firstbit;
    spi_config.crc_polarity = 0;

    dtm_spi_init(DTM_SPI2_BASE, &spi_config);
    dtm_spi_enable(DTM_SPI2_BASE);

    dtm_gpio_pin_set(slave->gpiox, slave->pinx, 0);
    for (i = 0; i < xfer->len; i++) {
        uint8_t tx = xfer->tx_buf ? xfer->tx_buf[i] : 0xFF;
        uint8_t rx = dtm_spi_transmit_byte(DTM_SPI2_BASE, tx);
        if (xfer->rx_buf) {
            xfer->rx_buf[i] = rx;
        }
    }
    dtm_gpio_pin_set(slave->gpiox, slave->pinx, 1);

    xfer->actual_len = xfer->len;
    return PUP_DEVFS_OK;
}

static int spi2_ioctl(pup_devfs_file_t *file, int cmd, void *arg)
{
    (void)file;

    switch (cmd) {
    case PUP_SPI_IOCTL_REGISTER_SLAVE:
        return spi2_register_slave((pup_spi_slave_config_t *)arg);
    case PUP_SPI_IOCTL_TRANSFER:
        return spi2_transfer((pup_spi_transfer_t *)arg);
    default:
        PUP_LOG_ERROR(PUP_LOG_MODULE_DEV, "Unknown SPI ioctl: 0x%x", cmd);
        return PUP_DEVFS_EINVAL;
    }
}

static const pup_devfs_file_ops_t spi2_ops = {
    .open = spi2_open,
    .close = spi2_close,
    .read = spi2_read,
    .write = spi2_write,
    .ioctl = spi2_ioctl
};

static const pup_devfs_device_t spi2_device = {
    .name = "spi2",
    .ops = &spi2_ops,
    .private_data = &g_spi2_ctx
};

int dev_spi_init(void)
{
#if defined(DTM_BINFDING_SPI_ARM_M4_ST_GD) && defined(DTM_SPI2_BASE)
    libcm_memset(&g_spi2_ctx, 0, sizeof(g_spi2_ctx));
    g_spi2_ctx.next_slave_id = 1;

    PUP_LOG_INFO(PUP_LOG_MODULE_DEV, "Registering SPI2 device");
    return pup_devfs_register_device(&spi2_device);
#else
    PUP_LOG_INFO(PUP_LOG_MODULE_DEV, "SPI2 is not available on current board, skip registration");
    return PUP_DEVFS_OK;
#endif
}