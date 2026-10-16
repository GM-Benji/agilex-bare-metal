#include <stdint.h>
#include "gic.h"
#include "platform.h"

// --- Redystrybutor (RD_base) ---
#define GICR_TYPER        0x0008
#define GICR_WAKER        0x0014
#define GICR_TYPER_LAST   (1u << 4)
#define WAKER_PSLEEP      (1u << 1)
#define WAKER_CASLEEP     (1u << 2)
// --- Redystrybutor (SGI_base = RD_base + 64 KB) ---
#define SGI_BASE          0x10000
#define GICR_ISENABLER0   (SGI_BASE + 0x0100)
#define GICR_ICPENDR0     (SGI_BASE + 0x0280)
#define GICR_IPRIORITYR   (SGI_BASE + 0x0400)
// --- Dystrybutor ---
#define GICD_ISPENDR      0x0200

// Rejestry systemowe interfejsu CPU (kodowanie S3_*, bo stary assembler może nie znać nazw)
#define ICC_SRE_EL2       "S3_4_C12_C9_5"
#define ICC_SRE_EL1       "S3_0_C12_C12_5"
#define ICC_PMR_EL1       "S3_0_C4_C6_0"
#define ICC_IAR1_EL1      "S3_0_C12_C12_0"
#define ICC_EOIR1_EL1     "S3_0_C12_C12_1"
#define ICC_BPR1_EL1      "S3_0_C12_C12_3"
#define ICC_CTLR_EL1      "S3_0_C12_C12_4"
#define ICC_IGRPEN1_EL1   "S3_0_C12_C12_7"
#define ICC_SGI1R_EL1     "S3_0_C12_C11_5"
#define ICC_RPR_EL1       "S3_0_C12_C11_3"
#define ICC_HPPIR1_EL1    "S3_0_C12_C12_2"
#define ICC_AP1R0_EL1     "S3_0_C12_C9_0"

#define read_sysreg(r) ({ uint64_t __v; __asm__ volatile ("mrs %0, " r : "=r" (__v)); __v; })
#define write_sysreg(r, v) __asm__ volatile ("msr " r ", %0" : : "r" ((uint64_t)(v)))
#define isb() __asm__ volatile ("isb" : : : "memory")
#define dsb() __asm__ volatile ("dsb sy" : : : "memory")

static inline uint32_t r32(uintptr_t a) { return *(volatile uint32_t *)a; }
static inline void w32(uintptr_t a, uint32_t v) { *(volatile uint32_t *)a = v; }

// Szuka ramki redystrybutora, której GICR_TYPER.Affinity pasuje do MPIDR tego rdzenia
static uintptr_t find_redistributor(void) {
    uint64_t mpidr = read_sysreg("mpidr_el1");
    uint32_t aff = (uint32_t)(mpidr & 0xFFFFFF) | (uint32_t)(((mpidr >> 32) & 0xFF) << 24);

    for (int i = 0; i < GICR_MAX_FRAMES; i++) {
        uintptr_t rd = GICR_BASE + i * GICR_STRIDE;
        uint32_t typer_lo = r32(rd + GICR_TYPER);
        uint32_t typer_hi = r32(rd + GICR_TYPER + 4);
        if (typer_hi == aff) return rd;
        if (typer_lo & GICR_TYPER_LAST) break;
    }
    return 0;
}

int gic_cpu_init(uint32_t sgi) {
    uintptr_t rd = find_redistributor();
    if (!rd) return 1;

    // 1. Obudzenie redystrybutora (ATF zwykle już to zrobił przy CPU_ON - operacja idempotentna)
    w32(rd + GICR_WAKER, r32(rd + GICR_WAKER) & ~WAKER_PSLEEP);
    for (int t = 0; r32(rd + GICR_WAKER) & WAKER_CASLEEP; t++)
        if (t > 1000000) return 2;

    // 2. Priorytet, wyczyszczenie starego pending, włączenie SGI.
    // GICR_IGROUPR0 jest RAZ/WI dla Non-secure (GICD_CTRL.DS=0) - grupę ustawia ATF.
    // Bity Secure przerwań też są RAZ/WI, więc jeśli enable się nie "trzyma",
    // to SGI nie jest w Group 1 Non-secure.
    *(volatile uint8_t *)(rd + GICR_IPRIORITYR + sgi) = 0xA0;
    w32(rd + GICR_ICPENDR0, 1u << sgi);
    w32(rd + GICR_ISENABLER0, 1u << sgi);
    dsb();
    if (!(r32(rd + GICR_ISENABLER0) & (1u << sgi))) return 3;

    // 4. Interfejs CPU przez rejestry systemowe
    uint64_t el = (read_sysreg("CurrentEL") >> 2) & 3, sre;
    if (el == 2) {
        write_sysreg(ICC_SRE_EL2, read_sysreg(ICC_SRE_EL2) | 0xF); // SRE|DFB|DIB|Enable
        isb();
        sre = read_sysreg(ICC_SRE_EL2);
    } else {
        write_sysreg(ICC_SRE_EL1, read_sysreg(ICC_SRE_EL1) | 0x7);
        isb();
        sre = read_sysreg(ICC_SRE_EL1);
    }
    if (!(sre & 1)) return 4;

    write_sysreg(ICC_PMR_EL1, 0xFF);                               // przepuszczaj wszystkie priorytety
    write_sysreg(ICC_BPR1_EL1, 0);
    write_sysreg(ICC_CTLR_EL1, read_sysreg(ICC_CTLR_EL1) & ~2ULL); // EOImode = 0 (EOI = drop + deactivate)
    write_sysreg(ICC_IGRPEN1_EL1, 1);
    isb();
    return 0;
}

uint32_t gic_ack(void) {
    uint32_t iar = (uint32_t)read_sysreg(ICC_IAR1_EL1) & 0xFFFFFF;
    __asm__ volatile ("dsb sy" : : : "memory");
    return iar;
}

void gic_eoi(uint32_t intid) {
    write_sysreg(ICC_EOIR1_EL1, intid);
    isb();
}

void gic_raise_spi(uint32_t intid) {
    dsb(); // dane w ring bufferze muszą być widoczne zanim Linux dostanie przerwanie
    w32(GICD_BASE + GICD_ISPENDR + (intid / 32) * 4, 1u << (intid % 32));
    dsb();
}

// SGI do samego siebie (test ścieżki odbioru bez udziału Linuksa)
void gic_send_sgi_self(uint32_t sgi) {
    uint64_t mpidr = read_sysreg("mpidr_el1");
    uint64_t val = (((mpidr >> 32) & 0xFF) << 48) | (((mpidr >> 16) & 0xFF) << 32) |
                   ((uint64_t)sgi << 24) | (((mpidr >> 8) & 0xFF) << 16) | (1u << (mpidr & 0xF));
    dsb();
    write_sysreg(ICC_SGI1R_EL1, val);
    isb();
}

void gic_snapshot(volatile uint64_t *d) {
    uint64_t el = (read_sysreg("CurrentEL") >> 2) & 3;
    d[0] = el == 2 ? read_sysreg(ICC_SRE_EL2) : read_sysreg(ICC_SRE_EL1);
    d[1] = read_sysreg(ICC_IGRPEN1_EL1);
    d[2] = read_sysreg(ICC_PMR_EL1);
    d[3] = read_sysreg(ICC_CTLR_EL1);
    d[4] = el == 2 ? read_sysreg("hcr_el2") : 0;
    d[5] = read_sysreg("isr_el1");
    d[6] = read_sysreg(ICC_RPR_EL1);
    d[7] = read_sysreg(ICC_HPPIR1_EL1);
    d[8] = read_sysreg(ICC_AP1R0_EL1);
    d[9] = el == 2 ? read_sysreg("sctlr_el2") : read_sysreg("sctlr_el1");
}
