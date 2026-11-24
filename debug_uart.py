#!/usr/bin/env python3
"""Query DirtyJTAG UART bridge debug state via CMD_INFO."""
import usb.core
import struct
import sys
import time

VID, PID = 0x1209, 0xC0CA
EP_OUT, EP_IN = 0x01, 0x82
CMD_INFO = 0x01
CMD_STOP = 0x00

dev = usb.core.find(idVendor=VID, idProduct=PID)
if dev is None:
    print("DirtyJTAG device not found")
    sys.exit(1)

# Claim interface 0 (vendor bulk) — config already set by kernel CDC driver
if dev.is_kernel_driver_active(0):
    dev.detach_kernel_driver(0)
usb.util.claim_interface(dev, 0)

while True:
    # Send CMD_INFO + CMD_STOP
    dev.write(EP_OUT, bytes([CMD_INFO, CMD_STOP]))
    try:
        data = dev.read(EP_IN, 64, timeout=1000)
    except usb.core.USBTimeoutError:
        print("Timeout reading response")
        continue

    if len(data) < 10:
        print(f"Short response: {len(data)} bytes")
        continue

    info_str = bytes(data[:10]).rstrip(b'\x00')
    print(f"Info: {info_str}")

    if len(data) >= 26:
        # Parse debug state after the 10-byte info string
        # struct uart_debug_state { uint32_t rx, tx; uint8_t configured, busy; 2 pad; uint32_t baud }
        # Total struct: 16 bytes, sizeof confirms with padding
        rx_bytes, tx_bytes, configured, busy, baudrate = struct.unpack_from('<IIBBxxI', data, 10)
        print(f"  UART RX→USB: {rx_bytes} bytes")
        print(f"  USB→UART TX: {tx_bytes} bytes")
        print(f"  CDC configured (DTR): {configured}")
        print(f"  CDC TX busy: {busy}")
        print(f"  Baudrate: {baudrate}")
        if len(data) >= 50:
            # BL702 only: struct usb_dc_debug (src/usb_dc_debug.h), six uint32
            (isr, setup_to, out_to, setup_bad,
             wait_max, deferred) = struct.unpack_from('<6I', data, 26)
            print(f"  USB ISR count: {isr}")
            print(f"  EP0 SETUP wait timeouts: {setup_to}")
            print(f"  EP0 OUT wait timeouts: {out_to}")
            print(f"  EP0 bad SETUP length: {setup_bad}")
            print(f"  EP0 longest RDY wait: {wait_max} us")
            print(f"  EP DONE deferred (RDY held): {deferred}")
    else:
        print(f"  No debug data (response {len(data)} bytes)")

    time.sleep(1)
