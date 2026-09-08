#include <cmd.h>
#include <tmux.h>
#include <mat.h>
#include <calc.h>
#include <stdio.h>
#include <string.h>

static int fn_canvas(cmd_ctx_t *ctx, const char *args) {
    (void)args;
    int r = -1;
    if (tmux_pane_is_shell(ctx->pane_id))
        r = tmux_create_canvas_in(ctx->pane_id);
    if (r == 0) printf("canvas %d created\n", tmux_pane_canvas_id(ctx->pane_id));
    else printf("canvas failed\n");
    return 0;
}

static int fn_panes(cmd_ctx_t *ctx, const char *args) {
    (void)args;
    tmux_list_panes(ctx->pane_id);
    return 0;
}

static int fn_windows(cmd_ctx_t *ctx, const char *args) {
    (void)args;
    tmux_list_windows(ctx->pane_id);
    return 0;
}

static int fn_unsticky(cmd_ctx_t *ctx, const char *args) {
    (void)args;
    tmux_sticky_clear(ctx->pane_id);
    printf("sticky canvas cleared\n");
    return 0;
}

static int fn_grid(cmd_ctx_t *ctx, const char *args) {
    args = cmd_skip_spaces(args);
    if (!strcmp(args, "off")) {
        mat_grid(ctx->canvas_id, 0);
        printf("canvas %d grid off\n", ctx->canvas_id);
    } else if (!strcmp(args, "on")) {
        mat_grid(ctx->canvas_id, 1);
        printf("canvas %d grid on\n", ctx->canvas_id);
    } else {
        printf("usage: %d> grid on|off\n", ctx->canvas_id);
    }
    return 0;
}

static int fn_autofit(cmd_ctx_t *ctx, const char *args) {
    (void)args;
    if (!mat_has_graph(ctx->canvas_id)) {
        printf("canvas %d has no stored graph\n", ctx->canvas_id);
        return 0;
    }
    mat_view_default(ctx->canvas_id);
    printf("canvas %d refit\n", ctx->canvas_id);
    return 0;
}

static int fn_zplus(cmd_ctx_t *ctx, const char *args) {
    (void)args;
    mat_zoom(ctx->canvas_id, 2.0f);
    return 0;
}

static int fn_zminus(cmd_ctx_t *ctx, const char *args) {
    (void)args;
    mat_zoom(ctx->canvas_id, 0.5f);
    return 0;
}

static int fn_zpan(cmd_ctx_t *ctx, const char *args, int dir) {
    args = cmd_skip_spaces(args);
    float n = 0;
    int err = 0;
    if (*args) {
        n = calc_eval(args, &err);
        if (err) n = 0;
    }
    if (*args && err) {
        printf("z<<: bad number\n");
        return 0;
    }
    if (!*args) {
        float a, b;
        mat_view_get(ctx->canvas_id, &a, &b);
        n = (b - a) * 0.1f;
    }
    mat_pan(ctx->canvas_id, dir * n);
    return 0;
}

static int fn_zshl(cmd_ctx_t *ctx, const char *args) {
    return fn_zpan(ctx, args, -1);
}

static int fn_zshr(cmd_ctx_t *ctx, const char *args) {
    return fn_zpan(ctx, args, 1);
}

const cmd_entry_t cmd_g_canvas[] = {
    {"canvas", fn_canvas, "canvas", "make this pane a canvas", 0, 1, 0, 0, 0, 0},
    {"panes", fn_panes, "panes", "list panes", 0, 1, 0, 0, 0, 0},
    {"windows", fn_windows, "windows", "list windows", 0, 1, 0, 0, 0, 0},
    {"#", fn_unsticky, "#", "clear sticky canvas", 0, 1, 0, 0, 0, 0},
    {"grid", fn_grid, "grid on|off", "toggle canvas grid", 0, 1, 0, 1, 1, 0},
    {"autofit", fn_autofit, "autofit", "refit stored graph", 0, 1, 0, 1, 1, 0},
    {"z+", fn_zplus, "z+", "zoom in", 0, 1, 0, 1, 1, "zoom: no canvas\n"},
    {"z-", fn_zminus, "z-", "zoom out", 0, 1, 0, 1, 1, "zoom: no canvas\n"},
    {"z<<", fn_zshl, "z<< [N]", "pan left", 0, 1, 0, 1, 1, "zoom: no canvas\n"},
    {"z>>", fn_zshr, "z>> [N]", "pan right", 0, 1, 0, 1, 1, "zoom: no canvas\n"},
};
const int cmd_g_canvas_n = 10;
