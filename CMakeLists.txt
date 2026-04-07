cmake_minimum_required(VERSION 3.15)
project(BaremetalA55 C ASM)

# Podpinamy skrypt linkera
set(CMAKE_EXE_LINKER_FLAGS "-T ${CMAKE_SOURCE_DIR}/linker.ld -nostdlib")

add_executable(baremetal_a55.elf 
    src/startup.S
    src/main.c
    src/ring_buffer.c
)


set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-warn-rwx-segments")

# Wskazujemy kompilatorowi folder z plikami nagłówkowymi (.h)
target_include_directories(baremetal_a55.elf PRIVATE inc)

add_custom_command(TARGET baremetal_a55.elf POST_BUILD
    COMMAND /home/benji/arm-gnu-toolchain/bin/aarch64-none-elf-objcopy -O binary baremetal_a55.elf baremetal_a55.bin
    COMMENT "Generowanie pliku .bin dla U-Boota"
)