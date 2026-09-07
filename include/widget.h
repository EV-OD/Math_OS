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
    WIDGET_WINDOW,
    WIDGET_TITLEBAR,
    WIDGET_STATUSBAR,
} widget_type_t;

typedef enum {
    SPLIT_NONE = 0,
    SPLIT_VERTICAL,
    SPLIT_HORIZONTAL,
} split_dir_t;

typedef struct widget widget_t;

struct widget {
    int id;
    widget_type_t type;
    int x, y, w, h;
    int visible;
    int focused;
    int canvas_id;
    split_dir_t split;
    int ratio;
    widget_t *parent;
    widget_t *children[WIDGET_CHILDREN];
    int child_count;
    void (*draw)(widget_t *self);
    int (*on_event)(widget_t *self, event_t *ev);
    void *data;
};

void widget_system_init(void);
widget_t *widget_create(widget_type_t type, int x,int y,int w,int h);
void widget_destroy(widget_t *w);
void widget_add_child(widget_t *parent, widget_t *child);
void widget_remove_child(widget_t *parent, widget_t *child);
void widget_set_focus(widget_t *w);
widget_t *widget_focused(void);
void widget_layout(widget_t *root);
void widget_draw_tree(widget_t *root);
int widget_dispatch(event_t *ev);
void widget_set_canvas_id(widget_t *w, int cid);

widget_t *widget_find_by_canvas(int cid);
widget_t *widget_find_focused_shell(void);

#endif
