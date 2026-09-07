#ifndef CANVAS_WIDGET_H
#define CANVAS_WIDGET_H

#include <widget.h>
#include <stdint.h>

void canvas_widget_init(void);
widget_t *canvas_create(int x,int y,int w,int h, int canvas_id);
void canvas_clear(widget_t *w, uint32_t color);
void canvas_present(widget_t *w);
int canvas_get_rect(widget_t *w, int *x,int *y,int *wout,int *hout);
void canvas_draw_plot(widget_t *w, const char *expr, float a,float b);
void canvas_draw_fft(widget_t *w, int n);

#endif
