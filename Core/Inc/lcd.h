#ifndef INC_LCD_H_
#define INC_LCD_H_
#include <stdint.h>

typedef uint8_t color_t;

#define FONT_H 24
#define FONT_W 12

#define DISPLAY_H 240
#define DISPLAY_W 320

#define LINE_LENGTH (DISPLAY_W / FONT_W)
#define LINE_COUNT (DISPLAY_H / FONT_H)

#define CLAMP(x, y) ( ((x) > (y)) ? (y) : (x) )
#define FLOAT_TO_INT_UNCLAMPED(x, y) ((uint16_t)((x) * (y)))
#define FLOAT_TO_INT(x, y) CLAMP(FLOAT_TO_INT_UNCLAMPED(x, y), y)

// 5bit R, 6bit G, 5bit B
#define COMBINE_COLOR(r, g, b) ( (((r) << 11) | ((g) << 5) | (b)) & 0xffff )
#define COMBINE_COLOR_FLOAT(r, g, b) COMBINE_COLOR(FLOAT_TO_INT(r, 31), FLOAT_TO_INT(g, 63), FLOAT_TO_INT(b, 31))

#define COLOR_BLACK		0
#define COLOR_WHITE		1
#define COLOR_RED		2
#define COLOR_GREEN		3
#define COLOR_BLUE		4
#define COLOR_ORANGE	5
#define COLOR_YELLOW	6
#define COLOR_GRAY		7

#define SPECIAL_DEGREE "\x80"

typedef struct {
	uint8_t row, col;
} cursor_pos_t;

void lcd_init(void);
void lcd_clear(void);
void lcd_clear_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void lcd_set_pixel(int16_t x, int16_t y, color_t color);
void lcd_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, color_t color);
void lcd_text_set_cursor(uint8_t x, uint8_t y);
void lcd_text_putc(color_t color, char c);
void lcd_text_puts(color_t color, const char *str);
void lcd_text_printf(color_t color, const char *format, ...);
void lcd_puts_freely(int16_t x, int16_t y, color_t color, const char *str);
void lcd_printf_freely(int16_t x, int16_t y, color_t color, const char *format, ...);
void lcd_dump_buffer(void);
void lcd_text_newline_with_clearing(void);

#endif /* INC_LCD_H_ */
