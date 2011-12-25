#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/wdt.h>

#include "usbdrv.h"
#include "requests.h" /* custom requests used */

#define N_NIXIES 1

static uint8_t nixie_val[N_NIXIES] = {0};
static uint8_t led_val[N_NIXIES][3] = { {255,0,0} };

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
	if (len > 2) {
		if (data[0] == CUSTOM_RQ_CONST_TUBE && data[1] < N_NIXIES) {
			nixie_val[data[1]] = data[2];
		}
		if (data[0] == CUSTOM_RQ_CONST_LED && data[1] < N_NIXIES && len >=5) {
			led_val[data[1]][0] = data[2];
			led_val[data[1]][1] = data[3];
			led_val[data[1]][2] = data[4];
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

static void set_led(uint8_t c[3], uint8_t count) {
	if (count > c[0] || c[0] == 0)
		PORTD &= ~(1<<PD4);
	else
		PORTD |= 1<<PD4;
	if (count > c[1] || c[1] == 0)
		PORTD &= ~(1<<PD1);
	else
		PORTD |= 1<<PD1;
	if (count > c[2] || c[2] == 0)
		PORTD &= ~(1<<PD0);
	else
		PORTD |= 1<<PD0;
}

int main(void) {
	DDRB |= (1<<PB0 | 1<<PB1 | 1<<PB2 | 1<<PB3);
	/* LED */
	DDRD |= (1<<PD5 | 1<<PD4 | 1<<PD1 | 1<<PD0 );

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

	uint8_t pwm_count = 0;
	uint8_t m_tube = 0;
	while(1) {
		PORTD &= ~(1<<PD5);
		set_led(led_val[m_tube], pwm_count);
		set_nixie(nixie_val[m_tube]);
		m_tube = (m_tube+1) % N_NIXIES;

		wdt_reset();
		usbPoll();

		pwm_count = (pwm_count == UINT8_MAX) ? 0 : pwm_count+1;
	}
	return 0;
}
