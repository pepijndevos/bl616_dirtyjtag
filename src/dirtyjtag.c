/*
 * DirtyJTAG v2 protocol handler
 *
 * Ported from DirtyJTAG/src/cmd.c (MIT license)
 * Adapted for CherryUSB on BL616
 *
 * Processes DirtyJTAG v2 commands received on a vendor bulk endpoint
 * and responds with JTAG data.
 */
#include <string.h>
#include "usbd_core.h"
#include "dirtyjtag.h"
#include "jtag_gpio.h"
#include "uart_bridge.h"
#include "usb_eps.h"

/* DirtyJTAG v2 command IDs */
enum {
    CMD_STOP   = 0x00,
    CMD_INFO   = 0x01,
    CMD_FREQ   = 0x02,
    CMD_XFER   = 0x03,
    CMD_SETSIG = 0x04,
    CMD_GETSIG = 0x05,
    CMD_CLK    = 0x06,
    CMD_REGS   = 0x0E,  /* debug: raw register dump, not DirtyJTAG */
};

/* Command modifiers */
enum {
    NO_READ       = 0x80,
    EXTEND_LENGTH = 0x40,
    READOUT       = 0x80,
};

/* Signal identifiers */
enum {
    SIG_TCK  = 1 << 1,
    SIG_TDI  = 1 << 2,
    SIG_TDO  = 1 << 3,
    SIG_TMS  = 1 << 4,
    SIG_TRST = 1 << 5,
    SIG_SRST = 1 << 6,
};

static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t rx_buf[64];
static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t tx_buf[64];
static volatile bool ep_tx_busy;

static void cmd_handle(const uint8_t *rxbuf, uint32_t count)
{
    const uint8_t *commands = rxbuf;
    uint8_t *output_buffer = tx_buf;

    while ((commands < (rxbuf + count)) && (*commands != CMD_STOP)) {
        switch ((*commands) & 0x0F) {
        case CMD_INFO: {
            char info_string[10] = "DJTAG2\n";
            memcpy(output_buffer, info_string, 10);
            output_buffer += 10;
            /* Append debug state (14 bytes) after standard info */
            struct uart_debug_state dbg;
            uart_bridge_get_debug(&dbg);
            memcpy(output_buffer, &dbg, sizeof(dbg));
            output_buffer += sizeof(dbg);
            break;
        }

        case CMD_FREQ:
            jtag_set_frequency((commands[1] << 8) | commands[2]);
            commands += 2;
            break;

        case CMD_XFER: {
            bool no_read = *commands & NO_READ;
            uint16_t transferred_bits = commands[1];
            if (*commands & EXTEND_LENGTH)
                transferred_bits += 256;
            /* Cap at 62*8 = 496 bits to avoid buffer overflow */
            if (transferred_bits > 62 * 8)
                transferred_bits = 62 * 8;

            uint16_t trbytes = (transferred_bits + 7) / 8;

            if (!no_read) {
                memset(output_buffer, 0, trbytes);
            }

            jtag_transfer(transferred_bits, commands + 2, no_read ? NULL : output_buffer);

            commands += 1 + trbytes;
            output_buffer += (no_read ? 0 : trbytes);
            break;
        }

        case CMD_SETSIG: {
            uint8_t signal_mask = commands[1];
            uint8_t signal_status = commands[2];

            if (signal_mask & SIG_TCK)
                jtag_set_tck(signal_status & SIG_TCK);
            if (signal_mask & SIG_TDI)
                jtag_set_tdi(signal_status & SIG_TDI);
            if (signal_mask & SIG_TMS)
                jtag_set_tms(signal_status & SIG_TMS);
            if (signal_mask & SIG_TRST)
                jtag_set_trst(signal_status & SIG_TRST);
            if (signal_mask & SIG_SRST)
                jtag_set_srst(signal_status & SIG_SRST);
            commands += 2;
            break;
        }

        case CMD_GETSIG: {
            uint8_t signal_status = 0;
            if (jtag_get_tdo())
                signal_status |= SIG_TDO;
            output_buffer[0] = signal_status;
            output_buffer += 1;
            break;
        }

        case CMD_CLK: {
            bool readout = !!(*commands & READOUT);
            uint8_t signals = commands[1];
            uint8_t clk_pulses = commands[2];

            bool readout_val = jtag_strobe(clk_pulses, signals & SIG_TMS, signals & SIG_TDI);
            if (readout) {
                output_buffer[0] = readout_val ? 0xFF : 0;
                output_buffer += 1;
            }
            commands += 2;
            break;
        }

#if defined(BL702)
        case CMD_REGS: {
            /* 12 raw words + edge count on GPIO27 (FPGA_TX) */
            static const uint32_t regs[] = {
                0x40000088, 0x400000c0, 0x40000134, 0x40000144, 0x40000148,
                0x40000180, 0x40000188, 0x40000190,
                0x4000A100, 0x4000A104, 0x4000A120, 0x4000A184,
            };
            for (unsigned i = 0; i < sizeof(regs) / sizeof(regs[0]); i++) {
                uint32_t v = *(volatile uint32_t *)regs[i];
                memcpy(output_buffer, &v, 4);
                output_buffer += 4;
            }
            uint32_t edges = 0, last = *(volatile uint32_t *)0x40000180 & (1u << 27);
            for (int i = 0; i < 200000; i++) {
                uint32_t now = *(volatile uint32_t *)0x40000180 & (1u << 27);
                edges += (now != last);
                last = now;
            }
            memcpy(output_buffer, &edges, 4);
            output_buffer += 4;
            break;
        }
#endif

        default:
            goto done;
        }

        commands++;
    }

done:
    /* Send response if there's data */
    if (output_buffer != tx_buf) {
        ep_tx_busy = true;
        usbd_ep_start_write(0, JTAG_IN_EP, tx_buf, output_buffer - tx_buf);
    }
}

static void jtag_bulk_out_cb(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;

    cmd_handle(rx_buf, nbytes);

    /* Re-arm OUT endpoint for next command */
    usbd_ep_start_read(0, JTAG_OUT_EP, rx_buf, sizeof(rx_buf));
}

static void jtag_bulk_in_cb(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    (void)nbytes;
    ep_tx_busy = false;
}

static struct usbd_endpoint jtag_out_ep = {
    .ep_addr = JTAG_OUT_EP,
    .ep_cb = jtag_bulk_out_cb,
};

static struct usbd_endpoint jtag_in_ep = {
    .ep_addr = JTAG_IN_EP,
    .ep_cb = jtag_bulk_in_cb,
};

static struct usbd_interface jtag_intf;

void dirtyjtag_init(uint8_t busid)
{
    usbd_add_interface(busid, &jtag_intf);
    usbd_add_endpoint(busid, &jtag_out_ep);
    usbd_add_endpoint(busid, &jtag_in_ep);
}

void dirtyjtag_start(void)
{
    usbd_ep_start_read(0, JTAG_OUT_EP, rx_buf, sizeof(rx_buf));
}
