#include "dtm_of_driver.h"
#include "dtm_clk.h"
#include "libcm.h"

static int dtm_of_node_matches(const dtm_of_driver_t *driver,
                               dtm_ofw_node_t node)
{
    unsigned int i;

    if (!driver || !driver->compatibles || !driver->probe) {
        return 0;
    }

    for (i = 0; i < driver->compatible_count; i++) {
        if (driver->compatibles[i] &&
            dtm_ofw_node_is_compatible(node, driver->compatibles[i])) {
            return 1;
        }
    }

    return 0;
}

int dtm_of_probe_all(const dtm_of_driver_t *const *drivers,
                     unsigned int driver_count)
{
    dtm_ofw_node_t node = DTM_OFW_NODE_INVALID;
    int probe_count = 0;

    if (!dtm_ofw_is_ready() || !drivers) {
        return 0;
    }

    while ((node = dtm_ofw_get_next_node(node)) >= 0) {
        unsigned int i;

        if (!dtm_ofw_node_is_available(node)) {
            continue;
        }

        for (i = 0; i < driver_count; i++) {
            const dtm_of_driver_t *driver = drivers[i];

            if (!dtm_of_node_matches(driver, node)) {
                continue;
            }

            if (driver->probe(node) == 0) {
                probe_count++;
            }
            break;
        }
    }

    return probe_count;
}

uint16_t dtm_of_pin_num_to_mask(uint32_t pin_num)
{
    if (pin_num >= 16U) {
        return 0U;
    }
    return (uint16_t)(1U << pin_num);
}

int dtm_of_parse_clock_id(dtm_ofw_node_t node,
                          const char *prop,
                          int index,
                          uint32_t *out_clk_id)
{
    dtm_ofw_cell_args_t args;
    uint32_t clk_id;

    if (!out_clk_id || !prop) {
        return -1;
    }

    if (dtm_ofw_parse_phandle_cells(node, prop, "#clock-cells", index, &args) < 0) {
        return -1;
    }

    if (args.args_count > 0U) {
        clk_id = args.args[0];
    } else if (dtm_ofw_prop_read_u32(args.node, "puppy,clock-id", &clk_id) < 0) {
        return -1;
    }

    if (clk_id >= CLK_COUNT) {
        return -1;
    }

    *out_clk_id = clk_id;
    return 0;
}

static int dtm_of_parse_pinmux_node(dtm_ofw_node_t pin_node,
                                    pup_dtm_pin_t *pins,
                                    uint32_t max_pins)
{
    int count;
    int i;
    int out = 0;

    if (!pins || max_pins == 0U) {
        return 0;
    }

    count = dtm_ofw_prop_count_u32(pin_node, "puppy,pins");
    if (count < 0 || (count % 3) != 0) {
        return 0;
    }

    for (i = 0; i + 2 < count && out < (int)max_pins; i += 3) {
        uint32_t gpio_base;
        uint32_t pin_num;
        uint32_t af;

        if (dtm_ofw_prop_read_u32_index(pin_node, "puppy,pins", i, &gpio_base) < 0 ||
            dtm_ofw_prop_read_u32_index(pin_node, "puppy,pins", i + 1, &pin_num) < 0 ||
            dtm_ofw_prop_read_u32_index(pin_node, "puppy,pins", i + 2, &af) < 0) {
            continue;
        }

        pins[out].gpio_base = gpio_base;
        pins[out].pin_mask = dtm_of_pin_num_to_mask(pin_num);
        pins[out].af = (uint8_t)af;
        pins[out].reserved = 0;
        if (pins[out].pin_mask != 0U) {
            out++;
        }
    }

    return out;
}

int dtm_of_parse_pinctrl(dtm_ofw_node_t node,
                         pup_dtm_pin_t *pins,
                         uint32_t max_pins)
{
    dtm_ofw_cell_args_t args;

    if (dtm_ofw_parse_phandle_cells(node, "pinctrl-0", 0, 0, &args) < 0) {
        return 0;
    }

    return dtm_of_parse_pinmux_node(args.node, pins, max_pins);
}
