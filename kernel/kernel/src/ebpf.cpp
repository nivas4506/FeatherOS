// FeatherOS Kernel — eBPF-like Telemetry Subsystem Implementation
// Safe, in-kernel bytecode interpreter for monitoring system calls,
// memory access, and agent behavior.

#include "ebpf.h"
#include "../../lib/include/printf.h"
#include "../../lib/include/string.h"
#include "../../task/include/task.h"

namespace feather::kernel::ebpf {
namespace {

// Register file for eBPF execution
struct ebpf_regs {
    uint64_t regs[11];  // R0-R10 (R10 is stack pointer, read-only)
};

// Stack for eBPF programs (512 bytes)
static uint8_t g_ebpf_stack[512];

// Global program state during execution
static ebpf_regs   g_regs;
static uint64_t    g_pc = 0;       // Program counter
static uint64_t    g_insn_cnt = 0; // Instruction count (for loop prevention)
static uint64_t    g_insn_limit = 0; // Max instructions (prevent infinite loops)
static ebpf_program* g_prog = nullptr;   // Currently executing program

// eBPF map storage
static ebpf_map g_maps[16];
static uint32_t g_map_count = 0;

// Trace hook
static trace_hook_t g_trace_hook = nullptr;

// Execute a single instruction
bool ebpf_exec_one() {
    if (g_pc >= g_prog->insn_cnt) return false;  // Program ended

    const ebpf_inst& insn = g_prog->insns[g_pc];
    g_pc++;

    uint64_t dst_val = g_regs.regs[insn.dst];
    uint64_t src_val;

    // Resolve source value
    if (insn.src & 0x01) {  // Immediate
        src_val = static_cast<uint64_t>(insn.imm);
    } else {
        src_val = g_regs.regs[insn.src & 0x0F];
    }

    switch (static_cast<ebpf_op>(insn.opcode)) {
        case ebpf_op::OP_ADD:
            g_regs.regs[insn.dst] = dst_val + src_val;
            break;
        case ebpf_op::OP_SUB:
            g_regs.regs[insn.dst] = dst_val - src_val;
            break;
        case ebpf_op::OP_MUL:
            g_regs.regs[insn.dst] = dst_val * src_val;
            break;
        case ebpf_op::OP_DIV:
            if (src_val == 0) return false;
            g_regs.regs[insn.dst] = dst_val / src_val;
            break;
        case ebpf_op::OP_OR:
            g_regs.regs[insn.dst] = dst_val | src_val;
            break;
        case ebpf_op::OP_AND:
            g_regs.regs[insn.dst] = dst_val & src_val;
            break;
        case ebpf_op::OP_LSH:
            g_regs.regs[insn.dst] = dst_val << src_val;
            break;
        case ebpf_op::OP_RSH:
            g_regs.regs[insn.dst] = dst_val >> src_val;
            break;
        case ebpf_op::OP_NEG:
            g_regs.regs[insn.dst] = static_cast<uint64_t>(-static_cast<int64_t>(dst_val));
            break;
        case ebpf_op::OP_MOD:
            if (src_val == 0) return false;
            g_regs.regs[insn.dst] = dst_val % src_val;
            break;
        case ebpf_op::OP_XOR:
            g_regs.regs[insn.dst] = dst_val ^ src_val;
            break;
        case ebpf_op::OP_MOV:
            g_regs.regs[insn.dst] = src_val;
            break;
        case ebpf_op::OP_ARSH:
            g_regs.regs[insn.dst] = static_cast<uint64_t>(static_cast<int64_t>(dst_val) >> src_val);
            break;
        case ebpf_op::OP_LE:
            g_regs.regs[insn.dst] = dst_val;  // Simplified: assume LE host
            break;
        case ebpf_op::OP_BE:
            g_regs.regs[insn.dst] = dst_val;  // Simplified: assume LE host
            break;
        case ebpf_op::OP_ADD_IMM:
            g_regs.regs[insn.dst] = dst_val + static_cast<uint64_t>(insn.imm);
            break;
        case ebpf_op::OP_LD_IMM:
            g_regs.regs[insn.dst] = insn.imm;
            break;
        case ebpf_op::OP_LD_MEM: {
            uint64_t addr = src_val + static_cast<uint64_t>(insn.off);
            if (addr + 8 > 512) return false;  // Stack bounds check
            g_regs.regs[insn.dst] = *reinterpret_cast<uint64_t*>(g_ebpf_stack + addr);
            break;
        }
        case ebpf_op::OP_ST_MEM: {
            uint64_t addr = dst_val + static_cast<uint64_t>(insn.off);
            if (addr + 8 > 512) return false;  // Stack bounds check
            *reinterpret_cast<uint64_t*>(g_ebpf_stack + addr) = src_val;
            break;
        }
        case ebpf_op::OP_LDXW: {
            uint64_t addr = src_val + static_cast<uint64_t>(insn.off);
            if (addr + 4 > 512) return false;
            g_regs.regs[insn.dst] = *reinterpret_cast<uint32_t*>(g_ebpf_stack + addr);
            break;
        }
        case ebpf_op::OP_STXW: {
            uint64_t addr = dst_val + static_cast<uint64_t>(insn.off);
            if (addr + 4 > 512) return false;
            *reinterpret_cast<uint32_t*>(g_ebpf_stack + addr) = static_cast<uint32_t>(src_val);
            break;
        }
        case ebpf_op::OP_JEQ:
            if (dst_val == src_val) g_pc += static_cast<uint64_t>(insn.off);
            break;
        case ebpf_op::OP_JNE:
            if (dst_val != src_val) g_pc += static_cast<uint64_t>(insn.off);
            break;
        case ebpf_op::OP_JGT:
            if (dst_val > src_val) g_pc += static_cast<uint64_t>(insn.off);
            break;
        case ebpf_op::OP_JGE:
            if (dst_val >= src_val) g_pc += static_cast<uint64_t>(insn.off);
            break;
        case ebpf_op::OP_JLT:
            if (dst_val < src_val) g_pc += static_cast<uint64_t>(insn.off);
            break;
        case ebpf_op::OP_JLE:
            if (dst_val <= src_val) g_pc += static_cast<uint64_t>(insn.off);
            break;
        case ebpf_op::OP_JSET:
            if (dst_val & src_val) g_pc += static_cast<uint64_t>(insn.off);
            break;
        case ebpf_op::OP_JNE_IMM:
            if (dst_val != static_cast<uint64_t>(insn.imm)) g_pc += static_cast<uint64_t>(insn.off);
            break;
        case ebpf_op::OP_CALL: {
            // Helper function call
            switch (static_cast<ebpf_helper>(insn.imm)) {
                case ebpf_helper::HELP_PRINTK:
                    g_regs.regs[0] = 0;
                    break;
                case ebpf_helper::HELP_GET_PID: {
                    auto* t = task::task_current();
                    g_regs.regs[0] = t ? t->id : 0;
                    break;
                }
                case ebpf_helper::HELP_GET_TID: {
                    auto* t = task::task_current();
                    g_regs.regs[0] = t ? t->id : 0;
                    break;
                }
                case ebpf_helper::HELP_GET_TIME:
                    g_regs.regs[0] = 0;  // Placeholder for uptime
                    break;
                case ebpf_helper::HELP_GET_MEM:
                    g_regs.regs[0] = 0;  // Placeholder
                    break;
                case ebpf_helper::HELP_MAP_GET: {
                    uint64_t key = g_regs.regs[1];
                    uint64_t val = 0;
                    for (uint32_t i = 0; i < g_map_count; i++) {
                        if (g_maps[i].keys[i % g_maps[i].count] == key) {
                            val = g_maps[i].values[i % g_maps[i].count];
                            break;
                        }
                    }
                    g_regs.regs[0] = val;
                    break;
                }
                case ebpf_helper::HELP_MAP_SET: {
                    uint64_t key = g_regs.regs[1];
                    uint64_t val = g_regs.regs[2];
                    // Find or create map entry
                    for (uint32_t i = 0; i < g_map_count; i++) {
                        if (!g_maps[i].valid) {
                            g_maps[i].keys[0] = key;
                            g_maps[i].values[0] = val;
                            g_maps[i].valid = true;
                            g_maps[i].count = 1;
                            break;
                        }
                    }
                    g_regs.regs[0] = 0;
                    break;
                }
                case ebpf_helper::HELP_RANDOM:
                    g_regs.regs[0] = g_pc * 0x9E3779B97F4A7C15ULL;
                    break;
                case ebpf_helper::HELP_TRACE:
                    if (g_trace_hook) {
                        g_trace_hook(g_regs.regs[1], g_regs.regs[2], g_regs.regs[3], g_regs.regs[4]);
                    }
                    g_regs.regs[0] = 0;
                    break;
                default:
                    g_regs.regs[0] = 0;
                    break;
            }
            break;
        }
        case ebpf_op::OP_EXIT:
            g_regs.regs[0] = dst_val;
            return false;  // Program complete
        case ebpf_op::OP_JA:
            g_pc += static_cast<uint64_t>(insn.off);
            break;
        default:
            return false;  // Unknown opcode
    }

    g_insn_cnt++;
    if (g_insn_cnt >= g_insn_limit) return false;  // Instruction limit exceeded

    return true;
}

} // anonymous namespace

// ──────────────────────────────────────────────────────────────
// Public API
// ──────────────────────────────────────────────────────────────

bool ebpf_load_program(const ebpf_inst* insns, uint32_t cnt, ebpf_program* prog) {
    if (!insns || !prog || cnt == 0 || cnt > 4096) return false;

    prog->insns = insns;
    prog->insn_cnt = cnt;
    prog->result = 0;

    return ebpf_verify(insns, cnt);
}

bool ebpf_verify(const ebpf_inst* insns, uint32_t cnt) {
    if (!insns || cnt == 0 || cnt > 4096) return false;

    // Simple validation: check for invalid opcodes, register bounds
    for (uint32_t i = 0; i < cnt; i++) {
        const ebpf_inst& ins = insns[i];

        // Check opcode range
        if (ins.opcode > 0x1F) return false;

        // Check register bounds
        if (ins.dst > 10) return false;
        if ((ins.src & 0x0F) > 10) return false;

        // Check for backward jumps (potential infinite loops)
        if (ins.opcode == static_cast<uint8_t>(ebpf_op::OP_JA) ||
            ins.opcode == static_cast<uint8_t>(ebpf_op::OP_JEQ) ||
            ins.opcode == static_cast<uint8_t>(ebpf_op::OP_JNE) ||
            ins.opcode == static_cast<uint8_t>(ebpf_op::OP_JGT) ||
            ins.opcode == static_cast<uint8_t>(ebpf_op::OP_JGE) ||
            ins.opcode == static_cast<uint8_t>(ebpf_op::OP_JLT) ||
            ins.opcode == static_cast<uint8_t>(ebpf_op::OP_JLE) ||
            ins.opcode == static_cast<uint8_t>(ebpf_op::OP_JSET) ||
            ins.opcode == static_cast<uint8_t>(ebpf_op::OP_JNE_IMM)) {
            if (ins.off < 0) return false;  // No backward jumps
        }
    }

    return true;
}

uint64_t ebpf_run_program(ebpf_program* prog) {
    if (!prog || !prog->insns || prog->insn_cnt == 0) return 0;

    g_prog = prog;
    g_pc = 0;
    g_insn_cnt = 0;
    g_insn_limit = 100000;  // Max 100K instructions

    // Reset registers
    for (int i = 0; i < 11; i++) g_regs.regs[i] = 0;

    while (g_pc < prog->insn_cnt) {
        if (!ebpf_exec_one()) break;
    }

    g_prog->result = g_regs.regs[0];
    return g_regs.regs[0];
}

bool ebpf_map_get(ebpf_map* map, uint64_t key, uint64_t* value) {
    if (!map || !value) return false;
    for (uint32_t i = 0; i < map->count; i++) {
        if (map->keys[i] == key) {
            *value = map->values[i];
            return true;
        }
    }
    return false;
}

bool ebpf_map_set(ebpf_map* map, uint64_t key, uint64_t value) {
    if (!map) return false;
    // Find existing
    for (uint32_t i = 0; i < map->count; i++) {
        if (map->keys[i] == key) {
            map->values[i] = value;
            return true;
        }
    }
    // Add new
    if (map->count >= map->max_entries) return false;
    map->keys[map->count] = key;
    map->values[map->count] = value;
    map->count++;
    return true;
}

void ebpf_register_trace(trace_hook_t hook) {
    g_trace_hook = hook;
}

void ebpf_trace_event(uint64_t event_type, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    if (g_trace_hook) {
        g_trace_hook(event_type, arg1, arg2, arg3);
    }
}

} // namespace feather::kernel::ebpf
