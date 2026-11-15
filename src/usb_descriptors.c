/*
 * USB Composite Descriptors for DirtyJTAG + CDC-ACM
 *
 * Interface 0: Vendor bulk (DirtyJTAG) — EP 0x01 OUT, EP 0x82 IN
 * Interface 1-2: CDC-ACM (UART bridge) — endpoints in usb_eps.h
 */
#include "usbd_core.h"
#include "usbd_cdc_acm.h"
#include "usb_eps.h"

#define USBD_VID           0x1209
#define USBD_PID           0xC0CA
#define USBD_MAX_POWER     100

/* Vendor interface descriptor length: interface(9) + EP OUT(7) + EP IN(7) = 23 */
#define VENDOR_INTF_DESC_LEN (9 + 7 + 7)

/* Total config descriptor length */
#define USB_CONFIG_SIZE (9 + VENDOR_INTF_DESC_LEN + CDC_ACM_DESCRIPTOR_LEN)

/* Number of interfaces: 1 vendor + 2 CDC = 3 */
#define INTF_NUM 3

static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0xEF, 0x02, 0x01, USBD_VID, USBD_PID, 0x0200, 0x01),
};

static const uint8_t config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, INTF_NUM, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    /* Interface 0: Vendor bulk (DirtyJTAG) */
    USB_INTERFACE_DESCRIPTOR_INIT(0x00, 0x00, 0x02, 0xFF, 0x00, 0x00, 0x02),
    /* Endpoint OUT 1 */
    USB_ENDPOINT_DESCRIPTOR_INIT(JTAG_OUT_EP, USB_ENDPOINT_TYPE_BULK, 0x0040, 0x00),
    /* Endpoint IN 2 */
    USB_ENDPOINT_DESCRIPTOR_INIT(JTAG_IN_EP, USB_ENDPOINT_TYPE_BULK, 0x0040, 0x00),
    /* Interfaces 1-2: CDC-ACM (includes IAD) */
    CDC_ACM_DESCRIPTOR_INIT(0x01, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, 0x0040, 0x00),
};

static const uint8_t device_quality_descriptor[] = {
    USB_DEVICE_QUALIFIER_DESCRIPTOR_INIT(USB_2_0, 0xEF, 0x02, 0x01, 0x01),
};

static const uint8_t other_speed_config_descriptor[] = {
    USB_OTHER_SPEED_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, INTF_NUM, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    /* Interface 0: Vendor bulk (DirtyJTAG) */
    USB_INTERFACE_DESCRIPTOR_INIT(0x00, 0x00, 0x02, 0xFF, 0x00, 0x00, 0x02),
    USB_ENDPOINT_DESCRIPTOR_INIT(JTAG_OUT_EP, USB_ENDPOINT_TYPE_BULK, 0x0040, 0x00),
    USB_ENDPOINT_DESCRIPTOR_INIT(JTAG_IN_EP, USB_ENDPOINT_TYPE_BULK, 0x0040, 0x00),
    CDC_ACM_DESCRIPTOR_INIT(0x01, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, 0x0040, 0x00),
};

static const char *string_descriptors[] = {
    (const char[]){ 0x09, 0x04 },  /* Langid */
    "DirtyJTAG",                    /* Manufacturer */
    "DirtyJTAG",                    /* Product */
    "0200",                         /* Serial Number */
};

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return device_descriptor;
}

static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return config_descriptor;
}

static const uint8_t *device_quality_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return device_quality_descriptor;
}

static const uint8_t *other_speed_config_descriptor_callback(uint8_t speed)
{
    (void)speed;
    return other_speed_config_descriptor;
}

static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    (void)speed;
    if (index >= sizeof(string_descriptors) / sizeof(string_descriptors[0])) {
        return NULL;
    }
    return string_descriptors[index];
}

const struct usb_descriptor dirtyjtag_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .other_speed_descriptor_callback = other_speed_config_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback,
};

void usb_descriptors_init(uint8_t busid)
{
    usbd_desc_register(busid, &dirtyjtag_descriptor);
}
