ENTRY(_start)

MEMORY {
    RAM (rwx) : ORIGIN = 0xA0100000, LENGTH = 256K 
    STACK (rw) : ORIGIN = 0xA0140000, LENGTH = 16K 
}

SECTIONS {
    .text : { 
        /* Wymuszamy, aby kod startowy był absolutnie pierwszy */
        KEEP(*(.text.boot)) 
        *(.text*) 
    } > RAM

    .rodata : { *(.rodata*) } > RAM
    .data : { *(.data*) } > RAM
    
    .bss : {
        /* Wyrównanie do 8 bajtów chroni przed Alignment Fault */
        . = ALIGN(8);
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        _bss_end = .;
    } > RAM
    
    _stack_top = ORIGIN(STACK) + LENGTH(STACK);
}