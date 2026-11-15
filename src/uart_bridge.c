/*
 * CDC-ACM to UART1 bridge for BL616
 *
 * USB CDC-ACM interface bridges to hardware UART1.
 * Host can set baud rate via SET_LINE_CODING.
 * UART RX is polled in a FreeRTOS task.
 *
 * Pin assignments:
 *   BL616 (Nano20K): UART1_TX = GPIO11, UART1_RX = GPIO13
 *   BL702 (Nano9K):  UART1_TX = GPIO26, UART1_RX = GPIO27
 */
#include <string.h>
#include "usbd_core.h"
#include "usbd_cdc_acm.h"
#include "bflb_uart.h"
#include "bflb_gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "uart_bridge.h"
#include "usb_eps.h"

#if defined(BL702)
#define UART_TX_PIN GPIO_PIN_26  /* Tang Nano 9K: to FPGA_RX */
#define UART_RX_PIN GPIO_PIN_27  /* Tang Nano 9K: from FPGA_TX */
#else
#define UART_TX_PIN GPIO_PIN_11
#define UART_RX_PIN GPIO_PIN_13
#endif

static struct bflb_device_s *uart1_dev;

static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t cdc_rx_buf[512];
static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t cdc_tx_buf[2][512];
static uint8_t tx_fill_idx;  /* buffer being filled by task */
static uint32_t tx_fill_count; /* bytes in fill buffer */
static volatile bool cdc_configured;
static volatile bool cdc_tx_busy;
static uint32_t debug_rx_bytes;
static uint32_t debug_tx_bytes;
static uint32_t debug_baudrate = 115200;

/* Deferred UART reconfiguration (set in ISR, applied in task) */
static volatile bool uart_reconfig_pending;
static struct cdc_line_coding pending_line_coding;

static void cdc_bulk_out_cb(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;

    /* Forward USB data to UART */
    for (uint32_t i = 0; i < nbytes; i++) {
        bflb_uart_putchar(uart1_dev, cdc_rx_buf[i]);
    }
    debug_tx_bytes += nbytes;

    /* Re-arm for next transfer */
    usbd_ep_start_read(0, CDC_OUT_EP, cdc_rx_buf, sizeof(cdc_rx_buf));
}

static void cdc_bulk_in_cb(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    (void)nbytes;
    cdc_tx_busy = false;
}

static struct usbd_endpoint cdc_out_ep = {
    .ep_addr = CDC_OUT_EP,
    .ep_cb = cdc_bulk_out_cb,
};

static struct usbd_endpoint cdc_in_ep = {
    .ep_addr = CDC_IN_EP,
    .ep_cb = cdc_bulk_in_cb,
};

static struct usbd_interface cdc_intf0;
static struct usbd_interface cdc_intf1;

void uart_bridge_usb_init(uint8_t busid)
{
    usbd_add_interface(busid, usbd_cdc_acm_init_intf(busid, &cdc_intf0));
    usbd_add_interface(busid, usbd_cdc_acm_init_intf(busid, &cdc_intf1));
    usbd_add_endpoint(busid, &cdc_out_ep);
    usbd_add_endpoint(busid, &cdc_in_ep);
}

void uart_bridge_uart_init(void)
{
    struct bflb_device_s *gpio = bflb_device_get_by_name("gpio");

    bflb_gpio_uart_init(gpio, UART_TX_PIN, GPIO_UART_FUNC_UART1_TX);
    bflb_gpio_uart_init(gpio, UART_RX_PIN, GPIO_UART_FUNC_UART1_RX);

    uart1_dev = bflb_device_get_by_name("uart1");

    struct bflb_uart_config_s cfg = {
        .baudrate = 115200,
        .direction = UART_DIRECTION_TXRX,
        .data_bits = UART_DATA_BITS_8,
        .stop_bits = UART_STOP_BITS_1,
        .parity = UART_PARITY_NONE,
        .bit_order = UART_LSB_FIRST,
        .flow_ctrl = 0,
        .tx_fifo_threshold = 7,
        .rx_fifo_threshold = 7,
    };
    bflb_uart_init(uart1_dev, &cfg);
}

/* Override CherryUSB weak callback — ignore reconfig for now */
void usbd_cdc_acm_set_line_coding(uint8_t busid, uint8_t intf, struct cdc_line_coding *line_coding)
{
    (void)busid;
    (void)intf;
    (void)line_coding;
}

static void apply_line_coding(const struct cdc_line_coding *line_coding)
{
    if (uart1_dev == NULL)
        return;

    bflb_uart_disable(uart1_dev);

    struct bflb_uart_config_s cfg = {
        .baudrate = line_coding->dwDTERate ? line_coding->dwDTERate : 115200,
        .direction = UART_DIRECTION_TXRX,
        .data_bits = UART_DATA_BITS_8,
        .stop_bits = UART_STOP_BITS_1,
        .parity = UART_PARITY_NONE,
        .bit_order = UART_LSB_FIRST,
        .flow_ctrl = 0,
        .tx_fifo_threshold = 7,
        .rx_fifo_threshold = 7,
    };

    /* Map CDC data bits */
    switch (line_coding->bDataBits) {
    case 5: cfg.data_bits = UART_DATA_BITS_5; break;
    case 6: cfg.data_bits = UART_DATA_BITS_6; break;
    case 7: cfg.data_bits = UART_DATA_BITS_7; break;
    default: cfg.data_bits = UART_DATA_BITS_8; break;
    }

    /* Map CDC stop bits: 0=1stop, 1=1.5stop, 2=2stop */
    if (line_coding->bCharFormat == 2)
        cfg.stop_bits = UART_STOP_BITS_2;

    /* Map CDC parity: 0=none, 1=odd, 2=even */
    switch (line_coding->bParityType) {
    case 1: cfg.parity = UART_PARITY_ODD; break;
    case 2: cfg.parity = UART_PARITY_EVEN; break;
    default: cfg.parity = UART_PARITY_NONE; break;
    }

    debug_baudrate = cfg.baudrate;
    bflb_uart_init(uart1_dev, &cfg);
}

void usbd_cdc_acm_set_dtr(uint8_t busid, uint8_t intf, bool dtr)
{
    (void)busid;
    (void)intf;
    cdc_configured = dtr;
    /* Reset tx busy flag — any in-flight write is stale after port close/reopen */
    if (dtr)
        cdc_tx_busy = false;
}

void uart_bridge_start(void)
{
    usbd_ep_start_read(0, CDC_OUT_EP, cdc_rx_buf, sizeof(cdc_rx_buf));
}

/* FreeRTOS task: poll UART RX and send to USB CDC IN endpoint */
void uart_task(void *param)
{
    (void)param;

    for (;;) {
        if (uart1_dev == NULL) {
            vTaskDelay(1);
            continue;
        }

        /* Apply deferred UART reconfiguration from ISR */
        if (uart_reconfig_pending) {
            uart_reconfig_pending = false;
            apply_line_coding(&pending_line_coding);
        }

        /* Always drain UART FIFO into fill buffer */
        while (tx_fill_count < sizeof(cdc_tx_buf[0])) {
            int ch = bflb_uart_getchar(uart1_dev);
            if (ch < 0)
                break;
            cdc_tx_buf[tx_fill_idx][tx_fill_count++] = (uint8_t)ch;
        }

        if (tx_fill_count > 0 && !cdc_tx_busy) {
            /* Submit fill buffer to USB and swap */
            debug_rx_bytes += tx_fill_count;
            cdc_tx_busy = true;
            usbd_ep_start_write(0, CDC_IN_EP, cdc_tx_buf[tx_fill_idx], tx_fill_count);
            tx_fill_idx ^= 1;
            tx_fill_count = 0;
        } else if (tx_fill_count == 0) {
            vTaskDelay(1);
        }
    }
}

void uart_bridge_get_debug(struct uart_debug_state *state)
{
    state->rx_bytes = debug_rx_bytes;
    state->tx_bytes = debug_tx_bytes;
    state->cdc_configured = cdc_configured;
    state->cdc_tx_busy = cdc_tx_busy;
    state->baudrate = debug_baudrate;
}
