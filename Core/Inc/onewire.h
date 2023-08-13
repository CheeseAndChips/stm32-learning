#ifndef INC_ONEWIRE_H_
#define INC_ONEWIRE_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <assert.h>

void onewire_init(TIM_HandleTypeDef *htim_);
void onewire_run_test(void);

#endif /* INC_ONEWIRE_H_ */
