#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/wdt.h>

#include "usbdrv.h"
#include "requests.h" /* custom requests used */

#define N_NIXIES 1

/* these are the values currently being displayed */
static uint8_t nixie_val[N_NIXIES] = {0};
/* the values that are actually to be displayed in the end */
static volatile uint8_t nixie_set[N_NIXIES] = {0};

static uint8_t led_val[N_NIXIES][3] = { {0,255,128} };

/* order of the layered electrodes */
static const uint8_t nixie_level[10] = {
	1,
	2,
	6,
	7,
	5,
	0,
	4,
	8,
	3
};

static uint8_t animation_style = CUSTOM_RQ_CONST_ANIMATION_NONE;
static uint8_t animation_speed = 10;

/* enough time has passed to show the next animation phase */
static volatile uint8_t time_passed = 0;

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
			nixie_set[data[1]] = data[2];
		}
		if (data[0] == CUSTOM_RQ_CONST_LED && data[1] < N_NIXIES && len >=5) {
			led_val[data[1]][0] = data[2];
			led_val[data[1]][1] = data[3];
			led_val[data[1]][2] = data[4];
		}
		if (data[0] == CUSTOM_RQ_CONST_ANIMATION && len >= 4) {
			animation_style = data[2];
			if (data[3] > 0) {
				animation_speed = data[3];
			}
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

static uint8_t get_level(uint8_t v) {
	uint8_t l = 0;
	for (l = 0; l<10; l++) {
		if (nixie_level[l] == v) return l;
	}
	return 0;
}

static void animate(void) {
	uint8_t i = 0;
	uint8_t cl = 0;
	uint8_t tl = 0;
	for (i = 0; i<N_NIXIES; i++) {
		switch (animation_style) {
			case CUSTOM_RQ_CONST_ANIMATION_STEP:
				if (nixie_val[i] > nixie_set[i]) {
					nixie_val[i]--;
				} else if (nixie_val[i]  < nixie_set[i]) {
					nixie_val[i]++;
				}
				break;
			case CUSTOM_RQ_CONST_ANIMATION_LEVEL:
				cl = get_level(nixie_val[i]);
				tl = get_level(nixie_set[i]);
				if (cl > tl) {
					/* move down a level */
					nixie_val[i] = nixie_level[cl-1];
				} else if (cl < tl) {
					nixie_val[i] = nixie_level[cl+1];
				}
				break;
			case CUSTOM_RQ_CONST_ANIMATION_NONE:
			default:
				nixie_val[i] = nixie_set[i];
				break;
		}
	}
}

int main(void) {
	DDRB |= (1<<PB0 | 1<<PB1 | 1<<PB2 | 1<<PB3);
	/* LED */
	DDRD |= (1<<PD5 | 1<<PD4 | 1<<PD1 | 1<<PD0 );

	/* configure timer for 100 Hz */
	TCCR1B = ( 1<<WGM12 | 1<<CS10 );
	OCR1A = 0x7D00;
	TIMSK |= (1 << OCIE1A);

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

		if (time_passed) {
			animate();
			time_passed = 0;
		}
	}
	return 0;
}

ISR(TIMER1_COMPA_vect) {
	static uint8_t count = 0;
	if (count++ >= animation_speed) {
		time_passed = 1;
		count = 0;
	}
}
