#ifndef USB_DC_DEBUG_H
#define USB_DC_DEBUG_H

#include <stdint.h>

/*
 * BL702 USB device driver (src/bflb_usb_v1.c) counters, appended to the
 * CMD_INFO reply after struct uart_debug_state.  Append new fields at the end.
 */
struct usb_dc_debug {
    uint32_t isr_count;               /* USB interrupts taken */
    uint32_t ep0_setup_wait_timeouts; /* EP0 RDY still held 1 ms after SETUP_DONE */
    uint32_t ep0_out_wait_timeouts;   /* EP0 RDY still held 1 ms after OUT_DONE */
    uint32_t ep0_setup_bad_len;       /* SETUP_DONE with != 8 bytes in the FIFO */
    uint32_t ep0_wait_max_us;         /* longest EP0 RDY wait seen, us */
    uint32_t ep_done_deferred;        /* EP1..7 DONE left latched, RDY still held */
};

void usb_dc_get_debug(struct usb_dc_debug *dbg);

#endif
