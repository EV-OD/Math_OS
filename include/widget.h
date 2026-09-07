#ifndef WIDGET_H
#define WIDGET_H

#include <stdint.h>
#include <event.h>

#define WIDGET_MAX 32
#define WIDGET_CHILDREN 8

typedef enum {
    WIDGET_CONTAINER = 0,
    WIDGET_SHELL,
    WIDGET_CANVAS,
    WIDGET_LABEL,
    WIDGET_STATUSBAR,
} widget_type_t;

typedef enum {
    SPLIT_NONE = 0,
    SPLIT_VERTICAL,
    SPLIT_HORIZONTAL,
} split_dir_t;

typedef struct widget widget_t;

typedef struct {
    uint32_t bg;
    uint32_t fg;
    uint32_t border;
    uint32_t border_focused;
    int border_width;
    int padding;
} widget_style_t;

struct widget {
    int id;
    widget_type_t type;
    int x, y, w, h;
    int visible;
    int focused;
    int canvas_id;
    char label[32];
    widget_style_t style;
    split_dir_t split;
    int ratio;
    widget_t *parent;
    widget_t *children[WIDGET_CHILDREN];
    int child_count;
    void (*draw)(widget_t *self, uint32_t *fb, uint32_t pitch);
    int (*on_event)(widget_t *self, event_t *ev);
    void *data;
};

void widget_system_init(uint32_t *fb, uint32_t pitch, uint32_t sw, uint32_t sh);
widget_t *widget_create(widget_type_t type, int x,int y,int w,int h, const char *label);
void widget_destroy(widget_t *w);
void widget_add_child(widget_t *parent, widget_t *child);
void widget_remove_child(widget_t *parent, widget_t *child);
void widget_set_focus(widget_t *w);
widget_t *widget_focused(void);
void widget_set_bounds(widget_t *w, int x,int y,int w_,int h_);
void widget_layout(widget_t *root);
void widget_draw_tree(widget_t *root);
int widget_dispatch(event_t *ev);
void widget_invalidate(void);

widget_t *widget_find_by_canvas(int cid);
widget_t *widget_find_focused_shell(void);
int widget_canvas_count(void);

extern widget_style_t style_shell;
extern widget_style_t style_canvas;
extern widget_style_t style_container;
extern widget_style_t style_status;

#endif
