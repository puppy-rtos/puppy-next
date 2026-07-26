#ifndef __DTM_OFW_H__
#define __DTM_OFW_H__

#include <stdint.h>
#include <stddef.h>

#define DTM_OFW_NODE_INVALID     (-1)
#define DTM_OFW_MAX_CELL_ARGS    8

#define DTM_OFW_OK               0
#define DTM_OFW_EINVAL           (-1)
#define DTM_OFW_ENOENT           (-2)
#define DTM_OFW_EBADF            (-3)
#define DTM_OFW_ENOSPC           (-4)

typedef int32_t dtm_ofw_node_t;

typedef struct {
    dtm_ofw_node_t node;
    uint32_t args_count;
    uint32_t args[DTM_OFW_MAX_CELL_ARGS];
} dtm_ofw_cell_args_t;

int dtm_ofw_init(const void *dtb);
int dtm_ofw_init_default(void);
int dtm_ofw_is_ready(void);
const void *dtm_ofw_blob(void);
uint32_t dtm_ofw_blob_size(void);

dtm_ofw_node_t dtm_ofw_root(void);
dtm_ofw_node_t dtm_ofw_get_parent(dtm_ofw_node_t node);
dtm_ofw_node_t dtm_ofw_get_next_node(dtm_ofw_node_t prev);
dtm_ofw_node_t dtm_ofw_get_next_child(dtm_ofw_node_t parent,
                                      dtm_ofw_node_t prev);

const char *dtm_ofw_node_name(dtm_ofw_node_t node);
int dtm_ofw_node_is_available(dtm_ofw_node_t node);
int dtm_ofw_node_is_compatible(dtm_ofw_node_t node,
                               const char *compatible);

dtm_ofw_node_t dtm_ofw_find_node_by_path(const char *path);
dtm_ofw_node_t dtm_ofw_find_node_by_phandle(uint32_t phandle);
dtm_ofw_node_t dtm_ofw_find_node_by_compatible(dtm_ofw_node_t from,
                                               const char *compatible);
dtm_ofw_node_t dtm_ofw_get_alias_node(const char *tag, int id);
dtm_ofw_node_t dtm_ofw_chosen_stdout(void);

const void *dtm_ofw_prop_read_raw(dtm_ofw_node_t node,
                                  const char *name,
                                  int *out_len);
int dtm_ofw_prop_read_u32_index(dtm_ofw_node_t node,
                                const char *name,
                                int index,
                                uint32_t *out_value);
int dtm_ofw_prop_read_u32(dtm_ofw_node_t node,
                          const char *name,
                          uint32_t *out_value);
int dtm_ofw_prop_read_u32_array(dtm_ofw_node_t node,
                                const char *name,
                                uint32_t *out_values,
                                int count);
int dtm_ofw_prop_count_u32(dtm_ofw_node_t node, const char *name);
int dtm_ofw_prop_read_string_index(dtm_ofw_node_t node,
                                   const char *name,
                                   int index,
                                   const char **out_string);
int dtm_ofw_prop_index_of_string(dtm_ofw_node_t node,
                                 const char *name,
                                 const char *string);

int dtm_ofw_bus_addr_cells(dtm_ofw_node_t node);
int dtm_ofw_bus_size_cells(dtm_ofw_node_t node);
int dtm_ofw_get_reg(dtm_ofw_node_t node,
                    int index,
                    uint64_t *out_addr,
                    uint64_t *out_size);
int dtm_ofw_parse_phandle_cells(dtm_ofw_node_t node,
                                const char *list_name,
                                const char *cells_name,
                                int index,
                                dtm_ofw_cell_args_t *out_args);

#endif
