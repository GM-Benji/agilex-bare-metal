#ifndef GIC_H
#define GIC_H

#include <stdint.h>

#define GIC_SPURIOUS 1020

// Konfiguruje tylko WŁASNY redystrybutor i interfejs CPU (dystrybutor należy do Linuksa)
int gic_cpu_init(uint32_t sgi);
uint32_t gic_ack(void);
void gic_eoi(uint32_t intid);
// Ustawia SPI jako pending w dystrybutorze (GICD_ISPENDR, write-1-to-set)
void gic_raise_spi(uint32_t intid);

void gic_send_sgi_self(uint32_t sgi);
void gic_snapshot(volatile uint64_t *d);

#endif
