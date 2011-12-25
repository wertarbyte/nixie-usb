#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/wdt.h>

#include "usbdrv.h"
#include "requests.h" /* custom requests used */

#define N_NIXIES 1

static uint8_t nixie_val[N_NIXIES] = {0};

PROGMEM char usbHidReportDescriptor[22] = {    /* USB report descriptor */
	0x06, 0x00, 0xff,              // USAGE_PAGE (Generic Desktop)
	0x09, 0x01,                    // USAGE (Vendor Usage 1)
	0xa1, 0x01,                    // COLLECTION (Application)
	0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
	0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
	0x75, 0x08,                    //   REPORT_SIZE (8)
	0x95, 0x01,                    //   REPORT_COUNT (1)
	0x09, 0x00,                    //   USAGE (Undefined)
	0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)
	0xc0                           // END_COLLECTION
};

void usbEventResetReady(void) {
}

usbMsgLen_t usbFunctionSetup(uchar data[8]) {
	usbRequest_t    *rq = (usbRequest_t *)data;

	if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_VENDOR) {
		switch(rq->bRequest) {
			case CUSTOM_RQ_SET_NIXIE:
				return USB_NO_MSG;
		}
	} else {
		/* calls requests USBRQ_HID_GET_REPORT and USBRQ_HID_SET_REPORT are
		* not implemented since we never call them. The operating system
		* won't call them either because our descriptor defines no meaning.
		*/
	}
	return 0;   /* default for not implemented requests: return no data back to host */
}

uchar usbFunctionWrite(uchar *data, uchar len) {
	if (len > 1) {
		if (data[0] < N_NIXIES) {
			nixie_val[data[0]] = data[1];
		}
	}
	return 1;
}

static void set_nixie(uint8_t v) {
	uint8_t mask = (1<<PB0 | 1<<PB1 | 1<<PB2 | 1<<PB3);
	uint8_t val = 0;
	if (v & 1<<0) val |= 1<<PB3; // A
	if (v & 1<<1) val |= 1<<PB1; // B
	if (v & 1<<2) val |= 1<<PB0; // C
	if (v & 1<<3) val |= 1<<PB2; // D

	PORTB = (PORTB & ~mask) | (val & mask);
}

int main(void) {
	DDRB |= (1<<PB0 | 1<<PB1 | 1<<PB2 | 1<<PB3);
	wdt_enable(WDTO_1S);

	/* prepare USB */
	usbInit();
	usbDeviceDisconnect();
	/* fake USB disconnect for >250ms */
	uint8_t i = 255;
	while (i--) {
		wdt_reset();
		_delay_ms(1);
	}
	usbDeviceConnect();

	sei();

	uint8_t m_tube = 0;
	while(1) {
		set_nixie(nixie_val[m_tube]);
		m_tube = (m_tube+1) % N_NIXIES;

		wdt_reset();
		usbPoll();
	}
	return 0;
}
