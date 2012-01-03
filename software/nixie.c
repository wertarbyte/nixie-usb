/*
 * nixie.c
 *
 * By Stefan Tomanek <stefan@pico.ruhr.de>
 */

#include <stdio.h>
#include <stdint.h>
#include <errno.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <usb.h>
#include "../firmware/requests.h"

#define V_NAME "Wertarbyte.de"
#define P_NAME "Nixie"

#define TUBE_OFF 11

#define MAX_DIGITS 3

uint8_t open_usb(usb_dev_handle **handle) {
	uint16_t vid = USB_VID;
	uint16_t pid = USB_PID;
	char vendor[256];
	char product[256];
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
				if (target) {
					usb_get_string_simple(target, dev->descriptor.iManufacturer, vendor, sizeof(vendor));
					usb_get_string_simple(target, dev->descriptor.iProduct, product, sizeof(product));
					if (strcmp(vendor, V_NAME) == 0 && strcmp(product, P_NAME) == 0) {
						/* we found our device */
						break;
					}
				}
				usb_close(target);
				target = NULL;
			}
		}
	}
	if (target != NULL) {
		usb_claim_interface(target, 0);
		*handle = target;
		return 1;
	} else {
		return 0;
	}
}

static int send_usb_msg(usb_dev_handle *handle, uint8_t req, uint16_t i, uint16_t v, uint8_t *buf, uint8_t l) {
	uint8_t retry = 10;
	int8_t sent = -1;
	do {
		sent = usb_control_msg(handle,
			USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
			req,
			i, v,
			buf, l,
			100);
	} while (sent < l && retry-- && (usleep(5000) == 0));

	if (sent < l) {
		perror("Error sending command");
		return 1;
	}
	return 0;
}

static int send_buffer(usb_dev_handle *handle, uint8_t *buf, uint8_t l) {
	return send_usb_msg(handle, CUSTOM_RQ_SET_NIXIE, 0, 0, buf, l);
}

static int set_tube(usb_dev_handle *handle, uint8_t tube, uint8_t value) {
	uint8_t buf[8];
	buf[0] = CUSTOM_RQ_CONST_TUBE;
	buf[1] = (uint8_t) tube;
	buf[2] = (uint8_t) value;
	return send_buffer(handle, buf, sizeof(buf));
}

static int set_led(usb_dev_handle *handle, uint8_t led, uint8_t r, uint8_t g, uint8_t b) {
	uint8_t buf[8];
	buf[0] = CUSTOM_RQ_CONST_LED;
	buf[1] = (uint8_t) led;
	buf[2] = (uint8_t) r;
	buf[3] = (uint8_t) g;
	buf[4] = (uint8_t) b;
	return send_buffer(handle, buf, sizeof(buf));
}

static int set_animation(usb_dev_handle *handle, uint8_t style, uint8_t speed) {
	uint8_t buf[8];
	buf[0] = CUSTOM_RQ_CONST_ANIMATION;
	buf[1] = (uint8_t) 0; /* not used yet */
	buf[2] = (uint8_t) style;
	buf[3] = (uint8_t) speed;
	return send_buffer(handle, buf, sizeof(buf));
}

static int set_number(usb_dev_handle *handle, int number, uint8_t leading_zero) {
	int i = 0;
	while (number || i < MAX_DIGITS) { /* handle at least MAX_DIGITS tubes until we have a reset function */
		uint8_t v = number % 10;
		if (!leading_zero && number == 0 && i != 0) v = TUBE_OFF; /* deactivate leading 0s */
		int r = set_tube(handle, i, v);
		number /= 10;
		if (r) return r;
		i++;
	}
	return 0;
}

static int set_color(usb_dev_handle *handle, uint8_t r, uint8_t g, uint8_t b) {
	int i = 0;
	while (i < MAX_DIGITS) { /* handle at least MAX_DIGITS tubes until we have a reset function */
		int res = set_led(handle, i++, r, g, b);
		if (res) return res;
	}
	return 0;
}

static int tubes_off(usb_dev_handle *handle) {
	int i = 0;
	/* this should probably be done by the firmware */
	while (i < MAX_DIGITS) { /* handle at least MAX_DIGITS tubes until we have a reset function */
		int res = set_tube(handle, i++, TUBE_OFF);
		if (res) return res;
	}
	return 0;
}

static int process_command(usb_dev_handle *handle, char *cmd);

static int read_cmds(usb_dev_handle *handle, uint8_t autoquit) {
	char *l = NULL;
	while (l = readline("> ")) {
		int r = process_command(handle, l);
		free(l);
		if (r != 0 && autoquit) return r;
	}
	return 0;
}

static int process_command(usb_dev_handle *handle, char *cmd) {
	int tube = 0;
	int value = 0;
	int anim = 0;
	int speed = 0;
	int r = 0;
	int g = 0;
	int b = 0;
	if (sscanf(cmd, "t%d:%d", &tube, &value) == 2 && tube >= 0 && value >= 0) {
		printf("Setting nixie tube %u to %u.\n", tube, value);
		return set_tube(handle, tube, value);
	} else if (sscanf(cmd, "l%d:%d/%d/%d", &tube, &r, &g, &b) == 4 && tube >= 0) {
		printf("Setting nixie LED %u to %u/%u/%u.\n", tube, r, g, b);
		return set_led(handle, tube, r, g, b);
	} else if (sscanf(cmd, "anim:%d:%d", &anim, &speed) == 2 && anim >= 0 && anim >= 0) {
		printf("Setting animation style %u with speed %u.\n", anim, speed);
		return set_animation(handle, anim, speed);
	} else if (sscanf(cmd, "lnum:%d", &value) == 1 && value >= 0) {
		printf("Setting number %u\n", value);
		return set_number(handle, value, 1);
	} else if (sscanf(cmd, "num:%d", &value) == 1 && value >= 0) {
		printf("Setting number %u\n", value);
		return set_number(handle, value, 0);
	} else if (sscanf(cmd, "color:%d/%d/%d", &r, &g, &b) == 3) {
		printf("Setting color %u/%u/%u\n", r, g, b);
		return set_color(handle, r, g, b);
	} else if (strcmp(cmd, "off") == 0) {
		printf("Turning off all tubes...\n");
		return tubes_off(handle);
	} else if (strcmp(cmd, "read") == 0) {
		printf("Reading commands from stdin...\n");
		read_cmds(handle, 0);
	} else if (strcmp(cmd, "readf") == 0) {
		printf("Reading commands from stdin (autofail)...\n");
		read_cmds(handle, 1);
	} else {
		fprintf(stderr, "Unable to parse command: %s\n", cmd);
		return 2;
	}
	return 0;

}

int main(int argc, char *argv[]) {
	usb_dev_handle *handle = NULL;
	int usb_present = open_usb(&handle);
	if (!handle) {
		perror("Unable to open usb device");
		return 1;
	}

	argc--;
	argv++;
	while (argc) {
		int result = process_command(handle, argv[0]);
		if (result == 1) {
			usb_close(handle);
			return 1;
		} else if (result == 2) {
			fprintf(stderr, "Unable to parse command line item: %s\n", argv[0]);
		}
		argc--;
		argv++;
	}
	usb_close(handle);
	return 0;
}
