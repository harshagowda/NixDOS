/* NixDOS 2 native programs: flat image loaded at 0x400000, starting with
   a struct nxn_header (see src/kernel/kernel.h). */
ENTRY(_start)

SECTIONS
{
    . = 0x400000;
    __image_start = .;

    .header : {
        LONG(0x314E584E)                    /* "NXN1" */
        LONG(_start)
        LONG(__image_end - __image_start)
        LONG(__bss_end - __image_end)
    }

    .text : { *(.text.start) *(.text*) }
    .rodata : { *(.rodata*) }
    .init_array : {
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    }
    .data : { *(.data*) }
    . = ALIGN(16);
    __image_end = .;

    .bss (NOLOAD) : {
        *(.bss*)
        *(COMMON)
        . = ALIGN(16);
        __bss_end = .;
    }

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.fini_array*) *(.dtors*) }

    ASSERT(__bss_end <= 0x780000, "program image + bss larger than 3.5 MiB")
}
