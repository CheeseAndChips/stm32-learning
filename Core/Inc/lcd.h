#ifndef INC_LCD_H_
#define INC_LCD_H_
#include <stdint.h>

#define FONT_H 24
#define FONT_W 12

#define DISPLAY_H 240
#define DISPLAY_W 320

#define FLOAT_TO_INT(x, y) ((int16_t)((x) * (y)))

// 5bit R, 6bit G, 5bit B
#define COMBINE_COLOR(r, g, b) ( (((r) << 11) | ((g) << 5) | (b)) & 0xffff )
#define COMBINE_COLOR_FLOAT(r, g, b) COMBINE_COLOR(FLOAT_TO_INT(r, 31), FLOAT_TO_INT(g, 63), FLOAT_TO_INT(b, 31))

#define COLOR_WHITE COMBINE_COLOR_FLOAT(1, 1, 1)
#define COLOR_RED COMBINE_COLOR_FLOAT(1, 0, 0)
#define COLOR_GREEN COMBINE_COLOR_FLOAT(0, 1, 0)
#define COLOR_BLUE COMBINE_COLOR_FLOAT(0, 0, 1)
#define COLOR_ORANGE COMBINE_COLOR_FLOAT(1, 0.5, 0)


#define SPECIAL_DEGREE "\x80"

typedef struct {
	uint8_t row, col;
} cursor_pos_t;

void lcd_init(void);
void lcd_text_init(void);
// void lcd_text_newline(void);
// void lcd_text_backspace(void);
void lcd_text_set_cursor(uint8_t x, uint8_t y);
void lcd_clear(void);
void lcd_set_pixel(int16_t x, int16_t y, uint16_t color);
void lcd_text_putc(char c);
void lcd_text_puts(const char *str);
void lcd_text_printf(const char *format, ...);
uint8_t lcd_text_lines_left(void);
void lcd_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

#endif /* INC_LCD_H_ */
