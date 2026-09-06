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

#endif /* DISPLAY_H */
