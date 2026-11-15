/*
 * GPIO bit-bang JTAG engine
 *
 * Pin assignments:
 *   BL616 (Tang Nano 20K): TMS = GPIO16, TCK = GPIO10, TDI = GPIO12, TDO = GPIO14
 *   BL702 (Tang Nano 9K):  TMS = GPIO2,  TCK = GPIO15, TDI = GPIO0,  TDO = GPIO1
 *     (from Sipeed's stock 9K firmware; the RV-Debugger-BL702 9K pin map is wrong
 *      for this board)
 *
 * Uses direct register access for inner-loop performance.
 * Reference: firmware-bl616/fpga/programmer.cpp
 */
#include "jtag_gpio.h"
#include "bflb_gpio.h"

#if defined(BL702)

#define PIN_TMS GPIO_PIN_2
#define PIN_TCK GPIO_PIN_15
#define PIN_TDI GPIO_PIN_0
#define PIN_TDO GPIO_PIN_1

/* GLB_GPIO_CFGCTL30 (input) / GLB_GPIO_CFGCTL32 (output): one bit per pin */
#define GPIO_IN  (*(volatile uint32_t *)0x40000180)
#define GPIO_OUT (*(volatile uint32_t *)0x40000188)

/* On BL702 the "reg" handles are bit masks into GPIO_IN / GPIO_OUT */
#define reg_tms (1u << PIN_TMS)
#define reg_tck (1u << PIN_TCK)
#define reg_tdi (1u << PIN_TDI)
#define reg_tdo (1u << PIN_TDO)

#define PIN_SET(p)      (GPIO_OUT |= (p))
#define PIN_CLR(p)      (GPIO_OUT &= ~(p))
#define PIN_WRITE(p, v) ((v) ? PIN_SET(p) : PIN_CLR(p))
#define PIN_READ(p)     ((GPIO_IN & (p)) != 0)

#define CPU_KHZ 144000

#else

#define PIN_TMS GPIO_PIN_16
#define PIN_TCK GPIO_PIN_10
#define PIN_TDI GPIO_PIN_12
#define PIN_TDO GPIO_PIN_14

/* GPIO config register addresses for Nano20K JTAG pins */
static volatile uint32_t *reg_tms = (volatile uint32_t *)0x20000904;  /* GPIO16 */
static volatile uint32_t *reg_tck = (volatile uint32_t *)0x200008ec;  /* GPIO10 */
static volatile uint32_t *reg_tdi = (volatile uint32_t *)0x200008f4;  /* GPIO12 */
static volatile uint32_t *reg_tdo = (volatile uint32_t *)0x200008fc;  /* GPIO14 */

#define GPIO_SET   (1 << 25)
#define GPIO_CLEAR (1 << 26)
#define GPIO_READ  (1 << 28)

#define PIN_SET(p)      (*(p) |= GPIO_SET)
#define PIN_CLR(p)      (*(p) |= GPIO_CLEAR)
#define PIN_WRITE(p, v) (*(p) |= (v) ? GPIO_SET : GPIO_CLEAR)
#define PIN_READ(p)     ((*(p) & GPIO_READ) != 0)

#define CPU_KHZ 320000

#endif

static volatile uint32_t jtag_delay = 0;

static inline void delay_loop(void)
{
    volatile uint32_t n = jtag_delay;
    while (n--) {
        __asm__ volatile("nop");
    }
}

void jtag_gpio_init(void)
{
    struct bflb_device_s *gpio = bflb_device_get_by_name("gpio");

    bflb_gpio_init(gpio, PIN_TMS, GPIO_OUTPUT | GPIO_FLOAT | GPIO_SMT_EN | GPIO_DRV_3);
    bflb_gpio_init(gpio, PIN_TCK, GPIO_OUTPUT | GPIO_FLOAT | GPIO_SMT_EN | GPIO_DRV_3);
    bflb_gpio_init(gpio, PIN_TDI, GPIO_OUTPUT | GPIO_FLOAT | GPIO_SMT_EN | GPIO_DRV_3);
    bflb_gpio_init(gpio, PIN_TDO, GPIO_INPUT  | GPIO_FLOAT | GPIO_SMT_EN | GPIO_DRV_3);

    /* Start with TCK and TMS low */
    PIN_CLR(reg_tck);
    PIN_CLR(reg_tms);
    PIN_CLR(reg_tdi);
}

void jtag_transfer(uint16_t bits, const uint8_t *tdi, uint8_t *tdo)
{
    for (uint16_t i = 0; i < bits; i++) {
        uint8_t bitmask = 0x80 >> (i & 7);

        /* Set TDI */
        if (tdi[i >> 3] & bitmask) {
            PIN_SET(reg_tdi);
        } else {
            PIN_CLR(reg_tdi);
        }

        /* TCK low */
        PIN_CLR(reg_tck);
        delay_loop();

        /* TCK high */
        PIN_SET(reg_tck);
        delay_loop();

        /* Read TDO */
        if (tdo) {
            if (PIN_READ(reg_tdo)) {
                tdo[i >> 3] |= bitmask;
            }
        }
    }

    /* Leave TCK low */
    PIN_CLR(reg_tck);
}

void jtag_set_tck(bool value)
{
    PIN_WRITE(reg_tck, value);
}

void jtag_set_tdi(bool value)
{
    PIN_WRITE(reg_tdi, value);
}

void jtag_set_tms(bool value)
{
    PIN_WRITE(reg_tms, value);
}

void jtag_set_trst(bool value)
{
    /* TRST not connected on Tang Nano 20K / 9K — no-op */
    (void)value;
}

void jtag_set_srst(bool value)
{
    /* SRST not connected on Tang Nano 20K / 9K — no-op */
    (void)value;
}

bool jtag_get_tdo(void)
{
    return PIN_READ(reg_tdo);
}

bool jtag_strobe(uint8_t pulses, bool tms, bool tdi)
{
    PIN_WRITE(reg_tms, tms);
    PIN_WRITE(reg_tdi, tdi);

    bool last_tdo = false;
    for (uint8_t i = 0; i < pulses; i++) {
        PIN_CLR(reg_tck);
        delay_loop();
        PIN_SET(reg_tck);
        delay_loop();
        last_tdo = PIN_READ(reg_tdo);
    }
    PIN_CLR(reg_tck);

    return last_tdo;
}

void jtag_set_frequency(uint16_t kHz)
{
    if (kHz == 0) {
        jtag_delay = 0;
        return;
    }
    /*
     * Rough estimate: CPU runs at CPU_KHZ (BL616 ~320MHz, BL702 144MHz).
     * Each delay iteration ~3 cycles (loop + nop).
     * For frequency f: half-period = 1/(2f), cycles = CPU_KHZ/(2f) / 3
     */
    uint32_t target = CPU_KHZ / (2 * (uint32_t)kHz * 3);
    jtag_delay = (target > 1) ? target : 0;
}
