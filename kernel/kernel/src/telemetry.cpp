// FeatherOS Kernel — Cognitive Telemetry Subsystem Implementation
// Real-time telemetry for AI-native cognitive monitoring.

#include "telemetry.h"
#include "../../lib/include/printf.h"
#include "../../lib/include/string.h"
#include "../../arch/x86_64/include/cpu.h"
#include "../../task/include/task.h"

namespace feather::kernel::telemetry {
namespace {

// Telemetry buffer (single buffer for simplicity)
static telemetry_event_t* g_events = nullptr;
static uint32_t           g_capacity = 0;
static uint32_t           g_head = 0;
static uint32_t           g_tail = 0;
static uint32_t           g_dropped = 0;
static bool               g_wrapping = false;
static bool               g_enabled = false;

// Filter function (optional)
static telemetry_filter_t g_filter = nullptr;

// Circular buffer write
static bool buffer_write(const telemetry_event_t& ev) {
    if (!g_events || !g_enabled) return false;

    // Check if buffer is full
    uint32_t next_head = (g_head + 1) % g_capacity;
    if (next_head == g_tail) {
        g_dropped++;
        return false;
    }

    g_events[g_head] = ev;
    g_head = next_head;
    if (g_head == 0) g_wrapping = true;

    return true;
}

} // anonymous namespace

void telemetry_init() {
    // Allocate telemetry buffer (4096 events = 256 KiB)
    static telemetry_event_t events[4096];
    g_events = events;
    g_capacity = 4096;
    g_head = 0;
    g_tail = 0;
    g_dropped = 0;
    g_wrapping = false;
    g_enabled = true;
    g_filter = nullptr;

    lib::kputs("[telemetry] cognitive telemetry subsystem initialized\n");
}

void telemetry_shutdown() {
    g_enabled = false;
    g_events = nullptr;
    g_capacity = 0;
    g_head = 0;
    g_tail = 0;
    g_dropped = 0;
    g_wrapping = false;
}

void telemetry_record(uint64_t event_type,
                      uint64_t arg1,
                      uint64_t arg2,
                      uint64_t arg3,
                      uint64_t arg4) {
    if (!g_enabled || !g_events) return;

    // Apply filter if registered
    if (g_filter && !g_filter(event_type, arg1, arg2, arg3, arg4)) return;

    telemetry_event_t ev;
    ev.timestamp = 0;  // Would use arch::cpu::read_cycle_counter() in real impl
    ev.event_type = event_type;

    auto* t = task::task_current();
    ev.pid = t ? t->id : 0;

    ev.arg1 = arg1;
    ev.arg2 = arg2;
    ev.arg3 = arg3;
    ev.arg4 = arg4;

    buffer_write(ev);
}

uint32_t telemetry_read(telemetry_event_t* out_buf, uint32_t max_events) {
    if (!g_events || !out_buf || max_events == 0) return 0;

    uint32_t available = (g_head + g_capacity - g_tail) % g_capacity;
    uint32_t to_read = (max_events < available) ? max_events : available;

    for (uint32_t i = 0; i < to_read; i++) {
        out_buf[i] = g_events[g_tail];
        g_tail = (g_tail + 1) % g_capacity;
    }

    return to_read;
}

uint32_t telemetry_pending() {
    if (!g_events) return 0;
    return (g_head + g_capacity - g_tail) % g_capacity;
}

uint32_t telemetry_capacity() {
    return g_capacity;
}

void telemetry_register_filter(telemetry_filter_t filter) {
    g_filter = filter;
}

void telemetry_set_enabled(bool enabled) {
    g_enabled = enabled;
}

bool telemetry_is_enabled() {
    return g_enabled;
}

uint64_t telemetry_dropped_count() {
    return g_dropped;
}

} // namespace feather::kernel::telemetry
