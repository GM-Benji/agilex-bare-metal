cmake_minimum_required(VERSION 3.15)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/toolchain.cmake)
endif()

project(BaremetalA55 C ASM)

# Podpinamy skrypt linkera
set(CMAKE_EXE_LINKER_FLAGS "-T ${CMAKE_SOURCE_DIR}/linker.ld -nostdlib -static -no-pie -Wl,--build-id=none")

add_executable(baremetal_a55.elf 
    src/startup.S
    src/main.c
    src/ring_buffer.c
    src/gic.c
    src/libc.c
)


set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-warn-rwx-segments")

# MMU wyłączone = pamięć typu Device -> brak dostępów niewyrównanych (-mstrict-align),
# brak FP/SIMD (nie zapisujemy ich w obsłudze IRQ), brak wywołań do libgcc
target_compile_options(baremetal_a55.elf PRIVATE
    -O2 -ffreestanding -fno-builtin -fno-pie -mgeneral-regs-only -mstrict-align
    -mno-outline-atomics -fno-asynchronous-unwind-tables -fno-stack-protector -Wall
)

# Wskazujemy kompilatorowi folder z plikami nagłówkowymi (.h)
target_include_directories(baremetal_a55.elf PRIVATE inc)

add_custom_command(TARGET baremetal_a55.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary baremetal_a55.elf baremetal_a55.bin
    COMMENT "Generowanie pliku baremetal_a55.bin"
)
