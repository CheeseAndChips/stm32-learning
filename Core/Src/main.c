/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "onewire.h"
#include "lcd.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MAX_ONEWIRE_DEVICES 16
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim6;

UART_HandleTypeDef huart3;

PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* USER CODE BEGIN PV */
static enum {
	START_READINGS,
	WAIT_FOR_READINGS,
	DISPLAY_READINGS,
	DISPLAY_CHART,
} main_loop_state = START_READINGS;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USB_OTG_FS_PCD_Init(void);
static void MX_TIM6_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int __io_putchar(int ch) {
	uint8_t c = ch;
	HAL_UART_Transmit(&huart3, &c, 1, 0xffff);
	return 1;
}

int __io_getchar(void) {
	uint8_t data;
	HAL_UART_Receive(&huart3, &data, 1, HAL_MAX_DELAY);
	return data;
}

typedef struct {
	uint16_t temp; // TODO: negatives probably don't work
	int32_t time;
} datapoint;

static int32_t map_value(int32_t in1, int32_t in2, int32_t out1, int32_t out2, int32_t x) {
	int64_t a = x - in1;
	int64_t b = in2 - in1;
	int64_t c = out2 - out1;
	int64_t d = (a*c)/b;
	return out1 + d;
}

static const uint16_t all_colors[] = {
	COLOR_RED,
	COLOR_GREEN,
};

#define MAX_CHART_DATAPOINTS 128
#define MARGIN_TOP 10
#define MARGIN_BOTTOM 10
#define MARGIN_LEFT 50
#define MARGIN_RIGHT 10
const int oldest_timestamp = -2 * 60 * 1000; // 2 minutes
static void draw_chart_line(datapoint *data, int datapoint_count, uint16_t min_temp, uint16_t max_temp, uint16_t color) {
	int32_t prev_temp = map_value(min_temp, max_temp, DISPLAY_H - MARGIN_BOTTOM, MARGIN_TOP, data[0].temp);
	int32_t prev_time = map_value(oldest_timestamp, 0, MARGIN_LEFT, DISPLAY_W - MARGIN_RIGHT, data[0].time);
	for(int i = 1; i < datapoint_count; i++) {
		int32_t curr_temp = map_value(min_temp, max_temp, DISPLAY_H - MARGIN_BOTTOM, MARGIN_TOP, data[i].temp);
		int32_t curr_time = map_value(oldest_timestamp, 0, MARGIN_LEFT, DISPLAY_W - MARGIN_RIGHT, data[i].time);

		lcd_draw_line(prev_time, prev_temp, curr_time, curr_temp, color);

		prev_temp = curr_temp;
		prev_time = curr_time;
	}
}

static uint8_t draw_chart(datapoint data[][MAX_CHART_DATAPOINTS], int datapoints_len[MAX_ONEWIRE_DEVICES], int device_cnt) {
	uint16_t min_temp = 0xffff;
	uint16_t max_temp = 0;
	for(int i = 0; i < device_cnt; i++) {
		for(int j = 0; j < datapoints_len[i]; j++) {
			if(data[i][j].temp < min_temp) min_temp = data[i][j].temp;
			if(data[i][j].temp > max_temp) max_temp = data[i][j].temp;
		}
	}

	if(min_temp >= max_temp) { // TODO handle amplitude of 0
		return 1;
	}

	int16_t temp_amplitude = max_temp - min_temp;

	int16_t markings[] = {
		(min_temp & ~0x7) + 0x8,
		(max_temp & ~0x7) - 0x8,
	};

	min_temp -= temp_amplitude / 4;
	max_temp += temp_amplitude / 4;

	char axis_buffer[128];
	for(int i = 0; i < sizeof(markings) / sizeof(markings[0]); i++) {
		int32_t location = map_value(min_temp, max_temp, DISPLAY_H - MARGIN_BOTTOM, MARGIN_TOP, markings[i]);
		if(!(min_temp <= markings[i] && markings[i] <= max_temp)) {
			printf("Warn: bad location (%i) with temperature %u\n", location, markings[0]);
			continue;
		}
		onewire_format_temperature(markings[i], axis_buffer, sizeof(axis_buffer));
		lcd_puts_freely(0, location - FONT_H / 2, COLOR_WHITE, axis_buffer);

		for(int j = MARGIN_LEFT + 1; j < DISPLAY_W - MARGIN_RIGHT; j++) {
			if(j % 16 < 8) {
				lcd_set_pixel(j, location, COLOR_GRAY);
			}
		}
	}

	for(int i = 0; i < device_cnt; i++) {
		draw_chart_line(data[i], datapoints_len[i], min_temp, max_temp, all_colors[i]);
	}

	return 0;
}

static void draw_chart_axes(uint16_t color) {
	lcd_draw_line(MARGIN_LEFT, DISPLAY_H - MARGIN_BOTTOM, DISPLAY_W - MARGIN_RIGHT, DISPLAY_H - MARGIN_BOTTOM, color);
	lcd_draw_line(MARGIN_LEFT, MARGIN_TOP, MARGIN_LEFT, DISPLAY_H - MARGIN_BOTTOM, color);
}

static void clean_old_datapoints(datapoint all_datapoints[][MAX_CHART_DATAPOINTS], int datapoints_len[], int device_cnt) {
	int start;
	for(int i = 0; i < device_cnt; i++) {
		for(start = 0; start < datapoints_len[i]; start++) {
			if(all_datapoints[i][start].time >= oldest_timestamp)
				break;
		}

		if(start > 0) {
			memmove(all_datapoints[i], all_datapoints[i] + start, sizeof(datapoint) * datapoints_len[i]);
			datapoints_len[i] -= start;
		}
	}
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_USART3_UART_Init();
	MX_USB_OTG_FS_PCD_Init();
	MX_TIM6_Init();
	/* USER CODE BEGIN 2 */
	printf("---- PROGRAM START ----\n\n");
	lcd_init();
	for(int i = 0; i < 16; i++) {
		if(i & 1) lcd_set_pixel(0, i, COLOR_RED);
		else lcd_set_pixel(0, i, COLOR_BLUE);
	}
	lcd_set_pixel(1, 0, COLOR_GRAY);
	lcd_set_pixel(2, 0, COLOR_GRAY);
	lcd_dump_buffer();
	for(;;) { }
	onewire_init(&htim6);

	datapoint all_datapoints[MAX_ONEWIRE_DEVICES][MAX_CHART_DATAPOINTS];
	int datapoints_len[MAX_ONEWIRE_DEVICES];
	memset(datapoints_len, 0, sizeof(datapoints_len));

	uint64_t roms[MAX_ONEWIRE_DEVICES];
	onewire_start_search();
	int device_cnt = 0;
	do {
		roms[device_cnt++] = onewire_search();
	} while(onewire_get_search_state() == RUNNING && device_cnt < MAX_ONEWIRE_DEVICES);

	switch(onewire_get_search_state()) {
		case NOT_INITIALIZED:
			printf("Should be impossible to reach?\n");
			Error_Handler();
		case BAD_CRC:
			printf("Bad CRC\n");
			Error_Handler();
		case NO_DEVICE:
			printf("No device connected\n");
			Error_Handler();
		case RUNNING:
			printf("Warning: too many devices\n");
		case DONE:
			break;
	}

	for(int i = 0; i < device_cnt; i++) {
		onewire_set_resolution(roms[i], ONEWIRE_RESOLUTION_11BIT);
	}
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	uint32_t last_reading = 0, current_time;
	uint32_t last_button_press = 0;
	uint8_t show_chart = 1;
	while (1) {
		if(HAL_GPIO_ReadPin(USER_Btn_GPIO_Port, USER_Btn_Pin) == GPIO_PIN_SET && last_button_press + 1000 < HAL_GetTick()) {
			last_button_press = HAL_GetTick();
			lcd_clear();
			show_chart = !show_chart;
			if(show_chart) {
				memset(datapoints_len, 0, sizeof(datapoints_len));
			}
		}

		switch(main_loop_state) {
			case START_READINGS: ;
				current_time = HAL_GetTick();
				if(current_time - last_reading >= 1000) {
					last_reading = current_time;
					onewire_request_conversion_multiple(roms, device_cnt);
					main_loop_state = WAIT_FOR_READINGS;
				}
				break;
			case WAIT_FOR_READINGS:
				if(!onewire_get_request_status())
					main_loop_state = show_chart ? DISPLAY_CHART : DISPLAY_READINGS;

				break;
			case DISPLAY_READINGS: ;
				char buffer[32];
				lcd_text_set_cursor(0, 0);
				for(int i = 0; i < device_cnt; i++) {
					uint16_t temp_raw = onewire_read_temperature(roms[i]);
					onewire_format_temperature(temp_raw, buffer, sizeof(buffer));
					lcd_text_printf(COLOR_WHITE, "Temp %i: %s " SPECIAL_DEGREE "C", i+1, buffer);
					lcd_text_newline_with_clearing();
				}
				main_loop_state = START_READINGS;
				break;
			case DISPLAY_CHART: ;
				for(int i = 0; i < device_cnt; i++) {
					uint16_t temp_raw = onewire_read_temperature(roms[i]);

					int current_index = datapoints_len[i]++;
					assert(current_index < MAX_CHART_DATAPOINTS);
					all_datapoints[i][current_index].time = current_time;
					all_datapoints[i][current_index].temp = temp_raw;
				}

				for(int i = 0; i < device_cnt; i++) {
					for(int j = 0; j < datapoints_len[i]; j++) {
						all_datapoints[i][j].time -= current_time;
					}
				}

				clean_old_datapoints(all_datapoints, datapoints_len, device_cnt);

				lcd_clear();
				draw_chart_axes(COLOR_WHITE);
				draw_chart(all_datapoints, datapoints_len, device_cnt);
				lcd_dump_buffer();

				for(int i = 0; i < device_cnt; i++) {
					for(int j = 0; j < datapoints_len[i]; j++) {
						all_datapoints[i][j].time += current_time;
					}
				}

				main_loop_state = START_READINGS;
				break;
		}
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Configure the main internal regulator output voltage
	 */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 4;
	RCC_OscInitStruct.PLL.PLLN = 168;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 7;
	RCC_OscInitStruct.PLL.PLLR = 2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief TIM6 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM6_Init(void) {

	/* USER CODE BEGIN TIM6_Init 0 */

	/* USER CODE END TIM6_Init 0 */

	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	/* USER CODE BEGIN TIM6_Init 1 */

	/* USER CODE END TIM6_Init 1 */
	htim6.Instance = TIM6;
	htim6.Init.Prescaler = 83;
	htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim6.Init.Period = 65535;
	htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim6) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM6_Init 2 */

	/* USER CODE END TIM6_Init 2 */

}

/**
 * @brief USART3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART3_UART_Init(void) {

	/* USER CODE BEGIN USART3_Init 0 */

	/* USER CODE END USART3_Init 0 */

	/* USER CODE BEGIN USART3_Init 1 */

	/* USER CODE END USART3_Init 1 */
	huart3.Instance = USART3;
	huart3.Init.BaudRate = 115200;
	huart3.Init.WordLength = UART_WORDLENGTH_8B;
	huart3.Init.StopBits = UART_STOPBITS_1;
	huart3.Init.Parity = UART_PARITY_NONE;
	huart3.Init.Mode = UART_MODE_TX_RX;
	huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart3.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart3) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART3_Init 2 */

	/* USER CODE END USART3_Init 2 */

}

/**
 * @brief USB_OTG_FS Initialization Function
 * @param None
 * @retval None
 */
static void MX_USB_OTG_FS_PCD_Init(void) {

	/* USER CODE BEGIN USB_OTG_FS_Init 0 */

	/* USER CODE END USB_OTG_FS_Init 0 */

	/* USER CODE BEGIN USB_OTG_FS_Init 1 */

	/* USER CODE END USB_OTG_FS_Init 1 */
	hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
	hpcd_USB_OTG_FS.Init.dev_endpoints = 6;
	hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
	hpcd_USB_OTG_FS.Init.dma_enable = DISABLE;
	hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
	hpcd_USB_OTG_FS.Init.Sof_enable = ENABLE;
	hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
	hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
	hpcd_USB_OTG_FS.Init.vbus_sensing_enable = ENABLE;
	hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;
	if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USB_OTG_FS_Init 2 */

	/* USER CODE END USB_OTG_FS_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* USER CODE BEGIN MX_GPIO_Init_1 */
	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOF_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOG_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOF,
			LCD_CS_Pin | LCD_RST_Pin | LCD_D0_Pin | LCD_D7_Pin | LCD_D4_Pin
					| LCD_D2_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOC, LCD_WR_Pin | LCD_RS_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(LCD_RD_GPIO_Port, LCD_RD_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOB, LD1_Pin | LD3_Pin | LD2_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOE, LCD_D6_Pin | LCD_D5_Pin | LCD_D3_Pin,
			GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(LCD_D1_GPIO_Port, LCD_D1_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(USB_PowerSwitchOn_GPIO_Port, USB_PowerSwitchOn_Pin,
			GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(ONEWIRE_OUT_GPIO_Port, ONEWIRE_OUT_Pin, GPIO_PIN_SET);

	/*Configure GPIO pin : USER_Btn_Pin */
	GPIO_InitStruct.Pin = USER_Btn_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(USER_Btn_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : LCD_CS_Pin LCD_RST_Pin LCD_D0_Pin LCD_D7_Pin
	 LCD_D4_Pin LCD_D2_Pin */
	GPIO_InitStruct.Pin = LCD_CS_Pin | LCD_RST_Pin | LCD_D0_Pin | LCD_D7_Pin
			| LCD_D4_Pin | LCD_D2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

	/*Configure GPIO pins : LCD_WR_Pin LCD_RS_Pin */
	GPIO_InitStruct.Pin = LCD_WR_Pin | LCD_RS_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pin : LCD_RD_Pin */
	GPIO_InitStruct.Pin = LCD_RD_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(LCD_RD_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : LD1_Pin LD3_Pin LD2_Pin */
	GPIO_InitStruct.Pin = LD1_Pin | LD3_Pin | LD2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pins : LCD_D6_Pin LCD_D5_Pin LCD_D3_Pin */
	GPIO_InitStruct.Pin = LCD_D6_Pin | LCD_D5_Pin | LCD_D3_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

	/*Configure GPIO pin : LCD_D1_Pin */
	GPIO_InitStruct.Pin = LCD_D1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(LCD_D1_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : USB_PowerSwitchOn_Pin */
	GPIO_InitStruct.Pin = USB_PowerSwitchOn_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(USB_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : USB_OverCurrent_Pin */
	GPIO_InitStruct.Pin = USB_OverCurrent_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(USB_OverCurrent_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : ONEWIRE_OUT_Pin */
	GPIO_InitStruct.Pin = ONEWIRE_OUT_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(ONEWIRE_OUT_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : ONEWIRE_IN_Pin */
	GPIO_InitStruct.Pin = ONEWIRE_IN_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(ONEWIRE_IN_GPIO_Port, &GPIO_InitStruct);

	/* USER CODE BEGIN MX_GPIO_Init_2 */
	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	printf("\n\n---- ERROR HANDLER REACHED ----\n\n");
	fflush(stdout);
	__disable_irq();
	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
