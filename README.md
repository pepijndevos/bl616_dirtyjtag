# BL616 DirtyJTAG

Open-source replacement JTAG/UART firmware for the **Tang Nano 20K**'s onboard BL616 MCU.

Provides a [DirtyJTAG v2](https://github.com/jeanthom/DirtyJTAG) compatible JTAG interface alongside a CDC-ACM UART bridge as a composite USB device. Works with [openFPGALoader](https://github.com/trabucayre/openFPGALoader) out of the box.

## Why?

The Tang Nano 20K ships with Sipeed's proprietary BL616 firmware that emulates an FTDI FT2232. This firmware has known reliability issues with JTAG programming. Since the BL616 has eFuse secure boot enabled at the factory, you cannot simply replace the firmware at address 0x0.

This project works around the limitation by using Sipeed's **partner firmware** at flash address 0x0 (which handles secure boot) and installs DirtyJTAG at address 0x40000. The partner firmware hands off to the custom firmware when no USB host is initially detected at boot.

## Features

- **DirtyJTAG v2 protocol** — compatible with openFPGALoader (`-c dirtyJtag`)
- **CDC-ACM UART bridge** — serial port on UART1 (GPIO11 TX, GPIO13 RX) with configurable baud rate
- **Composite USB device** — JTAG and UART available simultaneously
- **Minimal footprint** — ~76 KB firmware, runs on FreeRTOS

## Hardware

- [Sipeed Tang Nano 20K](https://wiki.sipeed.com/hardware/en/tang/tang-nano-20k/nano-20k.html) (GW2AR-18 FPGA with onboard BL616)

### Pin assignments

| Function | GPIO | BL616 Pin |
|----------|------|-----------|
| JTAG TMS | GPIO16 | To FPGA |
| JTAG TCK | GPIO10 | To FPGA |
| JTAG TDI | GPIO12 | To FPGA |
| JTAG TDO | GPIO14 | To FPGA |
| UART1 TX | GPIO11 | To FPGA |
| UART1 RX | GPIO13 | To FPGA |

## USB identity

| Field | Value |
|-------|-------|
| VID | 0x1209 |
| PID | 0xC0CA |
| Product | DirtyJTAG |

Registered via [pid.codes](https://pid.codes/) (the DirtyJTAG project allocation).

## Prerequisites

- [Bouffalo SDK](https://github.com/bouffalolab/bouffalo_sdk) (tested with v2.3.16)
- Sipeed partner firmware binary: `bl616_fpga_partner_20kNano.bin`
  - Download from [Sipeed](https://dl.sipeed.com/shareURL/TANG/Debugger/onboard/BL616/2025030317)
  - Place in the project root directory

## Building

Clone the SDK (or symlink) into the project directory:

```bash
git clone https://github.com/bouffalolab/bouffalo_sdk.git
# or: ln -s /path/to/bouffalo_sdk .
```

Build:

```bash
export PATH=$(pwd)/bouffalo_sdk/tools/toolchain_gcc_t-head_linux/bin:$PATH
make CHIP=bl616 BOARD=bl616dk
```

## Flashing

The flash configuration (`flash_prog_cfg.ini`) writes two images:
1. **Sipeed partner firmware** at 0x0 — encrypted first-stage that handles secure boot
2. **DirtyJTAG firmware** at 0x40000 — this project's firmware

To flash:

1. Hold the **BOOT** button (next to the HDMI connector) while plugging in USB
2. The BL616 will appear as `Bouffalo CDC DEMO` on a virtual COM port
3. Run:

```bash
make flash CHIP=bl616 COMX=/dev/ttyACM0
```

## Tang Nano 9K (BL702) target

The same firmware also builds for the **Tang Nano 9K**'s onboard BL702 (same USB identity, DirtyJTAG v2 + CDC-ACM). The BL702 has no secure boot, so the image (with its normal bootheader) goes straight to flash 0x0 — no partner firmware, no boot2.

| Function | GPIO |
|----------|------|
| JTAG TMS | GPIO2  |
| JTAG TCK | GPIO15 |
| JTAG TDI | GPIO0  |
| JTAG TDO | GPIO1  |
| UART1 TX (to FPGA_RX) | GPIO26 |
| UART1 RX (from FPGA_TX) | GPIO27 |

USB is full-speed on the BL702; the CDC endpoints are 0x83/0x04/0x85 because BL702 endpoints are unidirectional (BL616 keeps 0x83/0x03/0x84).

Build (output in `build_bl702/`):

```bash
export PATH=$(pwd)/bouffalo_sdk/tools/toolchain_gcc_t-head_linux/bin:$PATH
make CHIP=bl702 BOARD=bl702dk
```

The USB serial number is the chip's factory MAC (e.g. `54f5634cb97c`), so every board
gets its own `/dev/serial/by-id/` name.

Flash: hold **Boot_Strap (GPIO28) high** while plugging in USB to enter the BL702 ROM download mode, then:

```bash
bouffalo_sdk/tools/bflb_tools/bouffalo_flash_cube/BLFlashCommand-ubuntu \
    --interface=uart --baudrate=2000000 --port=/dev/ttyACM0 \
    --chipname=bl702 --config=flash_prog_cfg_bl702.ini
```

This overwrites Sipeed's FT2232-emulation firmware at 0x0. Use `-b tangnano9k` with openFPGALoader.

## Boot sequence

The Tang Nano 20K's BL616 has **eFuse secure boot** enabled by Sipeed at the factory. This means:

1. The ROM bootloader only executes AES-128 encrypted firmware at flash address 0x0
2. The partner firmware at 0x0 runs first
3. **If no USB host is detected** at boot, the partner firmware loads and executes the firmware at 0x40000
4. If USB is detected, the partner firmware stays in FTDI bridge mode

### Important: power-on sequence

Because the handoff only occurs when no USB is initially detected, you need to:

1. Power the Tang Nano 20K **without** a USB data connection (e.g., power-only cable, external supply, or USB hub with data lines disconnected)
2. After the DirtyJTAG firmware boots, connect the USB data lines
3. The device will enumerate as `1209:c0ca DirtyJTAG`

Alternatively, if the FPGA is configured to not pull the USB detect lines, the handoff may occur automatically. The exact detection mechanism is internal to Sipeed's encrypted partner firmware.

## Usage

### JTAG (openFPGALoader)

```bash
# Detect FPGA
openFPGALoader -c dirtyJtag --detect

# Program bitstream
openFPGALoader -c dirtyJtag -b tangnano20k bitstream.fs

# Program to flash
openFPGALoader -c dirtyJtag -b tangnano20k -f bitstream.fs
```

### UART

The CDC-ACM interface appears as `/dev/ttyACMx`. Connect with any serial terminal:

```bash
picocom /dev/ttyACM1 -b 115200
```

The baud rate is configurable via the standard CDC SET_LINE_CODING request (most terminal programs handle this automatically).

## Project structure

```
├── Makefile              # SDK build system entry point
├── CMakeLists.txt        # Project config (FreeRTOS + CherryUSB)
├── FreeRTOSConfig.h      # FreeRTOS configuration
├── usb_config.h          # CherryUSB device configuration
├── flash_prog_cfg.ini    # Flash programming layout (BL616)
├── flash_prog_cfg_bl702.ini # Flash programming layout (BL702, 0x0 only)
└── src/
    ├── main.c            # Entry point, USB + FreeRTOS init
    ├── usb_descriptors.c # Composite USB descriptors
    ├── usb_eps.h         # USB endpoint numbers (per chip)
    ├── dirtyjtag.c/h     # DirtyJTAG v2 protocol handler
    ├── jtag_gpio.c/h     # GPIO bit-bang JTAG engine
    └── uart_bridge.c/h   # CDC-ACM to UART1 bridge
```

## Known limitations

- The partner firmware handoff requires initial boot without USB data connection
- Bulk endpoint max packet size is 64 bytes (openFPGALoader expects this, but Linux logs warnings about HS compliance)
- JTAG frequency control is approximate (delay loop calibration)
- No TRST/SRST support (not connected on Tang Nano 20K)

## Background: Tang Nano 20K secure boot

Starting around early 2024, Sipeed began shipping Tang Nano 20K boards with **eFuse secure boot** enabled on the BL616 MCU. The eFuses contain an AES key and the secure boot flag is permanently blown, meaning:

- The ROM bootloader at address 0x0 only accepts encrypted firmware
- You cannot flash custom unencrypted firmware directly
- The encryption key is not publicly available

The **partner firmware** (`bl616_fpga_partner_20kNano.bin`) is a Sipeed-signed encrypted binary that:
- Provides FTDI FT2232 emulation for JTAG/UART (the factory default behavior)
- Supports loading a secondary unencrypted firmware from flash address 0x40000
- Decides at boot whether to run as FTDI bridge (USB detected) or hand off to 0x40000 (standalone)

For more details, see:
- [Tang Nano 20K variants](https://github.com/MiSTle-Dev/.github/wiki/Versions_TangNano20k)
- [nand2mario's writeup on the BL616 architecture](https://nand2mario.github.io/posts/2025/mcu_for_better_fpga_gaming/)
- [FPGA-Companion project](https://github.com/MiSTle-Dev/FPGA-Companion)

## Credits

- [DirtyJTAG](https://github.com/jeanthom/DirtyJTAG) by Jean THOMAS — protocol and command implementation (MIT license)
- [pico-dirtyJtag](https://github.com/phdussud/pico-dirtyJtag) — reference implementation
- [FPGA-Companion](https://github.com/MiSTle-Dev/FPGA-Companion) — partner firmware documentation and flash layout
- [firmware-bl616](https://github.com/nand2mario/firmware-bl616) — BL616 USB/GPIO reference for Tang boards
- [Bouffalo SDK](https://github.com/bouffalolab/bouffalo_sdk) — BL616 HAL and CherryUSB stack

## License

MIT
