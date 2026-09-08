#ifndef CMD_H
#define CMD_H

typedef struct {
    int pane_id;
    int canvas_id;
    int has_canvas;
    int strict;
} cmd_ctx_t;

typedef int (*cmd_fn_t)(cmd_ctx_t *ctx, const char *args);

typedef struct {
    const char *name;
    cmd_fn_t fn;
    const char *usage;
    const char *help;
    int needs_shell;
    int tmux_only;
    int graph_opt;
    int graph_req;
    int draws;
    const char *need_msg;
} cmd_entry_t;

const cmd_entry_t *cmd_find(const char *name, int len);
int cmd_run(cmd_ctx_t *ctx, const char *name, const char *args);
int cmd_dispatch(cmd_ctx_t *ctx, char *line);

const cmd_entry_t *cmd_table_begin(void);
int cmd_table_count(void);

const char *cmd_skip_spaces(const char *s);
int cmd_is_end(char c);
int cmd_has_equals(const char *s);
int cmd_is_call_syntax(const char *line);

#endif
