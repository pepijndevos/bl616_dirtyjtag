/*
 * CherryUSB device configuration for BL616 DirtyJTAG
 * Based on CherryDAP/projects/bl616/usb_config.h
 */
#ifndef CHERRYUSB_CONFIG_H
#define CHERRYUSB_CONFIG_H

/* ================ USB common Configuration ================ */

#define CONFIG_USB_PRINTF(...) printf(__VA_ARGS__)

#ifndef CONFIG_USB_DBG_LEVEL
#define CONFIG_USB_DBG_LEVEL USB_DBG_INFO
#endif

#define CONFIG_USB_PRINTF_COLOR_ENABLE

#ifndef CONFIG_USB_ALIGN_SIZE
#define CONFIG_USB_ALIGN_SIZE 4
#endif

#if defined(BL702)
#define USB_NOCACHE_RAM_SECTION  /* no D-cache; usb_v1 copies through FIFO */
#else
#define USB_NOCACHE_RAM_SECTION __attribute__((section(".noncacheable")))
#endif

/* ================= USB Device Stack Configuration ================ */

#ifndef CONFIG_USBDEV_REQUEST_BUFFER_LEN
#define CONFIG_USBDEV_REQUEST_BUFFER_LEN 512
#endif

/* ================ USB Device Port Configuration ================ */

#ifndef CONFIG_USBDEV_MAX_BUS
#define CONFIG_USBDEV_MAX_BUS 1
#endif

#ifndef CONFIG_USBDEV_EP_NUM
#define CONFIG_USBDEV_EP_NUM 5
#endif

#define CONFIG_USBDEV_ADVANCE_DESC

/*
 * USB Host defines — required by bflb_usb_v2.c LHAL driver which includes
 * both usbd_core.h and usbh_core.h even in device-only builds.
 */
#define CONFIG_USB_EHCI_HCOR_RESERVED_DISABLE
#define CONFIG_USBHOST_MAX_RHPORTS          1
#define CONFIG_USBHOST_MAX_EXTHUBS          1
#define CONFIG_USBHOST_MAX_EHPORTS          4
#define CONFIG_USBHOST_MAX_INTERFACES       8
#define CONFIG_USBHOST_MAX_INTF_ALTSETTINGS 8
#define CONFIG_USBHOST_MAX_ENDPOINTS        4
#define CONFIG_USBHOST_DEV_NAMELEN          16
#define CONFIG_USBHOST_MAX_BUS              1
#define CONFIG_USBHOST_PIPE_NUM             10
#define CONFIG_USB_EHCI_FRAME_LIST_SIZE     1024
#define CONFIG_USB_EHCI_QH_NUM              CONFIG_USBHOST_PIPE_NUM
#define CONFIG_USB_EHCI_QTD_NUM             3
#define CONFIG_USB_EHCI_ITD_NUM             20

#endif
