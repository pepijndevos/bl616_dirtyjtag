/*
 * USB descriptor structural tests.
 *
 * Walks the configuration descriptor by bLength/bDescriptorType and asserts
 * the invariants the BL702 USB_v1 driver depends on. The key one is that
 * every endpoint has a distinct ep_idx: on USB_v1 an IN and an OUT sharing an
 * index silently drops the OUT direction, which is the trap this port had to
 * avoid. No hardware fakes needed — the descriptors are pure data.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// clang-format off
/* Order matters: usbd_core.h pulls in usb_util.h, which defines __PACKED
 * used by usb_cdc.h's structs. */
#include "usbd_core.h"
#include "usbd_cdc_acm.h"
// clang-format on

extern void usb_descriptors_init(uint8_t busid);

static int failures, checks;

#define CHECK(cond)                                          \
  do {                                                       \
    checks++;                                                \
    if (!(cond)) {                                           \
      failures++;                                            \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    }                                                        \
  } while (0)

#define CHECK_EQ(a, b)                                                             \
  do {                                                                             \
    long a_ = (long)(a), b_ = (long)(b);                                           \
    checks++;                                                                      \
    if (a_ != b_) {                                                                \
      failures++;                                                                  \
      printf("FAIL %s:%d: %s == %ld, want %ld\n", __FILE__, __LINE__, #a, a_, b_); \
    }                                                                              \
  } while (0)

/* Capture the descriptor table via the registration callback. */
static const struct usb_descriptor *desc;

void usbd_desc_register(uint8_t busid, const struct usb_descriptor *d) {
  (void)busid;
  desc = d;
}

#define MAX_EPS 8

struct ep {
  uint8_t addr, attrs;
  uint16_t mps;
};

static struct ep eps[MAX_EPS];
static int ep_count, iface_count;

static void walk(const uint8_t *d) {
  uint16_t total = (uint16_t)(d[2] | (d[3] << 8));
  uint16_t off = 0;
  while (off + 1 < total) {
    uint8_t len = d[off], type = d[off + 1];
    if (len < 2 || off + len > total)
      break; /* malformed */
    if (type == 0x05 && ep_count < MAX_EPS) {
      eps[ep_count].addr = d[off + 2];
      eps[ep_count].attrs = d[off + 3];
      eps[ep_count].mps = (uint16_t)(d[off + 4] | (d[off + 5] << 8));
      ep_count++;
    } else if (type == 0x04) {
      iface_count++;
    }
    off += len;
  }
}

static void load(void) {
  desc = 0;
  usb_descriptors_init(0);
  CHECK(desc != 0);
  ep_count = iface_count = 0;
  walk(desc->config_descriptor_callback(0));
}

static void test_config(void) {
  load();

  const uint8_t *d = desc->config_descriptor_callback(0);
  CHECK_EQ(d[2] | (d[3] << 8), 9 + 23 + CDC_ACM_DESCRIPTOR_LEN);
  CHECK_EQ(iface_count, 3); /* vendor + CDC control + CDC data */
  CHECK_EQ(ep_count, 5);

  /* Distinct addresses and distinct USB_v1 ep_idx */
  for (int i = 0; i < ep_count; i++) {
    for (int j = i + 1; j < ep_count; j++) {
      CHECK(eps[i].addr != eps[j].addr);
      CHECK((eps[i].addr & 0x0f) != (eps[j].addr & 0x0f));
    }
  }

  /* Bulk endpoints carry 64-byte packets; the CDC notify EP is interrupt
   * (its size comes from the descriptor macro). */
  for (int i = 0; i < ep_count; i++) {
    if (eps[i].addr == 0x85) {
      CHECK_EQ(eps[i].attrs & 0x03, 0x03);
    } else {
      CHECK_EQ(eps[i].attrs & 0x03, 0x02);
      CHECK_EQ(eps[i].mps, 64);
    }
  }
}

static void test_device(void) {
  load();
  const uint8_t *d = desc->device_descriptor_callback(0);
  CHECK_EQ(d[0], 18);
  CHECK_EQ(d[8] | (d[9] << 8), 0x1209);   /* VID */
  CHECK_EQ(d[10] | (d[11] << 8), 0xC0CA); /* PID */
}

static void test_other_speed(void) {
  load();
  const uint8_t *d = desc->other_speed_descriptor_callback(0);
  CHECK_EQ(d[2] | (d[3] << 8), 9 + 23 + CDC_ACM_DESCRIPTOR_LEN);
  CHECK_EQ(iface_count, 3);
  CHECK_EQ(ep_count, 5);
}

static void test_strings(void) {
  load();
  CHECK(desc->string_descriptor_callback(0, 1) != 0);
  CHECK(desc->string_descriptor_callback(0, 99) == 0); /* out of range */
}

static void run_tests(void) {
  test_config();
  test_device();
  test_other_speed();
  test_strings();
}

int main(void) {
  run_tests();
  if (!failures) {
    printf("PASS descriptors (%d checks)\n", checks);
    return 0;
  }
  printf("FAILED descriptors (%d/%d)\n", failures, checks);
  return 1;
}
