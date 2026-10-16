set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Prefiks kompilatora: -DCROSS=... albo automatycznie
# (aarch64-none-elf z ~/arm-gnu-toolchain na laptopie, inaczej aarch64-linux-gnu z apt)
if(NOT CROSS)
    find_program(NONE_ELF_GCC aarch64-none-elf-gcc HINTS $ENV{HOME}/arm-gnu-toolchain/bin)
    if(NONE_ELF_GCC)
        string(REGEX REPLACE "gcc$" "" CROSS "${NONE_ELF_GCC}")
    else()
        set(CROSS aarch64-linux-gnu-)
    endif()
endif()

set(CMAKE_C_COMPILER "${CROSS}gcc")
set(CMAKE_ASM_COMPILER "${CROSS}gcc")
set(CMAKE_OBJCOPY "${CROSS}objcopy" CACHE FILEPATH "" FORCE)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
