/*
 * Host override of the project's usb_config.h.
 *
 * Identical to the firmware version except USB_NOCACHE_RAM_SECTION: the
 * firmware parks USB buffers in .noncacheable, a custom section that
 * AddressSanitizer does not redzone, hiding buffer overruns. On host, plain
 * static storage is equivalent and fully instrumented.
 */
#ifndef CHERRYUSB_CONFIG_H
#define CHERRYUSB_CONFIG_H

#include <stdio.h>

#define CONFIG_USB_PRINTF(...) printf(__VA_ARGS__)
#define CONFIG_USB_PRINTF_COLOR_ENABLE

#ifndef CONFIG_USB_ALIGN_SIZE
#define CONFIG_USB_ALIGN_SIZE 4
#endif

#define USB_NOCACHE_RAM_SECTION

#ifndef CONFIG_USBDEV_REQUEST_BUFFER_LEN
#define CONFIG_USBDEV_REQUEST_BUFFER_LEN 512
#endif

#ifndef CONFIG_USBDEV_MAX_BUS
#define CONFIG_USBDEV_MAX_BUS 1
#endif

#ifndef CONFIG_USBDEV_EP_NUM
#define CONFIG_USBDEV_EP_NUM 5
#endif

#define CONFIG_USBDEV_ADVANCE_DESC

#endif
