#include <calc.h>
#include <kmath.h>
#include <string.h>

#define NVARS 32
#define NAMELEN 16
#define NFUNCS 16

static struct { char name[NAMELEN]; float val; int used; } vars[NVARS];
static int nvars = 0;
static float ans_val = 0;

static struct {
    char name[NAMELEN];
    char params[CALC_NPARAMS][NAMELEN];
    int nparams;
    char body[CALC_BODYLEN];
    int used;
} funcs[NFUNCS];
static int call_depth = 0;
static int last_def = 0;
static char last_sig[64];

int calc_last_was_def(void) { return last_def; }
const char *calc_last_sig(void) { return last_sig; }

static int is_builtin1(const char *n) {
    return !strcmp(n,"sin")||!strcmp(n,"cos")||!strcmp(n,"tan")||!strcmp(n,"sqrt")||
           !strcmp(n,"exp")||!strcmp(n,"ln")||!strcmp(n,"log")||!strcmp(n,"log10")||
           !strcmp(n,"abs")||!strcmp(n,"floor")||!strcmp(n,"ceil");
}

static int is_builtin(const char *n) {
    return is_builtin1(n)||!strcmp(n,"pow")||!strcmp(n,"min")||!strcmp(n,"max");
}

static int find_func(const char *name) {
    for (int i = 0; i < NFUNCS; i++)
        if (funcs[i].used && strcmp(funcs[i].name, name) == 0) return i;
    return -1;
}

static void skip(const char **p);
static float parse_expr(const char **p, int *err);

static float call_user(int fi, float *argv, int argc, int *err) {
    if (call_depth >= 16) { *err = CALC_DEPTH; return 0; }
    static struct { char name[NAMELEN]; float val; int used; } saved[16][NVARS];
    static int saved_n[16];
    static float saved_ans[16];
    int level = call_depth;
    call_depth++;
    memcpy(saved[level], vars, sizeof(vars));
    saved_n[level] = nvars;
    saved_ans[level] = ans_val;
    for (int i = 0; i < argc; i++) calc_set_var(funcs[fi].params[i], argv[i]);
    const char *p = funcs[fi].body;
    float v = parse_expr(&p, err);
    if (!*err) { skip(&p); if (*p) *err = CALC_SYNTAX; }
    memcpy(vars, saved[level], sizeof(vars));
    nvars = saved_n[level];
    ans_val = saved_ans[level];
    call_depth--;
    return v;
}

static void skip(const char **p) {
    while (**p == ' ' || **p == '\t') (*p)++;
}

static int is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

static float parse_expr(const char **p, int *err);

static float parse_number(const char **p, int *err) {
    skip(p);
    float v = 0;
    int got = 0;
    while (is_digit(**p)) { v = v * 10 + (**p - '0'); (*p)++; got = 1; }
    if (**p == '.') {
        (*p)++;
        float scale = 0.1f;
        while (is_digit(**p)) { v += (**p - '0') * scale; scale *= 0.1f; (*p)++; got = 1; }
    }
    if (!got) { *err = CALC_SYNTAX; return 0; }
    if (**p == 'e' || **p == 'E') {
        (*p)++;
        int neg = 0;
        if (**p == '+' || **p == '-') { neg = (**p == '-'); (*p)++; }
        int e = 0, egot = 0;
        while (is_digit(**p)) { e = e * 10 + (**p - '0'); (*p)++; egot = 1; }
        if (!egot) { *err = CALC_SYNTAX; return 0; }
        float m = neg ? 0.1f : 10.0f;
        for (int i = 0; i < e; i++) v = neg ? v * m : v * m;
    }
    return v;
}

static int parse_ident(const char **p, char *out) {
    skip(p);
    int i = 0;
    if (!is_alpha(**p)) return 0;
    while (is_alnum(**p) && i < NAMELEN - 1) { out[i++] = **p; (*p)++; }
    while (is_alnum(**p)) (*p)++;
    out[i] = 0;
    return 1;
}

static float call1(const char *name, float a, int *err) {
    if (strcmp(name, "sin") == 0) return k_sin(a);
    if (strcmp(name, "cos") == 0) return k_cos(a);
    if (strcmp(name, "tan") == 0) {
        float c = k_cos(a);
        if (k_fabs(c) < 1e-7f) { *err = CALC_DOMAIN; return 0; }
        return k_sin(a) / c;
    }
    if (strcmp(name, "sqrt") == 0) {
        if (a < 0) { *err = CALC_DOMAIN; return 0; }
        return k_sqrt(a);
    }
    if (strcmp(name, "exp") == 0) return k_exp(a);
    if (strcmp(name, "ln") == 0 || strcmp(name, "log") == 0) {
        if (a <= 0) { *err = CALC_DOMAIN; return 0; }
        return k_log(a);
    }
    if (strcmp(name, "log10") == 0) {
        if (a <= 0) { *err = CALC_DOMAIN; return 0; }
        return k_log10(a);
    }
    if (strcmp(name, "abs") == 0) return k_fabs(a);
    if (strcmp(name, "floor") == 0) return k_floor(a);
    if (strcmp(name, "ceil") == 0) return k_ceil(a);
    *err = CALC_UNKNOWN;
    return 0;
}

static float parse_primary(const char **p, int *err) {
    skip(p);
    if (**p == '(') {
        (*p)++;
        float v = parse_expr(p, err);
        if (*err) return 0;
        skip(p);
        if (**p != ')') { *err = CALC_SYNTAX; return 0; }
        (*p)++;
        return v;
    }
    if (is_digit(**p) || **p == '.') return parse_number(p, err);
    char name[NAMELEN];
    if (parse_ident(p, name)) {
        skip(p);
        if (**p == '(') {
            (*p)++;
            float argv[CALC_NPARAMS];
            int argc = 0;
            skip(p);
            if (**p != ')') {
                for (;;) {
                    float a = parse_expr(p, err);
                    if (*err) return 0;
                    if (argc < CALC_NPARAMS) argv[argc] = a;
                    argc++;
                    skip(p);
                    if (**p == ',') { (*p)++; continue; }
                    break;
                }
            }
            if (**p != ')') { *err = CALC_SYNTAX; return 0; }
            (*p)++;
            if (strcmp(name, "pow") == 0) {
                if (argc != 2) { *err = CALC_SYNTAX; return 0; }
                return k_pow(argv[0], argv[1]);
            }
            if (strcmp(name, "min") == 0) {
                if (argc != 2) { *err = CALC_SYNTAX; return 0; }
                return argv[0] < argv[1] ? argv[0] : argv[1];
            }
            if (strcmp(name, "max") == 0) {
                if (argc != 2) { *err = CALC_SYNTAX; return 0; }
                return argv[0] > argv[1] ? argv[0] : argv[1];
            }
            if (is_builtin1(name)) {
                if (argc != 1) { *err = CALC_SYNTAX; return 0; }
                return call1(name, argv[0], err);
            }
            {
                int fi = find_func(name);
                if (fi < 0) { *err = CALC_UNKNOWN; return 0; }
                if (argc != funcs[fi].nparams) { *err = CALC_SYNTAX; return 0; }
                return call_user(fi, argv, argc, err);
            }
        }
        int found = 0;
        float v = calc_get_var(name, &found);
        if (!found) { *err = CALC_UNKNOWN; return 0; }
        return v;
    }
    *err = CALC_SYNTAX;
    return 0;
}

static float parse_unary(const char **p, int *err) {
    skip(p);
    if (**p == '-') { (*p)++; return -parse_unary(p, err); }
    if (**p == '+') { (*p)++; return parse_unary(p, err); }
    return parse_primary(p, err);
}

static float parse_factor(const char **p, int *err) {
    float base = parse_unary(p, err);
    if (*err) return 0;
    skip(p);
    if (**p == '^') {
        (*p)++;
        float expo = parse_factor(p, err);
        if (*err) return 0;
        if (base == 0 && expo <= 0) { *err = CALC_DOMAIN; return 0; }
        if (base < 0) {
            float ie = k_floor(expo);
            if (ie != expo || expo > 1000000.0f || expo < -1000000.0f) {
                *err = CALC_DOMAIN;
                return 0;
            }
            float r = k_pow(-base, expo);
            if (k_fmod(ie, 2.0f) != 0.0f) r = -r;
            return r;
        }
        return k_pow(base, expo);
    }
    return base;
}

static float parse_term(const char **p, int *err) {
    float v = parse_factor(p, err);
    if (*err) return 0;
    for (;;) {
        skip(p);
        if (**p == '*') {
            (*p)++;
            float r = parse_factor(p, err);
            if (*err) return 0;
            v *= r;
        } else if (**p == '/') {
            (*p)++;
            float r = parse_factor(p, err);
            if (*err) return 0;
            if (r == 0) { *err = CALC_DIV0; return 0; }
            v /= r;
        } else if (**p == '%') {
            (*p)++;
            float r = parse_factor(p, err);
            if (*err) return 0;
            if (r == 0) { *err = CALC_DIV0; return 0; }
            v = k_fmod(v, r);
        } else {
            return v;
        }
    }
}

static float parse_expr(const char **p, int *err) {
    float v = parse_term(p, err);
    if (*err) return 0;
    for (;;) {
        skip(p);
        if (**p == '+') {
            (*p)++;
            float r = parse_term(p, err);
            if (*err) return 0;
            v += r;
        } else if (**p == '-') {
            (*p)++;
            float r = parse_term(p, err);
            if (*err) return 0;
            v -= r;
        } else {
            return v;
        }
    }
}

float calc_eval(const char *s, int *err) {
    const char *p = s;
    *err = CALC_OK;
    last_def = 0;
    skip(&p);
    const char *save = p;
    char name[NAMELEN];
    if (parse_ident(&p, name)) {
        const char *q = p;
        skip(&q);
        if (*q == '(') {
            const char *r = q + 1;
            char params[CALC_NPARAMS][NAMELEN];
            int nparams = 0, ok = 1;
            skip(&r);
            if (*r == ')') {
                r++;
            } else {
                for (;;) {
                    char pn[NAMELEN];
                    skip(&r);
                    if (!parse_ident(&r, pn) || nparams >= CALC_NPARAMS) { ok = 0; break; }
                    strcpy(params[nparams], pn);
                    nparams++;
                    skip(&r);
                    if (*r == ',') { r++; continue; }
                    break;
                }
                if (!ok || *r != ')') ok = 0;
                else r++;
            }
            skip(&r);
            if (ok && *r == '=' && *(r + 1) != '=') {
                const char *body = r + 1;
                const char *be = body;
                skip(&be);
                if (!*be) { *err = CALC_SYNTAX; return 0; }
                if (is_builtin(name) || !strcmp(name, "pi") || !strcmp(name, "e") || !strcmp(name, "ans")) {
                    *err = CALC_SYNTAX;
                    return 0;
                }
                for (int i = 0; i < nparams; i++)
                    for (int j = i + 1; j < nparams; j++)
                        if (!strcmp(params[i], params[j])) { *err = CALC_SYNTAX; return 0; }
                int fi = find_func(name);
                if (fi < 0) {
                    fi = -1;
                    for (int i = 0; i < NFUNCS; i++)
                        if (!funcs[i].used) { fi = i; break; }
                    if (fi < 0) { *err = CALC_DOMAIN; return 0; }
                }
                strncpy(funcs[fi].name, name, NAMELEN - 1);
                funcs[fi].name[NAMELEN - 1] = 0;
                funcs[fi].nparams = nparams;
                for (int i = 0; i < nparams; i++) strcpy(funcs[fi].params[i], params[i]);
                int bi = 0;
                while (body[bi] && bi < CALC_BODYLEN - 1) { funcs[fi].body[bi] = body[bi]; bi++; }
                funcs[fi].body[bi] = 0;
                funcs[fi].used = 1;
                last_def = 1;
                {
                    int k = 0;
                    while (name[k] && k < 20) { last_sig[k] = name[k]; k++; }
                    last_sig[k++] = '(';
                    for (int i = 0; i < nparams && k < 55; i++) {
                        if (i) last_sig[k++] = ',';
                        int m = 0;
                        while (params[i][m] && k < 55) last_sig[k++] = params[i][m++];
                    }
                    if (k < 60) last_sig[k++] = ')';
                    last_sig[k] = 0;
                }
                return 0;
            }
        }
        if (*q == '=' && *(q + 1) != '=') {
            p = q + 1;
            float v = parse_expr(&p, err);
            if (*err) return 0;
            skip(&p);
            if (*p) { *err = CALC_SYNTAX; return 0; }
            calc_set_var(name, v);
            ans_val = v;
            calc_set_var("ans", v);
            return v;
        }
    }
    p = save;
    float v = parse_expr(&p, err);
    if (*err) return 0;
    skip(&p);
    if (*p) { *err = CALC_SYNTAX; return 0; }
    ans_val = v;
    calc_set_var("ans", v);
    return v;
}

void calc_set_var(const char *name, float val) {
    if (strcmp(name, "pi") == 0 || strcmp(name, "e") == 0) return;
    for (int i = 0; i < nvars; i++) {
        if (strcmp(vars[i].name, name) == 0) { vars[i].val = val; return; }
    }
    if (nvars < NVARS) {
        strncpy(vars[nvars].name, name, NAMELEN - 1);
        vars[nvars].name[NAMELEN - 1] = 0;
        vars[nvars].val = val;
        vars[nvars].used = 1;
        nvars++;
    }
}

float calc_get_var(const char *name, int *found) {
    if (strcmp(name, "pi") == 0) { *found = 1; return K_PI; }
    if (strcmp(name, "e") == 0) { *found = 1; return K_E; }
    if (strcmp(name, "ans") == 0) { *found = 1; return ans_val; }
    for (int i = 0; i < nvars; i++) {
        if (vars[i].used && strcmp(vars[i].name, name) == 0) {
            *found = 1;
            return vars[i].val;
        }
    }
    *found = 0;
    return 0;
}

const char *calc_errstr(int err) {
    switch (err) {
        case CALC_SYNTAX: return "syntax error";
        case CALC_DIV0: return "division by zero";
        case CALC_UNKNOWN: return "unknown name";
        case CALC_DOMAIN: return "domain error";
        case CALC_DEPTH: return "recursion too deep";
        default: return "error";
    }
}
