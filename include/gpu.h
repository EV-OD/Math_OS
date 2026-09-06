#ifndef GPU_H
#define GPU_H

#include <stdint.h>

void gpu_init(uint32_t fb_addr, uint32_t pitch, uint32_t w, uint32_t h);
void gpu_set_view(int x, int y, int w, int h);
uint32_t gpu_width(void);
uint32_t gpu_height(void);
uint32_t gpu_screen_width(void);
uint32_t gpu_screen_height(void);
int gpu_vx(void);
int gpu_vy(void);
uint32_t gpu_rgb(uint8_t r, uint8_t g, uint8_t b);

int gpu_claim(void);
void gpu_release(void);
void gpu_release_if_owner(int pid);
int gpu_owned(void);

void gpu_clear(uint32_t color);
void gpu_pixel(int x, int y, uint32_t color);
void gpu_rect(int x, int y, int w, int h, uint32_t color);
void gpu_rect_outline(int x, int y, int w, int h, uint32_t color);
void gpu_line(int x0, int y0, int x1, int y1, uint32_t color);
void gpu_circle(int cx, int cy, int r, uint32_t color);
void gpu_circle_fill(int cx, int cy, int r, uint32_t color);
void gpu_text(int x, int y, const char *s, uint32_t fg);
void gpu_present(void);
uint32_t *gpu_buffer(void);
uint32_t gpu_stride(void);

#endif
