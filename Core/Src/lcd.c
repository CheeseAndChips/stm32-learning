#include "lcd.h"
#include "main.h"
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>

/* FONT BEGIN */

#define FONT_H 24
#define FONT_W 12

#define DISPLAY_H 240
#define DISPLAY_W 320

#define LINE_LENGTH (DISPLAY_W / FONT_W)
#define LINE_COUNT (DISPLAY_H / FONT_H)

// size of a single character in bytes
static const int CHAR_SIZE = (FONT_H * FONT_W) / 8 + (((FONT_H * FONT_W) % 8) != 0);

extern char _binary_font_hex_start;
extern char _binary_font_hex_end;
extern char _binary_font_hex_size;

size_t get_font_size() {
	return (size_t)(&_binary_font_hex_size);
}

const char *get_char_data(uint8_t c) {
	const char *start = &_binary_font_hex_start;
	const char *ret = start + c * CHAR_SIZE;
	assert(ret <= &_binary_font_hex_end);
	return ret;
}

extern char _binary_font_special_hex_start;
extern char _binary_font_special_hex_end;
extern char _binary_font_special_hex_size;

size_t get_special_font_size() {
	return (size_t)(&_binary_font_hex_size);
}

const char *get_special_char_data(uint8_t c) {
	const char *start = &_binary_font_special_hex_start;
	const char *ret = start + c * CHAR_SIZE;
	assert(ret <= &_binary_font_special_hex_end);
	return ret;
}

/* FONT END */

void lcd_text_update_cursor(void);

cursor_pos_t cursor_pos;

#define GET_GPIO_PORT(pin) (pin ## _GPIO_Port)
#define GET_GPIO_PIN_NUM(pin) (pin ## _Pin)

#define GET_GPIO_MASK(pin) GET_GPIO_PIN_NUM(pin)
#define GET_GPIO_MASK_INV(pin) (0xffff ^ GET_GPIO_MASK(pin))

#define SET_H(pin) GET_GPIO_PORT(pin)->ODR |= GET_GPIO_MASK(pin)
#define SET_L(pin) GET_GPIO_PORT(pin)->ODR &= GET_GPIO_MASK_INV(pin)
#define SET_PIN(pin, state) HAL_GPIO_WritePin(pin ## _GPIO_Port, pin ## _Pin, state)
#define GET_BSRR_MASK(pin, data) (GET_GPIO_MASK(LCD_D ## pin) << (16 * !(((data) >> pin) & 1)))

void lcd_write(uint8_t data) {
	SET_L(LCD_WR);

	GPIOF->BSRR |= \
		GET_BSRR_MASK(0, data) | \
		GET_BSRR_MASK(2, data) | \
		GET_BSRR_MASK(4, data) | \
		GET_BSRR_MASK(7, data);

	GPIOE->BSRR |= \
		GET_BSRR_MASK(3, data) | \
		GET_BSRR_MASK(5, data) | \
		GET_BSRR_MASK(6, data);

	GPIOD->BSRR |= \
		GET_BSRR_MASK(1, data);

	SET_H(LCD_WR);
}

static inline void lcd_command_write(uint8_t command) {
	SET_L(LCD_RS);
	lcd_write(command);
}

static inline void lcd_data_write(uint8_t data) {
	SET_H(LCD_RS);
	lcd_write(data);
}

void lcd_set_address(int16_t y1, int16_t y2, int16_t x1, int16_t x2) {
	lcd_command_write(0x2a);
	lcd_data_write(y1 >> 8);
	lcd_data_write(y1);
	lcd_data_write(y2 >> 8);
	lcd_data_write(y2);

	lcd_command_write(0x2b);
	lcd_data_write(x1 >> 8);
	lcd_data_write(x1);
	lcd_data_write(x2 >> 8);
	lcd_data_write(x2);
}

void lcd_set_pixel(int16_t x, int16_t y, int16_t color) {
	if(x >= 0 && y >= 0 && x < DISPLAY_W && y < DISPLAY_H) {
		lcd_set_address(y, y+1, x, x+1);
		lcd_command_write(0x2c);
		lcd_data_write(color >> 8);
		lcd_data_write(color);
	}
}

void lcd_init(void) {
	SET_H(LCD_RST);
	HAL_Delay(20);
	SET_L(LCD_RST);
	HAL_Delay(20);
	SET_H(LCD_RST);
	HAL_Delay(20);

	SET_H(LCD_CS);
	SET_H(LCD_WR);
	SET_H(LCD_RD);
	SET_L(LCD_CS);

	// VCOM1
	lcd_command_write(0xc5);
	lcd_data_write(0x3f);
	lcd_data_write(0x3c);

	// Memory access control
	lcd_command_write(0x36);
	lcd_data_write(0x00);

	// pixel format
	lcd_command_write(0x3a);
	lcd_data_write(0x55); // 16 bit

	lcd_command_write(0x11); // disable sleep
	HAL_Delay(10);
	lcd_command_write(0x29); // display on

	lcd_clear();
	
	lcd_text_update_cursor();
}

void lcd_clear(void) {
	lcd_set_address(0, DISPLAY_H, 0, DISPLAY_W);
	lcd_command_write(0x2c);
	SET_H(LCD_RS);
	lcd_write(0x00);
	for(int i = 0; i < 2*DISPLAY_H*DISPLAY_W - 1; i++) {
		SET_L(LCD_WR);
		SET_H(LCD_WR);
	}

	cursor_pos.row = 0;
	cursor_pos.col = 0;
	lcd_text_update_cursor();
}

void lcd_text_write_symbol_raw(const char *ch) {
	int8_t shift = 7;
	for(int i = 0; i < FONT_W * FONT_H; i++) {
		if(*ch & (1 << shift)) {
			lcd_data_write(0xff);
			lcd_data_write(0xff);
		} else {
			lcd_data_write(0x00);
			lcd_data_write(0x00);
		}
		if(--shift < 0) {
			shift = 7;
			ch++;
		}
	}
}

void lcd_text_update_cursor(void) {
	lcd_set_address(1 + cursor_pos.row * FONT_H, (1 + cursor_pos.row) * FONT_H, cursor_pos.col * FONT_W, DISPLAY_W);
	lcd_command_write(0x2c);
}

void lcd_text_set_cursor(uint8_t x, uint8_t y) {
	cursor_pos.row = x;
	cursor_pos.col = y;
	lcd_text_update_cursor();
}

void lcd_text_newline(void) {
	cursor_pos.row++;
	cursor_pos.col = 0;
	lcd_text_update_cursor();
}

void lcd_text_backspace(void) {
	if(cursor_pos.col) {
		cursor_pos.col--;
		lcd_text_update_cursor();
		lcd_text_write_symbol_raw(0);
		lcd_text_update_cursor();
	}
}

void lcd_text_putc(char c) {
	if(c == '\b') {
		lcd_text_backspace();
	} else if(c == '\n') {
		lcd_text_newline();
	} else {
		if(cursor_pos.col < LINE_LENGTH) {
			if(c & 0x80) { // special symbol
				lcd_text_write_symbol_raw(get_special_char_data(c ^ 0x80));
			} else {
				lcd_text_write_symbol_raw(get_char_data(c - ' '));
			}
			cursor_pos.col++;
		}
	}
}

void lcd_text_puts(const char *str) {
	while(*str) {
		lcd_text_putc(*str++);
	}
}

void lcd_text_printf(const char *format, ...) {
	va_list args;
	va_start(args, format);
	char buf[LINE_LENGTH + 1];
	vsnprintf(buf, LINE_LENGTH + 1, format, args);
	lcd_text_puts(buf);
	va_end(args);
}

// https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
static uint8_t do_swap = 0;
static void lcd_draw_line_low(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t color) {
	int16_t dx, dy, yi, D, y;

	dx = x1 - x0;
    dy = y1 - y0;
    yi = 1;
    if(dy < 0) {
        yi = -1;
        dy = -dy;
	}
    D = (2 * dy) - dx;
    y = y0;

    for(int16_t x = x0; x <= x1; x++) {
		if(do_swap) lcd_set_pixel(y, x, color);
		else lcd_set_pixel(x, y, color);
        if(D > 0) {
            y = y + yi;
            D = D + (2 * (dy - dx));
		} else {
            D = D + 2*dy;
		}
	}
}

static void lcd_draw_line_high(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t color) {
	do_swap = 1;
	lcd_draw_line_low(y0, x0, y1, x1, color);
}

static int16_t abs16(int16_t x) {
	if(x < 0) return -x;
	else return x;
}

void lcd_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t color) {
	do_swap = 0;
	if(abs16(y1 - y0) < abs16(x1 - x0)){
        if(x0 > x1) {
            lcd_draw_line_low(x1, y1, x0, y0, color);
		} else {
            lcd_draw_line_low(x0, y0, x1, y1, color);
		}
	} else {
        if(y0 > y1) {
			lcd_draw_line_high(x1, y1, x0, y0, color);
		} else {
			lcd_draw_line_high(x0, y0, x1, y1, color);
		}
	}
}
