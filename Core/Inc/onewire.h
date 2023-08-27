#ifndef INC_ONEWIRE_H_
#define INC_ONEWIRE_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <assert.h>

#define ONEWIRE_LOW() (ONEWIRE_OUT_GPIO_Port->BSRR = ONEWIRE_OUT_Pin << 16)
#define ONEWIRE_RELEASE() (ONEWIRE_OUT_GPIO_Port->BSRR = ONEWIRE_OUT_Pin)
#define ONEWIRE_READ() ((ONEWIRE_IN_GPIO_Port->IDR & ONEWIRE_IN_Pin) != 0)

typedef enum {
	ONEWIRE_RESOLUTION_9BIT = 0x1f,
	ONEWIRE_RESOLUTION_10BIT = 0x3f,
	ONEWIRE_RESOLUTION_11BIT = 0x5f,
	ONEWIRE_RESOLUTION_12BIT = 0x7f,
} onewire_resolution;

typedef enum {
	NOT_INITIALIZED = 0,
	RUNNING,
	DONE,
	BAD_CRC,
	NO_DEVICE,
} onewire_search_state;

void onewire_init(TIM_HandleTypeDef *htim_);
int32_t onewire_get_devices(int32_t max_device_cnt, uint64_t *roms);
void onewire_request_conversion(uint64_t rom);
void onewire_request_conversion_multiple(uint64_t *roms, int cnt);
void onewire_start_search(void);
uint64_t onewire_search(void);
onewire_search_state onewire_get_search_state(void);
uint8_t onewire_get_request_status();
int16_t onewire_read_temperature(uint64_t rom);
void onewire_format_temperature(int16_t temp, char *dest, size_t len);
uint8_t onewire_set_resolution(uint64_t rom, onewire_resolution resolution);

#endif /* INC_ONEWIRE_H_ */
