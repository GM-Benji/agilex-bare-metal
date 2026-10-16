# Agilex 5 bare-metal A55 (AMP)

Firmware for the 4th HPS core (A55, MPIDR 0x100) of the DE25-Nano. Linux loads it to `0xA0100000` and starts it with PSCI `CPU_ON` through the `wake_a55` driver in [agilex5-linux-amp](https://github.com/GM-Benji/agilex5-linux-amp).

## Build
```bash
cmake -S . -B build        # finds aarch64-none-elf-gcc (~/arm-gnu-toolchain/bin) or falls back to aarch64-linux-gnu-gcc
cmake --build build        # -> build/baremetal_a55.bin
# a different toolchain: cmake -S . -B build -DCROSS=/path/to/aarch64-none-elf-
```

## How IPC works
* Two ring buffers + an `IpcControl` block at `0xA0180000` (`inc/ring_buffer.h`, must match `wake_a55.c`).
* The core sleeps in `WFI` (`src/main.c`). Linux writes into `linux_to_bm` and sends **SGI 7**. The core wakes, echoes the data into `bm_to_linux` and raises **SPI 500** (INTID 532) in `GICD_ISPENDR`, which gives Linux an IRQ.
* `src/gic.c` sets up only this core's redistributor and CPU interface (the GIC distributor belongs to Linux):
  * finds its frame by matching `GICR_TYPER` to `MPIDR`
  * enables SGI 7
  * sets `ICC_SRE_EL2`, `PMR`, `IGRPEN1`
* `src/startup.S`:
  * sets the 2 KB-aligned vector table (`VBAR_EL2`)
  * sets `HCR_EL2.IMO/FMO/AMO`, which is needed because the core runs at EL2 (PSCI starts it at the same EL as Linux), and with IMO=0 IRQs are never taken
* At boot the core sends SGI 7 to itself as a self-test (`bm_selftest` in `/sys/kernel/baremetal/gic`). If GIC setup fails, it falls back to polling.
* SGI 8-15 cannot be used: ATF makes them Secure.
* Stop: Linux sets `stop_req`, and the core parks itself at `0xA017F000` (PSCI `CPU_OFF` + `CPU_ON` does not work in the Agilex 5 ATF), so new firmware can be loaded without a reboot.

Constraints: the MMU is off, so all memory is Device type. The code is built with `-mstrict-align -mgeneral-regs-only` (no unaligned access, no FP/SIMD registers in the IRQ handler).
