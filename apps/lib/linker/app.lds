/* puppy-next user app linker script
 * Base address = 0 for simplified relocation.
 *
 * Layout (all virtual, linked from 0):
 *   .text  (ro)  — code + rodata + .rel.dyn + app_header
 *   .data  (rw)  — GOT at head, then .data proper, then .app_runtime, then .stack
 *   .bss   (rw)  — zero-init
 */
ENTRY(app_main)

__user_app_stack_size = DEFINED(__user_app_stack_size) ? __user_app_stack_size : 0x2000;
__user_app_arg_size = DEFINED(__user_app_arg_size) ? __user_app_arg_size : 0x100;

SECTIONS
{
    .app_header :
    {
        . = ALIGN(4);
        _app_start = .;
        __text_start__ = .;
        KEEP(*(.app_header))
        . = ALIGN(4);
    }

    .text :
    {
        . = ALIGN(4);
        _stext = .;
        *(.text)
        *(.text.*)
        . = ALIGN(4);
        _etext = .;
    }

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    }

    /* .rel.dyn placed at end of ROM section.
     * Collects .rel.dyn for all RELATIVE relocations (GOT + non-GOT .data).
     * Uses ADDR+SIZEOF because the PIE linker may append extra entries
     * after the explicit input patterns. */
    .rel.dyn :
    {
        . = ALIGN(4);
        __rel_start__ = .;
        *(.rel.dyn .rel.data .rel.data.* .rel.gnu.linkonce.d.*)
        . = ALIGN(4);
    }

    __rel_end__ = ADDR(.rel.dyn) + SIZEOF(.rel.dyn);

    /* .data follows .rel.dyn in the binary image (VMA == LMA).
     * GOT is placed at the head of .data. */
    .data :
    {
        . = ALIGN(4);
        __data_start__ = .;
        _sdata = .;

        __got_start__ = .;
        *(.got)
        *(.got.plt)
        __got_end__ = .;
        . = ALIGN(4);

        *(.data)
        *(.data.*)
        . = ALIGN(4);
        __data_end__ = .;
        _edata = .;
    }

    /* data_load == data_start since VMA == LMA (no AT directive) */
    __data_load__ = __data_start__;

    .bss :
    {
        . = ALIGN(4);
        __bss_start__ = .;
        _sbss = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
        _ebss = .;
    }

    .app_runtime :
    {
        . = ALIGN(8);
        __app_arg_start__ = .;
        . = . + __user_app_arg_size;
        __app_arg_end__ = .;
        . = ALIGN(8);
    }

    .stack :
    {
        . = ALIGN(4);
        _sstack = .;
        . = . + __user_app_stack_size;
        _estack = .;
    }

    PROVIDE(__ram_size__ = _estack - __data_start__);

    .app_end :
    {
        . = ALIGN(4);
        _app_end = .;
        _app_size = _app_end - _app_start;
    }
}
