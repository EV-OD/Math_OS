#ifndef EVENT_H
#define EVENT_H

#include <stdint.h>
#include <keyboard.h>

typedef enum {
    EVT_NONE = 0,
    EVT_KEY,
    EVT_FOCUS,
    EVT_RESIZE,
    EVT_CLOSE,
    EVT_SPLIT_V,
    EVT_SPLIT_H,
    EVT_NEW_WINDOW,
    EVT_NEXT_WINDOW,
    EVT_PREV_WINDOW,
    EVT_SELECT_WINDOW,
    EVT_CANVAS_CREATE,
} event_type_t;

typedef struct {
    event_type_t type;
    KeyOutput key;
    char serial;
    int has_key;
    int has_serial;
    int window;
    int canvas_id;
    int dir;
} event_t;

int event_from_key(event_t *out, KeyOutput *k, char ser, int has_ser);
int event_is_prefix(KeyOutput *k, char ser, int has_ser);

#endif
