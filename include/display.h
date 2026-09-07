#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stddef.h>
#include <multiboot.h>

void init_display(multiboot_info_t* mb_info_data);
void draw_pixel(uint32_t x, uint32_t y, uint32_t color);
void draw_rect(uint32_t start_x, uint32_t start_y, uint32_t w, uint32_t h, uint32_t color);
void draw_char(char c);
void draw_string(char *c);
void set_cursor(uint32_t x, uint32_t y);
void move_cursor_text();
void newline();
void backspace();
void display_clear(void);
void display_scroll_up(void);
void display_scroll_down(void);
void display_set_region(uint32_t x0, uint32_t top, uint32_t x1, uint32_t bot);
void display_set_fg(uint32_t c);
void display_set_bg(uint32_t c);
void display_get_cursor(uint32_t *x, uint32_t *y);
uint32_t display_width(void);
uint32_t display_height(void);
void display_text_at(uint32_t x, uint32_t y, char *s);
void display_enable_double_buffer(int enable);
void display_present(void);
uint32_t *display_get_backbuffer(void);
int display_is_double_buffered(void);

#define DISPLAY_MAX_CONSOLES 32
void display_select(int id);
void display_create(int id, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);
void display_render(int id);
void display_render_all(void);
int display_current(void);
void display_get_fg_bg(uint32_t *fg, uint32_t *bg);

#endif
