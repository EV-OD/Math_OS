#include <cmd.h>
#include <mat.h>
#include <stdio.h>

static int fn_fir(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    cmd_fir(args);
    return 0;
}

static int fn_iir(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    cmd_iir(args);
    return 0;
}

static int fn_freqz(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    (void)args;
    cmd_freqz("");
    return 0;
}

static int fn_pz(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    (void)args;
    cmd_pz("");
    return 0;
}

static int fn_filter(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    cmd_filter(args);
    return 0;
}

const cmd_entry_t cmd_g_filter[] = {
    {"fir", fn_fir, "fir avg N | fir sinc <fc> [taps]", "design FIR filter", 0, 0, 0, 0, 0, 0},
    {"iir", fn_iir, "iir lp|hp <fc>", "design IIR filter", 0, 0, 0, 0, 0, 0},
    {"freqz", fn_freqz, "freqz", "plot freq response", 0, 0, 1, 0, 1, 0},
    {"pz", fn_pz, "pz", "plot poles/zeros", 0, 0, 1, 0, 1, 0},
    {"filter", fn_filter, "filter <expr>", "apply filter", 0, 0, 1, 0, 1, 0},
};
const int cmd_g_filter_n = 5;
