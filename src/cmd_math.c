#include <cmd.h>
#include <mat.h>
#include <stdio.h>

static int fn_calc(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    cmd_calc(args);
    return 0;
}

static int fn_plot(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    cmd_plot(args);
    return 0;
}

static int fn_fft(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    cmd_fft(args);
    return 0;
}

static int fn_freq(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    (void)args;
    cmd_freq("");
    return 0;
}

static int fn_dft(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    cmd_dft(args);
    return 0;
}

static int fn_adc(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    cmd_adc(args);
    return 0;
}

static int fn_dac(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    (void)args;
    cmd_dac("");
    return 0;
}

static int fn_afft(cmd_ctx_t *ctx, const char *args) {
    (void)ctx;
    cmd_afft(args);
    return 0;
}

const cmd_entry_t cmd_g_math[] = {
    {"calc", fn_calc, "calc <expr>", "evaluate expression", 0, 0, 0, 0, 0, 0},
    {"plot", fn_plot, "plot <expr> [a] [b]", "plot graph", 0, 0, 0, 1, 1, "plot: no canvas (make one with canvas)\n"},
    {"fft", fn_fft, "fft [expr] [n]", "fft spectrum", 0, 0, 1, 0, 1, 0},
    {"freq", fn_freq, "freq", "redraw spectrum", 0, 0, 1, 0, 1, 0},
    {"dft", fn_dft, "dft [expr] [n]", "dft spectrum", 0, 0, 1, 0, 1, 0},
    {"adc", fn_adc, "adc <expr> <bits> [n]", "quantize signal", 0, 0, 1, 0, 1, 0},
    {"dac", fn_dac, "dac", "reconstruct signal", 0, 0, 1, 0, 1, 0},
    {"afft", fn_afft, "afft [expr] [Fs] [n]", "analog spectrum Hz", 0, 0, 1, 0, 1, 0},
};
const int cmd_g_math_n = 8;
