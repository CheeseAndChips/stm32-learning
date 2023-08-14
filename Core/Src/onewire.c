// https://www.analog.com/en/technical-articles/1wire-communication-through-software.html
#include <stdio.h>
#include <inttypes.h>
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

#ifndef ONEWIRE_LOW
#error "ONEWIRE_LOW not defined"
#endif

#ifndef ONEWIRE_RELEASE
#error "ONEWIRE_RELEASE not defined"
#endif

#ifndef ONEWIRE_READ
#error "ONEWIRE_READ not defined"
#endif

/*
 * Private function prototypes
 */
static void onewire_delay_us(const uint32_t us);
static void onewire_write_1(void);
static void onewire_write_0(void);
static void onewire_write_bit(uint8_t bit);
static uint8_t onewire_read_bit(void);
static uint8_t onewire_reset(void);

/*
 * Private variables
 */
static volatile uint16_t onewire_delay_counter = 0;
static TIM_HandleTypeDef *htim = NULL;

/* 
 * Private functions
 */
static void onewire_delay_us(const uint32_t us) {
//#ifdef DEBUG
//	assert(htim != NULL);
//#endif
	if (us == 0) {
		return;
	}

	onewire_delay_counter = us;
	htim->Instance->CNT = 0;
	HAL_TIM_Base_Start(htim);

	while (htim->Instance->CNT < us - 1)
		;

	HAL_TIM_Base_Stop(htim);
}

static void onewire_write_1(void) {
	ONEWIRE_LOW();
	onewire_delay_us(DELAY_A);
	ONEWIRE_RELEASE();
	onewire_delay_us(DELAY_B);
}

static void onewire_write_0(void) {
	ONEWIRE_LOW();
	onewire_delay_us(DELAY_C);
	ONEWIRE_RELEASE();
	onewire_delay_us(DELAY_D);
}

static void onewire_write_bit(uint8_t bit) {
	if (bit) {
		onewire_write_1();
	} else {
		onewire_write_0();
	}
}

static uint8_t onewire_read_bit(void) {
	ONEWIRE_LOW();
	onewire_delay_us(DELAY_A);
	ONEWIRE_RELEASE();
	onewire_delay_us(DELAY_E);
	uint8_t result = ONEWIRE_READ();
	onewire_delay_us(DELAY_F);
	return result;
}

static uint8_t onewire_reset(void) {
	onewire_delay_us(DELAY_G);
	ONEWIRE_LOW();
	onewire_delay_us(DELAY_H);
	ONEWIRE_RELEASE();
	onewire_delay_us(DELAY_I);
	uint8_t result = ONEWIRE_READ();
	onewire_delay_us(DELAY_J);
	return result;
}

/*
 * Public functions
 */
void onewire_init(TIM_HandleTypeDef *htim_) {
	htim = htim_;
}

//TODO: check CRC
//returns 0 if device cnt on bus != 1
uint64_t onewire_get_single_address(void) {
	onewire_reset();
	onewire_write_byte(0x0f);
	uint64_t rom = 0;
	for(uint8_t i = 0; i < 64; i++) {
		uint8_t b1 = onewire_read_bit();
		uint8_t b2 = onewire_read_bit();
		if(b1 == b2) {
			return 0;
		}
		rom = (rom << 1) | b1;
		onewire_write_bit(b1);
	}
	return rom;
}

void onewire_write_byte(uint8_t byte) {
	for (uint8_t i = 0; i < 8; i++) {
		if (byte & 0x80) {
			onewire_write_1();
		} else {
			onewire_write_0();
		}
		byte <<= 1;
	}
}

void onewire_search(void) {
	
}
