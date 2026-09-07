#include <canvas_widget.h>
#include <widget.h>
#include <gpu.h>
#include <display.h>

void canvas_widget_init(void){}

widget_t *canvas_create(int x,int y,int w,int h,int canvas_id){
    widget_t *wd = widget_create(WIDGET_CANVAS, x,y,w,h);
    if(!wd) return 0;
    wd->canvas_id=canvas_id;
    gpu_set_view(x+2,y+2,w-4,h-4);
    gpu_clear(0x0B1020);
    gpu_present();
    return wd;
}
void canvas_clear(widget_t *w,uint32_t color){
    if(!w||w->type!=WIDGET_CANVAS) return;
    gpu_set_view(w->x+2,w->y+2,w->w-4,w->h-4);
    gpu_clear(color);
    gpu_present();
}
void canvas_present(widget_t *w){
    if(!w) return;
    gpu_set_view(w->x+2,w->y+2,w->w-4,w->h-4);
    gpu_present();
}
int canvas_get_rect(widget_t *w,int *x,int *y,int *wout,int *hout){
    if(!w||w->type!=WIDGET_CANVAS) return -1;
    if(x) *x=w->x+2;
    if(y) *y=w->y+2;
    if(wout) *wout=w->w-4;
    if(hout) *hout=w->h-4;
    return 0;
}
void canvas_draw_plot(widget_t *w,const char *expr,float a,float b){
    if(!w) return;
    gpu_set_view(w->x+2,w->y+2,w->w-4,w->h-4);
}
void canvas_draw_fft(widget_t *w,int n){
    if(!w) return;
    gpu_set_view(w->x+2,w->y+2,w->w-4,w->h-4);
}
