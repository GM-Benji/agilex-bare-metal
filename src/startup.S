.global _start
.extern main
.extern irq_handler
.extern exc_handler
.extern _bss_start
.extern _bss_end
.extern _stack_top

/* Zmieniamy zwykłe .text na dedykowaną sekcję startową */
.section .text.boot

_start:
    /* 0. Maskowanie wszystkich przerwań do czasu konfiguracji GIC */
    msr daifset, #0xf

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

    /* 3. Tablica wektorów wyjątków - PSCI CPU_ON startuje nas na EL2 (jak Linuksa) */
    ldr x0, =vectors
    mrs x1, CurrentEL
    ubfx x1, x1, #2, #2
    cmp x1, #2
    b.ne 1f
    msr vbar_el2, x0
    /* HCR_EL2.IMO/FMO/AMO = 1: przerwania fizyczne kierowane na EL2.
     * Bez tego (IMO=0) IRQ trafia na EL1, a na EL2 jest zamaskowane -
     * GIC sygnalizuje przerwanie (ISR_EL1.I=1), ale rdzeń go nie przyjmuje ani nie budzi się z WFI. */
    mrs x2, hcr_el2
    orr x2, x2, #(0x7 << 3)
    msr hcr_el2, x2
    b 2f
1:  msr vbar_el1, x0
2:  isb

    /* 4. Skok do Twojego kodu w C */
    bl main

    /* 5. Pułapka bezpieczeństwa */
hang:
    wfe
    b hang

/* ---------------- Obsługa IRQ ----------------
 * Zapisujemy rejestry caller-saved (x0-x18, x29, x30) - resztę chroni ABI.
 * Kod kompilowany z -mgeneral-regs-only, więc rejestrów FP/SIMD nie trzeba zapisywać.
 * W trakcie obsługi IRQ są zamaskowane (PSTATE.I=1), więc ELR/SPSR się nie zmienią. */
irq_entry:
    sub sp, sp, #176
    stp x0,  x1,  [sp, #0]
    stp x2,  x3,  [sp, #16]
    stp x4,  x5,  [sp, #32]
    stp x6,  x7,  [sp, #48]
    stp x8,  x9,  [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x29, [sp, #144]
    str x30,      [sp, #160]
    bl irq_handler
    ldp x0,  x1,  [sp, #0]
    ldp x2,  x3,  [sp, #16]
    ldp x4,  x5,  [sp, #32]
    ldp x6,  x7,  [sp, #48]
    ldp x8,  x9,  [sp, #64]
    ldp x10, x11, [sp, #80]
    ldp x12, x13, [sp, #96]
    ldp x14, x15, [sp, #112]
    ldp x16, x17, [sp, #128]
    ldp x18, x29, [sp, #144]
    ldr x30,      [sp, #160]
    add sp, sp, #176
    eret

/* Pętla parkowania - kopiowana pod PARK_ADDR (poza obszar firmware'u), bo Linux nadpisze kod.
 * x0 = &ctrl.release, x2 = adres startowy nowego firmware'u */
.global park_stub
.global park_stub_end
park_stub:
1:  wfe
    ldr w1, [x0]
    cbz w1, 1b
    dsb sy
    isb
    br x2
park_stub_end:

/* Każdy inny wyjątek: zapis ESR/ELR do pamięci dzielonej i zatrzymanie rdzenia */
exc_entry:
    bl exc_handler
    b hang

/* ---------------- Tablica wektorów (wyrównanie 2 KB, 16 wpisów po 0x80) ---------------- */
.macro vec target
    .balign 0x80
    b \target
.endm

.balign 2048
vectors:
    vec exc_entry   /* Current EL, SP0:  Sync   */
    vec irq_entry   /*                   IRQ    */
    vec exc_entry   /*                   FIQ    */
    vec exc_entry   /*                   SError */
    vec exc_entry   /* Current EL, SPx:  Sync   */
    vec irq_entry   /*                   IRQ    */
    vec exc_entry   /*                   FIQ    */
    vec exc_entry   /*                   SError */
    vec exc_entry   /* Lower EL, AArch64 (nieużywane) */
    vec exc_entry
    vec exc_entry
    vec exc_entry
    vec exc_entry   /* Lower EL, AArch32 (nieużywane) */
    vec exc_entry
    vec exc_entry
    vec exc_entry
