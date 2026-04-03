#include "libc.h"
#include "syscall.h"
#include "dtm_gpio.h"
#include "dtm_spi.h"
#include "dtm_bindings.h"
#include "shared/pup_devfs_proto.h"
#include "shared/pup_spi.h"
#include "driver_spi.h"

int g_spi_next_slave_id = 1;
int g_spi_initialized = 0;
spi_slave_t g_spi_slaves[SPI_MAX_SLAVES];

int spi_cs_from_id(uint8_t cs_id, uint32_t *gpiox, uint16_t *pinx)
{
    if (cs_id == PUP_SPI2_CS0) {
        *gpiox = GPIOB;
        *pinx = DTM_GPIO_PIN12;
        return 0;
    }
    return -1;
}

void spi2_gpio_init(void)
{
    dtm_gpio_set(GPIOC, DTM_GPIO_PIN2, DTM_GPIO_MODE_AF, DTM_GPIO_OTYPE_PP,
                 DTM_GPIO_SPEED_HIGH, DTM_GPIO_PUPD_PU);
    dtm_gpio_af_set(GPIOC, DTM_GPIO_PIN2, 5);  /* SPI2_SCK */

    dtm_gpio_set(GPIOC, DTM_GPIO_PIN3, DTM_GPIO_MODE_AF, DTM_GPIO_OTYPE_PP,
                 DTM_GPIO_SPEED_HIGH, DTM_GPIO_PUPD_PU);
    dtm_gpio_af_set(GPIOC, DTM_GPIO_PIN3, 5);  /* SPI2_MISO */

    dtm_gpio_set(GPIOB, DTM_GPIO_PIN13, DTM_GPIO_MODE_AF, DTM_GPIO_OTYPE_PP,
                 DTM_GPIO_SPEED_HIGH, DTM_GPIO_PUPD_PU);
    dtm_gpio_af_set(GPIOB, DTM_GPIO_PIN13, 5); /* SPI2_MOSI */
}

static int spi_handle_ioctl(pup_devfs_proxy_payload_t *req)
{
    int i;

    if (!req || !req->data_buf) {
        return -1;
    }

    switch (req->cmd) {
    case PUP_SPI_IOCTL_REGISTER_SLAVE: {
        pup_spi_slave_config_t *cfg = (pup_spi_slave_config_t *)req->data_buf;
        int slot = -1;
        uint32_t cs_gpiox;
        uint16_t cs_pinx;

        for (i = 0; i < SPI_MAX_SLAVES; i++) {
            if (!g_spi_slaves[i].active) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            printf("[driver] spi: no free slave slots\n");
            return -1;
        }

        if (spi_cs_from_id(cfg->cs_id, &cs_gpiox, &cs_pinx) != 0) {
            printf("[driver] spi: unknown cs_id=%d\n", cfg->cs_id);
            return -1;
        }

        g_spi_slaves[slot].active = 1;
        g_spi_slaves[slot].slave_id = g_spi_next_slave_id++;
        g_spi_slaves[slot].cs_id = cfg->cs_id;
        g_spi_slaves[slot].cs_gpiox = cs_gpiox;
        g_spi_slaves[slot].cs_pinx = cs_pinx;
        g_spi_slaves[slot].mode = cfg->mode;
        g_spi_slaves[slot].baudrate_prescaler = cfg->baudrate_prescaler;
        g_spi_slaves[slot].datasize = cfg->datasize;
        g_spi_slaves[slot].firstbit = cfg->firstbit;
        for (i = 0; i < PUP_SPI_NAME_MAX; i++) {
            g_spi_slaves[slot].name[i] = cfg->name[i];
        }

        /* Configure CS pin as output, default HIGH (deselected) */
        dtm_gpio_set(cs_gpiox, cs_pinx, DTM_GPIO_MODE_OUT, DTM_GPIO_OTYPE_PP,
                     DTM_GPIO_SPEED_HIGH, DTM_GPIO_PUPD_PU);
        dtm_gpio_pin_set(cs_gpiox, cs_pinx, 1);

        cfg->slave_id = g_spi_slaves[slot].slave_id;

        return 0;
    }

    case PUP_SPI_IOCTL_TRANSFER: {
        pup_spi_transfer_t *xfer = (pup_spi_transfer_t *)req->data_buf;
        spi_slave_t *slave = (spi_slave_t *)0;
        uint8_t *tx_data;
        uint8_t *rx_data;

        for (i = 0; i < SPI_MAX_SLAVES; i++) {
            if (g_spi_slaves[i].active && g_spi_slaves[i].slave_id == xfer->slave_id) {
                slave = &g_spi_slaves[i];
                break;
            }
        }
        if (!slave) {
            printf("[driver] spi: transfer: unknown slave_id=%d\n", xfer->slave_id);
            return -1;
        }

        tx_data = (uint8_t *)(xfer->tx_buf);
        rx_data = (uint8_t *)(xfer->rx_buf);

        /* Reconfigure SPI mode/speed for this slave if needed */
        {
            dtm_spi_config_t spi_cfg;
            spi_cfg.mode = slave->mode;
            spi_cfg.datasize = (slave->datasize == 1) ? DTM_SPI_DATASIZE_16BIT : DTM_SPI_DATASIZE_8BIT;
            spi_cfg.baudrate_prescaler = slave->baudrate_prescaler;
            spi_cfg.firstbit = slave->firstbit;
            spi_cfg.crc_polarity = 0;
            dtm_spi_init(DTM_SPI2_BASE, &spi_cfg);
            dtm_spi_enable(DTM_SPI2_BASE);
        }

        if (tx_data && rx_data && xfer->len > 0) {
            /* Pull CS LOW to select slave */
            dtm_gpio_pin_set(slave->cs_gpiox, slave->cs_pinx, 0);

            dtm_spi_transmit_receive(DTM_SPI2_BASE, tx_data, rx_data, xfer->len);

            /* Pull CS HIGH to deselect slave */
            dtm_gpio_pin_set(slave->cs_gpiox, slave->cs_pinx, 1);
        } else if (rx_data && xfer->len > 0) {
            dtm_gpio_pin_set(slave->cs_gpiox, slave->cs_pinx, 0);
            dtm_spi_receive(DTM_SPI2_BASE, rx_data, xfer->len);
            dtm_gpio_pin_set(slave->cs_gpiox, slave->cs_pinx, 1);
        } else if (tx_data && xfer->len > 0) {
            dtm_gpio_pin_set(slave->cs_gpiox, slave->cs_pinx, 0);
            dtm_spi_transmit(DTM_SPI2_BASE, tx_data, xfer->len);
            dtm_gpio_pin_set(slave->cs_gpiox, slave->cs_pinx, 1);
        }

        xfer->actual_len = xfer->len;
        return 0;
    }

    default:
        printf("[driver] spi: unknown ioctl cmd=0x%x\n", req->cmd);
        return -1;
    }
}

int spi_handle_request(pup_devfs_proxy_payload_t *req)
{
    if (!req) {
        return -1;
    }

    switch (req->op) {
    case DEVFS_PROXY_OP_OPEN:
        if (!g_spi_initialized) {
            spi2_gpio_init();
            g_spi_initialized = 1;
        }
        return 0;

    case DEVFS_PROXY_OP_CLOSE:
        return 0;

    case DEVFS_PROXY_OP_IOCTL:
        return spi_handle_ioctl(req);

    default:
        printf("[driver] spi2: unsupported op=%d\n", req->op);
        return -1;
    }
}

int spi_register_one(void)
{
    extern int g_server_channel;
    pup_user_devfs_registration_t reg;
    char name_buf[17];

    name_buf[0] = 's'; name_buf[1] = 'p'; name_buf[2] = 'i';
    name_buf[3] = '2'; name_buf[4] = '\0';

    reg.name = name_buf;
    reg.flags = PUP_DRIVER_DEVICE_FLAG_NONE;
    reg.capabilities = PUP_DRIVER_CAP_NONE;
    reg.channel_id = g_server_channel;
    reg.device_id = SPI_DEVICE_ID;

    if (devfs_register(&reg) != 0) {
        printf("[driver] devfs_register failed for spi2\n");
        return -1;
    }

    printf("[driver] registered /dev/spi2 channel=%d devid=%d\n",
           g_server_channel, SPI_DEVICE_ID);
    return 0;
}
