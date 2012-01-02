/* This header is shared between the firmware and the host software. It
 * defines the USB request numbers (and optionally data types) used to
 * communicate between the host and the device.
 */

#ifndef __REQUESTS_H_INCLUDED__
#define __REQUESTS_H_INCLUDED__

#define USB_VID 0x16c0
#define USB_PID 0x05dc

#define CUSTOM_RQ_SET_NIXIE 3
#define CUSTOM_RQ_CONST_TUBE 0
#define CUSTOM_RQ_CONST_LED 1

#define CUSTOM_RQ_CONST_ANIMATION 4

#define CUSTOM_RQ_CONST_ANIMATION_NONE 0
#define CUSTOM_RQ_CONST_ANIMATION_STEP 1
#define CUSTOM_RQ_CONST_ANIMATION_LEVEL 2

#endif /* __REQUESTS_H_INCLUDED__ */
