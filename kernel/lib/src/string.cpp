// FeatherOS Kernel — string implementation
#include "string.h"

#include <cstddef>

namespace feather::lib {

size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

int strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char* a, const char* b, size_t n) {
    while (n && *a && (*a == *b)) { a++; b++; n--; }
    return n ? (unsigned char)*a - (unsigned char)*b : 0;
}

char* strcpy(char* dst, const char* src) {
    char* d = dst;
    while ((*d++ = *src++));
    return dst;
}

char* strncpy(char* dst, const char* src, size_t n) {
    char* d = dst;
    while (n-- > 0 && (*d++ = *src++));
    while (n-- > 0) *d++ = 0;
    return dst;
}

void* memset(void* ptr, int value, size_t num) {
    unsigned char* p = (unsigned char*)ptr;
    while (num--) *p++ = (unsigned char)value;
    return ptr;
}

void* memcpy(void* dst, const void* src, size_t num) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (num--) *d++ = *s++;
    return dst;
}

int memcmp(const void* a, const void* b, size_t num) {
    const unsigned char* pa = (const unsigned char*)a;
    const unsigned char* pb = (const unsigned char*)b;
    while (num--) {
        if (*pa != *pb) return *pa - *pb;
        pa++; pb++;
    }
    return 0;
}

char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    return nullptr;
}

} // namespace feather::lib

extern "C" void* memcpy(void* dst, const void* src, size_t n) {
    return feather::lib::memcpy(dst, src, n);
}

extern "C" void* memset(void* dst, int value, size_t n) {
    return feather::lib::memset(dst, value, n);
}

extern "C" void* memmove(void* dst, const void* src, size_t n) {
    unsigned char* d = static_cast<unsigned char*>(dst);
    const unsigned char* s = static_cast<const unsigned char*>(src);
    if (d == s || n == 0) return dst;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

extern "C" int memcmp(const void* a, const void* b, size_t n) {
    return feather::lib::memcmp(a, b, n);
}
