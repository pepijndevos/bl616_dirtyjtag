/*
 * Fakes for the protocol tests: the jtag_* surface dirtyjtag.c drives, the
 * uart debug accessor it reports, and the CherryUSB endpoints it registers.
 */
#include "fakes.h"

#include "uart_bridge.h"
#include "usbd_core.h"

uint8_t fake_tdo;
uint16_t fake_bits;
bool fake_no_read;

uint8_t *fake_rx_buf[FAKE_MAX_EPS];
struct usbd_endpoint *fake_eps[FAKE_MAX_EPS];
int fake_ep_count;
int fake_write_count;
uint8_t fake_write_ep;
size_t fake_write_len;
uint8_t fake_write_buf[512];

void fakes_reset(void) {
  fake_tdo = 0;
  fake_bits = 0;
  fake_no_read = false;
  memset(fake_rx_buf, 0, sizeof(fake_rx_buf));
  memset(fake_eps, 0, sizeof(fake_eps));
  fake_ep_count = 0;
  fake_write_count = 0;
  fake_write_ep = 0;
  fake_write_len = 0;
  memset(fake_write_buf, 0, sizeof(fake_write_buf));
}

/* ---- jtag_gpio fakes ---- */

void jtag_gpio_init(void) {}

void jtag_transfer(uint16_t bits, const uint8_t *tdi, uint8_t *tdo) {
  (void)tdi;
  fake_bits = bits;
  fake_no_read = (tdo == 0);
  if (tdo) {
    size_t n = (bits + 7) / 8;
    for (size_t i = 0; i < n; i++)
      tdo[i] |= fake_tdo;
  }
}

void jtag_set_tck(bool value) {
  (void)value;
}
void jtag_set_tdi(bool value) {
  (void)value;
}
void jtag_set_tms(bool value) {
  (void)value;
}
void jtag_set_trst(bool value) {
  (void)value;
}
void jtag_set_srst(bool value) {
  (void)value;
}
bool jtag_get_tdo(void) {
  return true;
}
bool jtag_strobe(uint8_t pulses, bool tms, bool tdi) {
  (void)pulses;
  (void)tms;
  (void)tdi;
  return true;
}
void jtag_set_frequency(uint16_t kHz) {
  (void)kHz;
}

/* ---- uart_bridge fake ---- */

void uart_bridge_get_debug(struct uart_debug_state *state) {
  memset(state, 0, sizeof(*state));
  state->baudrate = 115200;
}

/* ---- CherryUSB device-core fakes ---- */

int usbd_ep_start_write(uint8_t busid, const uint8_t ep, const uint8_t *data, uint32_t len) {
  (void)busid;
  fake_write_count++;
  fake_write_ep = ep;
  fake_write_len = len;
  if (data && len && len <= sizeof(fake_write_buf))
    memcpy(fake_write_buf, data, len);
  return 0;
}

int usbd_ep_start_read(uint8_t busid, const uint8_t ep, uint8_t *data, uint32_t len) {
  (void)busid;
  (void)len;
  if ((ep & 0x0f) < FAKE_MAX_EPS)
    fake_rx_buf[ep & 0x0f] = data;
  return 0;
}

void usbd_add_endpoint(uint8_t busid, struct usbd_endpoint *ep) {
  (void)busid;
  if (fake_ep_count < FAKE_MAX_EPS)
    fake_eps[fake_ep_count++] = ep;
}

void usbd_add_interface(uint8_t busid, struct usbd_interface *intf) {
  (void)busid;
  (void)intf;
}

void usbd_desc_register(uint8_t busid, const struct usb_descriptor *desc) {
  (void)busid;
  (void)desc;
}

struct usbd_endpoint *fake_find_ep(uint8_t addr) {
  for (int i = 0; i < fake_ep_count; i++)
    if (fake_eps[i]->ep_addr == addr)
      return fake_eps[i];
  return 0;
}
