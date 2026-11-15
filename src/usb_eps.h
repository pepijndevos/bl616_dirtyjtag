#ifndef USB_EPS_H
#define USB_EPS_H

/* DirtyJTAG vendor interface */
#define JTAG_OUT_EP  0x01
#define JTAG_IN_EP   0x82

/* CDC-ACM interface */
#if defined(BL702)
/* BL702 USB endpoints are unidirectional: each EP number needs its own index */
#define CDC_INT_EP   0x83
#define CDC_OUT_EP   0x04
#define CDC_IN_EP    0x85
#else
#define CDC_INT_EP   0x83
#define CDC_OUT_EP   0x03
#define CDC_IN_EP    0x84
#endif

#endif
