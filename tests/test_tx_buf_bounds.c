/*
 * Regression test: cmd_handle() must not write past its 64-byte tx_buf.
 *
 * Three CMD_INFO commands in one packet produce 26 bytes each (10-byte info
 * string + 16-byte debug state) = 78 bytes, but tx_buf is only 64 bytes and
 * the handler has no output bounds check, so it overruns into the adjacent
 * static (ep_tx_busy).
 *
 * A separate binary because AddressSanitizer aborts the process on the
 * overrun, which would mask the other tests' results.
 */
#include "fakes.h"

#include "dirtyjtag.h"

#include "usbd_core.h"

#define CMD_STOP 0x00
#define CMD_INFO 0x01
#define TX_BUF_SIZE 64

static void run_tests(void) {
  fakes_reset();
  dirtyjtag_init(0);
  dirtyjtag_start();

  uint8_t *rx = fake_rx_buf[0x01];
  CHECK(rx != 0);
  if (!rx)
    return;

  memset(rx, 0x00, TX_BUF_SIZE);
  rx[0] = CMD_INFO;
  rx[1] = CMD_INFO;
  rx[2] = CMD_INFO;
  rx[3] = CMD_STOP;

  struct usbd_endpoint *ep = fake_find_ep(0x01);
  CHECK(ep != 0);
  if (ep)
    ep->ep_cb(0, 0x01, 4);

  /* 3 * (10 + sizeof(debug state)) must fit in tx_buf */
  CHECK(fake_write_len <= TX_BUF_SIZE);
}

TEST_MAIN("tx_buf_bounds")
