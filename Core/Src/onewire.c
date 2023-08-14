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

	while (htim->Instance->CNT < us)
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

void onewire_run_test(void) {
	onewire_reset();
	HAL_Delay(1);
	onewire_reset();
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
