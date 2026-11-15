# Tang Primer 20K Dock (BL702) DirtyJTAG

Open-source replacement JTAG/UART firmware for the **Tang Primer 20K Dock**'s onboard BL702 MCU.

Provides a [DirtyJTAG v2](https://github.com/jeanthom/DirtyJTAG) compatible JTAG interface alongside a CDC-ACM UART bridge as a composite USB device. Works with [openFPGALoader](https://github.com/trabucayre/openFPGALoader) out of the box.

Based on the excellent work by [@pepijndevos](https://github.com/pepijndevos) for the [BL616 on the Tang Nano 20K](https://github.com/pepijndevos/bl616_dirtyjtag).

## Why?

The default BL702 MCU firmware shipped with the Tang Primer 20K Dock was intermittently failing when flashing FPGA bitstreams via openFPGALoader. Most files would transfer correctly, but some would error and refuse to complete. Replacing the firmware with a clean, open-source implementation eliminates this unreliability.

## Features

- **DirtyJTAG v2 protocol** — compatible with openFPGALoader (`-c dirtyJtag`)
- **CDC-ACM UART bridge** — serial port on UART1 (GPIO_24 TX, GPIO_25 RX) with configurable baud rate, bridged to the FPGA
- **Composite USB device** — JTAG and UART available simultaneously
- **Console UART** — UART0 on GPIO_14 TX / GPIO_23 RX, available at the J2 header (unpopulated by default)
- **Minimal footprint** — firmware fits in a single flash image, runs on FreeRTOS

## Hardware

- [Sipeed Tang Primer 20K Dock](https://wiki.sipeed.com/hardware/en/tang/tang-primer-20k/primer-20k.html) (GW2A FPGA with onboard BL702)

### Pin assignments

The BL702 routes JTAG and UART signals to the FPGA via a resistor network. The pin assignments below are taken from the dock schematic (P001_Bl702, Rev 1.1) and verified empirically.

| Function | BL702 GPIO | Notes |
|----------|-----------|-------|
| JTAG TMS | GPIO_2 | Output to FPGA |
| JTAG TCK | GPIO_15 | Output to FPGA |
| JTAG TDI | GPIO_0 | Output to FPGA |
| JTAG TDO | GPIO_1 | Input from FPGA |
| UART1 TX | GPIO_24 | CDC-ACM bridge to FPGA |
| UART1 RX | GPIO_25 | CDC-ACM bridge from FPGA |
| Console UART0 TX | GPIO_14 | Available at unpopulated J2 header |
| Console UART0 RX | GPIO_23 | Available at unpopulated J2 header |

## USB identity

| Field | Value |
|-------|-------|
| VID | 0x1209 |
| PID | 0xC0CA |
| Product | DirtyJTAG |

Registered via [pid.codes](https://pid.codes/) (the DirtyJTAG project allocation).

## Prerequisites

The Bouffalo SDK and the Xuantie/T-Head RISC-V toolchain are both included as Git submodules. Initialise them after cloning:

```bash
git submodule update --init
```

This populates `bouffalo_sdk/` and `toolchain/` (the patched `riscv64-unknown-elf-gcc` 10.2.0 cross-compiler that the SDK expects).

Add the toolchain's `bin` directory to your `PATH`:

```bash
export PATH="$(pwd)/toolchain/bin:$PATH"
```

## Building

The project uses CMake via the Bouffalo SDK's build system wrapper. The default target is `CHIP=bl702 BOARD=tang_primer_20k`.

```bash
make
```

The firmware binary is written to:

```
build/build_out/dirtyjtag_tangprimer20k_bl702.bin
```

## Testing

Host-side regression tests run on a PC — no board, no cross-toolchain:

```bash
make test
```

They compile the real `src/` sources against small fakes (`tests/`) and cover
the DirtyJTAG command parser, its response buffers, and the USB descriptor
tables. The build uses AddressSanitizer, so buffer overruns in the firmware
fail the run rather than corrupting memory silently.

`test_tx_buf_bounds` currently **fails**: `cmd_handle()` writes 78 bytes into
its 64-byte `tx_buf` when a single packet carries several `CMD_INFO`
commands. The test reproduces the overrun and is expected to fail until that
handler bounds-checks its output cursor.

JTAG electrical timing (TDI setup before TCK, TDO sample point) is not covered
here — a host test has no time axis. Verify that on hardware with
`openFPGALoader -c dirtyJtag --detect` followed by an SRAM write.

## Flashing

The BL702 on the Tang Primer 20K Dock does **not** have secure boot enabled, so the firmware can be flashed as a single image at address `0x0` using [blisp](https://github.com/pine64/blisp):

1. Hold the **BOOT** button while plugging in USB, or short the BOOT pin to ground.
2. The BL702 enters ROM bootloader mode.
3. Run:

```bash
make upload PORT=/dev/ttyUSB0
```

Or directly with blisp:

```bash
blisp write -c bl70x -p /dev/ttyUSB0 --reset build/build_out/dirtyjtag_tangprimer20k_bl702.bin
```

On some boards, the bootloader enumerates as a CDC device. If so, replace the port path with the correct `/dev/ttyACMx` node.

After flashing, reset the board. The device will enumerate as `1209:c0ca DirtyJTAG`.

## Usage

### JTAG (openFPGALoader)

```bash
# Detect FPGA
openFPGALoader -c dirtyJtag --detect

# Program bitstream to SRAM
openFPGALoader -c dirtyJtag -b tangprimer20k bitstream.fs

# Program bitstream to flash
openFPGALoader -c dirtyJtag -b tangprimer20k -f bitstream.fs
```

### UART (CDC-ACM bridge)

The CDC-ACM interface appears as `/dev/ttyACMx`. Connect with any serial terminal:

```bash
picocom /dev/ttyACM0 -b 115200
```

The baud rate is configurable via the standard CDC SET_LINE_CODING request (most terminal programs handle this automatically). The bridge carries data between the host and the FPGA's UART via UART1 (GPIO_24/25).

### Console UART

The console UART0 (`printf` output from the BL702 firmware) is routed to GPIO_14 TX / GPIO_23 RX, which correspond to the J2 header on the dock. This header is shipped unpopulated, so observing the console requires soldering to those pads or populating the header. The console operates at 2 Mbaud.

To observe the console output:

1. Solder to the J2 TX pad (GPIO_14) and GND. The RX pad (GPIO_23) is only needed for bidirectional use.
2. Connect a 3.3 V USB-TTL adapter or logic analyser.
3. Open a terminal at 2 Mbaud and reset the board.

If your adapter does not support 2 Mbaud reliably, lower the baud rate temporarily in `src/board/tang_primer_20k/board.c` before rebuilding.

The console UART is completely untested and not verified to function.

## Project structure

```
├── Makefile                    # SDK build wrapper (CMake underneath)
├── CMakeLists.txt              # Project configuration
├── default.nix                 # Nix dev shell with toolchain
├── src/
│   ├── board/
│   │   ├── pins.h              # JTAG and UART pin definitions
│   │   └── tang_primer_20k/
│   │       ├── board.c         # Clock init, console UART, boot banner
│   │       ├── board.h         # Board header
│   │       ├── CMakeLists.txt  # Board-specific build rules
│   │       └── bl702_flash.ld  # Linker script
│   ├── main.c                  # Entry point: init, USB, FreeRTOS
│   ├── usb_descriptors.c       # Composite USB descriptors (JTAG + CDC-ACM)
│   ├── dirtyjtag.c/h           # DirtyJTAG v2 protocol handler
│   ├── jtag_gpio.c/h           # GPIO bit-bang JTAG engine
│   └── uart_bridge.c/h         # CDC-ACM to UART1 bridge
└── bouffalo_sdk/               # SDK submodule (Git)
```

## Known limitations

- **J2 header is unpopulated** — the console UART0 output is available but not accessible without soldering.
- **JTAG frequency control is approximate** — delay loop calibration, not hardware-timed.
- **No TRST/SRST support** — not connected between the BL702 and the FPGA on this board.
- **Bulk endpoint max packet size is 64 bytes** — openFPGALoader expects this, but Linux may log warnings about high-speed compliance because the device enumerates as USB 2.0.

## Background

This port started from the BL616 DirtyJTAG project by Pepijn de Vos, which targets the Tang Nano 20K. The Tang Primer 20K Dock uses a different MCU (BL702 instead of BL616) and a different flash layout (single image, no secure boot partner firmware), so the GPIO HAL, clock tree, USB endpoint numbering, and build system all needed adjustment. The BL702's GPIO mux is more flexible than the BL616's, which simplifies pin assignment but requires care when mapping UART signals to pads.

## Credits

- [DirtyJTAG](https://github.com/jeanthom/DirtyJTAG) by Jean THOMAS — protocol and command implementation (MIT license)
- [pico-dirtyJtag](https://github.com/phdussud/pico-dirtyJtag) — reference implementation
- [bl616_dirtyjtag](https://github.com/pepijndevos/bl616_dirtyjtag) by Pepijn de Vos — original BL616 port for Tang Nano 20K
- [Bouffalo SDK](https://github.com/bouffalolab/bouffalo_sdk) — BL702 HAL, FreeRTOS, and CherryUSB stack

## License

MIT
