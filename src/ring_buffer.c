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


int write_to_ring(RingBuffer *rb, const uint8_t *buffer, uint32_t length) {
    uint32_t next_head;
    uint32_t i;

    for (i = 0; i < length; i++) {
        next_head = (rb->head + 1) % RING_BUFFER_SIZE;
        
        if (next_head == rb->tail) {
            break; // Bufor pełny
        }
        
        rb->data[rb->head] = buffer[i];
        
        // BARIERA 1: Upewnij się, że bajt danych fizycznie dotarł do RAM, 
        // zanim przesuniemy wskaźnik head.
        __asm__ volatile ("dmb sy" : : : "memory");
        
        rb->head = next_head;
        
        // BARIERA 2: Upewnij się, że nowa wartość head dotarła do RAM,
        // żeby Linux natychmiast zauważył, że są nowe dane.
        __asm__ volatile ("dmb sy" : : : "memory");
    }
    
    return i; 
}

int read_from_ring(RingBuffer *rb, uint8_t *buffer, uint32_t max_length) {
    uint32_t i = 0;
    
    while (i < max_length && rb->tail != rb->head) {
        buffer[i] = rb->data[rb->tail];
        
        // BARIERA: Upewnij się, że skopiowaliśmy bajt do naszego bufora (buffer),
        // zanim przesuniemy wskaźnik tail i pozwolimy Linuksowi go nadpisać.
        __asm__ volatile ("dmb sy" : : : "memory");
        
        rb->tail = (rb->tail + 1) % RING_BUFFER_SIZE;
        
        __asm__ volatile ("dmb sy" : : : "memory");
        i++;
    }
    return i; 
}