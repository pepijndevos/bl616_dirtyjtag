/*
 * DirtyJTAG v2 protocol handler
 *
 * Ported from DirtyJTAG/src/cmd.c (MIT license)
 * Adapted for CherryUSB on BL702
 *
 * Processes DirtyJTAG v2 commands received on a vendor bulk endpoint
 * and responds with JTAG data.
 */
#include "dirtyjtag.h"
#include <string.h>
#include "jtag_gpio.h"
#include "uart_bridge.h"
#include "usbd_core.h"

/* Each OUT packet produces at most one IN packet. A command is not executed
 * unless its operands fit in the OUT packet and its response fits in tx_buf. */

#define JTAG_OUT_EP 0x01
#define JTAG_IN_EP 0x82

/* DirtyJTAG v2 command IDs */
enum {
  CMD_STOP = 0x00,
  CMD_INFO = 0x01,
  CMD_FREQ = 0x02,
  CMD_XFER = 0x03,
  CMD_SETSIG = 0x04,
  CMD_GETSIG = 0x05,
  CMD_CLK = 0x06,
  /* Project-specific diagnostics, not part of the DirtyJTAG protocol */
  CMD_UART_DEBUG = 0x07,
};

/* Command modifiers */
enum {
  NO_READ = 0x80,
  EXTEND_LENGTH = 0x40,
  READOUT = 0x80,
};

/* Signal identifiers */
enum {
  SIG_TCK = 1 << 1,
  SIG_TDI = 1 << 2,
  SIG_TDO = 1 << 3,
  SIG_TMS = 1 << 4,
  SIG_TRST = 1 << 5,
  SIG_SRST = 1 << 6,
};

static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t rx_buf[64];
static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t tx_buf[64];
static volatile bool ep_tx_busy;

static void cmd_handle(const uint8_t *rxbuf, uint32_t count) {
  const uint8_t *commands = rxbuf;
  const uint8_t *end = rxbuf + count;
  uint8_t *output_buffer = tx_buf;

  while ((commands < end) && (*commands != CMD_STOP)) {
    switch ((*commands) & 0x0F) {
      case CMD_INFO: {
        static const char info_string[10] = "DJTAG2\n";
        if (output_buffer + sizeof(info_string) > tx_buf + sizeof(tx_buf))
          goto done;
        memcpy(output_buffer, info_string, sizeof(info_string));
        output_buffer += sizeof(info_string);
        break;
      }

      case CMD_FREQ:
        if (commands + 3 > end)
          goto done;
        jtag_set_frequency((commands[1] << 8) | commands[2]);
        commands += 2;
        break;

      case CMD_XFER: {
        if (commands + 2 > end)
          goto done;

        bool no_read = *commands & NO_READ;
        uint16_t transferred_bits = commands[1];
        if (*commands & EXTEND_LENGTH)
          transferred_bits += 256;
        /* Cap at 62*8 = 496 bits to avoid buffer overflow */
        if (transferred_bits > 62 * 8)
          transferred_bits = 62 * 8;

        uint16_t trbytes = (transferred_bits + 7) / 8;

        if (commands + 2 + trbytes > end)
          goto done;
        if (!no_read && output_buffer + trbytes > tx_buf + sizeof(tx_buf))
          goto done;

        if (!no_read)
          memset(output_buffer, 0, trbytes);

        jtag_transfer(transferred_bits, commands + 2, no_read ? NULL : output_buffer);

        commands += 1 + trbytes;
        output_buffer += (no_read ? 0 : trbytes);
        break;
      }

      case CMD_SETSIG: {
        if (commands + 3 > end)
          goto done;

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
        if (output_buffer >= tx_buf + sizeof(tx_buf))
          goto done;

        uint8_t signal_status = 0;
        if (jtag_get_tdo())
          signal_status |= SIG_TDO;
        output_buffer[0] = signal_status;
        output_buffer += 1;
        break;
      }

      case CMD_CLK: {
        if (commands + 3 > end)
          goto done;

        bool readout = !!(*commands & READOUT);
        uint8_t signals = commands[1];
        uint8_t clk_pulses = commands[2];

        if (readout && output_buffer >= tx_buf + sizeof(tx_buf))
          goto done;

        bool readout_val = jtag_strobe(clk_pulses, signals & SIG_TMS, signals & SIG_TDI);
        if (readout) {
          output_buffer[0] = readout_val ? 0xFF : 0;
          output_buffer += 1;
        }
        commands += 2;
        break;
      }

      case CMD_UART_DEBUG: {
        struct uart_debug_state dbg;
        uint8_t *out = output_buffer;

        if (output_buffer + 14 > tx_buf + sizeof(tx_buf))
          goto done;

        uart_bridge_get_debug(&dbg);
        /* Explicit little-endian wire format, independent of struct layout */
        out[0] = dbg.rx_bytes;
        out[1] = dbg.rx_bytes >> 8;
        out[2] = dbg.rx_bytes >> 16;
        out[3] = dbg.rx_bytes >> 24;
        out[4] = dbg.tx_bytes;
        out[5] = dbg.tx_bytes >> 8;
        out[6] = dbg.tx_bytes >> 16;
        out[7] = dbg.tx_bytes >> 24;
        out[8] = dbg.cdc_configured;
        out[9] = dbg.cdc_tx_busy;
        out[10] = dbg.baudrate;
        out[11] = dbg.baudrate >> 8;
        out[12] = dbg.baudrate >> 16;
        out[13] = dbg.baudrate >> 24;
        output_buffer += 14;
        break;
      }

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

static void jtag_bulk_out_cb(uint8_t busid, uint8_t ep, uint32_t nbytes) {
  (void)busid;
  (void)ep;

  cmd_handle(rx_buf, nbytes);

  /* Re-arm OUT endpoint for next command */
  usbd_ep_start_read(0, JTAG_OUT_EP, rx_buf, sizeof(rx_buf));
}

static void jtag_bulk_in_cb(uint8_t busid, uint8_t ep, uint32_t nbytes) {
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

void dirtyjtag_init(uint8_t busid) {
  usbd_add_interface(busid, &jtag_intf);
  usbd_add_endpoint(busid, &jtag_out_ep);
  usbd_add_endpoint(busid, &jtag_in_ep);
}

void dirtyjtag_start(void) {
  usbd_ep_start_read(0, JTAG_OUT_EP, rx_buf, sizeof(rx_buf));
}
