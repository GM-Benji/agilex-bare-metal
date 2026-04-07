#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>

#define RING_BUFFER_SIZE 1024

typedef struct {
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile uint8_t data[RING_BUFFER_SIZE];
} RingBuffer;

typedef struct {
    RingBuffer linux_to_bm;
    RingBuffer bm_to_linux;
} SharedMemoryMap;

// Prototypy funkcji
int write_to_ring(RingBuffer *rb, const uint8_t *buffer, uint32_t length);
int read_from_ring(RingBuffer *rb, uint8_t *buffer, uint32_t max_length);

#endif