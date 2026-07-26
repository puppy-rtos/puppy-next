#include "libc.h"
#include "syscall.h"
#include "driver_of.h"

static uint32_t g_driver_of_source = PUP_DTM_SOURCE_FALLBACK;
static int g_driver_of_ready;

int driver_of_init(void)
{
    pup_dtm_blob_info_t info;
    int ret;

    g_driver_of_source = PUP_DTM_SOURCE_FALLBACK;
    g_driver_of_ready = 0;

    libcm_memset(&info, 0, sizeof(info));
    ret = dtm_query(PUP_DTM_QUERY_BLOB_INFO, &info, sizeof(info));
    if (ret != 0 ||
        info.source != PUP_DTM_SOURCE_DTB ||
        info.addr == 0U ||
        info.size == 0U ||
        (info.flags & PUP_DTM_BLOB_FLAG_READABLE) == 0U) {
        printf("[driver] no readable DTB mapping\n");
        return -1;
    }

    ret = dtm_ofw_init((const void *)info.addr);
    if (ret != DTM_OFW_OK) {
        printf("[driver] DTB parse failed ret=%d\n", ret);
        return -1;
    }

    g_driver_of_source = PUP_DTM_SOURCE_DTB;
    g_driver_of_ready = 1;
    printf("[driver] DTB mapped for user-mode driver probing: addr=0x%08x size=%u\n",
           (unsigned int)info.addr,
           (unsigned int)info.size);
    return 0;
}

int driver_of_is_ready(void)
{
    return g_driver_of_ready;
}

uint32_t driver_of_source(void)
{
    return g_driver_of_source;
}

void driver_of_copy_name(char *dst, const char *src)
{
    if (!dst) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }

    libcm_strncpy(dst, src, PUP_DTM_NAME_MAX - 1U);
    dst[PUP_DTM_NAME_MAX - 1U] = '\0';
}

const char *driver_of_node_base_name(const char *name)
{
    static char tmp[PUP_DTM_NAME_MAX];
    unsigned int i = 0;

    if (!name) {
        return "";
    }

    while (i < PUP_DTM_NAME_MAX - 1U && name[i] != '\0' && name[i] != '@') {
        tmp[i] = name[i];
        i++;
    }
    tmp[i] = '\0';
    return tmp;
}

int driver_of_prop_read_string(dtm_ofw_node_t node,
                               const char *name,
                               const char **out)
{
    return dtm_ofw_prop_read_string_index(node, name, 0, out);
}

int driver_of_prop_has(dtm_ofw_node_t node, const char *name)
{
    int len;
    return dtm_ofw_prop_read_raw(node, name, &len) != 0;
}

int driver_of_parse_gpio_spec(dtm_ofw_node_t node,
                              const char *prop,
                              int index,
                              uint32_t *out_gpio_base,
                              uint16_t *out_pin_mask,
                              uint32_t *out_flags)
{
    dtm_ofw_cell_args_t args;
    uint64_t addr = 0;
    uint16_t pin_mask;

    if (dtm_ofw_parse_phandle_cells(node, prop, "#gpio-cells", index, &args) < 0) {
        return -1;
    }

    if (args.args_count < 2U || dtm_ofw_get_reg(args.node, 0, &addr, 0) < 0) {
        return -1;
    }

    pin_mask = dtm_of_pin_num_to_mask(args.args[0]);
    if (pin_mask == 0U) {
        return -1;
    }

    if (out_gpio_base) {
        *out_gpio_base = (uint32_t)addr;
    }
    if (out_pin_mask) {
        *out_pin_mask = pin_mask;
    }
    if (out_flags) {
        *out_flags = args.args[1];
    }

    return 0;
}
