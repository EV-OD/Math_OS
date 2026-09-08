#include <cmd.h>
#include <tmux.h>
#include <mat.h>
#include <stdio.h>
#include <string.h>

const char *cmd_skip_spaces(const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

int cmd_is_end(char c) {
    return c == 0 || c == ' ' || c == '\t';
}

int cmd_has_equals(const char *s) {
    while (*s) {
        if (*s == '=') return 1;
        s++;
    }
    return 0;
}

static int is_alpha_us(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_alnum_us(char c) {
    return is_alpha_us(c) || (c >= '0' && c <= '9');
}

int cmd_is_call_syntax(const char *line) {
    if (!is_alpha_us(line[0]) && line[0] != '_') return 0;
    int i = 0;
    while (is_alnum_us(line[i]) || line[i] == '_') i++;
    if (line[i] != '(') return 0;
    int len = strlen(line);
    while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t')) len--;
    if (len <= 0 || line[len - 1] != ')') return 0;
    return 1;
}

extern const cmd_entry_t cmd_g_help[];
extern const int cmd_g_help_n;
extern const cmd_entry_t cmd_g_proc[];
extern const int cmd_g_proc_n;
extern const cmd_entry_t cmd_g_sys[];
extern const int cmd_g_sys_n;
extern const cmd_entry_t cmd_g_fs[];
extern const int cmd_g_fs_n;
extern const cmd_entry_t cmd_g_canvas[];
extern const int cmd_g_canvas_n;
extern const cmd_entry_t cmd_g_math[];
extern const int cmd_g_math_n;
extern const cmd_entry_t cmd_g_filter[];
extern const int cmd_g_filter_n;

#define CMD_MAX 64
static cmd_entry_t kFlat[CMD_MAX];
static int kFlatN = -1;

static void ensure_flat(void) {
    if (kFlatN >= 0) return;
    kFlatN = 0;
    const cmd_entry_t *groups[] = {
        cmd_g_help, cmd_g_proc, cmd_g_sys, cmd_g_fs,
        cmd_g_canvas, cmd_g_math, cmd_g_filter
    };
    const int counts[] = {
        cmd_g_help_n, cmd_g_proc_n, cmd_g_sys_n, cmd_g_fs_n,
        cmd_g_canvas_n, cmd_g_math_n, cmd_g_filter_n
    };
    for (int g = 0; g < 7; g++) {
        for (int i = 0; i < counts[g] && kFlatN < CMD_MAX; i++)
            kFlat[kFlatN++] = groups[g][i];
    }
}

const cmd_entry_t *cmd_table_begin(void) {
    ensure_flat();
    return kFlat;
}

int cmd_table_count(void) {
    ensure_flat();
    return kFlatN;
}

const cmd_entry_t *cmd_find(const char *name, int len) {
    ensure_flat();
    for (int i = 0; i < kFlatN; i++) {
        int n = strlen(kFlat[i].name);
        if (n == len && !memcmp(kFlat[i].name, name, len))
            return &kFlat[i];
    }
    return 0;
}

int cmd_run(cmd_ctx_t *ctx, const char *name, const char *args) {
    const cmd_entry_t *e = cmd_find(name, strlen(name));
    if (!e) return 0;
    if (e->tmux_only && ctx->pane_id < 0) return 0;
    if (e->needs_shell && ctx->pane_id >= 0 && !tmux_pane_is_shell(ctx->pane_id)) {
        printf("%s: needs a shell pane\n", name);
        return 1;
    }
    if ((e->graph_opt || e->graph_req || e->draws) && !ctx->has_canvas) {
        if (ctx->strict) {
            if (e->graph_opt || e->graph_req || e->draws) {
                printf("no canvas %d\n", ctx->canvas_id);
                return 1;
            }
        } else if (ctx->pane_id >= 0) {
            int cid = tmux_resolve_canvas(ctx->pane_id);
            if (cid && tmux_has_canvas(cid)) {
                tmux_begin_canvas(cid);
                ctx->canvas_id = cid;
                ctx->has_canvas = 1;
            } else {
                mat_set_target(0);
                if (e->graph_req) {
                    if (e->need_msg) printf(e->need_msg, ctx->canvas_id);
                    else printf("%s: no canvas\n", name);
                    return 1;
                }
            }
        } else if (e->graph_req) {
            if (e->need_msg) printf(e->need_msg, ctx->canvas_id);
            else printf("%s: no canvas\n", name);
            return 1;
        }
    }
    e->fn(ctx, args);
    if (e->draws && ctx->has_canvas)
        tmux_mark_canvas_dirty(ctx->canvas_id);
    return 1;
}

int cmd_dispatch(cmd_ctx_t *ctx, char *line) {
    line = (char *)cmd_skip_spaces(line);
    if (!*line) return 1;
    int len = 0;
    while (line[len] && line[len] != ' ' && line[len] != '\t') len++;
    const char *args = cmd_skip_spaces(line + len);
    char name[32];
    if (len >= 32) return 0;
    memcpy(name, line, len);
    name[len] = 0;
    if (cmd_run(ctx, name, args)) return 1;
    if (cmd_has_equals(line) || cmd_is_call_syntax(line)) {
        cmd_calc(line);
        return 1;
    }
    return 0;
}
