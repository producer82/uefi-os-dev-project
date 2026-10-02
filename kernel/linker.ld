ENTRY(KernelEntry)

SECTIONS
{
    . = 0xFFFFFFFF80000000;
    
    .text ALIGN(4K) : AT(ADDR(.text))
    {
        *(.text.entry)
        *(.text*)       
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata))
    {
        *(.rodata*)
    }

    .data ALIGN(4K) : AT(ADDR(.data))
    {
        *(.data*)
    }

    .bss ALIGN(4K) : AT(ADDR(.bss))
    {
        *(COMMON)
        *(.bss*)
    }
}
