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
static int adc_valid = 0;
static int adc_n = 0;
static int adc_bits = 0;
static float adc_min = 0, adc_max = 0;
static float adc_samp[256];
static char adc_expr[220];

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
        calc_set_var("t", x);
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
    if (target_cid > 0 && target_cid < 16 && grid_on[target_cid]) {
        for (uint32_t x = 0; x < W; x += 40) gpu_line((int)x, 0, (int)x, (int)H, gpu_rgb(26, 31, 58));
        for (uint32_t y = 0; y < H; y += 40) gpu_line(0, (int)y, (int)W, (int)y, gpu_rgb(26, 31, 58));
    }
    int x0 = 10, x1 = (int)W - 10, y0 = 52, y1 = (int)H - 16;
    gpu_rect_outline(x0, y0, x1 - x0, y1 - y0, gpu_rgb(120, 130, 160));
    gpu_line(x0, y1, x1, y1, gpu_rgb(70, 75, 100));
    int nb = n / 2 + 1;
    if (mmax <= 0) mmax = 1;
    uint32_t gc = gpu_rgb(80, 255, 140);
    int px = -1, py = -1;
    for (int k = 0; k < nb; k++) {
        int sx = x0 + (int)((float)k / (nb - 1) * (x1 - x0));
        int sy = y1 - (int)(mags[k] / mmax * (y1 - y0 - 4));
        if (px >= 0) gpu_line(px, py, sx, sy, gc);
        else gpu_pixel(sx, sy, gc);
        int ispeak = k > 0 && k < nb - 1 && mags[k] > mags[k - 1] &&
                     mags[k] > mags[k + 1] && mags[k] > 0.2f * mmax;
        if (k == 0) ispeak = mags[0] > 0.2f * mmax;
        if (ispeak) gpu_rect(sx - 2, sy - 2, 5, 5, gpu_rgb(255, 210, 60));
        px = sx;
        py = sy;
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

static int fft_parse_args(const char *args, const char *tag, int pow2,
                            int *has_expr, char *expr, int *n, float *fa, float *fb) {
    while (*args == ' ' || *args == '\t') args++;
    *n = 64;
    *fa = 0;
    *fb = 2 * K_PI;
    *has_expr = 0;
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
            if (len >= 220) { printf("%s: expression too long\n", tag); return 0; }
            memcpy(expr, args, len);
            expr[len] = 0;
            *has_expr = 1;
        } else if (nums == 1 && end == 0) {
            int err = 0;
            float v = lit_value(args + ns[0], ne[0] - ns[0], &err);
            if (err) { printf("usage: %s [expr] [a] [b] [n]\n", tag); return 0; }
            *n = (int)v;
        } else if ((nums == 1 || nums == 2 || nums == 3) && end > 0) {
            int err = 0;
            if (nums == 3) {
                *n = (int)lit_value(args + ns[0], ne[0] - ns[0], &err);
                if (err) { printf("usage: %s [expr] [a] [b] [n]\n", tag); return 0; }
            }
            if (nums >= 2) {
                int ai = nums == 3 ? 2 : 1;
                int bi = nums == 3 ? 1 : 0;
                *fa = lit_value(args + ns[ai], ne[ai] - ns[ai], &err);
                *fb = lit_value(args + ns[bi], ne[bi] - ns[bi], &err);
                if (err) { printf("usage: %s [expr] [a] [b] [n]\n", tag); return 0; }
                if (*fa == *fb) { printf("%s: empty domain\n", tag); return 0; }
                if (*fa > *fb) { float t = *fa; *fa = *fb; *fb = t; }
            }
            if (nums == 1) {
                *n = (int)lit_value(args + ns[0], ne[0] - ns[0], &err);
                if (err) { printf("usage: %s [expr] [a] [b] [n]\n", tag); return 0; }
            }
            if (end >= 220) { printf("%s: expression too long\n", tag); return 0; }
            memcpy(expr, args, end);
            expr[end] = 0;
            *has_expr = 1;
        } else {
            printf("usage: %s [expr] [a] [b] [n]\n", tag);
            return 0;
        }
    }
    if (pow2) {
        if (!fft_valid_n(*n)) {
            printf("usage: %s [expr] [a] [b] [n], n in 16|32|64|128|256\n", tag);
            return 0;
        }
    } else {
        if (!dft_valid_n(*n)) {
            printf("usage: %s [expr] [a] [b] [n], n in 8..256\n", tag);
            return 0;
        }
    }
    return 1;
}

static void fft_fill_demo(Complex *buf, int n) {
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)n;
        float s = k_sin(2 * K_PI * 5 * t) + 0.5f * k_sin(2 * K_PI * 12 * t) + 0.3f;
        buf[i].re = s;
        buf[i].im = 0;
    }
}

static int fft_fill_expr(Complex *buf, int n, const char *expr, float fa, float fb) {
    int err = 0;
    for (int i = 0; i < n; i++) {
        float x = fa + (fb - fa) * (float)i / (float)n;
        calc_set_var("x", x);
        calc_set_var("t", x);
        float v = calc_eval(expr, &err);
        if (err || v != v || !k_isfinite(v)) v = 0;
        buf[i].re = v;
        buf[i].im = 0;
    }
    return err;
}

static void fft_finish(int n, Complex *buf, const char *title, const char *tag, int direct) {
    static float mags[129];
    if (direct) dft_direct(buf, n);
    else fft_inplace(buf, n);
    last_n = n;
    float mmax = 0;
    for (int k = 0; k <= n / 2; k++) {
        mags[k] = fft_mag(buf[k]);
        last_mags[k] = mags[k];
        if (mags[k] > mmax) mmax = mags[k];
    }
    printf("%s n=%d (bins 0..%d)\n", tag, n, n / 2);
    for (int k = 0; k <= n / 2; k++) {
        int ispeak = k > 0 && k < n / 2 && mags[k] > mags[k - 1] &&
                     mags[k] > mags[k + 1] && mags[k] > 0.2f * mmax;
        if (k == 0) ispeak = mags[0] > 0.2f * mmax;
        char mb[32];
        fmt_float(mb, mags[k]);
        printf("bin %d mag %s%s\n", k, mb, ispeak ? " * peak" : "");
    }
    if (gpu_owned()) {
        printf("%s: graph skipped, canvas busy\n", tag);
    } else if (target_cid > 0) {
        fft_bars(n, mags, mmax, title);
        printf("spectrum on canvas (stays until next graph)\n");
    }
}

void cmd_fft(const char *args) {
    int n, has_expr;
    float fa, fb;
    char expr[220];
    {
        const char *q = args;
        while (*q == ' ' || *q == '\t') q++;
        if (!strncmp(q, "adc", 3)) {
            const char *r = q + 3;
            while (*r == ' ' || *r == '\t') r++;
            if (!*r) {
                if (!adc_valid) { printf("fft: run adc first\n"); return; }
                static Complex abuf[256];
                n = adc_n;
                for (int i = 0; i < n; i++) { abuf[i].re = adc_samp[i]; abuf[i].im = 0; }
                fft_finish(n, abuf, "FFT of ADC samples", "fft", 0);
                return;
            }
        }
    }
    if (!fft_parse_args(args, "fft", 1, &has_expr, expr, &n, &fa, &fb)) return;
    static Complex buf[256];
    if (has_expr) {
        if (fft_fill_expr(buf, n, expr, fa, fb)) { printf("fft: bad expression\n"); return; }
    } else {
        fft_fill_demo(buf, n);
    }
    fft_finish(n, buf, has_expr ? expr : "FFT spectrum", "fft", 0);
}

void cmd_dft(const char *args) {
    int n, has_expr;
    float fa, fb;
    char expr[220];
    if (!fft_parse_args(args, "dft", 0, &has_expr, expr, &n, &fa, &fb)) return;
    static Complex buf[256];
    if (has_expr) {
        if (fft_fill_expr(buf, n, expr, fa, fb)) { printf("dft: bad expression\n"); return; }
    } else {
        fft_fill_demo(buf, n);
    }
    fft_finish(n, buf, has_expr ? expr : "DFT spectrum", "dft", 1);
}

void cmd_adc(const char *args) {
    while (*args == ' ' || *args == '\t') args++;
    int len = strlen(args);
    while (len > 0 && (args[len - 1] == ' ' || args[len - 1] == '\t')) len--;
    int nums = 0;
    int ns[2], ne[2];
    int end = len;
    while (nums < 2) {
        int s, e;
        if (!peel_token(args, end, &s, &e)) break;
        if (!is_numlit(args + s, e - s)) break;
        ns[nums] = s;
        ne[nums] = e;
        nums++;
        end = s;
    }
    while (end > 0 && (args[end - 1] == ' ' || args[end - 1] == '\t')) end--;
    if (nums < 1 || nums > 2 || end <= 0) {
        printf("usage: adc <expr> <bits> [n]\n");
        return;
    }
    int err = 0;
    int bits = (int)lit_value(args + ns[nums - 1], ne[nums - 1] - ns[nums - 1], &err);
    int n = 64;
    if (nums == 2) {
        n = (int)lit_value(args + ns[0], ne[0] - ns[0], &err);
    }
    if (err || bits < 1 || bits > 16 || n < 8 || n > 256) {
        printf("usage: adc <expr> <bits 1..16> [n 8..256]\n");
        return;
    }
    if (end >= 220) { printf("adc: expression too long\n"); return; }
    char expr[220];
    memcpy(expr, args, end);
    expr[end] = 0;
    static float raw[256];
    float mn = 0, mx = 0;
    for (int i = 0; i < n; i++) {
        float x = 2 * K_PI * (float)i / (float)n;
        calc_set_var("x", x);
        calc_set_var("t", x);
        float v = calc_eval(expr, &err);
        if (err || v != v || !k_isfinite(v)) v = 0;
        raw[i] = v;
        if (i == 0 || v < mn) mn = v;
        if (i == 0 || v > mx) mx = v;
    }
    if (err) { printf("adc: bad expression\n"); return; }
    int levels = (1 << bits) - 1;
    for (int i = 0; i < n; i++) {
        float q = 0;
        if (mx - mn > 1e-9f) {
            long qi = (long)((raw[i] - mn) / (mx - mn) * levels + 0.5f);
            if (qi < 0) qi = 0;
            if (qi > levels) qi = levels;
            q = mn + (float)qi / (float)levels * (mx - mn);
        } else {
            q = mn;
        }
        adc_samp[i] = q;
    }
    adc_valid = 1;
    adc_n = n;
    adc_bits = bits;
    adc_min = mn;
    adc_max = mx;
    int ei = 0;
    while (expr[ei] && ei < 219) { adc_expr[ei] = expr[ei]; ei++; }
    adc_expr[ei] = 0;
    printf("adc n=%d bits=%d range ", n, bits);
    char sb[32], se[32];
    fmt_float(sb, mn);
    fmt_float(se, mx);
    printf("%s..%s\n", sb, se);
    int show = n < 16 ? n : 16;
    for (int i = 0; i < show; i++) {
        char vb[32];
        fmt_float(vb, adc_samp[i]);
        printf("s[%d]=%s\n", i, vb);
    }
    if (n > show) printf("... (%d more)\n", n - show);
    if (gpu_owned() || target_cid <= 0) return;
    uint32_t W = gpu_width(), H = gpu_height();
    gpu_clear(gpu_rgb(4, 6, 16));
    int x0 = 10, x1 = (int)W - 10, y0 = 52, y1 = (int)H - 30;
    gpu_rect_outline(x0, y0, x1 - x0, y1 - y0, gpu_rgb(120, 130, 160));
    float lo = mn, hi = mx;
    if (hi - lo < 1e-9f) { lo -= 1; hi += 1; }
    int px = x0, py = 0;
    for (int i = 0; i < n; i++) {
        int sx = x0 + (int)((float)i / (n - 1) * (x1 - x0));
        int sy = y1 - (int)((adc_samp[i] - lo) / (hi - lo) * (y1 - y0));
        if (i > 0) {
            gpu_line(px, py, sx, py, gpu_rgb(255, 210, 60));
            gpu_line(sx, py, sx, sy, gpu_rgb(255, 210, 60));
        }
        px = sx;
        py = sy;
    }
    gpu_text(12, 10, "ADC quantized steps", gpu_rgb(255, 255, 255));
    gpu_present();
    printf("steps on canvas (stays until next graph)\n");
}

void cmd_dac(const char *args) {
    while (*args == ' ' || *args == '\t') args++;
    if (*args) { printf("usage: dac\n"); return; }
    if (!adc_valid) { printf("dac: run adc first\n"); return; }
    int n = adc_n;
    float lo = adc_min, hi = adc_max;
    if (hi - lo < 1e-9f) { lo -= 1; hi += 1; }
    int err = 0;
    float se = 0;
    for (int i = 0; i < n; i++) {
        float xm = 2 * K_PI * ((float)i + 0.5f) / (float)n;
        calc_set_var("x", xm);
        calc_set_var("t", xm);
        float v = calc_eval(adc_expr, &err);
        if (err || v != v || !k_isfinite(v)) v = 0;
        float d = v - adc_samp[i];
        se += d * d;
    }
    if (err) { printf("dac: bad stored expression\n"); return; }
    char eb[32], lb[32], hb[32];
    fmt_float(eb, k_sqrt(se / n));
    fmt_float(lb, lo);
    fmt_float(hb, hi);
    printf("dac n=%d bits=%d rms=%s range %s..%s\n", n, adc_bits, eb, lb, hb);
    if (gpu_owned() || target_cid <= 0) return;
    uint32_t W = gpu_width(), H = gpu_height();
    gpu_clear(gpu_rgb(4, 6, 16));
    int x0 = 10, x1 = (int)W - 10, y0 = 52, y1 = (int)H - 30;
    gpu_rect_outline(x0, y0, x1 - x0, y1 - y0, gpu_rgb(120, 130, 160));
    int px = -1, py = -1;
    for (int i = 0; i < 360; i++) {
        float x = 2 * K_PI * (float)i / 359.0f;
        calc_set_var("x", x);
        calc_set_var("t", x);
        float v = calc_eval(adc_expr, &err);
        if (err || v != v || !k_isfinite(v)) v = 0;
        int sx = x0 + (int)((float)i / 359.0f * (x1 - x0));
        int sy = y1 - (int)((v - lo) / (hi - lo) * (y1 - y0));
        if (px >= 0) gpu_line(px, py, sx, sy, gpu_rgb(80, 255, 140));
        px = sx;
        py = sy;
    }
    px = -1;
    for (int i = 0; i < n; i++) {
        int sx = x0 + (int)((float)i / (n - 1) * (x1 - x0));
        int sy = y1 - (int)((adc_samp[i] - lo) / (hi - lo) * (y1 - y0));
        if (px >= 0) {
            gpu_line(px, py, sx, py, gpu_rgb(255, 210, 60));
            gpu_line(sx, py, sx, sy, gpu_rgb(255, 210, 60));
        }
        px = sx;
        py = sy;
    }
    gpu_text(12, 10, "DAC: original + steps", gpu_rgb(255, 255, 255));
    gpu_present();
    printf("overlay on canvas (stays until next graph)\n");
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

#define FILT_MAX_TAPS 64
static int filt_valid = 0;
static int filt_kind = 0;
static float filt_b[FILT_MAX_TAPS];
static float filt_a[FILT_MAX_TAPS];
static int filt_nb = 0, filt_na = 0;
static float filt_zre[FILT_MAX_TAPS], filt_zim[FILT_MAX_TAPS];
static int filt_nz = 0;
static float filt_pre, filt_pim;
static int filt_np = 0;
static char filt_desc[64];

static void filt_clear(void) {
    filt_valid = 0;
    filt_kind = 0;
    filt_nb = 0;
    filt_na = 0;
    filt_nz = 0;
    filt_np = 0;
    filt_desc[0] = 0;
}

static void filt_response(float *mag, float *phase, int npts) {
    for (int i = 0; i < npts; i++) {
        float w = K_PI * (float)i / (float)(npts - 1);
        float cw = k_cos(w), sw = k_sin(w);
        float pr = 1.0f, pi = 0.0f;
        float br = 0, bi = 0, ar = 0, ai = 0;
        for (int k = 0; k < filt_nb; k++) {
            br += filt_b[k] * pr;
            bi += filt_b[k] * pi;
            float nr = pr * cw + pi * sw;
            pi = pi * cw - pr * sw;
            pr = nr;
        }
        for (int k = 0; k < filt_na; k++) {
            ar += filt_a[k] * pr;
            ai += filt_a[k] * pi;
            float nr = pr * cw + pi * sw;
            pi = pi * cw - pr * sw;
            pr = nr;
        }
        float den = ar * ar + ai * ai;
        float hr, hi;
        if (den < 1e-18f) { hr = 0; hi = 0; }
        else {
            hr = (br * ar + bi * ai) / den;
            hi = (bi * ar - br * ai) / den;
        }
        mag[i] = k_sqrt(hr * hr + hi * hi);
        phase[i] = k_atan2(hi, hr);
    }
}

void cmd_fir(const char *args) {
    while (*args == ' ' || *args == '\t') args++;
    char w1[32], w2[32], w3[32];
    w1[0] = w2[0] = w3[0] = 0;
    int i = 0, nw = 0;
    char *words[3] = {w1, w2, w3};
    while (*args && nw < 3) {
        while (*args == ' ' || *args == '\t') args++;
        if (!*args) break;
        int c = 0;
        while (*args && *args != ' ' && *args != '\t' && c < 31) words[nw][c++] = *args++;
        words[nw][c] = 0;
        nw++;
        i++;
        if (i > 8) break;
    }
    (void)i;
    filt_clear();
    if (nw >= 2 && !strcmp(words[0], "avg")) {
        int N = atoi(words[1]);
        if (N < 2 || N > FILT_MAX_TAPS) { printf("usage: fir avg <N 2..64>\n"); return; }
        for (int k = 0; k < N; k++) filt_b[k] = 1.0f / N;
        filt_nb = N;
        filt_a[0] = 1.0f;
        filt_na = 1;
        for (int k = 1; k < N; k++) {
            float ang = 2 * K_PI * (float)k / (float)N;
            filt_zre[k - 1] = k_cos(ang);
            filt_zim[k - 1] = k_sin(ang);
        }
        filt_nz = N - 1;
        filt_pre = 0;
        filt_pim = 0;
        filt_np = 0;
        filt_kind = 1;
        sprintf(filt_desc, "FIR avg N=%d", N);
    } else if (nw >= 2 && !strcmp(words[0], "sinc")) {
        int err = 0;
        float fc = calc_eval(words[1], &err);
        if (err || fc <= 0 || fc >= 0.5f) { printf("usage: fir sinc <fc 0..0.5> [taps]\n"); return; }
        int M = 21;
        if (nw >= 3) {
            M = atoi(words[2]);
            if (M < 3 || M > FILT_MAX_TAPS) { printf("usage: fir sinc <fc> [taps 3..63 odd]\n"); return; }
            if ((M & 1) == 0) M++;
        }
        float sum = 0;
        for (int k = 0; k < M; k++) {
            float m = (float)k - (M - 1) * 0.5f;
            float h;
            if (m == 0) h = 2 * fc;
            else h = k_sin(2 * K_PI * fc * m) / (K_PI * m);
            float wgt = 0.54f - 0.46f * k_cos(2 * K_PI * (float)k / (float)(M - 1));
            filt_b[k] = h * wgt;
            sum += filt_b[k];
        }
        if (sum == 0) { printf("fir: bad cutoff\n"); filt_clear(); return; }
        for (int k = 0; k < M; k++) filt_b[k] /= sum;
        filt_nb = M;
        filt_a[0] = 1.0f;
        filt_na = 1;
        filt_nz = 0;
        filt_np = 0;
        filt_kind = 2;
        sprintf(filt_desc, "FIR sinc fc=");
        char fb[32];
        fmt_float(fb, fc);
        strcat(filt_desc, fb);
    } else {
        printf("usage: fir avg <N> | fir sinc <fc> [taps]\n");
        return;
    }
    filt_valid = 1;
    printf("designed %s\n", filt_desc);
    printf("b[0..%d]:", filt_nb - 1);
    int show = filt_nb < 6 ? filt_nb : 6;
    for (int k = 0; k < show; k++) {
        char vb[32];
        fmt_float(vb, filt_b[k]);
        printf(" %s", vb);
    }
    if (filt_nb > show) printf(" ...");
    printf("\nuse freqz to plot response, pz for poles/zeros\n");
}

void cmd_iir(const char *args) {
    while (*args == ' ' || *args == '\t') args++;
    char w1[32], w2[32];
    w1[0] = w2[0] = 0;
    int nw = 0;
    char *words[2] = {w1, w2};
    while (*args && nw < 2) {
        while (*args == ' ' || *args == '\t') args++;
        if (!*args) break;
        int c = 0;
        while (*args && *args != ' ' && *args != '\t' && c < 31) words[nw][c++] = *args++;
        words[nw][c] = 0;
        nw++;
    }
    filt_clear();
    if (nw < 1 || (strcmp(words[0], "lp") && strcmp(words[0], "hp"))) {
        printf("usage: iir lp|hp <fc 0..0.5>\n");
        return;
    }
    if (nw < 2) { printf("usage: iir lp|hp <fc 0..0.5>\n"); return; }
    int err = 0;
    float fc = calc_eval(words[1], &err);
    if (err || fc <= 0 || fc >= 0.5f) { printf("usage: iir lp|hp <fc 0..0.5>\n"); return; }
    float al = k_exp(-2 * K_PI * fc);
    if (!strcmp(words[0], "lp")) {
        float a0 = 1.0f - al;
        filt_b[0] = a0;
        filt_nb = 1;
        filt_a[0] = 1.0f;
        filt_a[1] = -al;
        filt_na = 2;
        filt_zre[0] = 0;
        filt_zim[0] = 0;
        filt_nz = 1;
        filt_pre = al;
        filt_pim = 0;
        filt_np = 1;
        filt_kind = 3;
        sprintf(filt_desc, "IIR lp fc=");
    } else {
        filt_b[0] = al;
        filt_b[1] = -al;
        filt_nb = 2;
        filt_a[0] = 1.0f;
        filt_a[1] = -al;
        filt_na = 2;
        filt_zre[0] = 1.0f;
        filt_zim[0] = 0;
        filt_nz = 1;
        filt_pre = al;
        filt_pim = 0;
        filt_np = 1;
        filt_kind = 4;
        sprintf(filt_desc, "IIR hp fc=");
    }
    char fb[32];
    fmt_float(fb, fc);
    strcat(filt_desc, fb);
    filt_valid = 1;
    printf("designed %s\n", filt_desc);
    char p0[32], p1[32], z0[32], z1[32];
    fmt_float(p0, filt_pre);
    fmt_float(p1, filt_pim);
    fmt_float(z0, filt_zre[0]);
    fmt_float(z1, filt_zim[0]);
    printf("pole %s+j%s zero %s+j%s\n", p0, p1, z0, z1);
    printf("use freqz to plot response, pz for poles/zeros\n");
}

void cmd_freqz(const char *args) {
    while (*args == ' ' || *args == '\t') args++;
    if (*args) { printf("usage: freqz\n"); return; }
    if (!filt_valid) { printf("freqz: design a filter first (fir/iir)\n"); return; }
    static float mag[128], phase[128];
    filt_response(mag, phase, 128);
    float mmax = 0;
    for (int i = 0; i < 128; i++) if (mag[i] > mmax) mmax = mag[i];
    if (mmax <= 0) mmax = 1;
    printf("%s\n", filt_desc);
    for (int i = 0; i < 8; i++) {
        int k = i * 127 / 7;
        char mb[32], pb[32], wb[32];
        fmt_float(mb, mag[k]);
        fmt_float(pb, phase[k]);
        fmt_float(wb, (float)k / 127.0f);
        printf("w=%s mag %s ph %s\n", wb, mb, pb);
    }
    if (gpu_owned() || target_cid <= 0) return;
    uint32_t W = gpu_width(), H = gpu_height();
    gpu_clear(gpu_rgb(4, 6, 16));
    int x0 = 10, x1 = (int)W - 10, y0 = 52, y1 = (int)H - 16;
    gpu_rect_outline(x0, y0, x1 - x0, y1 - y0, gpu_rgb(120, 130, 160));
    int px = -1, py = -1;
    for (int i = 0; i < 128; i++) {
        int sx = x0 + (int)((float)i / 127.0f * (x1 - x0));
        int sy = y1 - (int)(mag[i] / mmax * (y1 - y0 - 4));
        if (px >= 0) gpu_line(px, py, sx, sy, gpu_rgb(80, 255, 140));
        px = sx;
        py = sy;
    }
    char tt[44];
    int ti = 0;
    while (filt_desc[ti] && ti < 40) { tt[ti] = filt_desc[ti]; ti++; }
    tt[ti] = 0;
    gpu_text(12, 10, tt, gpu_rgb(255, 255, 255));
    gpu_present();
    printf("response on canvas (stays until next graph)\n");
}

void cmd_pz(const char *args) {
    while (*args == ' ' || *args == '\t') args++;
    if (*args) { printf("usage: pz\n"); return; }
    if (!filt_valid) { printf("pz: design a filter first (fir/iir)\n"); return; }
    if (filt_kind == 2) {
        printf("%s: no closed-form poles/zeros\n", filt_desc);
        printf("ROC: stable FIR (all z)\n");
    } else {
        for (int i = 0; i < filt_nz; i++) {
            char zr[32], zi[32];
            fmt_float(zr, filt_zre[i]);
            fmt_float(zi, filt_zim[i]);
            printf("zero %s+j%s\n", zr, zi);
        }
        for (int i = 0; i < filt_np; i++) {
            char pr[32], pi[32];
            fmt_float(pr, filt_pre);
            fmt_float(pi, filt_pim);
            printf("pole %s+j%s\n", pr, pi);
        }
        if (filt_kind == 1) printf("ROC: all z (FIR)\n");
        else {
            float rp = k_sqrt(filt_pre * filt_pre + filt_pim * filt_pim);
            char rb[32];
            fmt_float(rb, rp);
            printf("ROC: |z| > %s (causal)\n", rb);
        }
    }
    if (gpu_owned() || target_cid <= 0) return;
    uint32_t W = gpu_width(), H = gpu_height();
    gpu_clear(gpu_rgb(4, 6, 16));
    int cx = (int)W / 2, cy = (int)H / 2;
    int R = (W < H ? (int)W : (int)H) / 2 - 40;
    if (R < 20) R = 20;
    gpu_circle(cx, cy, R, gpu_rgb(120, 130, 160));
    gpu_line(10, cy, (int)W - 10, cy, gpu_rgb(70, 75, 100));
    gpu_line(cx, 10, cx, (int)H - 10, gpu_rgb(70, 75, 100));
    float sc = (float)R;
    for (int i = 0; i < filt_nz && filt_kind != 2; i++) {
        int zx = cx + (int)(filt_zre[i] * sc);
        int zy = cy - (int)(filt_zim[i] * sc);
        gpu_circle(zx, zy, 7, gpu_rgb(232, 236, 245));
    }
    if (filt_kind != 2) {
        for (int i = 0; i < filt_np; i++) {
            int px0 = cx + (int)(filt_pre * sc);
            int py0 = cy - (int)(filt_pim * sc);
            gpu_line(px0 - 6, py0 - 6, px0 + 6, py0 + 6, gpu_rgb(255, 80, 80));
            gpu_line(px0 - 6, py0 + 6, px0 + 6, py0 - 6, gpu_rgb(255, 80, 80));
        }
    }
    gpu_text(12, 10, "pole-zero + unit circle", gpu_rgb(255, 255, 255));
    gpu_present();
    printf("pz plot on canvas (stays until next graph)\n");
}

void cmd_filter(const char *args) {
    while (*args == ' ' || *args == '\t') args++;
    if (!*args) { printf("usage: filter <expr>\n"); return; }
    if (!filt_valid) { printf("filter: design a filter first (fir/iir)\n"); return; }
    int len = strlen(args);
    while (len > 0 && (args[len - 1] == ' ' || args[len - 1] == '\t')) len--;
    if (len <= 0 || len >= 220) { printf("filter: bad expression\n"); return; }
    char expr[220];
    memcpy(expr, args, len);
    expr[len] = 0;
    static float xs[128], ys[128];
    int n = 128;
    int err = 0;
    for (int i = 0; i < n; i++) {
        float x = 2 * K_PI * (float)i / (float)(n - 1);
        calc_set_var("x", x);
        calc_set_var("t", x);
        float v = calc_eval(expr, &err);
        if (err || v != v || !k_isfinite(v)) v = 0;
        xs[i] = v;
    }
    if (err) { printf("filter: bad expression\n"); return; }
    for (int i = 0; i < n; i++) {
        float acc = 0;
        for (int k = 0; k < filt_nb; k++) {
            int j = i - k;
            if (j >= 0) acc += filt_b[k] * xs[j];
        }
        for (int k = 1; k < filt_na; k++) {
            int j = i - k;
            if (j >= 0) acc -= filt_a[k] * ys[j];
        }
        ys[i] = acc;
    }
    float se = 0, sy = 0;
    for (int i = n / 4; i < n; i++) {
        se += ys[i] * ys[i];
        sy += xs[i] * xs[i];
    }
    char rb[32];
    fmt_float(rb, sy > 0 ? k_sqrt(se / sy) : 0);
    printf("%s: out/in rms %s\n", filt_desc, rb);
    for (int i = 0; i < 8; i++) {
        char vb[32];
        fmt_float(vb, ys[i * n / 8]);
        printf("y[%d]=%s\n", i * n / 8, vb);
    }
    if (gpu_owned() || target_cid <= 0) return;
    uint32_t W = gpu_width(), H = gpu_height();
    float lo = xs[0], hi = xs[0];
    for (int i = 0; i < n; i++) {
        if (xs[i] < lo) lo = xs[i];
        if (xs[i] > hi) hi = xs[i];
        if (ys[i] < lo) lo = ys[i];
        if (ys[i] > hi) hi = ys[i];
    }
    if (hi - lo < 1e-9f) { lo -= 1; hi += 1; }
    gpu_clear(gpu_rgb(4, 6, 16));
    int x0 = 10, x1 = (int)W - 10, y0 = 52, y1 = (int)H - 30;
    gpu_rect_outline(x0, y0, x1 - x0, y1 - y0, gpu_rgb(120, 130, 160));
    int px = -1, py = -1;
    for (int i = 0; i < n; i++) {
        int sx = x0 + (int)((float)i / (n - 1) * (x1 - x0));
        int sy = y1 - (int)((xs[i] - lo) / (hi - lo) * (y1 - y0));
        if (px >= 0) gpu_line(px, py, sx, sy, gpu_rgb(80, 255, 140));
        px = sx;
        py = sy;
    }
    px = -1;
    for (int i = 0; i < n; i++) {
        int sx = x0 + (int)((float)i / (n - 1) * (x1 - x0));
        int sy = y1 - (int)((ys[i] - lo) / (hi - lo) * (y1 - y0));
        if (px >= 0) gpu_line(px, py, sx, sy, gpu_rgb(255, 210, 60));
        px = sx;
        py = sy;
    }
    gpu_text(12, 10, "filter in green out yellow", gpu_rgb(255, 255, 255));
    gpu_present();
    printf("overlay on canvas (stays until next graph)\n");
}

void cmd_afft(const char *args) {
    while (*args == ' ' || *args == '\t') args++;
    int len = strlen(args);
    while (len > 0 && (args[len - 1] == ' ' || args[len - 1] == '\t')) len--;
    float Fs = 1000.0f;
    int n = 64;
    char expr[220];
    int has_expr = 0;
    if (len > 0) {
        int nums = 0;
        int ns[2], ne[2];
        int end = len;
        while (nums < 2) {
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
            if (len >= 220) { printf("afft: expression too long\n"); return; }
            memcpy(expr, args, len);
            expr[len] = 0;
            has_expr = 1;
        } else if (nums == 1 && end == 0) {
            int err = 0;
            float v = lit_value(args + ns[0], ne[0] - ns[0], &err);
            if (err) { printf("usage: afft [expr] [Fs] [n]\n"); return; }
            int ni = (int)v;
            if (v == (float)ni && fft_valid_n(ni)) n = ni;
            else Fs = v;
        } else if ((nums == 1 || nums == 2) && end > 0) {
            int err = 0;
            if (nums == 2) {
                n = (int)lit_value(args + ns[0], ne[0] - ns[0], &err);
                if (err) { printf("usage: afft [expr] [Fs] [n]\n"); return; }
                Fs = lit_value(args + ns[1], ne[1] - ns[1], &err);
                if (err) { printf("usage: afft [expr] [Fs] [n]\n"); return; }
            } else {
                float v = lit_value(args + ns[0], ne[0] - ns[0], &err);
                if (err) { printf("usage: afft [expr] [Fs] [n]\n"); return; }
                int ni = (int)v;
                if (v == (float)ni && fft_valid_n(ni)) n = ni;
                else Fs = v;
            }
            if (end >= 220) { printf("afft: expression too long\n"); return; }
            memcpy(expr, args, end);
            expr[end] = 0;
            has_expr = 1;
        } else {
            printf("usage: afft [expr] [Fs] [n]\n");
            return;
        }
    }
    if (Fs <= 0 || Fs > 1000000.0f) { printf("afft: bad Fs\n"); return; }
    if (!fft_valid_n(n)) {
        printf("usage: afft [expr] [Fs] [n], n in 16|32|64|128|256\n");
        return;
    }
    static Complex buf[256];
    static float mags[129];
    if (has_expr) {
        int err = 0;
        for (int i = 0; i < n; i++) {
            float t = (float)i / Fs;
            calc_set_var("t", t);
            calc_set_var("x", t);
            float v = calc_eval(expr, &err);
            if (err || v != v || !k_isfinite(v)) v = 0;
            buf[i].re = v;
            buf[i].im = 0;
        }
        if (err) { printf("afft: bad expression\n"); return; }
    } else {
        for (int i = 0; i < n; i++) {
            float t = (float)i / Fs;
            float s = k_sin(2 * K_PI * 62.5f * t) + 0.5f * k_sin(2 * K_PI * 187.5f * t);
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
    {
        char fs[32], fh[32];
        fmt_float(fs, Fs);
        fmt_float(fh, Fs * 0.5f);
        printf("afft Fs=%s n=%d (f=0..%s Hz)\n", fs, n, fh);
    }
    for (int k = 0; k <= n / 2; k++) {
        int ispeak = k > 0 && k < n / 2 && mags[k] > mags[k - 1] &&
                     mags[k] > mags[k + 1] && mags[k] > 0.2f * mmax;
        if (k == 0) ispeak = mags[0] > 0.2f * mmax;
        char mb[32], fb[32];
        fmt_float(mb, mags[k]);
        fmt_float(fb, (float)k * Fs / n);
        printf("f=%s mag %s%s\n", fb, mb, ispeak ? " * peak" : "");
    }
    if (gpu_owned()) {
        printf("afft: graph skipped, canvas busy\n");
    } else if (target_cid > 0) {
        fft_bars(n, mags, mmax, has_expr ? expr : "analog spectrum (Hz)");
        printf("spectrum on canvas (stays until next graph)\n");
    }
}
