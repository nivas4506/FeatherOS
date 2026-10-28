#pragma once
// FeatherOS Kernel — Cognitive Telemetry Subsystem
// Real-time telemetry for AI-native cognitive monitoring:
// syscall monitoring, memory access tracking, agent behavior analysis,
// and eBPF-triggered event collection.

#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>
#include <cstddef>

namespace feather::kernel::telemetry {

// ──────────────────────────────────────────────────────────────
// Telemetry Event Types
// ──────────────────────────────────────────────────────────────

enum class telemetry_event : uint64_t {
    TEL_SYSCALL_ENTRY   = 0x0001,
    TEL_SYSCALL_EXIT    = 0x0002,
    TEL_ALLOC           = 0x0003,
    TEL_FREE            = 0x0004,
    TEL_NET_SEND        = 0x0005,
    TEL_NET_RECV        = 0x0006,
    TEL_SCHEDULE        = 0x0007,
    TEL_AGENT_SPAWN     = 0x0008,
    TEL_AGENT_EXIT      = 0x0009,
    TEL_SAND_VIOLATION  = 0x000A,
    TEL_EBPF_TRIGGER    = 0x000B,
    TEL_MEM_ACCESS      = 0x000C,
    TEL_CUSTOM          = 0xF000,
};

// Telemetry event record
struct alignas(8) telemetry_event_t {
    uint64_t timestamp;   // Kernel uptime ticks
    uint64_t event_type;  // telemetry_event value
    uint64_t pid;         // Process/task ID
    uint64_t arg1;        // Event-specific arg 1
    uint64_t arg2;        // Event-specific arg 2
    uint64_t arg3;        // Event-specific arg 3
    uint64_t arg4;        // Event-specific arg 4
};

// Telemetry buffer (circular buffer for ring buffer consumption)
struct telemetry_buffer {
    telemetry_event_t* events;   // Event array
    uint32_t            capacity; // Number of slots
    uint32_t            head;     // Write index
    uint32_t            tail;     // Read index (consumed by userspace)
    uint32_t            dropped;  // Dropped events (buffer full)
    bool                wrapping; // Has wrapped around
};

// ──────────────────────────────────────────────────────────────
// Public API
// ──────────────────────────────────────────────────────────────

// Initialize telemetry subsystem
void telemetry_init();

// Shutdown telemetry subsystem
void telemetry_shutdown();

// Record a telemetry event
void telemetry_record(uint64_t event_type,
                      uint64_t arg1,
                      uint64_t arg2,
                      uint64_t arg3,
                      uint64_t arg4);

// Read events from telemetry buffer (for userspace consumption)
// Returns number of events copied
uint32_t telemetry_read(telemetry_event_t* out_buf, uint32_t max_events);

// Get number of pending events
uint32_t telemetry_pending();

// Get telemetry buffer size
uint32_t telemetry_capacity();

// Register an eBPF-based trace filter (filter events before recording)
typedef bool (*telemetry_filter_t)(uint64_t event_type,
                                   uint64_t arg1,
                                   uint64_t arg2,
                                   uint64_t arg3,
                                   uint64_t arg4);
void telemetry_register_filter(telemetry_filter_t filter);

// Enable/disable telemetry collection
void telemetry_set_enabled(bool enabled);
bool telemetry_is_enabled();

// Get dropped event count
uint64_t telemetry_dropped_count();

} // namespace feather::kernel::telemetry
