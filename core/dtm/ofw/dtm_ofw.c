#include "dtm_ofw.h"
#include "libcm.h"

#include <stdint.h>
#include <stddef.h>

#define FDT_MAGIC             0xd00dfeedUL
#define FDT_BEGIN_NODE        0x00000001UL
#define FDT_END_NODE          0x00000002UL
#define FDT_PROP              0x00000003UL
#define FDT_NOP               0x00000004UL
#define FDT_END               0x00000009UL

#ifndef DTM_OFW_MAX_NODES
#define DTM_OFW_MAX_NODES     64
#endif

#ifndef DTM_OFW_MAX_PROPS
#define DTM_OFW_MAX_PROPS     192
#endif

#ifndef DTM_OFW_MAX_DEPTH
#define DTM_OFW_MAX_DEPTH     32
#endif

typedef struct {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
} dtm_fdt_header_t;

typedef struct {
    const char *name;
    const void *value;
    int len;
    int next;
} dtm_ofw_prop_desc_t;

typedef struct {
    const char *name;
    int parent;
    int child;
    int sibling;
    int first_prop;
    uint32_t phandle;
} dtm_ofw_node_desc_t;

static const uint8_t *g_dtb;
static uint32_t g_dtb_size;
static const uint8_t *g_struct;
static uint32_t g_struct_size;
static const char *g_strings;
static uint32_t g_strings_size;

static dtm_ofw_node_desc_t g_nodes[DTM_OFW_MAX_NODES];
static dtm_ofw_prop_desc_t g_props[DTM_OFW_MAX_PROPS];
static int g_node_count;
static int g_prop_count;
static int g_ready;

extern const unsigned char __dtm_dtb_blob[] __attribute__((weak));
extern const unsigned int __dtm_dtb_blob_size __attribute__((weak));

static uint32_t fdt32_to_cpu(const void *ptr)
{
    const uint8_t *p = (const uint8_t *)ptr;

    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           ((uint32_t)p[3]);
}

static uint64_t fdt_read_number(const uint32_t *cells, int count)
{
    uint64_t value = 0;

    while (count-- > 0) {
        value = (value << 32) | fdt32_to_cpu(cells++);
    }

    return value;
}

static uintptr_t align4(uintptr_t value)
{
    return (value + 3U) & ~(uintptr_t)3U;
}

static int bounded_cstr_len(const char *str, const char *end)
{
    const char *p = str;

    while (p < end && *p != '\0') {
        p++;
    }

    if (p >= end) {
        return DTM_OFW_EBADF;
    }

    return (int)(p - str);
}

static int range_inside(uint32_t total_size,
                        uint32_t off,
                        uint32_t size)
{
    return off <= total_size && size <= total_size - off;
}

static int node_valid(dtm_ofw_node_t node)
{
    return node >= 0 && node < g_node_count;
}

static int prop_name_equal(const dtm_ofw_prop_desc_t *prop, const char *name)
{
    return prop->name && name && libcm_strcmp(prop->name, name) == 0;
}

static int str_prefix_len(const char *str, const char *prefix, int prefix_len)
{
    int i;

    if (!str || !prefix || prefix_len < 0) {
        return 0;
    }

    for (i = 0; i < prefix_len; i++) {
        if (str[i] == '\0' || str[i] != prefix[i]) {
            return 0;
        }
    }

    return 1;
}

static int node_name_equal_len(dtm_ofw_node_t node,
                               const char *name,
                               int name_len)
{
    const char *node_name;
    int i;

    if (!node_valid(node) || !name || name_len < 0) {
        return 0;
    }

    node_name = g_nodes[node].name;
    if (!node_name) {
        return 0;
    }

    for (i = 0; i < name_len; i++) {
        if (node_name[i] == '\0' || node_name[i] != name[i]) {
            return 0;
        }
    }

    return node_name[i] == '\0';
}

static dtm_ofw_prop_desc_t *get_prop_desc(dtm_ofw_node_t node,
                                          const char *name)
{
    int prop;

    if (!g_ready || !node_valid(node) || !name) {
        return NULL;
    }

    for (prop = g_nodes[node].first_prop; prop >= 0; prop = g_props[prop].next) {
        if (prop_name_equal(&g_props[prop], name)) {
            return &g_props[prop];
        }
    }

    return NULL;
}

static int prop_string_index(const dtm_ofw_prop_desc_t *prop,
                             const char *string)
{
    const char *value;
    int index = 0;
    int remaining;

    if (!prop || !string || prop->len <= 0) {
        return DTM_OFW_ENOENT;
    }

    value = (const char *)prop->value;
    remaining = prop->len;

    while (remaining > 0) {
        size_t len = libcm_strlen(value);

        if ((int)len >= remaining) {
            return DTM_OFW_EBADF;
        }

        if (libcm_strcmp(value, string) == 0) {
            return index;
        }

        remaining -= (int)len + 1;
        value += len + 1;
        index++;
    }

    return DTM_OFW_ENOENT;
}

static int append_node(int parent, const char *name)
{
    int node;
    int child;

    if (g_node_count >= DTM_OFW_MAX_NODES) {
        return DTM_OFW_ENOSPC;
    }

    node = g_node_count++;
    g_nodes[node].name = name;
    g_nodes[node].parent = parent;
    g_nodes[node].child = DTM_OFW_NODE_INVALID;
    g_nodes[node].sibling = DTM_OFW_NODE_INVALID;
    g_nodes[node].first_prop = -1;
    g_nodes[node].phandle = 0;

    if (parent >= 0) {
        child = g_nodes[parent].child;
        if (child < 0) {
            g_nodes[parent].child = node;
        } else {
            while (g_nodes[child].sibling >= 0) {
                child = g_nodes[child].sibling;
            }
            g_nodes[child].sibling = node;
        }
    }

    return node;
}

static int append_prop(dtm_ofw_node_t node,
                       const char *name,
                       const void *value,
                       int len)
{
    int prop;

    if (!node_valid(node) || !name || g_prop_count >= DTM_OFW_MAX_PROPS) {
        return DTM_OFW_ENOSPC;
    }

    prop = g_prop_count++;
    g_props[prop].name = name;
    g_props[prop].value = value;
    g_props[prop].len = len;
    g_props[prop].next = g_nodes[node].first_prop;
    g_nodes[node].first_prop = prop;

    if ((libcm_strcmp(name, "phandle") == 0 ||
         libcm_strcmp(name, "linux,phandle") == 0) &&
        len >= 4) {
        g_nodes[node].phandle = fdt32_to_cpu(value);
    }

    return DTM_OFW_OK;
}

static int parse_struct_block(void)
{
    const uint8_t *ptr = g_struct;
    const uint8_t *end = g_struct + g_struct_size;
    int stack[DTM_OFW_MAX_DEPTH];
    int depth = 0;

    while (ptr + 4 <= end) {
        uint32_t tag = fdt32_to_cpu(ptr);
        ptr += 4;

        switch (tag) {
        case FDT_BEGIN_NODE: {
            const char *name = (const char *)ptr;
            int name_len = bounded_cstr_len(name, (const char *)end);
            int parent;
            int node;

            if (name_len < 0) {
                return name_len;
            }

            ptr = (const uint8_t *)align4((uintptr_t)(ptr + name_len + 1));
            if (ptr > end || depth >= DTM_OFW_MAX_DEPTH) {
                return DTM_OFW_EBADF;
            }

            parent = depth > 0 ? stack[depth - 1] : DTM_OFW_NODE_INVALID;
            node = append_node(parent, name);
            if (node < 0) {
                return node;
            }

            stack[depth++] = node;
            break;
        }
        case FDT_END_NODE:
            if (depth <= 0) {
                return DTM_OFW_EBADF;
            }
            depth--;
            break;

        case FDT_PROP: {
            uint32_t len;
            uint32_t nameoff;
            const char *name;
            const uint8_t *value;

            if (ptr + 8 > end || depth <= 0) {
                return DTM_OFW_EBADF;
            }

            len = fdt32_to_cpu(ptr);
            nameoff = fdt32_to_cpu(ptr + 4);
            ptr += 8;

            if (nameoff >= g_strings_size || len > (uint32_t)(end - ptr)) {
                return DTM_OFW_EBADF;
            }

            name = g_strings + nameoff;
            if (bounded_cstr_len(name, g_strings + g_strings_size) < 0) {
                return DTM_OFW_EBADF;
            }

            value = ptr;
            ptr = (const uint8_t *)align4((uintptr_t)(ptr + len));
            if (ptr > end) {
                return DTM_OFW_EBADF;
            }

            if (append_prop(stack[depth - 1], name, value, (int)len) < 0) {
                return DTM_OFW_ENOSPC;
            }
            break;
        }
        case FDT_NOP:
            break;

        case FDT_END:
            return depth == 0 ? DTM_OFW_OK : DTM_OFW_EBADF;

        default:
            return DTM_OFW_EBADF;
        }
    }

    return DTM_OFW_EBADF;
}

int dtm_ofw_init(const void *dtb)
{
    const uint8_t *base = (const uint8_t *)dtb;
    const dtm_fdt_header_t *hdr = (const dtm_fdt_header_t *)dtb;
    uint32_t totalsize;
    uint32_t off_struct;
    uint32_t off_strings;
    uint32_t size_struct;
    uint32_t size_strings;
    int err;

    g_ready = 0;
    g_node_count = 0;
    g_prop_count = 0;
    g_dtb = NULL;
    g_dtb_size = 0;

    if (!dtb || fdt32_to_cpu(&hdr->magic) != FDT_MAGIC) {
        return DTM_OFW_EBADF;
    }

    totalsize = fdt32_to_cpu(&hdr->totalsize);
    off_struct = fdt32_to_cpu(&hdr->off_dt_struct);
    off_strings = fdt32_to_cpu(&hdr->off_dt_strings);
    size_struct = fdt32_to_cpu(&hdr->size_dt_struct);
    size_strings = fdt32_to_cpu(&hdr->size_dt_strings);

    if (totalsize < sizeof(dtm_fdt_header_t) ||
        !range_inside(totalsize, off_struct, size_struct) ||
        !range_inside(totalsize, off_strings, size_strings)) {
        return DTM_OFW_EBADF;
    }

    g_dtb = base;
    g_dtb_size = totalsize;
    g_struct = base + off_struct;
    g_struct_size = size_struct;
    g_strings = (const char *)(base + off_strings);
    g_strings_size = size_strings;

    err = parse_struct_block();
    if (err < 0) {
        g_node_count = 0;
        g_prop_count = 0;
        return err;
    }

    if (g_node_count <= 0) {
        return DTM_OFW_EBADF;
    }

    g_ready = 1;
    return DTM_OFW_OK;
}

int dtm_ofw_init_default(void)
{
    if (&__dtm_dtb_blob == NULL || &__dtm_dtb_blob_size == NULL ||
        __dtm_dtb_blob_size < sizeof(dtm_fdt_header_t)) {
        return DTM_OFW_ENOENT;
    }

    return dtm_ofw_init(__dtm_dtb_blob);
}

int dtm_ofw_is_ready(void)
{
    return g_ready;
}

const void *dtm_ofw_blob(void)
{
    return g_ready ? g_dtb : NULL;
}

uint32_t dtm_ofw_blob_size(void)
{
    return g_ready ? g_dtb_size : 0U;
}

dtm_ofw_node_t dtm_ofw_root(void)
{
    return g_ready && g_node_count > 0 ? 0 : DTM_OFW_NODE_INVALID;
}

dtm_ofw_node_t dtm_ofw_get_parent(dtm_ofw_node_t node)
{
    return node_valid(node) ? g_nodes[node].parent : DTM_OFW_NODE_INVALID;
}

dtm_ofw_node_t dtm_ofw_get_next_node(dtm_ofw_node_t prev)
{
    if (!g_ready || g_node_count <= 0) {
        return DTM_OFW_NODE_INVALID;
    }

    if (prev == DTM_OFW_NODE_INVALID) {
        return 0;
    }

    if (!node_valid(prev) || prev + 1 >= g_node_count) {
        return DTM_OFW_NODE_INVALID;
    }

    return prev + 1;
}

dtm_ofw_node_t dtm_ofw_get_next_child(dtm_ofw_node_t parent,
                                      dtm_ofw_node_t prev)
{
    if (!node_valid(parent)) {
        return DTM_OFW_NODE_INVALID;
    }

    if (prev == DTM_OFW_NODE_INVALID) {
        return g_nodes[parent].child;
    }

    if (!node_valid(prev) || g_nodes[prev].parent != parent) {
        return DTM_OFW_NODE_INVALID;
    }

    return g_nodes[prev].sibling;
}

const char *dtm_ofw_node_name(dtm_ofw_node_t node)
{
    return node_valid(node) ? g_nodes[node].name : NULL;
}

int dtm_ofw_node_is_available(dtm_ofw_node_t node)
{
    const char *status = dtm_ofw_prop_read_raw(node, "status", NULL);

    if (!node_valid(node)) {
        return 0;
    }

    return !status ||
           libcm_strcmp(status, "ok") == 0 ||
           libcm_strcmp(status, "okay") == 0;
}

int dtm_ofw_node_is_compatible(dtm_ofw_node_t node,
                               const char *compatible)
{
    return dtm_ofw_prop_index_of_string(node, "compatible", compatible) >= 0;
}

static dtm_ofw_node_t find_node_by_path_len(const char *path, int path_len)
{
    dtm_ofw_node_t node;
    int pos;

    if (!g_ready || !path || path_len <= 0 || path[0] != '/') {
        return DTM_OFW_NODE_INVALID;
    }

    node = 0;
    pos = 1;

    while (pos < path_len) {
        int start;
        int len;
        dtm_ofw_node_t child;

        while (pos < path_len && path[pos] == '/') {
            pos++;
        }
        if (pos >= path_len) {
            break;
        }

        start = pos;
        while (pos < path_len && path[pos] != '/') {
            pos++;
        }
        len = pos - start;

        child = g_nodes[node].child;
        while (child >= 0) {
            if (node_name_equal_len(child, path + start, len)) {
                break;
            }
            child = g_nodes[child].sibling;
        }

        if (child < 0) {
            return DTM_OFW_NODE_INVALID;
        }
        node = child;
    }

    return node;
}

dtm_ofw_node_t dtm_ofw_find_node_by_path(const char *path)
{
    if (!path) {
        return DTM_OFW_NODE_INVALID;
    }

    return find_node_by_path_len(path, (int)libcm_strlen(path));
}

dtm_ofw_node_t dtm_ofw_find_node_by_phandle(uint32_t phandle)
{
    int i;

    if (!g_ready || phandle == 0) {
        return DTM_OFW_NODE_INVALID;
    }

    for (i = 0; i < g_node_count; i++) {
        if (g_nodes[i].phandle == phandle) {
            return i;
        }
    }

    return DTM_OFW_NODE_INVALID;
}

dtm_ofw_node_t dtm_ofw_find_node_by_compatible(dtm_ofw_node_t from,
                                               const char *compatible)
{
    dtm_ofw_node_t node;

    if (!g_ready || !compatible) {
        return DTM_OFW_NODE_INVALID;
    }

    node = dtm_ofw_get_next_node(from);
    while (node >= 0) {
        if (dtm_ofw_node_is_available(node) &&
            dtm_ofw_node_is_compatible(node, compatible)) {
            return node;
        }
        node = dtm_ofw_get_next_node(node);
    }

    return DTM_OFW_NODE_INVALID;
}

static dtm_ofw_node_t get_alias_node_by_tag_len(const char *tag,
                                                int tag_len,
                                                int id)
{
    dtm_ofw_node_t aliases;
    int prop;

    if (!tag || tag_len <= 0 || id < 0) {
        return DTM_OFW_NODE_INVALID;
    }

    aliases = dtm_ofw_find_node_by_path("/aliases");
    if (aliases < 0) {
        return DTM_OFW_NODE_INVALID;
    }

    for (prop = g_nodes[aliases].first_prop; prop >= 0; prop = g_props[prop].next) {
        const char *name = g_props[prop].name;
        int suffix = 0;
        int i;

        if (!str_prefix_len(name, tag, tag_len)) {
            continue;
        }

        i = tag_len;
        if (name[i] == '\0') {
            continue;
        }

        while (name[i] >= '0' && name[i] <= '9') {
            suffix = suffix * 10 + name[i] - '0';
            i++;
        }

        if (name[i] == '\0' && suffix == id && g_props[prop].len > 0) {
            return dtm_ofw_find_node_by_path((const char *)g_props[prop].value);
        }
    }

    return DTM_OFW_NODE_INVALID;
}

dtm_ofw_node_t dtm_ofw_get_alias_node(const char *tag, int id)
{
    if (!tag) {
        return DTM_OFW_NODE_INVALID;
    }

    return get_alias_node_by_tag_len(tag, (int)libcm_strlen(tag), id);
}

static dtm_ofw_node_t resolve_path_or_alias(const char *value)
{
    int len = 0;

    if (!value) {
        return DTM_OFW_NODE_INVALID;
    }

    while (value[len] != '\0' && value[len] != ':') {
        len++;
    }

    if (len <= 0) {
        return DTM_OFW_NODE_INVALID;
    }

    if (value[0] == '/') {
        return find_node_by_path_len(value, len);
    }

    {
        int tag_len = len;
        int digit;
        int id = 0;

        while (tag_len > 0 &&
               value[tag_len - 1] >= '0' &&
               value[tag_len - 1] <= '9') {
            tag_len--;
        }

        if (tag_len == len) {
            return DTM_OFW_NODE_INVALID;
        }

        for (digit = tag_len; digit < len; digit++) {
            id = id * 10 + value[digit] - '0';
        }

        return get_alias_node_by_tag_len(value, tag_len, id);
    }
}

dtm_ofw_node_t dtm_ofw_chosen_stdout(void)
{
    dtm_ofw_node_t chosen;
    const char *path;

    chosen = dtm_ofw_find_node_by_path("/chosen");
    if (chosen < 0) {
        return DTM_OFW_NODE_INVALID;
    }

    path = dtm_ofw_prop_read_raw(chosen, "stdout-path", NULL);
    if (!path) {
        path = dtm_ofw_prop_read_raw(chosen, "linux,stdout-path", NULL);
    }

    return resolve_path_or_alias(path);
}

const void *dtm_ofw_prop_read_raw(dtm_ofw_node_t node,
                                  const char *name,
                                  int *out_len)
{
    dtm_ofw_prop_desc_t *prop = get_prop_desc(node, name);

    if (!prop) {
        if (out_len) {
            *out_len = DTM_OFW_ENOENT;
        }
        return NULL;
    }

    if (out_len) {
        *out_len = prop->len;
    }

    return prop->value;
}

int dtm_ofw_prop_read_u32_index(dtm_ofw_node_t node,
                                const char *name,
                                int index,
                                uint32_t *out_value)
{
    int len;
    const uint32_t *cells;

    if (index < 0 || !out_value) {
        return DTM_OFW_EINVAL;
    }

    cells = dtm_ofw_prop_read_raw(node, name, &len);
    if (!cells) {
        return DTM_OFW_ENOENT;
    }

    if (len < (index + 1) * (int)sizeof(uint32_t)) {
        return DTM_OFW_EINVAL;
    }

    *out_value = fdt32_to_cpu(&cells[index]);
    return DTM_OFW_OK;
}

int dtm_ofw_prop_read_u32(dtm_ofw_node_t node,
                          const char *name,
                          uint32_t *out_value)
{
    return dtm_ofw_prop_read_u32_index(node, name, 0, out_value);
}

int dtm_ofw_prop_read_u32_array(dtm_ofw_node_t node,
                                const char *name,
                                uint32_t *out_values,
                                int count)
{
    int i;

    if (!out_values || count <= 0) {
        return DTM_OFW_EINVAL;
    }

    for (i = 0; i < count; i++) {
        int err = dtm_ofw_prop_read_u32_index(node, name, i, &out_values[i]);
        if (err < 0) {
            return err;
        }
    }

    return count;
}

int dtm_ofw_prop_count_u32(dtm_ofw_node_t node, const char *name)
{
    int len;

    if (!dtm_ofw_prop_read_raw(node, name, &len)) {
        return DTM_OFW_ENOENT;
    }

    if (len < 0 || (len % (int)sizeof(uint32_t)) != 0) {
        return DTM_OFW_EBADF;
    }

    return len / (int)sizeof(uint32_t);
}

int dtm_ofw_prop_read_string_index(dtm_ofw_node_t node,
                                   const char *name,
                                   int index,
                                   const char **out_string)
{
    dtm_ofw_prop_desc_t *prop;
    const char *value;
    int remaining;
    int current = 0;

    if (!out_string || index < 0) {
        return DTM_OFW_EINVAL;
    }

    prop = get_prop_desc(node, name);
    if (!prop) {
        return DTM_OFW_ENOENT;
    }

    value = (const char *)prop->value;
    remaining = prop->len;

    while (remaining > 0) {
        size_t len = libcm_strlen(value);

        if ((int)len >= remaining) {
            return DTM_OFW_EBADF;
        }

        if (current == index) {
            *out_string = value;
            return DTM_OFW_OK;
        }

        remaining -= (int)len + 1;
        value += len + 1;
        current++;
    }

    return DTM_OFW_ENOENT;
}

int dtm_ofw_prop_index_of_string(dtm_ofw_node_t node,
                                 const char *name,
                                 const char *string)
{
    return prop_string_index(get_prop_desc(node, name), string);
}

int dtm_ofw_bus_addr_cells(dtm_ofw_node_t node)
{
    uint32_t cells;

    while (node_valid(node)) {
        if (dtm_ofw_prop_read_u32(node, "#address-cells", &cells) == 0) {
            return (int)cells;
        }
        node = g_nodes[node].parent;
    }

    return 1;
}

int dtm_ofw_bus_size_cells(dtm_ofw_node_t node)
{
    uint32_t cells;

    while (node_valid(node)) {
        if (dtm_ofw_prop_read_u32(node, "#size-cells", &cells) == 0) {
            return (int)cells;
        }
        node = g_nodes[node].parent;
    }

    return 1;
}

int dtm_ofw_get_reg(dtm_ofw_node_t node,
                    int index,
                    uint64_t *out_addr,
                    uint64_t *out_size)
{
    dtm_ofw_node_t parent;
    int addr_cells;
    int size_cells;
    int entry_cells;
    int count;
    int len;
    const uint32_t *cells;

    if (!node_valid(node) || index < 0 || (!out_addr && !out_size)) {
        return DTM_OFW_EINVAL;
    }

    cells = dtm_ofw_prop_read_raw(node, "reg", &len);
    if (!cells) {
        return DTM_OFW_ENOENT;
    }

    parent = g_nodes[node].parent;
    addr_cells = parent >= 0 ? dtm_ofw_bus_addr_cells(parent) : dtm_ofw_bus_addr_cells(node);
    size_cells = parent >= 0 ? dtm_ofw_bus_size_cells(parent) : dtm_ofw_bus_size_cells(node);
    entry_cells = addr_cells + size_cells;

    if (entry_cells <= 0 || addr_cells > 2 || size_cells > 2 ||
        len % (entry_cells * (int)sizeof(uint32_t)) != 0) {
        return DTM_OFW_EBADF;
    }

    count = len / (entry_cells * (int)sizeof(uint32_t));
    if (index >= count) {
        return DTM_OFW_ENOENT;
    }

    cells += index * entry_cells;

    if (out_addr) {
        *out_addr = fdt_read_number(cells, addr_cells);
    }
    if (out_size) {
        *out_size = fdt_read_number(cells + addr_cells, size_cells);
    }

    return DTM_OFW_OK;
}

int dtm_ofw_parse_phandle_cells(dtm_ofw_node_t node,
                                const char *list_name,
                                const char *cells_name,
                                int index,
                                dtm_ofw_cell_args_t *out_args)
{
    const uint32_t *cells;
    int len;
    int count;
    int pos = 0;
    int current = 0;

    if (!node_valid(node) || !list_name || index < 0 || !out_args) {
        return DTM_OFW_EINVAL;
    }

    cells = dtm_ofw_prop_read_raw(node, list_name, &len);
    if (!cells) {
        return DTM_OFW_ENOENT;
    }

    if (len < 0 || (len % (int)sizeof(uint32_t)) != 0) {
        return DTM_OFW_EBADF;
    }

    count = len / (int)sizeof(uint32_t);
    while (pos < count) {
        uint32_t phandle = fdt32_to_cpu(&cells[pos++]);
        dtm_ofw_node_t ref = dtm_ofw_find_node_by_phandle(phandle);
        uint32_t arg_count = 0;

        if (ref < 0) {
            return DTM_OFW_ENOENT;
        }

        if (cells_name &&
            dtm_ofw_prop_read_u32(ref, cells_name, &arg_count) < 0) {
            arg_count = 0;
        }

        if (arg_count > DTM_OFW_MAX_CELL_ARGS ||
            pos + (int)arg_count > count) {
            return DTM_OFW_EBADF;
        }

        if (current == index) {
            uint32_t i;

            out_args->node = ref;
            out_args->args_count = arg_count;
            for (i = 0; i < arg_count; i++) {
                out_args->args[i] = fdt32_to_cpu(&cells[pos + (int)i]);
            }
            return DTM_OFW_OK;
        }

        pos += (int)arg_count;
        current++;
    }

    return DTM_OFW_ENOENT;
}
