#include <log.h>
#include <serial.h>
#include <string.h>

typedef __builtin_va_list kva_list;
#define kva_start(ap, last) __builtin_va_start(ap, last)
#define kva_end(ap) __builtin_va_end(ap)

void log_init(void) {
    serial_init();
}

static void kvlog(const char *tag, const char *fmt, kva_list ap) {
    char buf[1024];
    vsprintf(buf, fmt, ap);
    serial_puts(tag);
    serial_puts(buf);
    serial_putc('\n');
}

void log_debug(const char *fmt, ...) {
    kva_list ap;
    kva_start(ap, fmt);
    kvlog("[DEBUG] ", fmt, ap);
    kva_end(ap);
}

void log_info(const char *fmt, ...) {
    kva_list ap;
    kva_start(ap, fmt);
    kvlog("[INFO] ", fmt, ap);
    kva_end(ap);
}

void log_warn(const char *fmt, ...) {
    kva_list ap;
    kva_start(ap, fmt);
    kvlog("[WARN] ", fmt, ap);
    kva_end(ap);
}

void log_error(const char *fmt, ...) {
    kva_list ap;
    kva_start(ap, fmt);
    kvlog("[ERROR] ", fmt, ap);
    kva_end(ap);
}
