/* Set the executable entry point to the _start symbol in boot.asm */
ENTRY(_start)

SECTIONS
{
    /* Begin placing sections at 1 MB */
    . = 1M;

    /* First, place the Multiboot header, followed by executable assembly/C code */
    .text : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    /* Read-only data section (string literals, constants) */
    .rodata : ALIGN(4K)
    {
        *(.rodata)
    }

    /* Initialized read-write data section (global/static variables) */
    .data : ALIGN(4K)
    {
        *(.data)
    }

    /* Uninitialized read-write data section (our 16 KB stack buffer) */
    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
}
