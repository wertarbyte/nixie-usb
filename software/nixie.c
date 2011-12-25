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

static int send_buffer(usb_dev_handle *handle, uint8_t *buf, uint8_t l) {
	int8_t sent = usb_control_msg(handle,
			USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
			CUSTOM_RQ_SET_NIXIE,
			0, 0,
			buf, l,
			100);
	if (sent < l) {
		fprintf(stderr, "Unable to send to USB nixie\n");
		return 1;
	}
	return 0;
}

static int set_tube(usb_dev_handle *handle, uint8_t tube, uint8_t value) {
	uint8_t buf[3];
	buf[0] = CUSTOM_RQ_CONST_TUBE;
	buf[1] = (uint8_t) tube;
	buf[2] = (uint8_t) value;
	return send_buffer(handle, buf, sizeof(buf));
}

static int set_led(usb_dev_handle *handle, uint8_t led, uint8_t r, uint8_t g, uint8_t b) {
	uint8_t buf[5];
	buf[0] = CUSTOM_RQ_CONST_LED;
	buf[1] = (uint8_t) led;
	buf[2] = (uint8_t) r;
	buf[3] = (uint8_t) g;
	buf[4] = (uint8_t) b;
	return send_buffer(handle, buf, sizeof(buf));
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
		int r = 0;
		int g = 0;
		int b = 0;
		if (sscanf(argv[0], "t%d:%d", &tube, &value) == 2 && tube >= 0 && value >= 0) {
			printf("Setting nixie tube %u to %u.\n", tube, value);
			if (set_tube(handle, tube, value)) {
				fprintf(stderr, "Unable to send to USB nixie\n");
				usb_close(handle);
				return 1;
			}
		} else if (sscanf(argv[0], "l%d:%d/%d/%d", &tube, &r, &g, &b) == 4 && tube >= 0) {
			printf("Setting nixie LED %u to %u/%u/%u.\n", tube, r, g, b);
			if (set_led(handle, tube, r, g, b)) {
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
