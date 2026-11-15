/*
 * DirtyJTAG v2 protocol tests.
 *
 * Drives the real cmd_handle() through the OUT endpoint callback, exactly as
 * USBD_IRQHandler does, and inspects the bytes written to the IN endpoint.
 * All hardware behaviour is faked, so this covers command parsing and
 * response buffers only.
 */
#include "fakes.h"

#include "dirtyjtag.h"
#include "uart_bridge.h"

#include "usbd_core.h"

enum {
  CMD_STOP = 0x00,
  CMD_INFO = 0x01,
  CMD_FREQ = 0x02,
  CMD_XFER = 0x03,
  CMD_SETSIG = 0x04,
  CMD_GETSIG = 0x05,
  CMD_CLK = 0x06,
  CMD_UART_DEBUG = 0x07,
};

#define NO_READ 0x80
#define EXTEND_LENGTH 0x40
#define READOUT 0x80
#define SIG_TDO (1 << 3)

static void setup(void) {
  fakes_reset();
  dirtyjtag_init(0);
  dirtyjtag_start();
}

/*
 * Copy `len` bytes into the firmware's rx_buf, poison the rest so stale-byte
 * reads are visible, then invoke the OUT completion callback.
 */
static void deliver(const uint8_t *pkt, size_t len) {
  uint8_t *rx = fake_rx_buf[0x01];
  CHECK(rx != 0);
  if (!rx)
    return;
  memset(rx, 0xEE, 64);
  memcpy(rx, pkt, len);

  struct usbd_endpoint *ep = fake_find_ep(0x01);
  CHECK(ep != 0);
  if (ep)
    ep->ep_cb(0, 0x01, len);
}

static void test_info(void) {
  setup();
  deliver((const uint8_t[]){CMD_INFO, CMD_STOP}, 2);

  CHECK_EQ(fake_write_count, 1);
  CHECK_EQ(fake_write_ep, 0x82);
  CHECK_EQ(fake_write_len, 10);
  CHECK(memcmp(fake_write_buf, "DJTAG2\n", 7) == 0);
}

static void test_uart_debug(void) {
  setup();
  deliver((const uint8_t[]){CMD_UART_DEBUG, CMD_STOP}, 2);

  CHECK_EQ(fake_write_count, 1);
  CHECK_EQ(fake_write_len, 14);
  CHECK_EQ(fake_write_buf[10] | (fake_write_buf[11] << 8) | (fake_write_buf[12] << 16)
               | (fake_write_buf[13] << 24),
           115200);
}

static void test_xfer_read(void) {
  setup();
  fake_tdo = 0x5A;
  deliver((const uint8_t[]){CMD_XFER, 8, 0xA5, CMD_STOP}, 4);

  CHECK_EQ(fake_bits, 8);
  CHECK_EQ(fake_no_read, false);
  CHECK_EQ(fake_write_len, 1);
  CHECK_EQ(fake_write_buf[0], 0x5A);
}

static void test_xfer_no_read(void) {
  setup();
  deliver((const uint8_t[]){CMD_XFER | NO_READ, 8, 0xFF, CMD_STOP}, 4);

  CHECK_EQ(fake_bits, 8);
  CHECK_EQ(fake_no_read, true);
  CHECK_EQ(fake_write_count, 0);
}

/* EXTEND_LENGTH adds 256 bits: 1 + 256 = 257 bits = 33 bytes. */
static void test_xfer_extend(void) {
  setup();
  uint8_t pkt[64] = {0};
  pkt[0] = CMD_XFER | EXTEND_LENGTH;
  pkt[1] = 1;
  deliver(pkt, sizeof(pkt));

  CHECK_EQ(fake_bits, 257);
  CHECK_EQ(fake_write_len, 33);
}

/* Length is capped at 62*8 so a request cannot overrun the transfer buffer.
 * Reaching the cap needs EXTEND_LENGTH: 0xFF + 256 = 511 > 496. The packet is
 * exactly 64 bytes (cmd + len + 62 payload), so there is no room for STOP. */
static void test_xfer_capped(void) {
  setup();
  uint8_t pkt[64] = {0};
  pkt[0] = CMD_XFER | EXTEND_LENGTH;
  pkt[1] = 0xFF;
  fake_tdo = 0xFF;
  deliver(pkt, sizeof(pkt));

  CHECK_EQ(fake_bits, 62 * 8);
  CHECK_EQ(fake_write_len, 62);
}

/* A packet whose XFER payload is truncated must not read past the received
 * byte count. */
static void test_xfer_truncated(void) {
  setup();
  deliver((const uint8_t[]){CMD_XFER, 0xFF, CMD_STOP}, 3);

  /* Only the length byte arrived; the framing is rejected before any shift */
  CHECK_EQ(fake_bits, 0);
  CHECK_EQ(fake_write_count, 0);
}

static void test_getsig(void) {
  setup();
  deliver((const uint8_t[]){CMD_GETSIG, CMD_STOP}, 2);

  CHECK_EQ(fake_write_len, 1);
  CHECK_EQ(fake_write_buf[0], SIG_TDO);
}

static void test_clk_readout(void) {
  setup();
  deliver((const uint8_t[]){CMD_CLK | READOUT, 0x00, 4, CMD_STOP}, 4);

  CHECK_EQ(fake_write_len, 1);
  CHECK_EQ(fake_write_buf[0], 0xFF);
}

static void test_clk_no_readout(void) {
  setup();
  deliver((const uint8_t[]){CMD_CLK, 0x00, 4, CMD_STOP}, 4);
  CHECK_EQ(fake_write_count, 0);
}

static void test_stop_terminates(void) {
  setup();
  deliver((const uint8_t[]){CMD_STOP, CMD_INFO, CMD_INFO}, 3);
  CHECK_EQ(fake_write_count, 0);
}

static void test_unknown_stops(void) {
  setup();
  deliver((const uint8_t[]){0x0F, CMD_INFO, CMD_STOP}, 3);
  CHECK_EQ(fake_write_count, 0);
}

static void run_tests(void) {
  test_info();
  test_uart_debug();
  test_xfer_read();
  test_xfer_no_read();
  test_xfer_extend();
  test_xfer_capped();
  test_xfer_truncated();
  test_getsig();
  test_clk_readout();
  test_clk_no_readout();
  test_stop_terminates();
  test_unknown_stops();
}

TEST_MAIN("protocol")
