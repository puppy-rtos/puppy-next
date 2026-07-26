#ifndef __DTM_OF_DRIVER_H__
#define __DTM_OF_DRIVER_H__

#include <stdint.h>
#include "dtm_ofw.h"
#include "dtm_resource.h"

typedef struct {
    const char *name;
    const char *const *compatibles;
    unsigned int compatible_count;
    int (*probe)(dtm_ofw_node_t node);
} dtm_of_driver_t;

#define DTM_ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

int dtm_of_probe_all(const dtm_of_driver_t *const *drivers,
                     unsigned int driver_count);

uint16_t dtm_of_pin_num_to_mask(uint32_t pin_num);
int dtm_of_parse_clock_id(dtm_ofw_node_t node,
                          const char *prop,
                          int index,
                          uint32_t *out_clk_id);
int dtm_of_parse_pinctrl(dtm_ofw_node_t node,
                         pup_dtm_pin_t *pins,
                         uint32_t max_pins);

#endif
