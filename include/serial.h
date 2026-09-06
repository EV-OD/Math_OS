#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);
int serial_has_data(void);
int serial_try_getc(char *out);
char serial_getc(void);

#endif
