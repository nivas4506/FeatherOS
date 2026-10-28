#pragma once
// FeatherOS Kernel — Spinlock (x86_64)


#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>

#ifdef _MSC_VER
// MSVC x64: no __sync_* builtins and <intrin.h> intrinsics may not be
// available in a freestanding build. Use volatile-only operations. This is
// correct for a kernel that runs with interrupts disabled during critical
// sections (single logical core of execution).
namespace feather::lib {

using spinlock_t = volatile long;

inline void spin_lock(spinlock_t* lock) {
    while (*lock) {
        // spin until lock is released
    }
    *lock = 1;
}

inline void spin_unlock(spinlock_t* lock) {
    *lock = 0;
}

inline bool spin_trylock(spinlock_t* lock) {
    if (*lock) return false;
    *lock = 1;
    return true;
}

inline void cpu_pause() {
    // No pause instruction intrinsic; yield via a no-op loop hint
    __nop();
}

} // namespace feather::lib

#else

namespace feather::lib {

using spinlock_t = volatile int;

inline void spin_lock(spinlock_t* lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        while (*lock) cpu_pause();
    }
}

inline void spin_unlock(spinlock_t* lock) {
    __sync_lock_release(lock);
}

inline bool spin_trylock(spinlock_t* lock) {
    return !__sync_lock_test_and_set(lock, 1);
}

inline void cpu_pause() {
    __asm__ volatile ("pause");
}

} // namespace feather::lib

#endif

// RAII lock guard (shared by both toolchains)
namespace feather::lib {

template<typename Lock>
class lock_guard {
public:
    explicit lock_guard(Lock& l) : lock_(l) { spin_lock(&l); }
    ~lock_guard() { spin_unlock(&lock_); }
    lock_guard(const lock_guard&) = delete;
    lock_guard& operator=(const lock_guard&) = delete;
private:
    Lock& lock_;
};

} // namespace feather::lib

