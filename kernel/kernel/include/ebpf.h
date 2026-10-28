// FeatherOS Kernel — eBPF-like bytecode interpreter for cognitive monitoring.
// Safe, in-kernel bytecode execution for eBPF programs loaded from userspace.

#pragma once
#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <cstdint>
#include <cstddef>

namespace feather::kernel::ebpf {

// eBPF instruction format (64-bit)
struct ebpf_inst {
    uint8_t  opcode;   // Operation code
    uint8_t  dst;      // Destination register (0-10)
    uint8_t  src;      // Source register (0-10)
    int16_t  off;      // Offset (for jumps/memory)
    int64_t  imm;      // Immediate value
};

// eBPF program
struct ebpf_program {
    const ebpf_inst* insns;
    uint32_t         insn_cnt;
    uint64_t         result;  // Last return value
};

// eBPF map (simple key-value store)
struct ebpf_map {
    uint64_t keys[256];
    uint64_t values[256];
    uint32_t count;
    uint32_t max_entries;
    bool     valid;
};

// Helper function IDs (called from eBPF)
enum class ebpf_helper : uint64_t {
    HELP_PRINTK   = 1,
    HELP_GET_PID  = 2,
    HELP_GET_TID  = 3,
    HELP_GET_TIME = 4,
    HELP_GET_MEM  = 5,
    HELP_MAP_GET  = 6,
    HELP_MAP_SET  = 7,
    HELP_RANDOM   = 8,
    HELP_TRACE    = 9,
};

// Load an eBPF program from memory
bool ebpf_load_program(const ebpf_inst* insns, uint32_t cnt, ebpf_program* prog);

// Run an eBPF program
uint64_t ebpf_run_program(ebpf_program* prog);

// Validate an eBPF program (bounds checking, no loops, etc.)
bool ebpf_verify(const ebpf_inst* insns, uint32_t cnt);

// Map operations
bool ebpf_map_get(ebpf_map* map, uint64_t key, uint64_t* value);
bool ebpf_map_set(ebpf_map* map, uint64_t key, uint64_t value);

// Register a trace hook (called on syscall entry/exit, etc.)
typedef void (*trace_hook_t)(uint64_t event_type, uint64_t arg1, uint64_t arg2, uint64_t arg3);
void ebpf_register_trace(trace_hook_t hook);

// Trigger a trace event
void ebpf_trace_event(uint64_t event_type, uint64_t arg1, uint64_t arg2, uint64_t arg3);

// Event types
enum class trace_event : uint64_t {
    EVENT_SYSCALL_ENTRY = 1,
    EVENT_SYSCALL_EXIT  = 2,
    EVENT_ALLOC         = 3,
    EVENT_FREE          = 4,
    EVENT_NET_SEND      = 5,
    EVENT_NET_RECV      = 6,
    EVENT_SCHEDULE      = 7,
    EVENT_AGENT_SPAWN   = 8,
    EVENT_AGENT_EXIT    = 9,
    EVENT_SAND_VIOLATION= 10,
};

} // namespace feather::kernel::ebpf
