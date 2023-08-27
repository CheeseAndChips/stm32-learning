// https://www.analog.com/en/technical-articles/1wire-communication-through-software.html
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
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

#define ONEWIRE_CMD_CONVERT_T 0x44
#define ONEWIRE_CMD_READ_SCRATCHPAD 0xbe
#define ONEWIRE_CMD_WRITE_SCRATCHPAD 0x4e

#define ONEWIRE_SEARCH 0xf0
#define ONEWIRE_MATCH_ROM 0x55

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
static void onewire_write_byte(uint8_t byte);
static void onewire_match_rom(uint64_t rom);
static uint8_t onewire_calculate_crc(void *src, uint8_t byte_cnt);
static uint8_t onewire_read_scratchpad(uint64_t rom, uint8_t dest[8]);

/*
 * Private variables
 */
static volatile uint16_t onewire_delay_counter = 0;
static TIM_HandleTypeDef *htim = NULL;

/* 
 * Private functions
 */
static void onewire_delay_us(const uint32_t us) {
#ifdef DEBUG
	assert(htim != NULL);
#endif
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

static void onewire_write_byte(uint8_t byte) {
	for (uint8_t i = 0; i < 8; i++) {
		if (byte & 1) {
			onewire_write_1();
		} else {
			onewire_write_0();
		}
		byte >>= 1;
	}
}


static void onewire_match_rom(uint64_t rom) {
	onewire_reset();
	onewire_write_byte(ONEWIRE_MATCH_ROM);

	for(int i = 0; i < 8; i++) {
		onewire_write_byte(((uint8_t*)&rom)[i]);
	}
}

static uint8_t onewire_calculate_crc(void *src, uint8_t byte_cnt) {
	uint8_t crc = 0;
	for(size_t byte = 0; byte < byte_cnt; byte++) {
		uint8_t data = ((uint8_t*)src)[byte_cnt - 1 -byte];
		for(int i = 0; i < 8; i++) {
			uint8_t new_bit = (data & 1) ^ (crc & 1);
			crc = crc >> 1;
			if(new_bit) {
				crc ^= 0b10001100;
			}
			data >>= 1;
		}
	}

	return crc;
}

static uint8_t onewire_read_scratchpad(uint64_t rom, uint8_t dest[8]) {
	onewire_match_rom(rom);

	onewire_write_byte(ONEWIRE_CMD_READ_SCRATCHPAD);
	uint8_t data[9];
	for(int i = 8; i >= 0; i--) {
		uint8_t *curr_byte = data + i;
		for(int j = 0; j < 8; j++) {
			*curr_byte = (*curr_byte >> 1) | (onewire_read_bit() << 7);
		}
	}

	uint8_t crc = onewire_calculate_crc(data + 1, 8);

	if(crc != data[0]) {
		return 0;
	}

	if(dest != NULL) {
		for(int i = 0; i < 8; i++) {
			dest[i] = data[i + 1];
		}
	}
	return 1;
}

/*
 * Public functions
 */
void onewire_init(TIM_HandleTypeDef *htim_) {
	htim = htim_;
}

// https://www.analog.com/en/app-notes/1wire-search-algorithm.html
static int32_t last_discrepancy;
static onewire_search_state search_state;
static uint64_t rom;
void onewire_start_search(void) {
	last_discrepancy = -1;
	search_state = RUNNING;
	rom = 0;
}

uint64_t onewire_search(void) {
	int32_t last_zero = -1;
	uint64_t mask = 1;

	switch(search_state) {
		case DONE:
		case BAD_CRC:
		case NO_DEVICE:
			return 0;
		case NOT_INITIALIZED:
			onewire_start_search();
			break;
		case RUNNING:
			break;
	}

	onewire_reset();
	onewire_write_byte(ONEWIRE_SEARCH);
	for(int32_t i = 0; i < 64; i++) {
		uint8_t b1 = onewire_read_bit();
		uint8_t b2 = onewire_read_bit();
		uint8_t next_bit;

		if(b1 == 1 && b2 == 1) {
			search_state = NO_DEVICE;
			return 0;
		} else if(b1 != b2) {
			next_bit = b1;
		} else { // b1 = b2 = 0 (discrepancy)
			if(i < last_discrepancy) {
				next_bit = (rom & mask) != 0;
			} else if(i > last_discrepancy) {
				next_bit = 0;
			} else {
				next_bit = 1;
			}

			if(next_bit == 0) {
				last_zero = i;
			}
		}

		if(next_bit) {
			rom |= mask;
		} else {
			rom &= ~mask;
		}
		rom |= mask * next_bit;
		mask <<= 1;
		onewire_write_bit(next_bit);
	}

	last_discrepancy = last_zero;
	if(last_discrepancy == -1) {
		search_state = DONE;
	}

	uint8_t rom_bin[8];
	uint8_t *ptr1 = rom_bin;
	uint8_t *ptr2 = (uint8_t*)(&rom) + 7;
	for(int i = 8; i > 0; i--) {
		*ptr1 = *ptr2;
		ptr1++;
		ptr2--;
	}

	if(onewire_calculate_crc(rom_bin, 8) != 0) {
		search_state = BAD_CRC;
		return 0;
	}

	return rom;
}

onewire_search_state onewire_get_search_state(void) {
	return search_state;
}

void onewire_request_conversion(uint64_t rom) {
	onewire_match_rom(rom);
	onewire_write_byte(ONEWIRE_CMD_CONVERT_T);
}

uint8_t onewire_get_request_status() {
	return onewire_read_bit();
}

int16_t onewire_read_temperature(uint64_t rom) {
	uint8_t scratchpad[8];
	if(!onewire_read_scratchpad(rom, scratchpad)) {
		return 0;
	}

	return (scratchpad[6] << 8) | scratchpad[7];
}

void onewire_format_temperature(int16_t temp, char *dest, size_t len) {
	int16_t temp_whole = temp / 16;
	int16_t temp_frac = temp & 0xf;

	const uint16_t fractions[] = {10000 / 16, 10000 / 8, 10000 / 4, 10000 / 2};
	uint16_t fractional_sum = 0;
	for(int i = 0; i < 4; i++) {
		fractional_sum += fractions[i] * ((temp_frac >> i) & 1);
	}

	if(temp < 0) {
		if(fractional_sum)
			fractional_sum = 10000 - fractional_sum;

		snprintf(dest, len, "-%d.%04d", -temp_whole, fractional_sum);
	} else {
		snprintf(dest, len, "%d.%04d", temp_whole, fractional_sum);
	}

	for(size_t i = strlen(dest) - 1; i > 0; i--) {
		if(dest[i] == '0' && dest[i - 1] != '.') {
			dest[i] = '\0';
		} else {
			break;
		}
	}
}

uint8_t onewire_set_resolution(uint64_t rom, onewire_resolution resolution) {
	uint8_t scratchpad[8];
	if(!onewire_read_scratchpad(rom, scratchpad))
		return 0;

	scratchpad[3] = resolution;
	onewire_match_rom(rom);
	onewire_write_byte(ONEWIRE_CMD_WRITE_SCRATCHPAD);
	for(int i = 2; i >= 0; i--) {
		onewire_write_byte(scratchpad[3 + i]);
	}
	return 1;
}
