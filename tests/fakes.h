/*
 * Minimal fakes for the protocol tests.
 *
 * dirtyjtag.c only calls nine jtag_* functions and uart_bridge_get_debug(),
 * so faking those keeps jtag_gpio.c and uart_bridge.c (and the whole bflb
 * HAL) out of the build. What is under test is cmd_handle's buffer and
 * command parsing, not the pin driver.
 */
#ifndef FAKES_H
#define FAKES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---- test macros ---- */

extern int failures;
extern int checks;

#define CHECK(cond)                                          \
  do {                                                       \
    checks++;                                                \
    if (!(cond)) {                                           \
      failures++;                                            \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    }                                                        \
  } while (0)

#define CHECK_EQ(actual, expected)                                                      \
  do {                                                                                  \
    long a_ = (long)(actual), e_ = (long)(expected);                                    \
    checks++;                                                                           \
    if (a_ != e_) {                                                                     \
      failures++;                                                                       \
      printf("FAIL %s:%d: %s == %ld, want %ld\n", __FILE__, __LINE__, #actual, a_, e_); \
    }                                                                                   \
  } while (0)

#define TEST_MAIN(name)                                    \
  int failures, checks;                                    \
  int main(void) {                                         \
    run_tests();                                           \
    if (!failures) {                                       \
      printf("PASS %s (%d checks)\n", name, checks);       \
      return 0;                                            \
    }                                                      \
    printf("FAILED %s (%d/%d)\n", name, failures, checks); \
    return 1;                                              \
  }

/* ---- jtag_gpio fakes ---- */

/* Byte jtag_transfer() writes into the caller's tdo buffer */
extern uint8_t fake_tdo;
/* Last bit count and whether tdo was NULL, for assertions */
extern uint16_t fake_bits;
extern bool fake_no_read;

/* ---- usbd fakes ---- */

#define FAKE_MAX_EPS 8

extern uint8_t *fake_rx_buf[FAKE_MAX_EPS];
extern struct usbd_endpoint *fake_eps[FAKE_MAX_EPS];
extern int fake_ep_count;
extern int fake_write_count;
extern uint8_t fake_write_ep;
extern size_t fake_write_len;
extern uint8_t fake_write_buf[512];

void fakes_reset(void);
struct usbd_endpoint *fake_find_ep(uint8_t addr);

#endif
