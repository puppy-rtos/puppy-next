#ifndef DRIVER_OF_H
#define DRIVER_OF_H

#include <stdint.h>
#include "dtm_of_driver.h"

int driver_of_init(void);
int driver_of_is_ready(void);
uint32_t driver_of_source(void);

void driver_of_copy_name(char *dst, const char *src);
const char *driver_of_node_base_name(const char *name);
int driver_of_prop_read_string(dtm_ofw_node_t node,
                               const char *name,
                               const char **out);
int driver_of_prop_has(dtm_ofw_node_t node, const char *name);
int driver_of_parse_gpio_spec(dtm_ofw_node_t node,
                              const char *prop,
                              int index,
                              uint32_t *out_gpio_base,
                              uint16_t *out_pin_mask,
                              uint32_t *out_flags);

#endif
