#ifndef KSTDIO_H
#define KSTDIO_H

void putchar(char c);
void puts(const char *s);
int printf(const char *fmt, ...);
int getchar(void);
int getch(void);
char *input(const char *prompt);
int input_buf(const char *prompt, char *buf, int max);
int scanf(const char *fmt, ...);

#endif
