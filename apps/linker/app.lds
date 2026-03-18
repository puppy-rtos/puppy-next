MEMORY
{
    USER_APP_ROM (rx) : ORIGIN = 0x08080000, LENGTH = 512k
    USER_APP_RAM (rw) : ORIGIN = 0x20010000, LENGTH = 64k
}
ENTRY(app_main)

__user_app_stack_size = DEFINED(__user_app_stack_size) ? __user_app_stack_size : 0x2000;

SECTIONS
{
    .app_header :
    {
        . = ALIGN(4);
        _app_start = .;
        __text_start__ = .;
        KEEP(*(.app_header))
        . = ALIGN(4);
    } > USER_APP_ROM

    .text :
    {
        . = ALIGN(4);
        _stext = .;
        *(.text)
        *(.text.*)
        . = ALIGN(4);
        _etext = .;
    } > USER_APP_ROM

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > USER_APP_ROM

    .got_rom :
    {
        . = ALIGN(4);
        __got_start__ = .;
        *(.got)
        *(.got.plt)
        . = ALIGN(4);
        __got_end__ = .;
    } > USER_APP_ROM

    .rel.dyn :
    {
        *(.rel.dyn .rel.data .rel.data.* .rel.gnu.linkonce.d.*)
    } > USER_APP_ROM

    __rel_start__ = ADDR(.rel.dyn);
    __rel_end__ = ADDR(.rel.dyn) + SIZEOF(.rel.dyn);

    _end_of_rom = .;

    .data : AT (_end_of_rom)
    {
        . = ALIGN(4);
        __data_start__ = .;
        _sdata = .;
        *(.data)
        *(.data.*)
        . = ALIGN(4);
        __data_end__ = .;
        _edata = .;
    } > USER_APP_RAM

    __data_load__ = LOADADDR(.data);

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
    } > USER_APP_RAM

    .stack :
    {
        . = ALIGN(4);
        _sstack = .;
        . = . + __user_app_stack_size;
        _estack = .;
    } > USER_APP_RAM

    .app_end :
    {
        . = ALIGN(4);
        _app_end = .;
        _app_size = _app_end - _app_start;
    } > USER_APP_ROM
}
