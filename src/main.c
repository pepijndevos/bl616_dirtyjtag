/*
 * BL616 / BL702 DirtyJTAG — Main entry point
 *
 * Composite USB device: DirtyJTAG v2 (vendor bulk) + CDC-ACM (UART bridge)
 */
#include "board.h"
#include "bflb_gpio.h"
#if defined(BL702)
#include "bl702_glb.h"
#endif
#include "usbd_core.h"
#include "FreeRTOS.h"
#include "task.h"

#include "dirtyjtag.h"
#include "jtag_gpio.h"
#include "uart_bridge.h"

#if defined(BL702)
#define USBD_REG_BASE 0x4000D800  /* bflb_usb_v1.c ignores this and hardcodes it */
#else
#define USBD_REG_BASE 0x20072000
#endif

/* Provided by usb_descriptors.c */
extern void usb_descriptors_init(uint8_t busid);

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    (void)busid;
    switch (event) {
    case USBD_EVENT_RESET:
        break;
    case USBD_EVENT_CONFIGURED:
        /* Kick off first read on both OUT endpoints */
        dirtyjtag_start();
        uart_bridge_start();
        break;
    default:
        break;
    }
}

int main(void)
{
#if defined(BL702)
    /* Free the SF2 pads GPIO23..28 (UART1 on 26/27) from the flash interface,
     * as Sipeed's stock 9K firmware does first thing in main. */
    GLB_Select_Internal_Flash();
#endif
    board_init();

    /* Init USB composite device */
    usb_descriptors_init(0);
    dirtyjtag_init(0);
    uart_bridge_usb_init(0);
    usbd_initialize(0, USBD_REG_BASE, usbd_event_handler);

    /* Init GPIO for JTAG */
    jtag_gpio_init();

    /* Init UART1 for CDC bridge */
    uart_bridge_uart_init();

    /* Create UART polling task */
    xTaskCreate(uart_task, "uart", 1024, NULL, 2, NULL);

    vTaskStartScheduler();

    while (1) {}
}
