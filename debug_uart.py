#!/usr/bin/env python3
"""Query DirtyJTAG UART bridge debug state via CMD_UART_DEBUG."""
import usb.core
import struct
import sys
import time

VID, PID = 0x1209, 0xC0CA
EP_OUT, EP_IN = 0x01, 0x82
CMD_UART_DEBUG = 0x07
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
    # Send CMD_UART_DEBUG + CMD_STOP
    dev.write(EP_OUT, bytes([CMD_UART_DEBUG, CMD_STOP]))
    try:
        data = dev.read(EP_IN, 64, timeout=1000)
    except usb.core.USBTimeoutError:
        print("Timeout reading response")
        continue

    if len(data) < 14:
        print(f"Short response: {len(data)} bytes")
        continue

    # Explicit little-endian wire format: rx, tx, configured, busy, baudrate
    rx_bytes, tx_bytes, configured, busy, baudrate = struct.unpack('<IIBBI', data[:14])
    print(f"UART RX→USB: {rx_bytes} bytes")
    print(f"USB→UART TX: {tx_bytes} bytes")
    print(f"CDC configured (DTR): {configured}")
    print(f"CDC TX busy: {busy}")
    print(f"Baudrate: {baudrate}")

    time.sleep(1)
