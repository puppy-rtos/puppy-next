#include "libc.h"
#include "syscall.h"
#include "dtm_bindings.h"
#include "dtm_gpio.h"
#include "dtm_spi.h"
#include "shared/pup_devfs_proto.h"
#include "shared/pup_spi.h"
#include "driver_spi.h"

#ifndef DTM_SPI2_BASE
#define DTM_SPI2_BASE 0U
#endif

#ifndef GPIOB
#define GPIOB 0U
#endif

#ifndef GPIOC
#define GPIOC 0U
#endif

int g_spi_next_slave_id = 1;
int g_spi_initialized = 0;
spi_slave_t g_spi_slaves[SPI_MAX_SLAVES];

static pup_dtm_spi_bus_t g_spi_buses[PUP_DTM_MAX_SPI_BUSES];
static dtm_ofw_node_t g_spi_bus_nodes[PUP_DTM_MAX_SPI_BUSES];
static unsigned int g_spi_bus_count;
static int g_spi_using_dtb;

static int spi_of_probe(dtm_ofw_node_t node);

static const char *const g_spi_compatibles[] = {
    "puppy,spi-arm-m4-st-gd",
    "st,stm32-spi",
};

const dtm_of_driver_t g_spi_of_driver = {
    "spi",
    g_spi_compatibles,
    DTM_ARRAY_SIZE(g_spi_compatibles),
    spi_of_probe,
};

static void spi_clear_buses(void)
{
    unsigned int i;

    libcm_memset(g_spi_buses, 0, sizeof(g_spi_buses));
    for (i = 0; i < PUP_DTM_MAX_SPI_BUSES; i++) {
        g_spi_bus_nodes[i] = DTM_OFW_NODE_INVALID;
    }
    g_spi_bus_count = 0U;
    g_spi_initialized = 0;
}

static void spi_begin_dtb_probe(void)
{
    if (g_spi_using_dtb) {
        return;
    }

    spi_clear_buses();
    g_spi_using_dtb = 1;
}

static pup_dtm_spi_bus_t *spi_alloc_bus(dtm_ofw_node_t node)
{
    if (g_spi_bus_count >= PUP_DTM_MAX_SPI_BUSES) {
        return (pup_dtm_spi_bus_t *)0;
    }

    g_spi_bus_nodes[g_spi_bus_count] = node;
    return &g_spi_buses[g_spi_bus_count];
}

void spi_resources_init(void)
{
    spi_clear_buses();
    g_spi_using_dtb = 0;
}

static int spi_of_probe(dtm_ofw_node_t node)
{
    pup_dtm_spi_bus_t *bus;
    uint64_t addr = 0;
    uint32_t value;
    const char *name = 0;
    int i;

    spi_begin_dtb_probe();
    bus = spi_alloc_bus(node);
    if (!bus) {
        return -1;
    }

    if (dtm_ofw_get_reg(node, 0, &addr, 0) < 0) {
        return -1;
    }

    libcm_memset(bus, 0, sizeof(*bus));
    bus->valid = 1U;
    bus->base = (uint32_t)addr;

    if (driver_of_prop_read_string(node, "puppy,dev-name", &name) < 0) {
        name = driver_of_node_base_name(dtm_ofw_node_name(node));
    }
    driver_of_copy_name(bus->name, name);

    if (dtm_ofw_prop_read_u32(node, "puppy,device-id", &value) == 0) {
        bus->device_id = (uint8_t)value;
    } else {
        bus->device_id = (uint8_t)(3U + g_spi_bus_count);
    }

    bus->default_mode = 0U;
    bus->default_datasize = 0U;
    bus->default_prescaler = 7U;
    bus->default_firstbit = 0U;
    if (dtm_ofw_prop_read_u32(node, "puppy,spi-mode", &value) == 0) {
        bus->default_mode = (uint8_t)value;
    }
    if (dtm_ofw_prop_read_u32(node, "puppy,baudrate-prescaler", &value) == 0) {
        bus->default_prescaler = (uint8_t)value;
    }

    bus->pin_count = (uint8_t)dtm_of_parse_pinctrl(node, bus->pins, PUP_DTM_MAX_SPI_PINS);

    for (i = 0; i < PUP_DTM_MAX_SPI_CS; i++) {
        uint32_t flags = 0;
        if (driver_of_parse_gpio_spec(node, "cs-gpios", i,
                                      &bus->cs[i].gpio_base,
                                      &bus->cs[i].pin_mask,
                                      &flags) < 0) {
            break;
        }
        bus->cs[i].cs_id = (uint8_t)i;
        bus->cs[i].flags = (uint8_t)flags;
    }
    bus->cs_count = (uint8_t)i;

    g_spi_bus_count++;
    return 0;
}

const pup_dtm_spi_bus_t *spi_bus_by_index(unsigned int index)
{
    if (index >= g_spi_bus_count ||
        index >= PUP_DTM_MAX_SPI_BUSES ||
        !g_spi_buses[index].valid) {
        return (const pup_dtm_spi_bus_t *)0;
    }

    return &g_spi_buses[index];
}

const pup_dtm_spi_bus_t *spi_bus(void)
{
    unsigned int i;

    for (i = 0; i < g_spi_bus_count && i < PUP_DTM_MAX_SPI_BUSES; i++) {
        if (g_spi_buses[i].valid) {
            return &g_spi_buses[i];
        }
    }

    return (const pup_dtm_spi_bus_t *)0;
}

const pup_dtm_spi_cs_t *spi_cs(uint8_t cs_id)
{
    const pup_dtm_spi_bus_t *bus = spi_bus();
    unsigned int i;

    if (!bus) {
        return (const pup_dtm_spi_cs_t *)0;
    }

    for (i = 0; i < bus->cs_count && i < PUP_DTM_MAX_SPI_CS; i++) {
        if (bus->cs[i].cs_id == cs_id) {
            return &bus->cs[i];
        }
    }

    return (const pup_dtm_spi_cs_t *)0;
}

int spi_bus_index_by_node(dtm_ofw_node_t node)
{
    unsigned int i;

    for (i = 0; i < g_spi_bus_count && i < PUP_DTM_MAX_SPI_BUSES; i++) {
        if (g_spi_bus_nodes[i] == node) {
            return (int)i;
        }
    }

    return -1;
}

int spi_cs_from_id(uint8_t cs_id, uint32_t *gpiox, uint16_t *pinx)
{
    const pup_dtm_spi_cs_t *cs = spi_cs(cs_id);

    if (cs && cs->gpio_base != 0U && cs->pin_mask != 0U) {
        *gpiox = cs->gpio_base;
        *pinx = cs->pin_mask;
        return 0;
    }
    return -1;
}

void spi2_gpio_init(void)
{
    const pup_dtm_spi_bus_t *bus = spi_bus();
    unsigned int i;

    if (!bus) {
        return;
    }

    for (i = 0; i < bus->pin_count && i < PUP_DTM_MAX_SPI_PINS; i++) {
        if (bus->pins[i].gpio_base == 0U || bus->pins[i].pin_mask == 0U) {
            continue;
        }
        dtm_gpio_set(bus->pins[i].gpio_base, bus->pins[i].pin_mask,
                     DTM_GPIO_MODE_AF, DTM_GPIO_OTYPE_PP,
                     DTM_GPIO_SPEED_HIGH, DTM_GPIO_PUPD_PU);
        dtm_gpio_af_set(bus->pins[i].gpio_base, bus->pins[i].pin_mask, bus->pins[i].af);
    }
}

int spi_bus_init(void)
{
    const pup_dtm_spi_bus_t *bus = spi_bus();
    dtm_spi_config_t spi_cfg;

    if (!bus || bus->base == 0U) {
        return -1;
    }

    if (g_spi_initialized) {
        return 0;
    }

    spi2_gpio_init();

    spi_cfg.mode = bus->default_mode;
    spi_cfg.datasize = bus->default_datasize;
    spi_cfg.baudrate_prescaler = bus->default_prescaler;
    spi_cfg.firstbit = bus->default_firstbit;
    spi_cfg.crc_polarity = 0;
    dtm_spi_init(bus->base, &spi_cfg);
    dtm_spi_enable(bus->base);

    g_spi_initialized = 1;
    printf("[driver] %s initialized\n", bus->name);
    return 0;
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

        dtm_gpio_set(cs_gpiox, cs_pinx, DTM_GPIO_MODE_OUT, DTM_GPIO_OTYPE_PP,
                     DTM_GPIO_SPEED_HIGH, DTM_GPIO_PUPD_PU);
        dtm_gpio_pin_set(cs_gpiox, cs_pinx, 1);

        cfg->slave_id = g_spi_slaves[slot].slave_id;

        return 0;
    }

    case PUP_SPI_IOCTL_TRANSFER: {
        pup_spi_transfer_t *xfer = (pup_spi_transfer_t *)req->data_buf;
        const pup_dtm_spi_bus_t *bus = spi_bus();
        spi_slave_t *slave = (spi_slave_t *)0;
        uint8_t *tx_data;
        uint8_t *rx_data;

        if (!bus || bus->base == 0U) {
            printf("[driver] spi: transfer: no SPI bus configured\n");
            return -1;
        }

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

        {
            dtm_spi_config_t spi_cfg;
            spi_cfg.mode = slave->mode;
            spi_cfg.datasize = (slave->datasize == 1) ? DTM_SPI_DATASIZE_16BIT : DTM_SPI_DATASIZE_8BIT;
            spi_cfg.baudrate_prescaler = slave->baudrate_prescaler;
            spi_cfg.firstbit = slave->firstbit;
            spi_cfg.crc_polarity = 0;
            dtm_spi_init(bus->base, &spi_cfg);
            dtm_spi_enable(bus->base);
        }

        if (tx_data && rx_data && xfer->len > 0) {
            dtm_gpio_pin_set(slave->cs_gpiox, slave->cs_pinx, 0);
            dtm_spi_transmit_receive(bus->base, tx_data, rx_data, xfer->len);
            dtm_gpio_pin_set(slave->cs_gpiox, slave->cs_pinx, 1);
        } else if (rx_data && xfer->len > 0) {
            dtm_gpio_pin_set(slave->cs_gpiox, slave->cs_pinx, 0);
            dtm_spi_receive(bus->base, rx_data, xfer->len);
            dtm_gpio_pin_set(slave->cs_gpiox, slave->cs_pinx, 1);
        } else if (tx_data && xfer->len > 0) {
            dtm_gpio_pin_set(slave->cs_gpiox, slave->cs_pinx, 0);
            dtm_spi_transmit(bus->base, tx_data, xfer->len);
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
        return spi_bus_init();

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
    const pup_dtm_spi_bus_t *bus = spi_bus();
    pup_user_devfs_registration_t reg;
    char name_buf[17];
    int i;

    if (!bus || !bus->valid || bus->name[0] == '\0') {
        return -1;
    }

    for (i = 0; i < 16 && bus->name[i] != '\0'; i++) {
        name_buf[i] = bus->name[i];
    }
    name_buf[i] = '\0';

    reg.name = name_buf;
    reg.flags = PUP_DRIVER_DEVICE_FLAG_NONE;
    reg.capabilities = PUP_DRIVER_CAP_NONE;
    reg.channel_id = g_server_channel;
    reg.device_id = bus->device_id;

    if (devfs_register(&reg) != 0) {
        printf("[driver] devfs_register failed for %s\n", bus->name);
        return -1;
    }

    printf("[driver] registered /dev/%s channel=%d devid=%d\n",
           bus->name, g_server_channel, bus->device_id);
    return 0;
}
