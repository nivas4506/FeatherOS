#include "printf.h"
#include "string.h"
#include "../../drivers/include/vga.h"
#include "../../drivers/include/serial.h"

#include <cstdarg>

namespace feather::lib {

static char* utoa(unsigned long long val, char* buf, int base) {
    char* p = buf;
    if (val == 0) {
        buf[0] = '0';
        buf[1] = 0;
        return buf;
    }
    while (val > 0) {
        int digit = static_cast<int>(val % static_cast<unsigned>(base));
        *p++ = (digit < 10) ? static_cast<char>('0' + digit) : static_cast<char>('A' + digit - 10);
        val /= static_cast<unsigned>(base);
    }
    *p = 0;
    char* q = p - 1;
    char* start = buf;
    while (start < q) {
        char tmp = *start;
        *start++ = *q;
        *q-- = tmp;
    }
    return buf;
}

void kputc(char c) {
    drivers::vga_putc(c);
    if (c == '\n') drivers::serial_putc('\r');
    drivers::serial_putc(c);
}

void kputs(const char* s) {
    while (*s) kputc(*s++);
}

void kprintf(const char* fmt, ...) {
    char buf[32];
    va_list args;
    va_start(args, fmt);
    while (*fmt) {
        if (*fmt != '%') {
            kputc(*fmt++);
            continue;
        }
        fmt++;
        bool is_long = false;
        while (*fmt == 'l' || *fmt == 'z') {
            is_long = true;
            fmt++;
        }
        switch (*fmt) {
            case 'c':
                kputc(static_cast<char>(va_arg(args, int)));
                break;
            case 's': {
                const char* s = va_arg(args, const char*);
                kputs(s ? s : "(null)");
                break;
            }
            case 'd': {
                long long val = is_long ? va_arg(args, long long) : va_arg(args, int);
                if (val < 0) {
                    kputc('-');
                    val = -val;
                }
                kputs(utoa(static_cast<unsigned long long>(val), buf, 10));
                break;
            }
            case 'u': {
                unsigned long long val = is_long ? va_arg(args, unsigned long long)
                                                 : va_arg(args, unsigned int);
                kputs(utoa(val, buf, 10));
                break;
            }
            case 'x':
            case 'p': {
                unsigned long long val = va_arg(args, unsigned long long);
                if (*fmt == 'p') kputs("0x");
                kputs(utoa(val, buf, 16));
                break;
            }
            case '%':
                kputc('%');
                break;
            default:
                kputc('%');
                if (*fmt) kputc(*fmt);
                break;
        }
        if (*fmt) fmt++;
    }
    va_end(args);
}

} // namespace feather::lib
