#pragma once
// FeatherOS Kernel — Spinlock (x86_64)

#include <cstdint>

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

// RAII lock guard
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