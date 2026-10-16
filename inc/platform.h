#ifndef PLATFORM_H
#define PLATFORM_H

// Bezpieczny adres wewnątrz strefy no-map (512 KB za Twoim kodem)
#define SHARED_MEM_BASE  0xA0180000 
#define AXI_FPGA_BASE    0x20000000 // Przykładowy adres mostu FPGA
#define FW_ENTRY         0xA0100000  // tu Linux wgrywa firmware i tu startuje rdzen
// Ostatnie 4 KB przed SHARED_MEM: petla parkowania na czas wgrywania nowego firmware'u
#define PARK_ADDR        0xA017F000

// --- GIC-600 (GICv3) Agilex 5 ---
#define GICD_BASE        0x1D000000UL
#define GICR_BASE        0x1D060000UL  // ramka redystrybutora rdzenia 0
#define GICR_STRIDE      0x20000UL     // RD_base + SGI_base (2 x 64 KB)
#define GICR_MAX_FRAMES  4

// --- Przerwania IPC (muszą pasować do wake_a55.c) ---
// SGI 8-15 są Secure w ATF (INTEL_SOCFPGA_IRQ_SEC_SGI_*), więc Non-secure ma tylko 0-7
#define IPC_SGI          7             // Linux -> BM
#define IPC_SPI          500           // BM -> Linux (numer SPI, jak w DT)
#define IPC_SPI_INTID    (32 + IPC_SPI)

#endif
