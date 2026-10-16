#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>

#define RING_BUFFER_SIZE 1024
#define IPC_MAGIC        0x31504D41  // "AMP1"

typedef struct {
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint8_t data[RING_BUFFER_SIZE];
} RingBuffer;

// Blok kontrolny IPC - MUSI byc identyczny jak w wake_a55.c (Linux)
typedef struct {
    volatile uint32_t magic;       // IPC_MAGIC gdy BM gotowy
    volatile uint32_t bm_mpidr;    // MPIDR rdzenia BM (cel dla SGI z Linuksa)
    volatile uint32_t sgi_count;   // ile SGI odebral BM
    volatile uint32_t wakeups;     // ile razy BM wyszedl z WFI
    volatile uint32_t spi_sent;    // ile SPI BM wyslal do Linuksa
    volatile uint32_t stop_req;    // Linux ustawia 1 -> BM parkuje sie (PARK_ADDR)
    volatile uint32_t current_el;  // EL, na ktorym dziala BM
    volatile uint32_t gic_status;  // 0 = OK, inaczej kod bledu gic_cpu_init
    volatile uint32_t other_irqs;  // nieoczekiwane INTID
    volatile uint32_t fault_esr;   // ESR ostatniego wyjatku (0 = brak)
    volatile uint64_t fault_elr;
    volatile uint32_t selftest;    // 1 = BM odebral SGI wyslane do samego siebie
    volatile uint32_t parked;      // 1 = BM czeka w petli parkowania (mozna wgrac nowy firmware)
    volatile uint32_t release;     // Linux ustawia 1 -> zaparkowany BM skacze do poczatku firmware'u
    volatile uint32_t reserved;
    volatile uint64_t dbg[10];     // ICC_SRE_EL2, IGRPEN1, PMR, CTLR, HCR_EL2, ISR_EL1, RPR, HPPIR1, AP1R0, SCTLR
} IpcControl;

typedef struct {
    RingBuffer linux_to_bm;
    RingBuffer bm_to_linux;
    IpcControl ctrl;
} SharedMemoryMap;

// Prototypy funkcji
int write_to_ring(RingBuffer *rb, const uint8_t *buffer, uint32_t length);
int read_from_ring(RingBuffer *rb, uint8_t *buffer, uint32_t max_length);

#endif
