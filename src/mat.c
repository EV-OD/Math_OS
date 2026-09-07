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
    if (calc_last_was_def()) {
        printf("defined %s\n", calc_last_sig());
        return;
    }
    char b[32];
    fmt_float(b, v);
    printf("= %s\n", b);
}

#define PLOT_N 360
#define MAX_STORED 16

static int target_cid = 0;
static struct { int valid; float x0, x1; } views[16];
static void view_default(int cid, float *a, float *b);
static int grid_on[16] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

void mat_grid(int cid, int on) {
    if (cid > 0 && cid < 16) grid_on[cid] = on ? 1 : 0;
}
static struct { int valid; int kind; char expr[220]; float a, b; } stored[16];
static int last_n = 0;
static float last_mags[129];

void mat_set_target(int cid) {
    if (cid >= 0 && cid < MAX_STORED) target_cid = cid;
}

void mat_view_get(int cid, float *a, float *b) {
    float x0 = -10.0f, x1 = 10.0f;
    view_default(cid, &x0, &x1);
    if (a) *a = x0;
    if (b) *b = x1;
}

static void view_default(int cid, float *a, float *b) {
    if (cid > 0 && cid < MAX_STORED && views[cid].valid) {
        *a = views[cid].x0;
        *b = views[cid].x1;
    } else {
        *a = -10.0f;
        *b = 10.0f;
    }
}

static void plot_core(const char *expr, float a, float b) {
    static float ys[PLOT_N];
    float ymin = 0, ymax = 0;
    int err = 0;
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
    uint32_t W = gpu_width(), H = gpu_height();
    gpu_clear(gpu_rgb(4, 6, 16));
    if (target_cid > 0 && target_cid < 16 && grid_on[target_cid]) {
        for (uint32_t x = 0; x < W; x += 40) gpu_line((int)x, 0, (int)x, (int)H, gpu_rgb(26, 31, 58));
        for (uint32_t y = 0; y < H; y += 40) gpu_line(0, (int)y, (int)W, (int)y, gpu_rgb(26, 31, 58));
    }
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
}

static void remember_plot(const char *expr, float a, float b) {
    int cid = target_cid;
    if (cid < 1 || cid >= MAX_STORED) return;
    stored[cid].valid = 1;
    stored[cid].kind = MAT_PLOT;
    int i = 0;
    while (expr[i] && i < 219) { stored[cid].expr[i] = expr[i]; i++; }
    stored[cid].expr[i] = 0;
    stored[cid].a = a;
    stored[cid].b = b;
    views[cid].valid = 1;
    views[cid].x0 = a;
    views[cid].x1 = b;
}

static int peel_token(const char *args, int end, int *sout, int *eout) {
    int e = end;
    while (e > 0 && (args[e - 1] == ' ' || args[e - 1] == '\t')) e--;
    int s = e;
    while (s > 0 && args[s - 1] != ' ' && args[s - 1] != '\t') s--;
    if (s >= e) return 0;
    *sout = s;
    *eout = e;
    return 1;
}

void cmd_plot(const char *args) {
    while (*args == ' ' || *args == '\t') args++;
    int len = strlen(args);
    while (len > 0 && (args[len - 1] == ' ' || args[len - 1] == '\t')) len--;
    if (len <= 0) {
        printf("usage: plot <expr> [a] [b]  (e.g. plot sin(x) 0 6.28)\n");
        return;
    }
    char expr[220];
    float a = 0, b = 0;
    int has_range = 0;
    int err = 0;
    int s2, e2, s1, e1;
    if (peel_token(args, len, &s2, &e2)) {
        char tb[32];
        int lb = e2 - s2;
        if (lb > 0 && lb < 32) {
            memcpy(tb, args + s2, lb);
            tb[lb] = 0;
            float vb = calc_eval(tb, &err);
            if (!err && s2 > 0 && peel_token(args, s2, &s1, &e1)) {
                char ta[32];
                int la = e1 - s1;
                if (la > 0 && la < 32) {
                    memcpy(ta, args + s1, la);
                    ta[la] = 0;
                    float va = calc_eval(ta, &err);
                    if (!err) {
                        int le = s1;
                        while (le > 0 && (args[le - 1] == ' ' || args[le - 1] == '\t')) le--;
                        if (le > 0 && le < 220) {
                            memcpy(expr, args, le);
                            expr[le] = 0;
                            a = va;
                            b = vb;
                            has_range = 1;
                        }
                    }
                }
            }
        }
    }
    if (!has_range) {
        if (len >= 220) { printf("plot: expression too long\n"); return; }
        memcpy(expr, args, len);
        expr[len] = 0;
        view_default(target_cid, &a, &b);
    }
    if (a == b) { printf("plot: empty range\n"); return; }
    if (a > b) { float t = a; a = b; b = t; }
    if (gpu_owned()) {
        printf("plot: canvas busy (kill gfx first)\n");
        return;
    }
    remember_plot(expr, a, b);
    plot_core(expr, a, b);
    printf("plotted on canvas (stays until next graph)\n");
}

int mat_refit_plot(int cid) {
    if (cid < 1 || cid >= MAX_STORED || !stored[cid].valid || stored[cid].kind != MAT_PLOT) return -1;
    float a, b;
    view_default(cid, &a, &b);
    if (a == b) return -1;
    plot_core(stored[cid].expr, a, b);
    return 0;
}

int mat_has_graph(int cid) {
    return cid > 0 && cid < MAX_STORED && stored[cid].valid;
}

void mat_zoom(int cid, float factor) {
    if (cid < 1 || cid >= MAX_STORED || factor <= 0) return;
    float a, b;
    view_default(cid, &a, &b);
    if (a == b) return;
    float mid = (a + b) * 0.5f;
    float half = (b - a) * 0.5f / factor;
    if (half < 1e-6f) half = 1e-6f;
    views[cid].valid = 1;
    views[cid].x0 = mid - half;
    views[cid].x1 = mid + half;
    mat_refit_plot(cid);
}

void mat_pan(int cid, float dx) {
    if (cid < 1 || cid >= MAX_STORED) return;
    float a, b;
    view_default(cid, &a, &b);
    if (a == b) return;
    views[cid].valid = 1;
    views[cid].x0 = a + dx;
    views[cid].x1 = b + dx;
    mat_refit_plot(cid);
}

void mat_view_default(int cid) {
    if (cid < 1 || cid >= MAX_STORED || !stored[cid].valid) return;
    views[cid].valid = 1;
    views[cid].x0 = stored[cid].a;
    views[cid].x1 = stored[cid].b;
    mat_refit_plot(cid);
}

static void fft_bars(int n, float *mags, float mmax, const char *title) {
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
    char tt[44];
    int ti = 0;
    while (title[ti] && ti < 40) { tt[ti] = title[ti]; ti++; }
    tt[ti] = 0;
    gpu_text(12, 10, tt, gpu_rgb(255, 255, 255));
    gpu_present();
}

static int is_numlit(const char *s, int len) {
    int i = 0, digits = 0;
    if (i < len && (s[i] == '+' || s[i] == '-')) i++;
    while (i < len && s[i] >= '0' && s[i] <= '9') { i++; digits++; }
    if (i < len && s[i] == '.') {
        i++;
        while (i < len && s[i] >= '0' && s[i] <= '9') { i++; digits++; }
    }
    if (digits == 0) return 0;
    if (i < len && (s[i] == 'e' || s[i] == 'E')) {
        i++;
        if (i < len && (s[i] == '+' || s[i] == '-')) i++;
        int ed = 0;
        while (i < len && s[i] >= '0' && s[i] <= '9') { i++; ed++; }
        if (ed == 0) return 0;
    }
    return i == len;
}

static float lit_value(const char *s, int len, int *err) {
    char b[32];
    if (len <= 0 || len >= 32) { *err = 1; return 0; }
    memcpy(b, s, len);
    b[len] = 0;
    return calc_eval(b, err);
}

void cmd_fft(const char *args) {
    while (*args == ' ' || *args == '\t') args++;
    int n = 64;
    float fa = 0, fb = 2 * K_PI;
    char expr[220];
    int has_expr = 0;
    if (*args) {
        int len = strlen(args);
        while (len > 0 && (args[len - 1] == ' ' || args[len - 1] == '\t')) len--;
        int nums = 0;
        int ns[3], ne[3];
        int end = len;
        while (nums < 3) {
            int s, e;
            if (!peel_token(args, end, &s, &e)) break;
            if (!is_numlit(args + s, e - s)) break;
            ns[nums] = s;
            ne[nums] = e;
            nums++;
            end = s;
        }
        while (end > 0 && (args[end - 1] == ' ' || args[end - 1] == '\t')) end--;
        if (nums == 0) {
            if (len >= 220) { printf("fft: expression too long\n"); return; }
            memcpy(expr, args, len);
            expr[len] = 0;
            has_expr = 1;
        } else if (nums == 1 && end == 0) {
            int err = 0;
            float v = lit_value(args + ns[0], ne[0] - ns[0], &err);
            if (err) { printf("usage: fft [expr] [a] [b] [n]\n"); return; }
            n = (int)v;
        } else if ((nums == 1 || nums == 2 || nums == 3) && end > 0) {
            int err = 0;
            if (nums == 3) {
                n = (int)lit_value(args + ns[0], ne[0] - ns[0], &err);
                if (err) { printf("usage: fft [expr] [a] [b] [n]\n"); return; }
            }
            if (nums >= 2) {
                int ai = nums == 3 ? 2 : 1;
                int bi = nums == 3 ? 1 : 0;
                fa = lit_value(args + ns[ai], ne[ai] - ns[ai], &err);
                fb = lit_value(args + ns[bi], ne[bi] - ns[bi], &err);
                if (err) { printf("usage: fft [expr] [a] [b] [n]\n"); return; }
                if (fa == fb) { printf("fft: empty domain\n"); return; }
                if (fa > fb) { float t = fa; fa = fb; fb = t; }
            }
            if (nums == 1) {
                n = (int)lit_value(args + ns[0], ne[0] - ns[0], &err);
                if (err) { printf("usage: fft [expr] [a] [b] [n]\n"); return; }
            }
            if (end >= 220) { printf("fft: expression too long\n"); return; }
            memcpy(expr, args, end);
            expr[end] = 0;
            has_expr = 1;
        } else {
            printf("usage: fft [expr] [a] [b] [n]\n");
            return;
        }
    }
    if (!fft_valid_n(n)) {
        printf("usage: fft [expr] [a] [b] [n], n in 16|32|64|128|256\n");
        return;
    }
    static Complex buf[256];
    static float mags[129];
    if (has_expr) {
        int err = 0;
        for (int i = 0; i < n; i++) {
            float x = fa + (fb - fa) * (float)i / (float)n;
            calc_set_var("x", x);
            float v = calc_eval(expr, &err);
            if (err || v != v || !k_isfinite(v)) v = 0;
            buf[i].re = v;
            buf[i].im = 0;
        }
        if (err) { printf("fft: bad expression\n"); return; }
    } else {
        for (int i = 0; i < n; i++) {
            float t = (float)i / (float)n;
            float s = k_sin(2 * K_PI * 5 * t) + 0.5f * k_sin(2 * K_PI * 12 * t) + 0.3f;
            buf[i].re = s;
            buf[i].im = 0;
        }
    }
    fft_inplace(buf, n);
    last_n = n;
    float mmax = 0;
    for (int k = 0; k <= n / 2; k++) {
        mags[k] = fft_mag(buf[k]);
        last_mags[k] = mags[k];
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
    } else if (target_cid > 0) {
        fft_bars(n, mags, mmax, has_expr ? expr : "FFT spectrum");
        printf("spectrum on canvas (stays until next graph)\n");
    }
}

void cmd_freq(const char *args) {
    (void)args;
    if (last_n <= 0 || !fft_valid_n(last_n)) {
        printf("freq: run fft first\n");
        return;
    }
    if (gpu_owned()) {
        printf("freq: canvas busy\n");
        return;
    }
    if (target_cid <= 0) {
        printf("spectrum f=0..%d computed (no canvas to draw on)\n", last_n / 2);
        return;
    }
    float mmax = 0;
    for (int k = 0; k <= last_n / 2; k++)
        if (last_mags[k] > mmax) mmax = last_mags[k];
    fft_bars(last_n, last_mags, mmax, "frequency spectrum (f)");
    printf("spectrum f=0..%d on canvas\n", last_n / 2);
}
