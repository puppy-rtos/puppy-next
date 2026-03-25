#include "app_loader.h"
#include "app_manager.h"
#include "libcm.h"
#include <puppy_core.h>
#include "mpu.h"
#include "puppy_log.h"
#include "kheap.h"

#ifndef ALIGN_UP
#define ALIGN_UP(size, align)   (((size) + (align) - 1) & ~((align) - 1))
#endif

user_app_t loaded_app;
static volatile int user_app_exit_code = 0;
static volatile int user_app_should_exit = 0;

static unsigned int relocate_runtime_address(unsigned int value,
                                             unsigned int gs, unsigned int ge,
                                             unsigned int ts, unsigned int tbase,
                                             unsigned int ram_start, unsigned int ram_end,
                                             unsigned int runtime_ram_base,
                                             unsigned int runtime_got_base)
{
    if (value >= gs && value < ge)
    {
        return runtime_got_base + (value - gs);
    }

    if (value >= ram_start && value < ram_end)
    {
        return runtime_ram_base + (value - ram_start);
    }

    if (value >= ts)
    {
        return tbase + (value - ts);
    }

    return value;
}

static unsigned int *resolve_runtime_pointer(unsigned int address,
                                             unsigned int gs, unsigned int ge,
                                             unsigned int ts, unsigned int tbase,
                                             unsigned int ram_start, unsigned int ram_end,
                                             unsigned int runtime_ram_base,
                                             unsigned int runtime_got_base)
{
    if (address >= gs && address < ge)
    {
        return (unsigned int *)(runtime_got_base + (address - gs));
    }

    if (address >= ram_start && address < ram_end)
    {
        return (unsigned int *)(runtime_ram_base + (address - ram_start));
    }

    if (address >= ts)
    {
        /*
         * Do not write relocations back to the flash image (.text/.rodata/
         * app header). Writing there would mutate the on-flash app metadata,
         * causing subsequent launches to parse corrupted header fields.
         */
        return NULL;
    }

    if (address >= PUP_RAM_ADDRESS_START && address < PUP_RAM_ADDRESS_END)
    {
        return (unsigned int *)address;
    }

    return NULL;
}

void kernel_reloc(unsigned int *gbase, unsigned int tbase, 
                  unsigned int gs, unsigned int ge, unsigned int ts,
                  unsigned int ram_start, unsigned int ram_end,
                  unsigned int runtime_ram_base,
                  unsigned int *rel_start, unsigned int *rel_end)
{
    int i;
    unsigned int s;
    unsigned int got_base_loaded = (unsigned int)gbase;
    unsigned int got_end_loaded = got_base_loaded + (ge - gs);

    PUP_LOG_DEBUG(PUP_LOG_MODULE_RELOC, "gbase=0x%08x, tbase=0x%08x", (unsigned int)gbase, tbase);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_RELOC, "gs=0x%08x, ge=0x%08x, ts=0x%08x", gs, ge, ts);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_RELOC, "GOT loaded range: 0x%08x - 0x%08x", got_base_loaded, got_end_loaded);
    
    if (rel_start && rel_end && rel_end > rel_start)
    {
        PUP_LOG_DEBUG(PUP_LOG_MODULE_RELOC, "Processing .rel.dyn segment...");
        PUP_LOG_DEBUG(PUP_LOG_MODULE_RELOC, "rel_start=0x%08x, rel_end=0x%08x", 
                   (unsigned int)rel_start, (unsigned int)rel_end);
        
        for (i = 0; i < (int)((rel_end - rel_start) / 2); i++)
        {
            unsigned int rel_addr = rel_start[i * 2];
            unsigned int rel_type = rel_start[i * 2 + 1];
            
            if (rel_type == 0x00000017)
            {
                unsigned int *pointer;
                unsigned int value;
                
                if (rel_addr >= gs && rel_addr < ge)
                {
                    PUP_LOG_DEBUG(PUP_LOG_MODULE_RELOC, "SKIP GOT entry at 0x%08x (will be handled by GOT reloc)", rel_addr);
                    continue;
                }
                pointer = resolve_runtime_pointer(rel_addr,
                                                 gs, ge,
                                                 ts, tbase,
                                                 ram_start, ram_end,
                                                 runtime_ram_base,
                                                 got_base_loaded);
                if (!pointer)
                {
                    PUP_LOG_DEBUG(PUP_LOG_MODULE_RELOC, "Skip unsupported reloc pointer 0x%08x", rel_addr);
                    continue;
                }
                
                value = *pointer;
                
                PUP_LOG_DEBUG(PUP_LOG_MODULE_RELOC, "Relocating pointer at 0x%08x (value=0x%08x)", 
                           (unsigned int)pointer, value);
                
                *pointer = relocate_runtime_address(value,
                                                    gs, ge,
                                                    ts, tbase,
                                                    ram_start, ram_end,
                                                    runtime_ram_base,
                                                    got_base_loaded);
            }
        }
        
        PUP_LOG_DEBUG(PUP_LOG_MODULE_RELOC, ".rel.dyn relocation completed, %d entries processed", i);
    }
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_RELOC, "Performing GOT relocation...");
    for (i = 0, s = gs; s < ge; s += 4, i++)
    {
        unsigned int value = gbase[i];

        gbase[i] = relocate_runtime_address(value,
                                            gs, ge,
                                            ts, tbase,
                                            ram_start, ram_end,
                                            runtime_ram_base,
                                            got_base_loaded);
    }
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_RELOC, "GOT relocation completed, %d entries processed", i);
}

static unsigned int app_mpu_choose_region_size(unsigned int app_base, unsigned int app_size)
{
    unsigned int region_size = 256;
    unsigned int app_end = app_base + app_size;

    if (app_size == 0 || app_end < app_base) {
        return 256;
    }

    while (region_size <= (1024U * 1024U)) {
        unsigned int front_base = app_base & ~(region_size - 1U);
        unsigned long long coverage_end = (unsigned long long)front_base + ((unsigned long long)region_size * 2ULL);

        if ((unsigned long long)app_end <= coverage_end) {
            return region_size;
        }

        region_size <<= 1U;
    }

    return 1024U * 1024U;
}

int app_loader_init(void) {
    PUP_LOG_INFO(PUP_LOG_MODULE_APP, "App loader initialized");
    return 0;
}

static int app_validate(void* flash_addr, pup_size_t size) {
    app_header_t* header = (app_header_t*)flash_addr;
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "=== App Validation ===");
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "App address: 0x%08x", (unsigned int)flash_addr);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "App size: %d bytes", size);
    
    if (size < sizeof(app_header_t)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "App too small");
        return -1;
    }
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "App header:");
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "  Magic: 0x%08x", header->magic);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "  Load addr: 0x%08x", header->load_addr);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "  Entry addr: 0x%08x", header->entry_addr);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "  App size: %d bytes", header->app_size);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "  Version: 0x%08x", header->version);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "  Stack: start=0x%08x size=%u", header->stack_addr, header->stack_size);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "  Arg area: 0x%08x - 0x%08x", header->arg_start, header->arg_end);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "  GOT: 0x%08x - 0x%08x", header->got_start, header->got_end);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "  Text start: 0x%08x", header->text_start);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "  Rel: 0x%08x - 0x%08x", header->rel_start, header->rel_end);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "  Data: 0x%08x - 0x%08x (load: 0x%08x)", 
               header->data_start, header->data_end, header->data_load);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "  BSS: 0x%08x - 0x%08x", header->bss_start, header->bss_end);
    
    if (header->magic != APP_HEADER_MAGIC) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Invalid magic number");
        return -1;
    }
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "App validation passed");
    return 0;
}

int app_calc_memory_requirement(void* flash_addr, pup_size_t size, unsigned int* required_size) {
    app_header_t* header = (app_header_t*)flash_addr;
    unsigned int ram_image_size;
    unsigned int got_size;
    unsigned int total_size;

    if (!required_size) {
        return -1;
    }

    if (app_validate(flash_addr, size) != 0) {
        return -1;
    }

    if (header->data_start == 0 || header->stack_addr < header->data_start || header->stack_size == 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Invalid RAM layout in app header");
        return -1;
    }

    if (header->arg_end <= header->arg_start) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Invalid app arg range");
        return -1;
    }

    if (header->arg_start < header->data_start ||
        header->arg_end > (header->stack_addr + header->stack_size)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "App arg range is outside runtime RAM image");
        return -1;
    }

    ram_image_size = (header->stack_addr + header->stack_size) - header->data_start;
    got_size = (header->got_end > header->got_start) ? (header->got_end - header->got_start) : 0;
    total_size = ram_image_size + got_size;

    if (total_size == 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "App memory requirement is zero");
        return -1;
    }

    *required_size = (unsigned int)ALIGN_UP(total_size, 8);
    return 0;
}

int app_load_into_memory(void* flash_addr, pup_size_t size,
                         unsigned int ram_base, unsigned int ram_size,
                         user_app_t* app) {
    app_header_t* header = (app_header_t*)flash_addr;
    unsigned int load_offset;
    unsigned int data_size;
    unsigned int bss_size;
    unsigned int ram_image_size;
    unsigned int got_size;
    unsigned int runtime_got_base;
    unsigned int *got_base;
    unsigned int *rel_start = NULL;
    unsigned int *rel_end = NULL;
    
    if (app_validate(flash_addr, size) != 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "App validation failed");
        return -1;
    }

    if (!app) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Output app image is NULL");
        return -1;
    }

    if (header->data_start == 0 || header->stack_addr < header->data_start || header->stack_size == 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Invalid RAM layout in app header");
        return -1;
    }

    if (header->arg_end <= header->arg_start) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Invalid app arg range");
        return -1;
    }

    if (header->arg_start < header->data_start ||
        header->arg_end > (header->stack_addr + header->stack_size)) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "App arg range is outside runtime RAM image");
        return -1;
    }

    load_offset = (unsigned int)flash_addr - header->load_addr;
    data_size = (header->data_end > header->data_start) ? (header->data_end - header->data_start) : 0;
    bss_size = (header->bss_end > header->bss_start) ? (header->bss_end - header->bss_start) : 0;
    ram_image_size = (header->stack_addr + header->stack_size) - header->data_start;
    got_size = (header->got_end > header->got_start) ? (header->got_end - header->got_start) : 0;

    if (ram_image_size > ram_size || ram_image_size + got_size > ram_size) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "App RAM image too large: image=%u got=%u slot=%u",
                      ram_image_size, got_size, ram_size);
        return -1;
    }

    libcm_memset((void *)ram_base, 0, ram_size);
    
    app->entry = (void *)((unsigned int)flash_addr + (header->entry_addr - header->load_addr));
    app->entry_arg = (void *)(ram_base + (header->arg_start - header->data_start));
    app->stack_addr = (void *)(ram_base + (header->stack_addr - header->data_start));
    app->stack_size = header->stack_size;
    app->arg_size = header->arg_end - header->arg_start;
    app->size = size;
    app->flash_addr = (unsigned int)flash_addr;
    app->ram_base = ram_base;
    app->ram_size = ram_size;
    app->compiled_ram_start = header->data_start;
    app->compiled_ram_end = header->stack_addr + header->stack_size;
    app->got_start = header->got_start;
    app->got_end = header->got_end;
    app->text_start = header->text_start;
    app->got_base = 0;
    
    PUP_LOG_INFO(PUP_LOG_MODULE_APP, "App loaded from flash: 0x%08x", (unsigned int)flash_addr);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Stack addr: 0x%08x, size: %u", header->stack_addr, header->stack_size);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Arg area: 0x%08x - 0x%08x, size=%u",
                  header->arg_start, header->arg_end, app->arg_size);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Load offset: 0x%08x", load_offset);
    
    /* Calculate app actual memory usage (not the allocated size)
     * ram_image_size is the actual app RAM usage including stack */
    uint32_t app_actual_size = ram_image_size;
    uint32_t app_actual_end = ram_base + app_actual_size;  /* Actual end, NOT aligned */
    
    /* Calculate precise MPU configuration using two overlapping regions
     * - Front region: aligned to 32KB, covers app start
     * - Back region: aligned to 32KB based on app_end, can overlap with front
     * - Two regions overlap to precisely cover app actual range [ram_base, app_actual_end)
     * - app_actual_end is the ACTUAL end (not aligned), only ram_base is aligned
     */
    uint32_t region_size = app_mpu_choose_region_size(ram_base, app_actual_size);
    uint32_t subregion_size = region_size / 8;
    uint32_t f_base = ram_base & ~(region_size - 1);
    uint32_t b_base = (app_actual_end - 1) & ~(region_size - 1);
    int b_region_enabled = (b_base != f_base);
    
    /* Calculate front sub-region mask
     * Enable sub-regions that overlap with [ram_base, app_actual_end) */
    uint32_t f_sub_mask = 0;
    char f_sub_map[9] = {0};
    for (int i = 0; i < 8; i++) {
        uint32_t subregion_start = f_base + i * subregion_size;
        uint32_t subregion_end = subregion_start + subregion_size;
        /* Overlap condition: subregion_start < app_actual_end AND subregion_end > ram_base */
        int overlaps = (subregion_start < app_actual_end) && (subregion_end > ram_base);
        if (!overlaps) {
            f_sub_mask |= (1 << i);
            f_sub_map[i] = (subregion_end <= ram_base) ? 'x' : 'X';  /* x=before, X=after */
        } else {
            f_sub_map[i] = 'o';  /* Enabled */
        }
    }
    f_sub_map[8] = '\0';

    uint32_t b_sub_mask = 0;
    char b_sub_map[9] = {0};
    if (b_region_enabled) {
        for (int i = 0; i < 8; i++) {
            uint32_t subregion_start = b_base + i * subregion_size;
            uint32_t subregion_end = subregion_start + subregion_size;
            int overlaps = (subregion_start < app_actual_end) && (subregion_end > ram_base);
            if (!overlaps) {
                b_sub_mask |= (1 << i);
                b_sub_map[i] = (subregion_end <= ram_base) ? 'x' : 'X';
            } else {
                b_sub_map[i] = 'o';
            }
        }
    } else {
        b_sub_mask = 0xFF;
        for (int i = 0; i < 8; i++) {
            b_sub_map[i] = 'X';
        }
    }
    b_sub_map[8] = '\0';

    /* Print app memory layout and precise MPU configuration */
    PUP_LOG_INFO(PUP_LOG_MODULE_APP, "=== App Memory Layout ===");
    PUP_LOG_INFO(PUP_LOG_MODULE_APP, "  RAM:   0x%08x - 0x%08x (%d bytes, allocated)",
                 ram_base, ram_base + ram_size, ram_size);
    PUP_LOG_INFO(PUP_LOG_MODULE_APP, "  Actual: 0x%08x - 0x%08x (%d bytes, used)",
                 ram_base, app_actual_end, app_actual_size);
    PUP_LOG_INFO(PUP_LOG_MODULE_APP, "  Stack: 0x%08x - 0x%08x (%d bytes)",
                 (unsigned int)app->stack_addr,
                 (unsigned int)app->stack_addr + app->stack_size,
                 app->stack_size);
    PUP_LOG_INFO(PUP_LOG_MODULE_APP, "=== Precise MPU Regions (Dual Overlapping) ===");
    PUP_LOG_INFO(PUP_LOG_MODULE_APP, "  [ALLOW] App Actual: 0x%08x - 0x%08x (RWX)", ram_base, app_actual_end);
    if (region_size >= 1024) {
        PUP_LOG_INFO(PUP_LOG_MODULE_APP, "    Granularity: region=%uKB, subregion=%uB",
                     region_size / 1024, subregion_size);
    } else {
        PUP_LOG_INFO(PUP_LOG_MODULE_APP, "    Granularity: region=%uB, subregion=%uB",
                     region_size, subregion_size);
    }
    PUP_LOG_INFO(PUP_LOG_MODULE_APP, "    Front Region: 0x%08x - 0x%08x (%uB, sub: %s, mask=0x%02x)",
                 f_base, f_base + region_size, region_size, f_sub_map, f_sub_mask);
    if (b_region_enabled) {
        PUP_LOG_INFO(PUP_LOG_MODULE_APP, "    Back Region:  0x%08x - 0x%08x (%uB, sub: %s, mask=0x%02x)",
                     b_base, b_base + region_size, region_size, b_sub_map, b_sub_mask);
    } else {
        PUP_LOG_INFO(PUP_LOG_MODULE_APP, "    Back Region:  disabled (single-region coverage)");
    }
    PUP_LOG_INFO(PUP_LOG_MODULE_APP, "    Note: Regions overlap to cover actual range [%08x, %08x)",
                 ram_base, app_actual_end);
    PUP_LOG_INFO(PUP_LOG_MODULE_APP, "  [ALLOW] Flash:    0x%08x - 0x%08x (RX)", 0x08000000, 0x08100000);
    PUP_LOG_INFO(PUP_LOG_MODULE_APP, "  [DENY]  Kernel:   0x%08x - 0x%08x (Privileged only)", 0x20000000, 0x20010000);
    
    if (data_size > 0) {
        PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Initializing .data segment...");
        PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "data_start=0x%08x, data_end=0x%08x, data_load=0x%08x", 
                   header->data_start, header->data_end, header->data_load);
        
        unsigned int* data_src = (unsigned int*)(header->data_load + load_offset);
        unsigned int* data_dst = (unsigned int*)ram_base;
        
        PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Copying %d bytes from 0x%08x to 0x%08x", 
                   data_size, (unsigned int)data_src, (unsigned int)data_dst);
        
        for (unsigned int i = 0; i < data_size / 4; i++) {
            data_dst[i] = data_src[i];
        }
        
        PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, ".data segment after copy:");
        for (unsigned int i = 0; i < data_size / 4 && i < 8; i++) {
            PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "  data_dst[%d] = 0x%08x", i, data_dst[i]);
        }
    }
    
    if (bss_size > 0) {
        PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Clearing .bss segment...");
        PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "bss_start=0x%08x, bss_end=0x%08x", 
                   header->bss_start, header->bss_end);
        
        unsigned int* bss_dst = (unsigned int*)(ram_base + (header->bss_start - header->data_start));
        
        PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Clearing %d bytes at 0x%08x", bss_size, (unsigned int)bss_dst);
        
        for (unsigned int i = 0; i < bss_size / 4; i++) {
            bss_dst[i] = 0;
        }
    }
    
    if (got_size > 0) {
        PUP_LOG_DEBUG(PUP_LOG_MODULE_RELOC, "Performing GOT relocation...");
        PUP_LOG_DEBUG(PUP_LOG_MODULE_RELOC, "App actual load addr: 0x%08x", (unsigned int)flash_addr);
        PUP_LOG_DEBUG(PUP_LOG_MODULE_RELOC, "App compiled load addr: 0x%08x", header->load_addr);
        
        runtime_got_base = ram_base + ram_image_size;
        got_base = (unsigned int*)runtime_got_base;
        libcm_memcpy(got_base,
                     (const void *)((unsigned int)flash_addr + (header->got_start - header->load_addr)),
                     got_size);
        app->got_base = runtime_got_base;
        
        if (header->rel_start != 0 && header->rel_end != 0 && header->rel_end > header->rel_start) {
            rel_start = (unsigned int*)((unsigned int)flash_addr + (header->rel_start - header->load_addr));
            rel_end = (unsigned int*)((unsigned int)flash_addr + (header->rel_end - header->load_addr));
        }
        
        kernel_reloc(got_base, (unsigned int)flash_addr,
                     header->got_start, header->got_end, header->text_start,
                     header->data_start, header->stack_addr + header->stack_size,
                     ram_base,
                     rel_start, rel_end);

        PUP_LOG_DEBUG(PUP_LOG_MODULE_RELOC, "Adjusted entry: 0x%08x", (unsigned int)app->entry);
        PUP_LOG_DEBUG(PUP_LOG_MODULE_RELOC, "Adjusted stack: 0x%08x", (unsigned int)app->stack_addr);
        PUP_LOG_DEBUG(PUP_LOG_MODULE_RELOC, "GOT base (r9): 0x%08x", app->got_base);
    }
    
    return 0;
}

int app_load_from_flash(void* flash_addr, pup_size_t size) {
    static void* legacy_app_ram = NULL;
    unsigned int required_size;

    if (app_calc_memory_requirement(flash_addr, size, &required_size) != 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Failed to calculate legacy app RAM requirement");
        return -1;
    }

    if (required_size > kheap_user_total_size()) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Legacy app RAM requirement too large: %u (pool=%u)",
                      required_size, (unsigned int)kheap_user_total_size());
        return -1;
    }

    if (required_size > kheap_user_free_size()) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Legacy app RAM not enough: need=%u free=%u",
                      required_size, (unsigned int)kheap_user_free_size());
        return -1;
    }

    if (legacy_app_ram) {
        kheap_user_free(legacy_app_ram);
        legacy_app_ram = NULL;
    }

    legacy_app_ram = kheap_user_memalign(8, required_size);
    if (!legacy_app_ram) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Failed to allocate legacy app RAM: %u", required_size);
        return -1;
    }

    loaded_app.pid = 0;
    return app_load_into_memory(flash_addr, size,
                                (unsigned int)legacy_app_ram, required_size,
                                &loaded_app);
}

static void* user_app_thread(void* arg) {
    user_app_t* app = (user_app_t*)arg;
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "User app thread started");
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Entry point: 0x%08x", (unsigned int)app->entry);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Stack addr: 0x%08x", (unsigned int)app->stack_addr);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "GOT base (r9): 0x%08x", app->got_base);
    
    user_app_exit_code = 0;
    user_app_should_exit = 0;
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Setting up dynamic MPU protection for app");
    /* Setup MPU protection based on the actual runtime span used by
     * image + GOT + argument area. This keeps fine-grained protection
     * while avoiding faults on valid runtime metadata access. */
    uint32_t protected_end = app->ram_base + (app->compiled_ram_end - app->compiled_ram_start);
    uint32_t got_size = (app->got_end > app->got_start) ? (app->got_end - app->got_start) : 0;
    uint32_t app_actual_size;

    if (app->got_base != 0 && got_size > 0) {
        uint32_t got_end = app->got_base + got_size;
        if (got_end > protected_end) {
            protected_end = got_end;
        }
    }

    if (app->entry_arg != NULL && app->arg_size > 0) {
        uint32_t arg_end = (uint32_t)app->entry_arg + app->arg_size;
        if (arg_end > protected_end) {
            protected_end = arg_end;
        }
    }

    app_actual_size = protected_end - app->ram_base;
    pup_mpu_setup_app_protection(app->ram_base, app_actual_size);
    
    extern void user_app_start(unsigned int got_base, void* entry, void* entry_arg);
    
    if (app->got_base != 0) {
        PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Calling user_app_start with r9=0x%08x, entry=0x%08x", 
                   app->got_base, (unsigned int)app->entry | 0x1);
        user_app_start(app->got_base,
                       (void*)((unsigned int)app->entry | 0x1),
                       app->entry_arg);
    } else {
        PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Switching to user mode...");
        arch_enter_user_mode();
        void (*app_entry)(void*) = (void (*)(void*))((unsigned int)app->entry | 0x1);
        PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Calling entry with Thumb bit: 0x%08x", (unsigned int)app_entry);
        app_entry(app->entry_arg);
    }
    
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "User app thread finished");
    return NULL;
}

static void __app_run_internal(user_app_t* app) {
    app_run(app, "user_app", NULL);
}

int app_run(user_app_t* app, const char* thread_name, pthread_t* thread_out) {
    pthread_t thread;
    pthread_attr_t attr;
    
    if (!app || !app->entry) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Invalid app");
        return -1;
    }
    
    PUP_LOG_INFO(PUP_LOG_MODULE_APP, "Running user app at 0x%08x", (unsigned int)app->entry);
    PUP_LOG_DEBUG(PUP_LOG_MODULE_APP, "Using app stack: addr=0x%08x, size=%d", 
               (unsigned int)app->stack_addr, app->stack_size);
    
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, app->stack_size);
    pthread_attr_setstackaddr(&attr, app->stack_addr);
    pthread_attr_setname(&attr, (char *)(thread_name ? thread_name : "user_app"));
    pthread_attr_setuser(&attr, 1);
    
    if (pthread_create(&thread, &attr, user_app_thread, app) != 0) {
        PUP_LOG_ERROR(PUP_LOG_MODULE_APP, "Failed to create user app thread");
        return -1;
    }

    if (thread_out) {
        *thread_out = thread;
    }
    
    return 0;
}

void app_run_legacy(user_app_t* app) {
    __app_run_internal(app);
}

int app_check_exit(void) {
    return user_app_should_exit;
}

int app_get_exit_code(void) {
    return user_app_exit_code;
}
