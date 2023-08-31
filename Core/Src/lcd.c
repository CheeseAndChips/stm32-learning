#include "lcd.h"
#include "main.h"
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* FONT BEGIN */

// size of a single character in bytes
static const int CHAR_SIZE = (FONT_H * FONT_W) / 8 + (((FONT_H * FONT_W) % 8) != 0);

extern char _binary_font_hex_start;
extern char _binary_font_hex_end;
extern char _binary_font_hex_size;

static size_t get_font_size() {
	return (size_t)(&_binary_font_hex_size);
}

static const char *get_char_data(uint8_t c) {
	const char *start = &_binary_font_hex_start;
	const char *ret = start + c * CHAR_SIZE;
	assert(ret <= &_binary_font_hex_end);
	return ret;
}

extern char _binary_font_special_hex_start;
extern char _binary_font_special_hex_end;
extern char _binary_font_special_hex_size;

static size_t get_special_font_size() {
	return (size_t)(&_binary_font_hex_size);
}

static const char *get_special_char_data(uint8_t c) {
	const char *start = &_binary_font_special_hex_start;
	const char *ret = start + c * CHAR_SIZE;
	assert(ret <= &_binary_font_special_hex_end);
	return ret;
}

const char *get_symbol_data(uint8_t c) {
	if(c & 0x80) return get_special_char_data(c & 0x7f);
	else return get_char_data(c);
}

/* FONT END */

#define GET_GPIO_PORT(pin) (pin ## _GPIO_Port)
#define GET_GPIO_PIN_NUM(pin) (pin ## _Pin)
#define GET_GPIO_MASK(pin) GET_GPIO_PIN_NUM(pin)

#define SET_H(pin) GET_GPIO_PORT(pin)->BSRR |= (GET_GPIO_MASK(pin))
#define SET_L(pin) GET_GPIO_PORT(pin)->BSRR |= (GET_GPIO_MASK(pin) << 16)
#define GET_BSRR_MASK(pin, data) (GET_GPIO_MASK(LCD_D ## pin) << (16 * !(((data) >> pin) & 1)))

#if COLOR_BLACK != 0
#error COLOR_BACK should be 0
#endif

static uint16_t color_table[16] = { // max 16 colors allowed
	COMBINE_COLOR_FLOAT(0, 0, 0), // COLOR_BLACK
	COMBINE_COLOR_FLOAT(1, 1, 1), // COLOR_WHITE
	COMBINE_COLOR_FLOAT(1, 0, 0), // COLOR_RED
	COMBINE_COLOR_FLOAT(0, 1, 0), // COLOR_GREEN
	COMBINE_COLOR_FLOAT(0, 0, 1), // COLOR_BLUE
	COMBINE_COLOR_FLOAT(1, 0.8, 0), // COLOR_ORANGE
	COMBINE_COLOR_FLOAT(1, 1, 0), // COLOR_YELLOW
	COMBINE_COLOR_FLOAT(0.4, 0.4, 0.4), // COLOR_GRAY
};

static cursor_pos_t cursor_pos;
static uint8_t screen_buffer[DISPLAY_H*DISPLAY_W];

/*
 * Private functions
 */

static inline int16_t abs16(int16_t x);
static void lcd_write(uint8_t data);
static inline void lcd_command_write(uint8_t command);
static inline void lcd_data_write(uint8_t data);
static inline void lcd_data_write16(uint16_t data);
static void lcd_set_address(int16_t y1, int16_t y2, int16_t x1, int16_t x2);
static void lcd_text_write_symbol_raw(int16_t x, int16_t y, const char *ch, color_t color);
static void lcd_draw_line_octant(int16_t x0, int16_t y0, int16_t x1, int16_t y1, color_t color, uint8_t swap);

static inline int16_t abs16(int16_t x) {
	if(x < 0) return -x;
	else return x;
}

static void lcd_write(uint8_t data) {
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

static inline void lcd_data_write16(uint16_t data) {
	SET_H(LCD_RS);
	lcd_write((data >> 8) & 0xff);
	lcd_write(data & 0xff);
}

static void lcd_set_address(int16_t y1, int16_t y2, int16_t x1, int16_t x2) {
	lcd_command_write(0x2a);
	lcd_data_write16(y1);
	lcd_data_write16(y2);

	lcd_command_write(0x2b);
	lcd_data_write16(x1);
	lcd_data_write16(x2);

	lcd_command_write(0x2c);
}

static void lcd_text_write_symbol_raw(int16_t x, int16_t y, const char *ch, color_t color) {
	int8_t shift = 7;
	for(int i = 0; i < FONT_H; i++) {
		for(int j = 0; j < FONT_W; j++) {
			if(*ch & (1 << shift)) {
				lcd_set_pixel(j + x, i + y, color);
			} else {
				lcd_set_pixel(j + x, i + y, COLOR_BLACK);
			}

			if(--shift < 0) {
				shift = 7;
				ch++;
			}
		}
	}
}

// https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
static void lcd_draw_line_octant(int16_t x0, int16_t y0, int16_t x1, int16_t y1, color_t color, uint8_t swap) {
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
		if(swap) lcd_set_pixel(y, x, color);
		else lcd_set_pixel(x, y, color);
        if(D > 0) {
            y = y + yi;
            D = D + (2 * (dy - dx));
		} else {
            D = D + 2*dy;
		}
	}
}


/*
 * Public functions
 */

void lcd_init(void) {
	for(int i = 0; i < sizeof(color_table) / sizeof(color_table[0]); i++) {
		printf("%u: %u\n", i+1, color_table[i]);
	}

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
	lcd_data_write(0x08);

	// pixel format
	lcd_command_write(0x3a);
	lcd_data_write(0x55); // 16 bit

	lcd_command_write(0x11); // disable sleep
	HAL_Delay(10);
	lcd_command_write(0x29); // display on

	lcd_set_address(0, DISPLAY_H, 0, DISPLAY_W);
	SET_H(LCD_RS);
	lcd_write(0x00);
	for(int i = 0; i < 2*DISPLAY_W*DISPLAY_H - 1; i++) {
		SET_L(LCD_WR);
		SET_H(LCD_WR);
	}

	lcd_clear();
}

void lcd_clear(void) {
	memset(screen_buffer, 0, sizeof(screen_buffer));
	cursor_pos.row = 0;
	cursor_pos.col = 0;
}

void lcd_dump_buffer(void) {
	for(int i = 0; i < 0x200; i += 0x10) {
		printf("%04x: ", i);
		for(int j = 0; j < 0x10 / 2; j++) {
			printf("%02x%02x ", screen_buffer[i + 2*j], screen_buffer[i + 2*j + 1]);
		}
		printf("\n");
	}

	lcd_set_address(0, DISPLAY_H, 0, DISPLAY_W);
	for(int i = 0; i < DISPLAY_W * DISPLAY_H / 2; i++) {
		color_t c1, c2;
		c1 = screen_buffer[i] >> 4;
		c2 = screen_buffer[i] & 0x0f;
		uint16_t w1 = color_table[c1];
		uint16_t w2 = color_table[c2];
		//if(c1 || c2)
	//		printf("%u %u\n", w1, w2);
		//w1 = 0xffff;
		//w2 = 0xffff;
		if(w1 || w2)
			printf("%u %u\n", w1, w2);
		lcd_data_write16(w1);
		lcd_data_write16(w2);
	}
}

void lcd_set_pixel(int16_t x, int16_t y, uint8_t color_index) {
	if(x >= 0 && y >= 0 && x < DISPLAY_W && y < DISPLAY_H) {
		int index = x * DISPLAY_W + y;
		//printf("Setting index %i\n", index);
		printf("Setting index %i to color %x\n", index, color_index);
		uint8_t *dest = &screen_buffer[index / 2];

		if(index & 1) { *dest = ((*dest & 0xf0) | color_index); }
		else { *dest = ((*dest & 0x0f) | (color_index << 4)); }
	}
}

void lcd_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, color_t color) {
	if(abs16(y1 - y0) < abs16(x1 - x0)){
        if(x0 > x1) {
            lcd_draw_line_octant(x1, y1, x0, y0, color, 0);
		} else {
            lcd_draw_line_octant(x0, y0, x1, y1, color, 0);
		}
	} else {
        if(y0 > y1) {
			lcd_draw_line_octant(y1, x1, y0, x0, color, 1);
		} else {
			lcd_draw_line_octant(y0, x0, y1, x1, color, 1);
		}
	}
}

void lcd_text_set_cursor(uint8_t x, uint8_t y) {
	cursor_pos.row = x;
	cursor_pos.col = y;
}

void lcd_text_putc(color_t color, char c) {
	if(c == '\b') {
		if(cursor_pos.col) {
			cursor_pos.col--;
			const char *space = get_char_data(' ');
			lcd_text_write_symbol_raw(cursor_pos.col * FONT_W, cursor_pos.row * FONT_H, space, color);
		}
	} else if(c == '\n') {
		cursor_pos.row++;
		cursor_pos.col = 0;
	} else {
		if(cursor_pos.col < LINE_LENGTH) {
			const char *symbol_data = get_symbol_data(c);
			lcd_text_write_symbol_raw(cursor_pos.col * FONT_W, cursor_pos.row * FONT_H, symbol_data, color);
			cursor_pos.col++;
		}
	}
}

void lcd_text_puts(color_t color, const char *str) {
	while(*str) {
		lcd_text_putc(color, *str++);
	}
}

void lcd_text_printf(color_t color, const char *format, ...) {
	va_list args;
	va_start(args, format);
	char buf[LINE_LENGTH + 1]; // TODO: bad buffer length (for multi-line)
	vsnprintf(buf, LINE_LENGTH + 1, format, args);
	lcd_text_puts(color, buf);
	va_end(args);
}

void lcd_putc_freely(int16_t x, int16_t y, color_t color, char c) {
	const char *symbol_data = get_symbol_data(c);
	lcd_text_write_symbol_raw(x, y, symbol_data, color);
}

void lcd_puts_freely(int16_t x, int16_t y, color_t color, const char *str) {
	size_t len = strlen(str);
	for(size_t i = 0; i < len; i++) {
		lcd_putc_freely(x + i*DISPLAY_W, y, color, str[i]);
	}
}

void lcd_printf_freely(int16_t x, int16_t y, color_t color, const char *format, ...) {
	va_list args;
	va_start(args, format);
	char buf[LINE_LENGTH + 1];
	vsnprintf(buf, LINE_LENGTH + 1, format, args);
	lcd_puts_freely(x, y, color, buf);
	va_end(args);
}

void lcd_text_newline_with_clearing(void) {
	char buffer[LINE_LENGTH + 1];
	int chars_left = LINE_LENGTH - cursor_pos.col;
	if(chars_left > 0) {
		memset(buffer, ' ', chars_left);
		buffer[chars_left] = 0;
		lcd_text_puts(COLOR_WHITE, buffer);
	}
	lcd_text_putc(COLOR_WHITE, '\n');
}

