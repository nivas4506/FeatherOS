#pragma once
// FeatherOS Kernel — Port I/O x86_64

#include <cstdint>

namespace feather::arch::io {

#ifdef _MSC_VER
// MSVC provides __inbyte/__outbyte/__inword/__outword/__indword/__outdword
#include <intrin.h>

inline void outb(uint16_t port, uint8_t val) {
    __outbyte(static_cast<unsigned short>(port), val);
}
inline uint8_t inb(uint16_t port) {
    return static_cast<uint8_t>(__inbyte(static_cast<unsigned short>(port)));
}
inline void outw(uint16_t port, uint16_t val) {
    __outword(static_cast<unsigned short>(port), val);
}
inline uint16_t inw(uint16_t port) {
    return static_cast<uint16_t>(__inword(static_cast<unsigned short>(port)));
}
inline void outl(uint16_t port, uint32_t val) {
    __outdword(static_cast<unsigned short>(port), val);
}
inline uint32_t inl(uint16_t port) {
    return __indword(static_cast<unsigned short>(port));
}
#else
inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
#endif

inline void io_wait() {
    outb(0x80, 0);
}

} // namespace feather::arch::io
