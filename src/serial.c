#include <serial.h>
#include <io.h>

#define COM1 0x3F8
#define LSR 5
#define THRE 0x20
#define DR 0x01

void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x01);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

static int tx_empty(void) {
    return inb(COM1 + LSR) & THRE;
}

void serial_putc(char c) {
    if (c == '\n') {
        while (!tx_empty()) {}
        outb(COM1, (uint8_t)'\r');
    }
    while (!tx_empty()) {}
    outb(COM1, (uint8_t)c);
}

void serial_puts(const char *s) {
    while (*s) serial_putc(*s++);
}

int serial_has_data(void) {
    uint8_t lsr = inb(COM1 + LSR);
    if (lsr == 0xFF) return 0;
    return lsr & DR;
}

int serial_try_getc(char *out) {
    if (!serial_has_data()) return 0;
    *out = (char)inb(COM1);
    return 1;
}

char serial_getc(void) {
    char c;
    while (!serial_try_getc(&c)) {}
    return c;
}
