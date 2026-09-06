#include <keyboard.h>
#include <pic.h>
#include <idt.h>
#include <io.h>
#include <stdbool.h>

KeyboardState g_kbd_state = {0};

static const char k_base[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

static const char k_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' '
};

#define RAW_SIZE 256
static volatile uint8_t raw_buf[RAW_SIZE];
static volatile int raw_read = 0;
static volatile int raw_write = 0;

static bool e0_pending = false;
static uint8_t e1_step = 0;
static uint8_t prtsc_step = 0;

static void raw_push(uint8_t b) {
    int next = (raw_write + 1) % RAW_SIZE;
    if (next == raw_read) return;
    raw_buf[raw_write] = b;
    raw_write = next;
}

static bool raw_pop(uint8_t *b) {
    if (raw_read == raw_write) return false;
    *b = raw_buf[raw_read];
    raw_read = (raw_read + 1) % RAW_SIZE;
    return true;
}

static KeyOutput make_char(char c) {
    return (KeyOutput){true, c, KEY_NONE, true};
}

static KeyOutput make_special(SpecialKey k, bool pressed) {
    return (KeyOutput){false, 0, k, pressed};
}

static char apply_shift_caps(char base, char shifted) {
    bool shift = g_kbd_state.lshift || g_kbd_state.rshift;
    if (base >= 'a' && base <= 'z' && g_kbd_state.capslock)
        shift = !shift;
    return shift ? shifted : base;
}

static bool decode_extended(uint8_t make, bool pressed, KeyOutput *out) {
    SpecialKey k = KEY_NONE;
    char ch = 0;
    switch (make) {
        case 0x1C: k = KEY_KP_ENTER; ch = '\n'; break;
        case 0x1D: g_kbd_state.rctrl = pressed; k = KEY_RCTRL; break;
        case 0x35: k = KEY_KP_SLASH; ch = '/'; break;
        case 0x38: g_kbd_state.ralt = pressed; k = KEY_RALT; break;
        case 0x47: k = KEY_HOME; break;
        case 0x48: k = KEY_UP; break;
        case 0x49: k = KEY_PAGE_UP; break;
        case 0x4B: k = KEY_LEFT; break;
        case 0x4D: k = KEY_RIGHT; break;
        case 0x4F: k = KEY_END; break;
        case 0x50: k = KEY_DOWN; break;
        case 0x51: k = KEY_PAGE_DOWN; break;
        case 0x52: k = KEY_INSERT; break;
        case 0x53: k = KEY_DELETE; break;
        case 0x5B: g_kbd_state.lgui = pressed; k = KEY_LGUI; break;
        case 0x5C: g_kbd_state.rgui = pressed; k = KEY_RGUI; break;
        case 0x5D: k = KEY_APPS; break;
        case 0x5E: k = KEY_POWER; break;
        case 0x5F: k = KEY_SLEEP; break;
        case 0x63: k = KEY_WAKE; break;
        case 0x10: k = KEY_MM_PREV; break;
        case 0x19: k = KEY_MM_NEXT; break;
        case 0x22: k = KEY_MM_PLAY; break;
        case 0x24: k = KEY_MM_STOP; break;
        case 0x20: k = KEY_MM_MUTE; break;
        case 0x2E: k = KEY_MM_VOL_DOWN; break;
        case 0x30: k = KEY_MM_VOL_UP; break;
        case 0x21: k = KEY_MM_SELECT; break;
        case 0x37: k = KEY_PRINTSCREEN; break;
        default: return false;
    }
    if (ch && pressed) {
        *out = make_char(ch);
        out->key_enum = k;
    } else {
        *out = make_special(k, pressed);
    }
    return true;
}

static bool decode_single(uint8_t make, bool pressed, KeyOutput *out) {
    switch (make) {
        case 0x2A: g_kbd_state.lshift = pressed; return false;
        case 0x36: g_kbd_state.rshift = pressed; return false;
        case 0x1D: g_kbd_state.lctrl = pressed; return false;
        case 0x38: g_kbd_state.lalt = pressed; return false;
        case 0x3A:
            if (pressed) {
                g_kbd_state.capslock = !g_kbd_state.capslock;
                *out = make_special(KEY_CAPSLOCK, true);
                return true;
            }
            return false;
        case 0x45:
            if (pressed) {
                g_kbd_state.numlock = !g_kbd_state.numlock;
                *out = make_special(KEY_NUMLOCK, true);
                return true;
            }
            return false;
        case 0x46:
            if (pressed) {
                g_kbd_state.scrolllock = !g_kbd_state.scrolllock;
                *out = make_special(KEY_SCROLLLOCK, true);
                return true;
            }
            return false;
        case 0x01:
            if (pressed) { *out = make_special(KEY_ESC, true); return true; }
            return false;
        case 0x3B: case 0x3C: case 0x3D: case 0x3E: case 0x3F:
        case 0x40: case 0x41: case 0x42: case 0x43: case 0x44:
            if (pressed) {
                *out = make_special((SpecialKey)(KEY_F1 + (make - 0x3B)), true);
                return true;
            }
            return false;
        case 0x57:
            if (pressed) { *out = make_special(KEY_F11, true); return true; }
            return false;
        case 0x58:
            if (pressed) { *out = make_special(KEY_F12, true); return true; }
            return false;
        default: break;
    }
    if (!pressed) return false;
    if (make >= 128) return false;

    bool shift = g_kbd_state.lshift || g_kbd_state.rshift;
    bool num = g_kbd_state.numlock && !shift;

    switch (make) {
        case 0x47: if (!num) { *out = make_special(KEY_HOME, true); return true; } break;
        case 0x48: if (!num) { *out = make_special(KEY_UP, true); return true; } break;
        case 0x49: if (!num) { *out = make_special(KEY_PAGE_UP, true); return true; } break;
        case 0x4B: if (!num) { *out = make_special(KEY_LEFT, true); return true; } break;
        case 0x4C: if (!num) return false; break;
        case 0x4D: if (!num) { *out = make_special(KEY_RIGHT, true); return true; } break;
        case 0x4F: if (!num) { *out = make_special(KEY_END, true); return true; } break;
        case 0x50: if (!num) { *out = make_special(KEY_DOWN, true); return true; } break;
        case 0x51: if (!num) { *out = make_special(KEY_PAGE_DOWN, true); return true; } break;
        case 0x52: if (!num) { *out = make_special(KEY_INSERT, true); return true; } break;
        case 0x53: if (!num) { *out = make_special(KEY_DELETE, true); return true; } break;
        case 0x37: *out = make_special(KEY_KP_STAR, true); out->is_character = true; out->ascii = '*'; return true;
        case 0x4A: *out = make_special(KEY_KP_MINUS, true); out->is_character = true; out->ascii = '-'; return true;
        case 0x4E: *out = make_special(KEY_KP_PLUS, true); out->is_character = true; out->ascii = '+'; return true;
        default: break;
    }

    char base = k_base[make];
    if (!base) return false;
    *out = make_char(apply_shift_caps(base, k_shift[make]));
    if (make == 0x1C) out->key_enum = KEY_ENTER;
    else if (make == 0x0E) out->key_enum = KEY_BACKSPACE;
    else if (make == 0x0F) out->key_enum = KEY_TAB;
    else if (make == 0x39) out->key_enum = KEY_SPACE;
    return true;
}

int scancode_array_to_key(const ScanCode *in, int count, KeyOutput *out) {
    int n = 0;
    for (int i = 0; i < count; i++) {
        KeyOutput tmp;
        bool ok = in[i].extended
            ? decode_extended(in[i].code & 0x7F, !(in[i].code & 0x80), &tmp)
            : decode_single(in[i].code & 0x7F, !(in[i].code & 0x80), &tmp);
        if (ok) out[n++] = tmp;
    }
    return n;
}

static bool decode_byte(uint8_t b, KeyOutput *out) {
    if (b == 0xE0) { e0_pending = true; return false; }
    if (b == 0xE1) { e1_step = 1; e0_pending = false; return false; }

    if (e1_step > 0) {
        static const uint8_t seq[5] = {0x1D, 0x45, 0xE1, 0x9D, 0xC5};
        if (b == seq[e1_step - 1]) {
            e1_step++;
            if (e1_step > 5) {
                e1_step = 0;
                *out = make_special(KEY_PAUSE, true);
                return true;
            }
            return false;
        }
        e1_step = 0;
        return false;
    }

    if (e0_pending) {
        e0_pending = false;
        bool pressed = !(b & 0x80);
        uint8_t make = b & 0x7F;
        if (prtsc_step == 0 && make == 0x2A && pressed) { prtsc_step = 1; return false; }
        if (prtsc_step == 1 && make == 0x37 && pressed) {
            prtsc_step = 0;
            *out = make_special(KEY_PRINTSCREEN, true);
            return true;
        }
        if (prtsc_step == 0 && make == 0x37 && !pressed) { prtsc_step = 2; return false; }
        if (prtsc_step == 2 && make == 0x2A && !pressed) {
            prtsc_step = 0;
            *out = make_special(KEY_PRINTSCREEN, false);
            return true;
        }
        if (make == 0x2A || make == 0x36) return false;
        prtsc_step = 0;
        return decode_extended(make, pressed, out);
    }

    prtsc_step = 0;
    return decode_single(b & 0x7F, !(b & 0x80), out);
}

void handle_scan_code(uint8_t sc) {
    raw_push(sc);
}

void keyboard_handler(struct cpu_state *cpu, struct stack_state *stack, unsigned int interrupt) {
    (void)cpu; (void)stack; (void)interrupt;
    raw_push(inb(KBD_DATA_PORT));
}

int keyboard_poll_keys(KeyOutput *out, int max_keys) {
    int n = 0;
    uint8_t b;
    while (n < max_keys && raw_pop(&b)) {
        KeyOutput tmp;
        if (decode_byte(b, &tmp))
            out[n++] = tmp;
    }
    return n;
}

bool keyboard_has_key(void) {
    return raw_read != raw_write;
}

const KeyboardState *keyboard_get_state(void) {
    return &g_kbd_state;
}

void init_keyboard(void) {
    raw_read = raw_write = 0;
    e0_pending = false;
    e1_step = 0;
    prtsc_step = 0;
    g_kbd_state.numlock = true;
    register_interrupt_handler(33, keyboard_handler);
    enable_keyboard();
}
