#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include <idt.h>

#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64
#define READ_ADDR KBD_DATA_PORT

typedef enum {
    KEY_NONE = 0,
    KEY_LSHIFT, KEY_RSHIFT,
    KEY_LCTRL,  KEY_RCTRL,
    KEY_LALT,   KEY_RALT,
    KEY_LGUI,   KEY_RGUI,
    KEY_CAPSLOCK, KEY_NUMLOCK, KEY_SCROLLLOCK,
    KEY_ESC, KEY_TAB, KEY_ENTER,
    KEY_BACKSPACE, KEY_SPACE,
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_HOME, KEY_END, KEY_PAGE_UP, KEY_PAGE_DOWN,
    KEY_INSERT, KEY_DELETE,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    KEY_PRINTSCREEN, KEY_PAUSE,
    KEY_APPS, KEY_POWER, KEY_SLEEP, KEY_WAKE,
    KEY_KP_0, KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_4,
    KEY_KP_5, KEY_KP_6, KEY_KP_7, KEY_KP_8, KEY_KP_9,
    KEY_KP_DOT, KEY_KP_ENTER, KEY_KP_PLUS, KEY_KP_MINUS,
    KEY_KP_STAR, KEY_KP_SLASH,
    KEY_MM_NEXT, KEY_MM_PREV, KEY_MM_STOP,
    KEY_MM_PLAY, KEY_MM_MUTE, KEY_MM_VOL_UP, KEY_MM_VOL_DOWN,
    KEY_MM_SELECT
} SpecialKey;

typedef struct {
    bool lshift, rshift;
    bool lctrl, rctrl;
    bool lalt, ralt;
    bool lgui, rgui;
    bool capslock, numlock, scrolllock;
} KeyboardState;

typedef struct {
    bool is_character;
    char ascii;
    SpecialKey key_enum;
    bool pressed;
} KeyOutput;

typedef struct {
    uint8_t code;
    bool extended;
} ScanCode;

extern KeyboardState g_kbd_state;

void init_keyboard(void);
void keyboard_handler(struct cpu_state *cpu, struct stack_state *stack, unsigned int interrupt);
int keyboard_poll_keys(KeyOutput *out, int max_keys);
bool keyboard_has_key(void);
const KeyboardState *keyboard_get_state(void);

int scancode_array_to_key(const ScanCode *in, int count, KeyOutput *out);

#endif
