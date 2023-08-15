#ifndef INC_ONEWIRE_H_
#define INC_ONEWIRE_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <assert.h>

#define ONEWIRE_LOW() (ONEWIRE_OUT_GPIO_Port->BSRR = ONEWIRE_OUT_Pin << 16)
#define ONEWIRE_RELEASE() (ONEWIRE_OUT_GPIO_Port->BSRR = ONEWIRE_OUT_Pin)
#define ONEWIRE_READ() ((ONEWIRE_IN_GPIO_Port->IDR & ONEWIRE_IN_Pin) != 0)

void onewire_init(TIM_HandleTypeDef *htim_);
uint64_t onewire_get_single_address(void);
void onewire_request_conversion(uint64_t rom);
uint8_t onewire_get_request_status();
int16_t onewire_read_temperature(uint64_t rom);
void onewire_format_temperature(int16_t temp, char *dest, size_t len);

#endif /* INC_ONEWIRE_H_ */
