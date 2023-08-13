// https://www.analog.com/en/technical-articles/1wire-communication-through-software.html
#include "onewire.h"
#include "main.h"

#define DELAY_A 6
#define DELAY_B 64
#define DELAY_C 60
#define DELAY_D 10
#define DELAY_E 9
#define DELAY_F 55
#define DELAY_G 0
#define DELAY_H 480
#define DELAY_I 70
#define DELAY_J 410

#define ONEWIRE_LOW()
#define ONEWIRE_RELEASE()
#define ONEWIRE_READ() 0

void onewire_delay_us(const uint32_t us) {
	if(us < 1) {
		return;
	}

	assert(0);
}

void onewire_write_1(void) {
	ONEWIRE_LOW();
	onewire_delay_us(DELAY_A);
	ONEWIRE_RELEASE();
	onewire_delay_us(DELAY_B);
}

void onewire_write_0(void) {
	ONEWIRE_LOW();
	onewire_delay_us(DELAY_C);
	ONEWIRE_RELEASE();
	onewire_delay_us(DELAY_D);
}

uint8_t onewire_read_bit(void) {
	ONEWIRE_LOW();
	onewire_delay_us(DELAY_A);
	ONEWIRE_RELEASE();
	onewire_delay_us(DELAY_E);
	uint8_t result = ONEWIRE_READ();
	onewire_delay_us(DELAY_F);
	return result;
}

uint8_t onewire_reset(void) {
	onewire_delay_us(DELAY_G);
	ONEWIRE_LOW();
	onewire_delay_us(DELAY_H);
	ONEWIRE_RELEASE();
	onewire_delay_us(DELAY_I);
	uint8_t result = ONEWIRE_READ();
	onewire_delay_us(DELAY_J);
	return result;
}
