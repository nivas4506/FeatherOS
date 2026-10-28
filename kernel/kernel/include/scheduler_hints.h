#pragma once
// FeatherOS Kernel — Scheduler Hints for AI-native workloads
// Per-task scheduling hints for cognitive agent workloads:
// real-time deadlines, workload classification, priority boosting.

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>
#include <cstddef>

namespace feather::kernel::sched_hints {

// ──────────────────────────────────────────────────────────────
// Scheduler Hint Types
// ──────────────────────────────────────────────────────────────

// Workload classification
enum class workload_class : uint8_t {
    WORKLOAD_GENERAL    = 0,  // General purpose task
    WORKLOAD_COGNITIVE  = 1,  // AI cognitive agent (high priority)
    WORKLOAD_REALTIME   = 2,  // Hard real-time task
    WORKLOAD_IOBOUND    = 3,  // I/O bound task
    WORKLOAD_COMPUTE    = 4,  // CPU bound task
    WORKLOAD_BACKGROUND = 5,  // Background task (low priority)
};

// Scheduler hint flags
enum class hint_flags : uint32_t {
    HINT_NONE           = 0x00000000,
    HINT_REALTIME       = 0x00000001,  // Real-time deadline enforcement
    HINT_NO_PREEMPT     = 0x00000002,  // Non-preemptible section
    HINT_CPU_AFFINITY   = 0x00000004,  // CPU affinity hint
    HINT_CACHE_AWARE    = 0x00000008,  // Cache-aware scheduling
    HINT_POWER_AWARE    = 0x00000010,  // Power-aware scheduling
    HINT_COGNITIVE      = 0x00000020,  // Cognitive agent task
};

// Per-task scheduler hints
struct sched_hints {
    workload_class  wclass;        // Workload classification
    uint32_t        flags;         // Hint flags (hint_flags)
    uint64_t        deadline_ns;   // Absolute deadline (nanoseconds)
    uint64_t        period_ns;     // Period for periodic tasks
    uint64_t        exec_time_ns;  // Estimated execution time
    uint32_t        cpu_affinity;  // CPU affinity mask (bitmask)
    uint8_t         priority_boost;// Priority boost (0-255)
    uint8_t         reserved[3];   // Padding
};

// Initialize scheduler hints subsystem
void sched_hints_init();

// Set scheduling hints for a task
void sched_set_hints(uint64_t task_id, const sched_hints* hints);

// Get scheduling hints for a task
bool sched_get_hints(uint64_t task_id, sched_hints* hints);

// Get effective priority for a task (considering hints)
uint8_t sched_get_effective_priority(uint64_t task_id);

// Check if a task should be preempted (based on hints and deadline)
bool sched_should_preempt(uint64_t current_tid, uint64_t candidate_tid);

// Get next task to schedule based on hints
uint64_t sched_select_next(uint64_t current_tid);

// Register a deadline miss callback
typedef void (*deadline_cb_t)(uint64_t task_id);
void sched_register_deadline_cb(deadline_cb_t cb);

// Check for deadline violations (called from scheduler tick)
void sched_check_deadlines();

} // namespace feather::kernel::sched_hints
