MEMORY
{
    KERNEL_ROM (rx) : ORIGIN = DTM_FLASH_BASE, LENGTH = DTM_FLASH_SIZE
    KERNEL_RAM (rw) : ORIGIN = DTM_RAM_BASE, LENGTH = 64k
}
ENTRY( Reset_Handler )
_system_stack_size = 0x4000;

SECTIONS
{
    .text :
    {
        . = ALIGN(4);
        _stext = .;
        KEEP(*(.isr_vector))

        . = ALIGN(4);
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        *(.gnu.linkonce.t*)

        . = ALIGN(4);
        *(.note.gnu.build-id)

        . = ALIGN(4);

        PROVIDE(__ctors_start__ = .);
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array))
        PROVIDE(__ctors_end__ = .);

        . = ALIGN(4);

        _etext = .;
    } > KERNEL_ROM

    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)

        _sidata = .;
    } > KERNEL_ROM
    __exidx_end = .;

    .data : AT (_sidata)
    {
        . = ALIGN(4);
        _sdata = . ;

        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d*)

        . = ALIGN(4);
        _edata = . ;
    } >KERNEL_RAM

    __bss_start = .;
    __bss_start__ = .;
    .bss :
    {
        . = ALIGN(4);
        _sstack = .;
        . = . + _system_stack_size;
        . = ALIGN(4);
        _estack = .;

        . = ALIGN(4);
        _sbss = .;

        *(.bss)
        *(.bss.*)
        *(COMMON)

        . = ALIGN(4);
        _ebss = . ;
        
        *(.bss.init)
    } > KERNEL_RAM
    __bss_end = .;
    __bss_end__ = .;

    _end = .;
    _heap_start = .;
    _heap_end = ORIGIN(KERNEL_RAM) + LENGTH(KERNEL_RAM);

    .stab          0 : { *(.stab) }
    .stabstr       0 : { *(.stabstr) }
    .stab.excl     0 : { *(.stab.excl) }
    .stab.exclstr  0 : { *(.stab.exclstr) }
    .stab.index    0 : { *(.stab.index) }
    .stab.indexstr 0 : { *(.stab.indexstr) }
    .comment       0 : { *(.comment) }
    .debug          0 : { *(.debug) }
    .line           0 : { *(.line) }
    .debug_srcinfo  0 : { *(.debug_srcinfo) }
    .debug_sfnames  0 : { *(.debug_sfnames) }
    .debug_aranges  0 : { *(.debug_aranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line) }
    .debug_frame    0 : { *(.debug_frame) }
    .debug_str      0 : { *(.debug_str) }
    .debug_loc      0 : { *(.debug_loc) }
    .debug_macinfo  0 : { *(.debug_macinfo) }
    .debug_weaknames 0 : { *(.debug_weaknames) }
    .debug_funcnames 0 : { *(.debug_funcnames) }
    .debug_typenames 0 : { *(.debug_typenames) }
    .debug_varnames  0 : { *(.debug_varnames) }
}
