#ifndef UART_BRIDGE_H
#define UART_BRIDGE_H

#include <stdint.h>

void uart_bridge_usb_init(uint8_t busid);
void uart_bridge_uart_init(void);
void uart_bridge_start(void);
void uart_task(void *param);

/* Debug state for diagnostics */
struct uart_debug_state {
    uint32_t rx_bytes;      /* UART RX → USB TX bytes */
    uint32_t tx_bytes;      /* USB RX → UART TX bytes */
    uint8_t  cdc_configured;
    uint8_t  cdc_tx_busy;
    uint32_t baudrate;
};
void uart_bridge_get_debug(struct uart_debug_state *state);
void uart_bridge_get_task_debug(uint32_t out[5]);

#endif
