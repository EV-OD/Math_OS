#ifndef TMUX_H
#define TMUX_H

#include <stdint.h>
#include <multiboot.h>

void tmux_init(multiboot_info_t *mb);
void tmux_run(multiboot_info_t *mb);

int tmux_get_canvas_rect(int canvas_id, int *x, int *y, int *w, int *h);
int tmux_first_canvas_id(void);
int tmux_canvas_count(void);
void tmux_set_default_canvas(int id);
int tmux_has_canvas(int id);
int tmux_parse_canvas_prefix(const char *line, int *canvas_id, const char **rest);
void tmux_status_tick(void);

int tmux_pane_is_shell(int pane_id);
int tmux_resolve_canvas(int pane_id);
void tmux_begin_canvas(int cid);
void tmux_mark_canvas_dirty(int cid);
int tmux_try_run_file(int pane_id, const char *name);
int tmux_create_canvas_in(int pane_id);
int tmux_pane_canvas_id(int pane_id);
int tmux_edit_file(int pane_id, const char *path);
int tmux_sticky_set(int pane_id, int cid);
void tmux_sticky_clear(int pane_id);
int tmux_list_panes(int pane_id);
int tmux_list_windows(int pane_id);

#endif
