#include <stdio.h>
#include <keyboard.h>
#include <display.h>
#include <string.h>
#include <process.h>
#include <serial.h>

typedef __builtin_va_list kva_list;
#define kva_start(ap, last) __builtin_va_start(ap, last)
#define kva_arg(ap, type) __builtin_va_arg(ap, type)
#define kva_end(ap) __builtin_va_end(ap)

static void con_puts(const char *s) {
    sched_lock();
    draw_string((char *)s);
    sched_unlock();
    serial_puts(s);
}

void putchar(char c) {
    char s[2] = {c, 0};
    con_puts(s);
}

void puts(const char *s) {
    con_puts(s);
    con_puts("\n");
}

int printf(const char *fmt, ...) {
    char buf[1024];
    kva_list ap;
    kva_start(ap, fmt);
    int n = vsprintf(buf, fmt, ap);
    kva_end(ap);
    con_puts(buf);
    return n;
}

static int poll_kbd(KeyOutput *batch, int max) {
    if (max > 16) max = 16;
    KeyOutput tmp[16];
    int n = keyboard_poll_keys(tmp, max);
    int out = 0;
        for (int i = 0; i < n; i++) {
            if (tmp[i].key_enum == KEY_PAGE_UP || tmp[i].key_enum == KEY_PAGE_DOWN) {
                if (tmp[i].pressed) {
                    if (tmp[i].key_enum == KEY_PAGE_UP) display_scroll_up();
                    else display_scroll_down();
                }
                continue;
            }
            batch[out++] = tmp[i];
        }
    return out;
}

static int feed_key(char c, int ctrl, char *buf, int *idx, int max) {
    if (ctrl && (c == 'c' || c == 'C')) {
        con_puts("^C\n");
        int pid = proc_kill_latest();
        if (pid >= 0) printf("killed pid=%d\n", pid);
        else printf("(no background process)\n");
        buf[0] = 0;
        *idx = 0;
        return 1;
    }
    if (c == '\n' || c == '\r') {
        con_puts("\n");
        buf[*idx] = 0;
        return 1;
    }
    if (c == '\b' || c == '\x7f') {
        if (*idx > 0) { (*idx)--; buf[*idx] = 0; con_puts("\b"); }
        return 0;
    }
    if (c == '\t') return 0;
    if (c < 32 || c > 126) return 0;
    if (*idx < max - 1) {
        buf[(*idx)++] = c;
        char s[2] = {c, 0};
        con_puts(s);
    }
    return 0;
}

int getch(void) {
    KeyOutput batch[8];
    for (;;) {
        int n = poll_kbd(batch, 8);
        for (int i = 0; i < n; i++) {
            if (!batch[i].pressed) continue;
            if (batch[i].is_character) return batch[i].ascii;
            if (batch[i].key_enum == KEY_ENTER) return '\n';
            if (batch[i].key_enum == KEY_TAB) return '\t';
        }
        char ch;
        while (serial_try_getc(&ch)) {
            if (ch == '\r' || ch == '\n') return '\n';
            if (ch == 0x03) return 0x03;
            if (ch == 0x1B) {
                char t;
                int d = 0;
                while (d < 4 && serial_try_getc(&t)) d++;
                continue;
            }
            if (ch >= 32 && ch <= 126) return ch;
        }
        __asm__ volatile("hlt");
    }
}

int getchar(void) {
    int c = getch();
    putchar((char)c);
    return c;
}

int input_buf(const char *prompt, char *buf, int max) {
    if (prompt) con_puts(prompt);
    int idx = 0;
    KeyOutput batch[16];
    for (;;) {
        int n = poll_kbd(batch, 16);
        for (int i = 0; i < n; i++) {
            KeyOutput k = batch[i];
            if (!k.pressed) continue;
            if (k.is_character) {
                const KeyboardState *ks = keyboard_get_state();
                int ctrl = ks->lctrl || ks->rctrl;
                if (feed_key(k.ascii, ctrl, buf, &idx, max)) return idx;
            } else if (k.key_enum == KEY_ENTER) {
                con_puts("\n");
                buf[idx] = 0;
                return idx;
            } else if (k.key_enum == KEY_BACKSPACE) {
                if (idx > 0) { idx--; buf[idx] = 0; con_puts("\b"); }
            }
        }
        char ch;
        while (serial_try_getc(&ch)) {
            if (ch == 0x1B) {
                char t;
                int d = 0;
                while (d < 4 && serial_try_getc(&t)) d++;
                continue;
            }
            int ctrl = (ch == 0x03);
            char c = ctrl ? 'c' : ch;
            if (ch == '\r') c = '\n';
            if (feed_key(c, ctrl, buf, &idx, max)) return idx;
        }
        __asm__ volatile("hlt");
    }
}

char *input(const char *prompt) {
    static char buf[256];
    memset(buf, 0, sizeof(buf));
    input_buf(prompt, buf, sizeof(buf));
    return buf;
}

static const char *skip_spaces(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n') p++;
    return p;
}

static int parse_int(const char **p, int base) {
    const char *s = skip_spaces(*p);
    int sign = 1, val = 0, any = 0;
    if (*s == '-' || *s == '+') { if (*s == '-') sign = -1; s++; }
    if (base == 16) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
        while ((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F')) {
            int d = *s <= '9' ? *s - '0' : (*s <= 'F' ? *s - 'A' + 10 : *s - 'a' + 10);
            val = val * 16 + d; s++; any = 1;
        }
    } else {
        while (*s >= '0' && *s <= '9') { val = val * 10 + *s - '0'; s++; any = 1; }
    }
    if (!any) return 0;
    *p = s;
    return sign * val;
}

int scanf(const char *fmt, ...) {
    char line[256];
    input_buf(0, line, sizeof(line));
    const char *p = line;
    kva_list ap;
    kva_start(ap, fmt);
    int assigned = 0;
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 'd': {
                    const char *before = p;
                    int v = parse_int(&p, 10);
                    if (p == before) goto done;
                    *kva_arg(ap, int *) = v; assigned++;
                    break;
                }
                case 'u':
                case 'x': {
                    const char *before = p;
                    int v = parse_int(&p, *fmt == 'x' ? 16 : 10);
                    if (p == before) goto done;
                    *kva_arg(ap, int *) = v; assigned++;
                    break;
                }
                case 's': {
                    p = skip_spaces(p);
                    char *dst = kva_arg(ap, char *);
                    if (!*p) goto done;
                    while (*p && *p != ' ' && *p != '\t' && *p != '\n') *dst++ = *p++;
                    *dst = 0; assigned++;
                    break;
                }
                case 'c': {
                    char *dst = kva_arg(ap, char *);
                    if (!*p) goto done;
                    *dst = *p++; assigned++;
                    break;
                }
                case '%':
                    if (*p == '%') p++;
                    break;
                default: goto done;
            }
            fmt++;
        } else if (*fmt == ' ' || *fmt == '\t') {
            p = skip_spaces(p); fmt++;
        } else {
            if (*p == *fmt) { p++; fmt++; }
            else break;
        }
    }
done:
    kva_end(ap);
    return assigned;
}
