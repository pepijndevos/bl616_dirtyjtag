/*
 * Regression test: command responses must not overrun the 64-byte tx_buf.
 *
 * A packet may contain multiple commands. Seven CMD_INFO commands produce
 * seven 10-byte responses (70 bytes total), which must not overrun the
 * one-packet response buffer.
 */
#include "fakes.h"

#include "dirtyjtag.h"

#include "usbd_core.h"

#define CMD_STOP 0x00
#define CMD_INFO 0x01
#define TX_BUF_SIZE 64

static void run_tests(void) {
  uint8_t pkt[TX_BUF_SIZE] = {0};

  fakes_reset();
  dirtyjtag_init(0);
  dirtyjtag_start();

  uint8_t *rx = fake_rx_buf[0x01];
  CHECK(rx != 0);
  if (!rx)
    return;

  for (size_t i = 0; i < 7; i++)
    pkt[i] = CMD_INFO;
  pkt[7] = CMD_STOP;
  memcpy(rx, pkt, sizeof(pkt));

  struct usbd_endpoint *ep = fake_find_ep(0x01);
  CHECK(ep != 0);
  if (ep)
    ep->ep_cb(0, 0x01, 8);

  /* Only complete responses may be returned; none may overrun tx_buf. */
  CHECK(fake_write_len <= TX_BUF_SIZE);
}

TEST_MAIN("tx_buf_bounds")
