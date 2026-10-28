// FeatherOS Kernel — Scheduler Hints Implementation
// Per-task scheduling hints for cognitive agent workloads.

#include "scheduler_hints.h"
#include "../../task/include/task.h"
#include "../../lib/include/printf.h"

namespace feather::kernel::sched_hints {
namespace {

// Max tasks with hints
static constexpr uint32_t MAX_HINTED_TASKS = 256;

// Hint storage (indexed by task ID modulo MAX_HINTED_TASKS)
static sched_hints g_hints[MAX_HINTED_TASKS] = {};
static bool g_hints_init = false;

// Deadline callback
static deadline_cb_t g_deadline_cb = nullptr;

// Get hints for a task (returns pointer to static storage)
static sched_hints* get_hints(uint64_t task_id) {
    if (!g_hints_init) return nullptr;
    uint32_t idx = static_cast<uint32_t>(task_id % MAX_HINTED_TASKS);
    return &g_hints[idx];
}

} // anonymous namespace

void sched_hints_init() {
    for (uint32_t i = 0; i < MAX_HINTED_TASKS; i++) {
        g_hints[i].wclass = workload_class::WORKLOAD_GENERAL;
        g_hints[i].flags = 0;
        g_hints[i].deadline_ns = 0;
        g_hints[i].period_ns = 0;
        g_hints[i].exec_time_ns = 0;
        g_hints[i].cpu_affinity = 0xFFFFFFFF;  // All CPUs
        g_hints[i].priority_boost = 0;
    }
    g_hints_init = true;

    lib::kputs("[sched_hints] scheduler hints subsystem initialized\n");
}

void sched_set_hints(uint64_t task_id, const sched_hints* hints) {
    if (!g_hints_init || !hints) return;

    sched_hints* h = get_hints(task_id);
    if (!h) return;

    *h = *hints;
}

bool sched_get_hints(uint64_t task_id, sched_hints* hints) {
    if (!g_hints_init || !hints) return false;

    sched_hints* h = get_hints(task_id);
    if (!h) return false;

    *hints = *h;
    return true;
}

uint8_t sched_get_effective_priority(uint64_t task_id) {
    auto* t = task::task_get(task_id);
    if (!t) return 0;

    sched_hints* h = get_hints(task_id);
    if (!h) return static_cast<uint8_t>(t->priority);

    uint8_t base = static_cast<uint8_t>(t->priority);

    // Apply boost
    if (h->priority_boost > 0) {
        uint8_t boosted = static_cast<uint8_t>(base + h->priority_boost);
        if (boosted > 255) boosted = 255;
        return boosted;
    }

    // Workload class adjustments
    switch (h->wclass) {
        case workload_class::WORKLOAD_REALTIME:
            return 250;  // Highest priority
        case workload_class::WORKLOAD_COGNITIVE:
            return 200;  // High priority for cognitive agents
        case workload_class::WORKLOAD_IOBOUND:
            return static_cast<uint8_t>(base > 100 ? base : 100);
        case workload_class::WORKLOAD_COMPUTE:
            return static_cast<uint8_t>(base < 150 ? base : 150);
        case workload_class::WORKLOAD_BACKGROUND:
            return 50;   // Low priority
        default:
            return base;
    }
}

bool sched_should_preempt(uint64_t current_tid, uint64_t candidate_tid) {
    if (!g_hints_init) return false;

    auto* cur = task::task_get(current_tid);
    auto* cand = task::task_get(candidate_tid);
    if (!cur || !cand) return false;

    uint8_t cur_prio = sched_get_effective_priority(current_tid);
    uint8_t cand_prio = sched_get_effective_priority(candidate_tid);

    // Preempt if candidate has higher priority
    if (cand_prio > cur_prio + 10) return true;

    // Check deadline: if candidate has a closer deadline, preempt
    sched_hints* cur_h = get_hints(current_tid);
    sched_hints* cand_h = get_hints(candidate_tid);

    if (cand_h && cand_h->deadline_ns > 0 && cand_h->wclass == workload_class::WORKLOAD_REALTIME) {
        if (cur_h && cur_h->deadline_ns == 0) return true;  // Real-time preempts non-real-time
        if (cur_h && cur_h->deadline_ns > cand_h->deadline_ns) return true;  // Earlier deadline wins
    }

    return false;
}

uint64_t sched_select_next(uint64_t current_tid) {
    if (!g_hints_init) return current_tid;

    // Find the highest priority ready task
    auto* current = task::task_get(current_tid);
    if (!current) return 0;

    // Simple priority-based selection: find highest priority ready task
    // In a full implementation, this would iterate the ready queue
    // For now, just return current task if it's still the best candidate

    sched_hints* cur_h = get_hints(current_tid);
    uint8_t cur_prio = sched_get_effective_priority(current_tid);

    // Check if any waiting task has higher priority
    // This is a simplified version - full implementation would iterate all tasks
    if (cur_h && cur_h->wclass == workload_class::WORKLOAD_BACKGROUND) {
        // Background tasks yield to everything
        return 0;  // Let scheduler pick something else
    }

    return current_tid;
}

void sched_register_deadline_cb(deadline_cb_t cb) {
    g_deadline_cb = cb;
}

void sched_check_deadlines() {
    if (!g_hints_init) return;

    // In a full implementation, iterate all tasks and check deadlines
    // For now, this is a placeholder
}

} // namespace feather::kernel::sched_hints
