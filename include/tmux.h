#ifndef TMUX_H
#define TMUX_H

#include <stdint.h>

void tmux_init(void);
void tmux_run(void);

int tmux_get_canvas_rect(int canvas_id, int *x, int *y, int *w, int *h);
int tmux_first_canvas_id(void);
int tmux_canvas_count(void);
void tmux_set_default_canvas(int id);
int tmux_has_canvas(int id);
int tmux_parse_canvas_prefix(const char *line, int *canvas_id, const char **rest);
void tmux_status_tick(void);

#endif
