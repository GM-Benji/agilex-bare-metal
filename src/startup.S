.global _start
.extern main
.extern _bss_start
.extern _bss_end
.extern _stack_top

/* Zmieniamy zwykłe .text na dedykowaną sekcję startową */
.section .text.boot

_start:
    /* 1. Ustawienie bezpiecznego wskaźnika stosu (SP) */
    ldr x0, =_stack_top
    mov sp, x0

    /* 2. Wyzerowanie sekcji BSS */
    ldr x1, =_bss_start
    ldr x2, =_bss_end
bss_loop:
    cmp x1, x2
    b.ge bss_done
    str xzr, [x1], #8
    b bss_loop
bss_done:

    /* 3. Skok do Twojego kodu w C */
    bl main

    /* 4. Pułapka bezpieczeństwa */
hang:
    b hang