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

/* Free space left in tx_buf from `out` onwards. */
static inline size_t out_avail(const uint8_t *out) {
  return sizeof(tx_buf) - (size_t)(out - tx_buf);
}

/* Bytes of the received packet still unread from `cmd` onwards. */
static inline uint32_t in_avail(const uint8_t *cmd, const uint8_t *end) {
  return (uint32_t)(end - cmd);
}

static void cmd_handle(const uint8_t *rxbuf, uint32_t count) {
  const uint8_t *commands = rxbuf;
  const uint8_t *end = rxbuf + count;
  uint8_t *output_buffer = tx_buf;

  while ((commands < end) && (*commands != CMD_STOP)) {
    switch ((*commands) & 0x0F) {
      case CMD_INFO: {
        char info_string[10] = "DJTAG2\n";
        /* Standard info string followed by the debug state */
        struct uart_debug_state dbg;
        uart_bridge_get_debug(&dbg);

        size_t needed = sizeof(info_string) + sizeof(dbg);
        if (out_avail(output_buffer) < needed)
          goto done;

        memcpy(output_buffer, info_string, sizeof(info_string));
        output_buffer += sizeof(info_string);
        memcpy(output_buffer, &dbg, sizeof(dbg));
        output_buffer += sizeof(dbg);
        break;
      }

      case CMD_FREQ:
        if (in_avail(commands, end) < 3)
          goto done;
        jtag_set_frequency((commands[1] << 8) | commands[2]);
        commands += 2;
        break;

      case CMD_XFER: {
        if (in_avail(commands, end) < 2)
          goto done;

        bool no_read = *commands & NO_READ;
        uint16_t transferred_bits = commands[1];
        if (*commands & EXTEND_LENGTH)
          transferred_bits += 256;
        /* Cap at 62*8 = 496 bits to avoid buffer overflow */
        if (transferred_bits > 62 * 8)
          transferred_bits = 62 * 8;

        uint16_t trbytes = (transferred_bits + 7) / 8;

        /* Reject a command whose payload was truncated by the endpoint */
        if (in_avail(commands, end) < (uint32_t)2 + trbytes)
          goto done;
        if (!no_read && out_avail(output_buffer) < trbytes)
          goto done;

        if (!no_read)
          memset(output_buffer, 0, trbytes);

        jtag_transfer(transferred_bits, commands + 2, no_read ? NULL : output_buffer);

        commands += 1 + trbytes;
        output_buffer += (no_read ? 0 : trbytes);
        break;
      }

      case CMD_SETSIG: {
        if (in_avail(commands, end) < 3)
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
        if (out_avail(output_buffer) < 1)
          goto done;

        uint8_t signal_status = 0;
        if (jtag_get_tdo())
          signal_status |= SIG_TDO;
        output_buffer[0] = signal_status;
        output_buffer += 1;
        break;
      }

      case CMD_CLK: {
        if (in_avail(commands, end) < 3)
          goto done;

        bool readout = !!(*commands & READOUT);
        uint8_t signals = commands[1];
        uint8_t clk_pulses = commands[2];

        if (readout && out_avail(output_buffer) < 1)
          goto done;

        bool readout_val = jtag_strobe(clk_pulses, signals & SIG_TMS, signals & SIG_TDI);
        if (readout) {
          output_buffer[0] = readout_val ? 0xFF : 0;
          output_buffer += 1;
        }
        commands += 2;
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
