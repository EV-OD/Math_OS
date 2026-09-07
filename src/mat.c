#include <mat.h>
#include <calc.h>
#include <fft.h>
#include <kmath.h>
#include <gpu.h>
#include <stdio.h>
#include <string.h>

static void fmt_float(char *dst, float x) {
    if (x != x) { strcpy(dst, "nan"); return; }
    int neg = 0;
    if (!k_isfinite(x)) {
        union { float f; unsigned u; } v;
        v.f = x;
        strcpy(dst, (v.u >> 31) ? "-inf" : "inf");
        return;
    }
    if (x < 0) { neg = 1; x = -x; }
    if (x > 2000000000.0f) { strcpy(dst, neg ? "-big" : "big"); return; }
    long ip = (long)x;
    long fr = (long)((x - (float)ip) * 10000.0f + 0.5f);
    if (fr >= 10000) { ip++; fr = 0; }
    sprintf(dst, "%s%d.%04d", neg ? "-" : "", (int)ip, (int)fr);
}

void cmd_calc(const char *args) {
    while (*args == ' ' || *args == '\t') args++;
    if (!*args) {
        printf("usage: calc <expr>  (e.g. calc sin(pi/2)^2+cos(pi/2)^2)\n");
        return;
    }
    int err = 0;
    float v = calc_eval(args, &err);
    if (err) {
        printf("calc error: %s\n", calc_errstr(err));
        return;
    }
    char b[32];
    fmt_float(b, v);
    printf("= %s\n", b);
}

#define PLOT_N 360

void cmd_plot(const char *args) {
    while (*args == ' ' || *args == '\t') args++;
    int len = strlen(args);
    while (len > 0 && (args[len - 1] == ' ' || args[len - 1] == '\t')) len--;
    int s2 = len;
    while (s2 > 0 && args[s2 - 1] != ' ' && args[s2 - 1] != '\t') s2--;
    int e1 = s2;
    while (e1 > 0 && (args[e1 - 1] == ' ' || args[e1 - 1] == '\t')) e1--;
    int s1 = e1;
    while (s1 > 0 && args[s1 - 1] != ' ' && args[s1 - 1] != '\t') s1--;
    if (s2 >= len || s1 >= e1) {
        printf("usage: plot <expr> <a> <b>  (e.g. plot sin(x) 0 6.28)\n");
        return;
    }
    char ta[32], tb[32], expr[220];
    int la = e1 - s1, lb = len - s2, le = s1;
    while (le > 0 && (args[le - 1] == ' ' || args[le - 1] == '\t')) le--;
    if (la <= 0 || la >= 32 || lb <= 0 || lb >= 32 || le <= 0 || le >= 220) {
        printf("usage: plot <expr> <a> <b>\n");
        return;
    }
    memcpy(ta, args + s1, la); ta[la] = 0;
    memcpy(tb, args + s2, lb); tb[lb] = 0;
    memcpy(expr, args, le); expr[le] = 0;
    int err = 0;
    float a = calc_eval(ta, &err);
    if (err) { printf("plot: bad range\n"); return; }
    float b = calc_eval(tb, &err);
    if (err) { printf("plot: bad range\n"); return; }
    if (a == b) { printf("plot: empty range\n"); return; }
    if (a > b) { float t = a; a = b; b = t; }

    static float ys[PLOT_N];
    float ymin = 0, ymax = 0;
    for (int i = 0; i < PLOT_N; i++) {
        float x = a + (b - a) * (float)i / (float)(PLOT_N - 1);
        calc_set_var("x", x);
        float v = calc_eval(expr, &err);
        if (err || v != v || !k_isfinite(v)) v = 0;
        ys[i] = v;
        if (i == 0 || v < ymin) ymin = v;
        if (i == 0 || v > ymax) ymax = v;
    }
    if (ymax - ymin < 1e-6f) { ymin -= 1; ymax += 1; }

    if (gpu_owned()) {
        printf("plot: canvas busy (kill gfx first)\n");
        return;
    }
    uint32_t W = gpu_width(), H = gpu_height();
    gpu_clear(gpu_rgb(4, 6, 16));
    int x0 = 10, x1 = (int)W - 10, y0 = 52, y1 = (int)H - 30;
    gpu_rect_outline(x0, y0, x1 - x0, y1 - y0, gpu_rgb(120, 130, 160));
    if (ymin < 0 && ymax > 0) {
        int zy = y1 - (int)((0 - ymin) / (ymax - ymin) * (y1 - y0));
        gpu_line(x0, zy, x1, zy, gpu_rgb(70, 75, 100));
    }
    if (a < 0 && b > 0) {
        int zx = x0 + (int)((0 - a) / (b - a) * (x1 - x0));
        gpu_line(zx, y0, zx, y1, gpu_rgb(70, 75, 100));
    }
    uint32_t gc = gpu_rgb(80, 255, 140);
    int px = -1, py = -1;
    for (int i = 0; i < PLOT_N; i++) {
        int sx = x0 + (int)((float)i / (PLOT_N - 1) * (x1 - x0));
        int sy = y1 - (int)((ys[i] - ymin) / (ymax - ymin) * (y1 - y0));
        if (px >= 0) gpu_line(px, py, sx, sy, gc);
        else gpu_pixel(sx, sy, gc);
        px = sx;
        py = sy;
    }
    char title[44];
    int ti = 0;
    while (expr[ti] && ti < 40) { title[ti] = expr[ti]; ti++; }
    title[ti] = 0;
    gpu_text(12, 10, title, gpu_rgb(255, 255, 255));
    char lo[32], hi[32];
    fmt_float(lo, ymin);
    fmt_float(hi, ymax);
    gpu_text(12, y1 + 6, lo, gpu_rgb(160, 170, 200));
    int hlen = strlen(hi);
    gpu_text(x1 - hlen * 18, y1 + 6, hi, gpu_rgb(160, 170, 200));
    gpu_present();
    printf("plotted on canvas (stays until next graph)\n");
}

void cmd_fft(const char *args) {
    while (*args == ' ' || *args == '\t') args++;
    int n = 64;
    if (*args) n = atoi(args);
    if (!fft_valid_n(n)) {
        printf("usage: fft [16|32|64|128|256]\n");
        return;
    }
    static Complex buf[256];
    static float mags[129];
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float s = k_sin(2 * K_PI * 5 * t) + 0.5f * k_sin(2 * K_PI * 12 * t) + 0.3f;
        buf[i].re = s;
        buf[i].im = 0;
    }
    fft_inplace(buf, n);
    float mmax = 0;
    for (int k = 0; k <= n / 2; k++) {
        mags[k] = fft_mag(buf[k]);
        if (mags[k] > mmax) mmax = mags[k];
    }
    printf("fft n=%d (bins 0..%d)\n", n, n / 2);
    for (int k = 0; k <= n / 2; k++) {
        int ispeak = k > 0 && k < n / 2 && mags[k] > mags[k - 1] &&
                     mags[k] > mags[k + 1] && mags[k] > 0.2f * mmax;
        if (k == 0) ispeak = mags[0] > 0.2f * mmax;
        char mb[32];
        fmt_float(mb, mags[k]);
        printf("bin %d mag %s%s\n", k, mb, ispeak ? " * peak" : "");
    }
    if (gpu_owned()) {
        printf("fft: graph skipped, canvas busy\n");
    } else {
        uint32_t W = gpu_width(), H = gpu_height();
        gpu_clear(gpu_rgb(4, 6, 16));
        int x0 = 10, x1 = (int)W - 10, y0 = 52, y1 = (int)H - 16;
        gpu_rect_outline(x0, y0, x1 - x0, y1 - y0, gpu_rgb(120, 130, 160));
        int nb = n / 2 + 1;
        int bw = (x1 - x0) / nb;
        if (bw < 1) bw = 1;
        for (int k = 0; k < nb; k++) {
            int bh = mmax > 0 ? (int)(mags[k] / mmax * (y1 - y0 - 4)) : 0;
            gpu_rect(x0 + k * bw, y1 - bh, bw > 1 ? bw - 1 : 1, bh, gpu_rgb(255, 210, 60));
        }
        gpu_text(12, 10, "FFT spectrum (peaks at bins 5, 12)", gpu_rgb(255, 255, 255));
        gpu_present();
        printf("spectrum on canvas (stays until next graph)\n");
    }
}

#define MAX_STORED 16
static struct { int valid; int kind; char args[220]; } stored[16];

void mat_remember(int cid, int kind, const char *args){
    if(cid < 1 || cid >= MAX_STORED || !args) return;
    stored[cid].valid = 1;
    stored[cid].kind = kind;
    int i = 0;
    while(args[i] && i < 219){ stored[cid].args[i] = args[i]; i++; }
    stored[cid].args[i] = 0;
}

int mat_recall(int cid, int *kind, char *out){
    if(cid < 1 || cid >= MAX_STORED || !stored[cid].valid) return 0;
    if(kind) *kind = stored[cid].kind;
    if(out) strcpy(out, stored[cid].args);
    return 1;
}
