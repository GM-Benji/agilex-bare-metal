#include "platform.h"
#include "ring_buffer.h"
#include "gic.h"

static SharedMemoryMap *const shared_ram = (SharedMemoryMap *)SHARED_MEM_BASE;
static volatile uint32_t sgi_pending;

static inline void irq_enable(void)  { __asm__ volatile ("msr daifclr, #2" : : : "memory"); }
static inline void irq_disable(void) { __asm__ volatile ("msr daifset, #2" : : : "memory"); }

// Wywoływane z irq_entry (startup.S) - tylko potwierdzenie przerwania, praca dzieje się w main
void irq_handler(void) {
    uint32_t intid = gic_ack();
    if (intid >= GIC_SPURIOUS) return;
    if (intid == IPC_SGI) {
        shared_ram->ctrl.sgi_count++;
        sgi_pending = 1;
    } else {
        shared_ram->ctrl.other_irqs++;
    }
    gic_eoi(intid);
}

// Wywoływane z exc_entry - zapisujemy przyczynę, żeby Linux mógł ją odczytać w stats
void exc_handler(void) {
    uint64_t el, esr, elr;
    __asm__ volatile ("mrs %0, CurrentEL" : "=r" (el));
    if (((el >> 2) & 3) == 2) {
        __asm__ volatile ("mrs %0, esr_el2" : "=r" (esr));
        __asm__ volatile ("mrs %0, elr_el2" : "=r" (elr));
    } else {
        __asm__ volatile ("mrs %0, esr_el1" : "=r" (esr));
        __asm__ volatile ("mrs %0, elr_el1" : "=r" (elr));
    }
    shared_ram->ctrl.fault_elr = elr;
    shared_ram->ctrl.fault_esr = (uint32_t)esr | 1; // |1 żeby ESR=0 też było widać jako błąd
}

// Powiadomienie Linuksa: SPI ustawione jako pending w dystrybutorze GIC
static void notify_linux(void) {
    gic_raise_spi(IPC_SPI_INTID);
    shared_ram->ctrl.spi_sent++;
}

static void send_to_linux(const char *msg) {
    uint32_t len = 0;
    while (msg[len] != '\0') len++;
    write_to_ring(&shared_ram->bm_to_linux, (const uint8_t *)msg, len);
}

// Parkowanie rdzenia: PSCI CPU_OFF + CPU_ON nie działa w ATF Agilex 5 (rdzeń zostaje w ON_PENDING),
// więc rdzeń zostaje włączony i czeka w pętli poza obszarem firmware'u, aż Linux wgra nowy kod.
extern const uint32_t park_stub[], park_stub_end[];
typedef void (*park_fn)(volatile uint32_t *release, uint64_t unused, uint64_t entry);

static void park(void) {
    irq_disable();
    __asm__ volatile ("msr S3_0_C12_C12_7, xzr; isb"); // ICC_IGRPEN1_EL1 = 0
    volatile uint32_t *dst = (volatile uint32_t *)PARK_ADDR;
    for (const uint32_t *src = park_stub; src < park_stub_end; src++) *dst++ = *src;
    shared_ram->ctrl.magic = 0;
    shared_ram->ctrl.release = 0;
    shared_ram->ctrl.parked = 1;
    __asm__ volatile ("dsb sy; isb" : : : "memory");
    ((park_fn)PARK_ADDR)(&shared_ram->ctrl.release, 0, FW_ENTRY);
}

static int work_pending(void) {
    return sgi_pending || shared_ram->ctrl.stop_req ||
           shared_ram->linux_to_bm.head != shared_ram->linux_to_bm.tail;
}

int main(void) {
    // Zerowanie całego bloku IPC (ringi + blok kontrolny)
    for (uint32_t i = 0; i < sizeof(SharedMemoryMap); i++) {
        ((volatile uint8_t*)shared_ram)[i] = 0;
    }

    uint64_t mpidr, el;
    __asm__ volatile ("mrs %0, mpidr_el1" : "=r" (mpidr));
    __asm__ volatile ("mrs %0, CurrentEL" : "=r" (el));
    shared_ram->ctrl.bm_mpidr = (uint32_t)(mpidr & 0xFFFFFF);
    shared_ram->ctrl.current_el = (uint32_t)((el >> 2) & 3);
    shared_ram->ctrl.gic_status = (uint32_t)gic_cpu_init(IPC_SGI);

    gic_snapshot(shared_ram->ctrl.dbg);

    // Autotest: SGI do siebie z odmaskowanym IRQ - czy ścieżka odbioru działa
    if (shared_ram->ctrl.gic_status == 0) {
        gic_send_sgi_self(IPC_SGI);
        irq_enable();
        for (volatile uint32_t t = 0; t < 1000000 && !sgi_pending; t++) { }
        irq_disable();
        shared_ram->ctrl.selftest = sgi_pending;
        sgi_pending = 0;
        shared_ram->ctrl.sgi_count = 0;
    }

    __asm__ volatile ("dmb sy" : : : "memory");
    shared_ram->ctrl.magic = IPC_MAGIC;

    if (shared_ram->ctrl.gic_status == 0)
        send_to_linux("SYSTEM BARE METAL: CZEKA NA PRZERWANIA (WFI)\n");
    else
        send_to_linux("SYSTEM BARE METAL: BLAD GIC\n");
    notify_linux();

    // Bez działającego GIC zostaje tryb odpytywania (jak dawniej), żeby stop/komendy działały
    int gic_ok = (shared_ram->ctrl.gic_status == 0 && shared_ram->ctrl.selftest);
    uint8_t rx_buffer[64];

    // Główna pętla: rdzeń śpi w WFI, budzi go SGI z Linuksa
    while (1) {
        // IRQ zamaskowane podczas sprawdzania warunku - WFI i tak się wybudzi, gdy przerwanie
        // stanie się pending, więc nie zgubimy SGI, które przyszło między sprawdzeniem a WFI
        irq_disable();
        while (!work_pending() && gic_ok) {
            __asm__ volatile ("wfi" : : : "memory");
            shared_ram->ctrl.wakeups++;
            irq_enable();   // tu wykonuje się irq_handler
            __asm__ volatile ("isb");
            irq_disable();
        }
        sgi_pending = 0;
        irq_enable();
        if (!gic_ok) gic_snapshot(shared_ram->ctrl.dbg); // diagnostyka w trybie odpytywania

        if (shared_ram->ctrl.stop_req) {
            park();
        }

        // Odbiór wszystkiego, co Linux zostawił w ringu
        int replied = 0;
        while (shared_ram->linux_to_bm.head != shared_ram->linux_to_bm.tail) {
            int bytes_read = read_from_ring(&shared_ram->linux_to_bm, rx_buffer, sizeof(rx_buffer) - 1);
            if (bytes_read <= 0) break;
            rx_buffer[bytes_read] = '\0'; // Znak końca tekstu

            // Odsyłamy Echo z powrotem do Linuksa, jako dowód odebrania
            if (!replied) send_to_linux("BM ODEBRAL: ");
            write_to_ring(&shared_ram->bm_to_linux, rx_buffer, bytes_read);
            replied = 1;

            // === TUTAJ W PRZYSZŁOŚCI BĘDZIESZ SPRAWDZAĆ KOMENDY ===
            // np. if (rx_buffer[0] == 'L') { zapal_diode(); }
        }
        if (replied) notify_linux();
    }
    return 0;
}
