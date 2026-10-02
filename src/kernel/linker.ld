/* NixDOS 2 kernel: flat binary loaded by the boot sector at 0x10000 */
ENTRY(_start)

SECTIONS
{
    . = 0x10000;

    .text : {
        *(.text.entry)
        *(.text*)
    }

    .rodata : { *(.rodata*) }

    .data : { *(.data*) }

    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }

    /* The boot stack starts at 0x90000 and grows down. */
    ASSERT(__bss_end < 0x80000, "kernel image + bss too large (must end below 0x80000)")
}
