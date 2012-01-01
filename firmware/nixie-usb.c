#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <string.h>

#include <LUFA/Version.h>
#include <LUFA/Drivers/USB/USB.h>

#include "requests.h" /* custom requests used */

#define N_NIXIES 3

#ifndef SUPPORT_ANIMATION
#define SUPPORT_ANIMATION 1
#endif

/* these are the values currently being displayed */
static uint8_t nixie_val[N_NIXIES] = {0};

#if SUPPORT_ANIMATION
/* the values that are actually to be displayed in the end */
static uint8_t nixie_set[N_NIXIES] = {0};

/* order of the layered electrodes */
static const uint8_t nixie_level[10+1] = {
	10, // OFF!
	1,
	2,
	6,
	7,
	5,
	0,
	4,
	9,
	8,
	3
};

static uint8_t animation_style = CUSTOM_RQ_CONST_ANIMATION_LEVEL;
static uint8_t animation_speed = 8;
#endif

static uint8_t led_val[N_NIXIES][3] = { {0,0,0} };

/* enough time has passed to switch to the next tube */
static volatile uint8_t time_passed = 1;

/* enough time has passed to show the next animation phase */
static volatile uint8_t animation_step = 0;

static uint8_t process_usb_data(uint8_t *data, uint8_t len) {
	if (len > 2) {
		if (data[0] == CUSTOM_RQ_CONST_TUBE && data[1] < N_NIXIES) {
#if SUPPORT_ANIMATION
			nixie_set[data[1]] = data[2];
#else
			nixie_val[data[1]] = data[2];
#endif
		}
		if (data[0] == CUSTOM_RQ_CONST_LED && data[1] < N_NIXIES && len >=5) {
			memcpy(led_val[data[1]], &data[2], 3);
		}
#if SUPPORT_ANIMATION
		if (data[0] == CUSTOM_RQ_CONST_ANIMATION && len >= 4) {
			animation_style = data[2];
			if (data[3] > 0) {
				animation_speed = data[3];
			}
		}
#endif
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

	PORTB = (PORTB & ~mask) | val;
}

static void set_led(uint8_t c[3], uint8_t count) {
	if (count >= c[0])
		PORTD &= ~(1<<PD4);
	else
		PORTD |= 1<<PD4;
	if (count >= c[1])
		PORTD &= ~(1<<PD1);
	else
		PORTD |= 1<<PD1;
	if (count >= c[2])
		PORTD &= ~(1<<PD0);
	else
		PORTD |= 1<<PD0;
}

#if SUPPORT_ANIMATION
static uint8_t get_level(uint8_t v) {
	uint8_t l = sizeof(nixie_level);
	while (--l && nixie_level[l] != v) {}
	return l;
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
#endif

void EVENT_USB_Device_ControlRequest(void) {
	uint8_t buf[8];
	if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_VENDOR | REQREC_DEVICE)) {
		switch (USB_ControlRequest.bRequest) {
			case CUSTOM_RQ_SET_NIXIE:
				Endpoint_ClearSETUP();
				Endpoint_Read_Control_Stream_LE(&buf, sizeof(buf));
				Endpoint_ClearOUT();
				while (!(Endpoint_IsINReady()));
				Endpoint_ClearIN();
				process_usb_data(buf, sizeof(buf));
		}
	}
}

int main(void) {
	DDRB = (
		/* BCD */
		1<<PB0 | 1<<PB1 | 1<<PB2 | 1<<PB3 |
		/* Multiplexing */
		1<<PB7 | 1<<PB6 | 1<<PB5
	);
	DDRD = (
		/* LED */
		1<<PD4 | 1<<PD1 | 1<<PD0
	);
	clock_prescale_set(clock_div_1);

	/* configure timer for 200 Hz */
	TCCR1B = ( 1<<WGM12 | 1<<CS11 );
	OCR1A = 0x2710;
	TIMSK1 = (1 << OCIE1A);

	wdt_enable(WDTO_1S);
	clock_prescale_set(clock_div_1);

	/* prepare USB */
	USB_Init();

	sei();

	uint8_t pwm_count = 0;

	uint8_t m_tube = 0;
	while(1) {
		if (time_passed) {
#if N_NIXIES == 2
			m_tube = 1-m_tube;

			if (m_tube == 0) {
				PORTB |= 1<<PB6;
				set_nixie(nixie_val[m_tube]);
				PORTB &= ~(1<<PB7);
			} else {
				set_nixie(nixie_val[m_tube]);
				PORTB &= ~(1<<PB6);
			}
			time_passed = 0;
#elif N_NIXIES == 3
			m_tube = (m_tube < (N_NIXIES-1)) ? m_tube+1 : 0;
			PORTB |= (1<<PB7 | 1<<PB6 | 1<<PB5);
			set_nixie(nixie_val[m_tube]);
			switch(m_tube) {
				case 0:
					PORTB &= ~(1<<PB7);
					break;
				case 1:
					PORTB &= ~(1<<PB6);
					break;
				case 2:
					PORTB &= ~(1<<PB5);
					break;
			}
			time_passed = 0;
#else
		/* add some generic multiplexing code here... */
#endif
		}
		set_led(led_val[m_tube], pwm_count);

		wdt_reset();
		USB_USBTask();

		pwm_count = (pwm_count == UINT8_MAX) ? 2 : pwm_count+1;

#if SUPPORT_ANIMATION
		if (animation_step) {
			animate();
			animation_step = 0;
		}
#endif
	}
	return 0;
}

ISR(TIMER1_COMPA_vect) {
#if SUPPORT_ANIMATION
	static uint8_t count = 0;
	if (count++ >= animation_speed) {
		animation_step = 1;
		count = 0;
	}
#endif
	time_passed = 1;
}
