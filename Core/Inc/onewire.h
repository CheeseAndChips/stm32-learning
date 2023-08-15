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
uint16_t onewire_read_temperature(uint64_t rom);

#endif /* INC_ONEWIRE_H_ */
