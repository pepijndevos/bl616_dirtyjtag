#ifndef _PINS_H
#define _PINS_H

#include "bflb_gpio.h"

/* JTAG bit-bang pins — Tang Primer 20K Dock (BL702)
 * Confirmed from dock schematic:
 *   JTAG_TDI# → GPIO_0, JTAG_TDO# → GPIO_1,
 *   JTAG_TMS# → GPIO_2, JTAG_TCK# → GPIO_15 */
#define JTAG_TMS_PIN GPIO_PIN_2
#define JTAG_TCK_PIN GPIO_PIN_15
#define JTAG_TDI_PIN GPIO_PIN_0
#define JTAG_TDO_PIN GPIO_PIN_1

/* CDC-ACM UART bridge — UART1 (Tang Primer 20K Dock)
 *
 * FPGA_TX = M11 -> BL702 UART1_RX,
 * FPGA_RX = T13 <- BL702 UART1_TX. */
#define UART1_TX_PIN GPIO_PIN_26
#define UART1_RX_PIN GPIO_PIN_27

#endif
