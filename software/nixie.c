/*
 * nixie.c
 *
 * By Stefan Tomanek <stefan@pico.ruhr.de>
 */

#include <stdio.h>
#include <stdint.h>

#include <usb.h>
#include "../firmware/requests.h"
#include "../firmware/usbconfig.h"

uint8_t open_usb(usb_dev_handle **handle) {
	uint16_t vid = 0x16c0;
	uint16_t pid = 0x05df;
	struct usb_bus *bus;
	struct usb_device *dev;
	usb_dev_handle *target = NULL;

	usb_init();
	usb_find_busses();
	usb_find_devices();
	for (bus=usb_get_busses(); bus; bus=bus->next) {
		for (dev=bus->devices; dev; dev=dev->next) {
			if (dev->descriptor.idVendor == vid && dev->descriptor.idProduct == pid) {
				target = usb_open(dev);
			}
		}
	}
	if (target != NULL) {
		*handle = target;
		return 1;
	} else {
		return 0;
	}
}

int main(int argc, char *argv[]) {
	usb_dev_handle *handle = NULL;
	int usb_present = open_usb(&handle);
	if (!handle) {
		fprintf(stderr, "Unable to open usb device.\n");
		return 1;
	}

	argc--;
	argv++;
	while (argc) {
		int tube = 0;
		int value = 0;
		if (sscanf(argv[0], "%d:%d", &tube, &value) == 2 && tube >= 0 && value >= 0) {
			printf("Setting nixie %u tube to %u.\n", tube, value);
			uint8_t buf[3];
			buf[0] = CUSTOM_RQ_CONST_TUBE;
			buf[1] = (uint8_t) tube;
			buf[2] = (uint8_t) value;
			int8_t sent = usb_control_msg(handle,
					USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
					CUSTOM_RQ_SET_NIXIE,
					0, 0,
					buf, sizeof(buf),
					100);
			if (sent < sizeof(buf)) {
				fprintf(stderr, "Unable to send to USB nixie\n");
				usb_close(handle);
				return 1;
			}

		} else {
			fprintf(stderr, "Unable to parse command line item: %s.\n", argv[0]);
			usb_close(handle);
			return 1;
		}
		argc--;
		argv++;
	}
	usb_close(handle);
	return 0;
}
