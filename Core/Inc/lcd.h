#ifndef INC_LCD_H_
#define INC_LCD_H_
#include <stdint.h>

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
void lcd_set_pixel(int16_t x, int16_t y, int16_t color);
void lcd_text_putc(char c);
void lcd_text_puts(const char *str);
void lcd_text_printf(const char *format, ...);
uint8_t lcd_text_lines_left(void);
void lcd_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t color);

#endif /* INC_LCD_H_ */
