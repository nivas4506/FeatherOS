#pragma once
// FeatherOS Kernel — Task/Process Management


#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>
#include <cstddef>

namespace feather::task {

// Task states
enum class task_state : uint8_t {
    READY,
    RUNNING,
    BLOCKED,
    SLEEPING,
    TERMINATED,
};

// Task priority levels
enum class task_priority : uint8_t {
    IDLE = 0,
    LOW = 1,
    NORMAL = 2,
    HIGH = 3,
    REALTIME = 4,
};

// CPU context for context switching (x86_64)
struct cpu_context {
    uint64_t rip;      // Instruction pointer
    uint64_t rsp;      // Stack pointer
    uint64_t rbp;      // Base pointer
    uint64_t rbx;      // Callee-saved registers
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rflags;   // Flags register
};

// Task control block (TCB)
struct task {
    uint64_t        id;              // Unique task ID
    task_state      state;           // Current state
    task_priority   priority;        // Priority level
    cpu_context     context;         // Saved CPU context
    void*           stack;           // Kernel stack pointer
    std::size_t     stack_size;      // Stack size in bytes
    void*           entry_point;     // Entry function
    void*           arg;             // Argument to entry function
    uint64_t        ticks_remaining; // Time slice
    uint64_t        total_ticks;     // Total CPU time used
    task*           next;            // Next in queue
    task*           prev;            // Previous in queue
    // Phase 7: Scheduler hints
    uint8_t         sched_hints_valid;   // Scheduler hints are set
    uint8_t         sched_wclass;        // Workload classification
    uint32_t        sched_flags;         // Scheduler hint flags
    uint64_t        sched_deadline_ns;   // Absolute deadline
};

// Initialize task subsystem
void task_init();

// Create a new kernel task
// Returns task ID on success, 0 on failure
uint64_t task_create(void (*entry)(void*), void* arg, task_priority priority = task_priority::NORMAL, std::size_t stack_size = 8192);

// Yield CPU to next task
void task_yield();

// Exit current task
[[noreturn]] void task_exit(int code = 0);

// Get current task
task* task_current();

// Sleep current task for `ticks` timer ticks
void task_sleep(uint64_t ticks);

// Wake up a sleeping task
void task_wake(uint64_t task_id);

// Get task by ID
task* task_get(uint64_t task_id);

// Scheduler tick (called from timer interrupt)
void scheduler_tick();

// Get scheduler statistics
struct scheduler_stats {
    uint64_t total_tasks;
    uint64_t running_tasks;
    uint64_t context_switches;
    uint64_t idle_ticks;
};
scheduler_stats scheduler_get_stats();

} // namespace feather::task
