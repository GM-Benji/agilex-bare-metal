#include "platform.h"
#include "ring_buffer.h"

int main(void) {
    SharedMemoryMap *shared_ram = (SharedMemoryMap *)SHARED_MEM_BASE;
    // Zerowanie całego bloku IPC (2064 bajty)
    for (int i = 0; i < sizeof(SharedMemoryMap); i++) {
        ((volatile uint8_t*)shared_ram)[i] = 0;
    }
    
    // Inicjalizacja: Bare Metal resetuje nadajnik i swój wskaźnik odbioru
    shared_ram->bm_to_linux.head = 0;
    shared_ram->bm_to_linux.tail = 0;
    shared_ram->linux_to_bm.tail = 0; 
    
    const char *msg = "SYSTEM BARE METAL: NASLUCHUJE...\n";
    uint32_t len = 0;
    while(msg[len] != '\0') len++;
    write_to_ring(&shared_ram->bm_to_linux, (const uint8_t *)msg, len);

    uint8_t rx_buffer[64];

    // Główna pętla asynchroniczna
    while (1) {
        // Sprawdzamy, czy wskaźnik nadawczy Linuksa uciekł do przodu
        if (shared_ram->linux_to_bm.head != shared_ram->linux_to_bm.tail) {
            
            // Odczytujemy paczkę danych
            int bytes_read = read_from_ring(&shared_ram->linux_to_bm, rx_buffer, sizeof(rx_buffer) - 1);
            
            if (bytes_read > 0) {
                rx_buffer[bytes_read] = '\0'; // Znak końca tekstu
                
                // Odsyłamy Echo z powrotem do Linuksa, jako dowód odebrania
                const char *reply = "BM ODEBRAL: ";
                write_to_ring(&shared_ram->bm_to_linux, (const uint8_t *)reply, 12);
                write_to_ring(&shared_ram->bm_to_linux, rx_buffer, bytes_read);
                
                // === TUTAJ W PRZYSZŁOŚCI BĘDZIESZ SPRAWDZAĆ KOMENDY ===
                // np. if (rx_buffer[0] == 'L') { zapal_diode(); }
            }
        }
    }
    return 0;
}