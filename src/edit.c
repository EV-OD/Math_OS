#include <edit.h>
#include <display.h>
#include <keyboard.h>
#include <serial.h>
#include <string.h>
#include <font.h>
#include <fs.h>
#include <process.h>

#define EDIT_MAX_LINES 128
#define EDIT_MAX_COLS 128
#define EDIT_CW (FONT_W + 2)
#define EDIT_LH (FONT_H + 2)

static char lines[EDIT_MAX_LINES][EDIT_MAX_COLS];
static int nlines;
static int cur_x, cur_y, top_line;
static int ex, ey, ew, eh;
static char epath[96];
static int dirty;
static char emsg[96];

static int line_len(int r) {
    int n = 0;
    while (n < EDIT_MAX_COLS - 1 && lines[r][n]) n++;
    return n;
}

static void render(void) {
    draw_rect(ex, ey, ew, eh, 0x0B1020);
    int bar_h = 2 * EDIT_LH + 8;
    int text_h = eh - bar_h - 6;
    int vis = text_h / EDIT_LH;
    if (vis < 1) vis = 1;
    if (cur_y < top_line) top_line = cur_y;
    if (cur_y >= top_line + vis) top_line = cur_y - vis + 1;
    for (int r = 0; r < vis; r++) {
        int gr = top_line + r;
        int py = ey + 6 + r * EDIT_LH;
        if (gr >= nlines) break;
        int n = line_len(gr);
        int maxc = (ew - 12) / EDIT_CW;
        for (int c = 0; c < n && c < maxc; c++) {
            uint8_t bmp[FONT_H * FONT_BPR];
            get_font_bitmap(lines[gr][c], bmp);
            for (int i = 0; i < FONT_H; i++)
                for (int b = 0; b < FONT_BPR; b++) {
                    uint8_t bits = bmp[i * FONT_BPR + b];
                    for (int j = 0; j < 8; j++) {
                        int pxx = b * 8 + j;
                        if (pxx >= FONT_W) break;
                        draw_pixel(ex + 6 + c * EDIT_CW + pxx, py + i,
                                   (bits << j) & 0x80 ? 0xE8ECF5 : 0x0B1020);
                    }
                }
        }
    }
    int cx = ex + 6 + cur_x * EDIT_CW;
    int cy = ey + 6 + (cur_y - top_line) * EDIT_LH;
    if (cur_y >= top_line && cur_y < top_line + vis)
        draw_rect(cx, cy, 3, FONT_H, 0xE8ECF5);
    int by = ey + eh - bar_h;
    draw_rect(ex, by, ew, bar_h, 0x16213E);
    char bar1[96];
    int bi = 0;
    const char *h1 = "^O save  ^X exit  arrows move   ";
    while (*h1 && bi < 90) bar1[bi++] = *h1++;
    int pi = 0;
    while (epath[pi] && bi < 90) bar1[bi++] = epath[pi++];
    if (dirty && bi < 90) bar1[bi++] = '*';
    bar1[bi] = 0;
    display_text_at(ex + 6, by + 4, bar1);
    display_text_at(ex + 6, by + 4 + EDIT_LH, emsg);
    emsg[0] = 0;
}

static void do_save(void) {
    char out[EDIT_MAX_LINES * EDIT_MAX_COLS];
    int o = 0;
    for (int r = 0; r < nlines; r++) {
        int n = line_len(r);
        for (int c = 0; c < n && o < (int)sizeof(out) - 2; c++) out[o++] = lines[r][c];
        if (r + 1 < nlines && o < (int)sizeof(out) - 2) out[o++] = '\n';
    }
    out[o] = 0;
    if (fs_write(epath, out, o) == 0) {
        dirty = 0;
        strcpy(emsg, "saved");
    } else {
        strcpy(emsg, "save failed");
    }
}

static void insert_char(char c) {
    int n = line_len(cur_y);
    if (n >= EDIT_MAX_COLS - 1) return;
    memmove(lines[cur_y] + cur_x + 1, lines[cur_y] + cur_x, n - cur_x + 1);
    lines[cur_y][cur_x] = c;
    cur_x++;
    dirty = 1;
}

static void backspace_key(void) {
    if (cur_x > 0) {
        int n = line_len(cur_y);
        memmove(lines[cur_y] + cur_x - 1, lines[cur_y] + cur_x, n - cur_x + 1);
        cur_x--;
        dirty = 1;
    } else if (cur_y > 0) {
        int pl = line_len(cur_y - 1);
        int n = line_len(cur_y);
        int room = EDIT_MAX_COLS - 1 - pl;
        if (room < 0) room = 0;
        int mv = n < room ? n : room;
        memcpy(lines[cur_y - 1] + pl, lines[cur_y], mv);
        lines[cur_y - 1][pl + mv] = 0;
        memmove(lines[cur_y], lines[cur_y + 1], (nlines - cur_y - 1) * EDIT_MAX_COLS);
        lines[nlines - 1][0] = 0;
        nlines--;
        cur_y--;
        cur_x = pl + mv;
        if (cur_x > line_len(cur_y)) cur_x = line_len(cur_y);
        dirty = 1;
    }
}

static void delete_key(void) {
    int n = line_len(cur_y);
    if (cur_x < n) {
        memmove(lines[cur_y] + cur_x, lines[cur_y] + cur_x + 1, n - cur_x);
        dirty = 1;
    } else if (cur_y + 1 < nlines) {
        int mv = line_len(cur_y + 1);
        int room = EDIT_MAX_COLS - 1 - n;
        if (room < 0) room = 0;
        if (mv > room) mv = room;
        memcpy(lines[cur_y] + n, lines[cur_y + 1], mv);
        lines[cur_y][n + mv] = 0;
        memmove(lines[cur_y + 1], lines[cur_y + 2], (nlines - cur_y - 2) * EDIT_MAX_COLS);
        lines[nlines - 1][0] = 0;
        nlines--;
        dirty = 1;
    }
}

static void enter_key(void) {
    if (nlines >= EDIT_MAX_LINES) return;
    int n = line_len(cur_y);
    for (int r = nlines; r > cur_y + 1; r--) memcpy(lines[r], lines[r - 1], EDIT_MAX_COLS);
    int mv = n - cur_x;
    if (mv < 0) mv = 0;
    memcpy(lines[cur_y + 1], lines[cur_y] + cur_x, mv);
    lines[cur_y + 1][mv] = 0;
    lines[cur_y][cur_x] = 0;
    nlines++;
    cur_y++;
    cur_x = 0;
    dirty = 1;
}

static void move_left(void) {
    if (cur_x > 0) cur_x--;
    else if (cur_y > 0) { cur_y--; cur_x = line_len(cur_y); }
}

static void move_right(void) {
    if (cur_x < line_len(cur_y)) cur_x++;
    else if (cur_y + 1 < nlines) { cur_y++; cur_x = 0; }
}

static void move_up(void) {
    if (cur_y > 0) {
        cur_y--;
        int n = line_len(cur_y);
        if (cur_x > n) cur_x = n;
    }
}

static void move_down(void) {
    if (cur_y + 1 < nlines) {
        cur_y++;
        int n = line_len(cur_y);
        if (cur_x > n) cur_x = n;
    }
}

static int ask_save(void) {
    char q[16] = "save? (y/n)";
    for (;;) {
        draw_rect(ex, ey + eh - (2 * EDIT_LH + 8), ew, 2 * EDIT_LH + 8, 0x16213E);
        display_text_at(ex + 6, ey + eh - (2 * EDIT_LH + 8) + 4, q);
        display_present();
        KeyOutput batch[16];
        int n = 0;
        while ((n = keyboard_poll_keys(batch, 16)) == 0) {
            char ch;
            while (serial_try_getc(&ch)) {
                if (ch == 'y' || ch == 'Y' || ch == '\r' || ch == '\n') return 1;
                if (ch == 'n' || ch == 'N' || ch == 0x1B) return 0;
            }
            __asm__ volatile("hlt");
        }
        for (int i = 0; i < n; i++) {
            KeyOutput k = batch[i];
            if (!k.pressed) continue;
            if (k.is_character && (k.ascii == 'y' || k.ascii == 'Y')) return 1;
            if (k.is_character && (k.ascii == 'n' || k.ascii == 'N')) return 0;
            if (!k.is_character && k.key_enum == KEY_ESC) return 0;
        }
    }
}

void edit_file(const char *path, int x, int y, int w, int h) {
    ex = x;
    ey = y;
    ew = w;
    eh = h;
    int pi = 0;
    while (path[pi] && pi < 95) { epath[pi] = path[pi]; pi++; }
    epath[pi] = 0;
    memset(lines, 0, sizeof(lines));
    nlines = 1;
    cur_x = 0;
    cur_y = 0;
    top_line = 0;
    dirty = 0;
    emsg[0] = 0;
    char content[8192];
    if (fs_read(path, content, sizeof(content)) >= 0) {
        int r = 0, c = 0;
        for (int i = 0; content[i] && r < EDIT_MAX_LINES; i++) {
            if (content[i] == '\n') {
                lines[r][c] = 0;
                r++;
                c = 0;
            } else if (content[i] != '\r' && c < EDIT_MAX_COLS - 1) {
                lines[r][c++] = content[i];
            }
        }
        lines[r][c] = 0;
        nlines = r + 1;
    }
    int esc = 0;
    int esc2 = 0;
    int need_draw = 1;
    for (;;) {
        if (need_draw) {
            render();
            display_present();
            need_draw = 0;
        }
        KeyOutput batch[16];
        int nkeys = 0;
        while ((nkeys = keyboard_poll_keys(batch, 16)) == 0) {
            char ch;
            int got = 0;
            while (serial_try_getc(&ch)) {
                got = 1;
                if (esc == 1) {
                    esc = (ch == '[') ? 2 : 0;
                } else if (esc == 2) {
                    esc = 0;
                    if (ch == 'D') move_left();
                    else if (ch == 'C') move_right();
                    else if (ch == 'A') move_up();
                    else if (ch == 'B') move_down();
                    else if (ch == '3') esc2 = 1;
                } else if (esc2) {
                    esc2 = 0;
                    if (ch == '~') delete_key();
                } else if (ch == 0x1B) {
                    esc = 1;
                } else if (ch == 0x0F) {
                    do_save();
                } else if (ch == 0x18) {
                    if (!dirty) return;
                    if (ask_save()) do_save();
                    return;
                } else if (ch == '\r' || ch == '\n') {
                    enter_key();
                } else if (ch == 8 || ch == 0x7f) {
                    backspace_key();
                } else if (ch >= 32 && ch < 127) {
                    insert_char(ch);
                }
            }
            if (got) { need_draw = 1; break; }
            __asm__ volatile("hlt");
        }
        for (int i = 0; i < nkeys; i++) {
            KeyOutput k = batch[i];
            if (!k.pressed) continue;
            const KeyboardState *ks = keyboard_get_state();
            int ctrl = ks->lctrl || ks->rctrl;
            if (k.is_character) {
                if (ctrl && (k.ascii == 'o' || k.ascii == 'O')) { do_save(); continue; }
                if (ctrl && (k.ascii == 'x' || k.ascii == 'X')) {
                    if (!dirty) return;
                    if (ask_save()) do_save();
                    return;
                }
                if (ctrl) continue;
                if (k.ascii == '\n' || k.ascii == '\r') { enter_key(); continue; }
                if (k.ascii == '\b') { backspace_key(); continue; }
                if (k.ascii == '\t') continue;
                insert_char(k.ascii);
            } else {
                if (k.key_enum == KEY_LEFT) move_left();
                else if (k.key_enum == KEY_RIGHT) move_right();
                else if (k.key_enum == KEY_UP) move_up();
                else if (k.key_enum == KEY_DOWN) move_down();
                else if (k.key_enum == KEY_BACKSPACE) backspace_key();
                else if (k.key_enum == KEY_DELETE) delete_key();
                else if (k.key_enum == KEY_ENTER) enter_key();
            }
        }
        need_draw = 1;
    }
}
