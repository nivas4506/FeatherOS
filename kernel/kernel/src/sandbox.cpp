// FeatherOS Kernel — Sandboxed Execution Domain
// Sandboxed execution domains (ring 3 + seccomp-like filters) for
// aegis-exec safety engine. Provides syscall filtering and memory
// domain isolation for untrusted cognitive agents.

#include "sandbox.h"
#include "../../lib/include/printf.h"
#include "../../lib/include/string.h"
#include "../../task/include/task.h"

namespace feather::kernel::sandbox {
namespace {

// Max sandbox domains
static constexpr uint32_t MAX_DOMAINS = 64;

// Max rules per domain
static constexpr uint32_t MAX_RULES_PER_DOMAIN = 256;

// Domain table
static sandbox_domain g_domains[MAX_DOMAINS];
static sandbox_rule g_rules[MAX_DOMAINS][MAX_RULES_PER_DOMAIN];
static uint32_t g_rule_count[MAX_DOMAINS] = {};
static bool g_domain_active[MAX_DOMAINS] = {false};

// Violation callback
static violation_cb_t g_violation_cb = nullptr;

// Violation tracking
static uint64_t g_domain_violations[MAX_DOMAINS] = {};

// Find domain by ID
static sandbox_domain* find_domain(sandbox_id_t id) {
    if (id >= MAX_DOMAINS || !g_domain_active[id]) return nullptr;
    return &g_domains[id];
}

// Check if a rule matches
static bool rule_matches(const sandbox_rule* rule, uint64_t syscall_num,
                         uint64_t arg1, uint64_t arg2, uint64_t arg3,
                         uint64_t arg4, uint64_t arg5) {
    if (rule->syscall_num != 0 && rule->syscall_num != syscall_num)
        return false;

    if (rule->arg_mask != 0) {
        // Simple arg matching: check if arg1 matches
        if ((arg1 & rule->arg_mask) != (rule->arg_value & rule->arg_mask))
            return false;
    }

    return true;
}

} // anonymous namespace

void sandbox_init() {
    for (uint32_t i = 0; i < MAX_DOMAINS; i++) {
        g_domains[i].id = static_cast<uint32_t>(i);
        g_domains[i].flags = 0;
        g_domains[i].max_memory = 0;
        g_domains[i].max_cpu_time_ns = 0;
        g_domains[i].max_fds = 0;
        g_domains[i].allow_network = false;
        g_domains[i].allow_filesystem = false;
        g_domains[i].allow_spawn = false;
        g_domains[i].allow_mmap_exec = false;
        g_rule_count[i] = 0;
        g_domain_active[i] = false;
        g_domain_violations[i] = 0;
    }
    g_violation_cb = nullptr;

    lib::kputs("[sandbox] sandboxed execution domain subsystem initialized\n");
}

sandbox_id_t sandbox_create(const sandbox_domain* config) {
    if (!config) return static_cast<sandbox_id_t>(-1);

    // Find free domain slot
    for (uint32_t i = 0; i < MAX_DOMAINS; i++) {
        if (!g_domain_active[i]) {
            g_domains[i] = *config;
            g_domains[i].id = static_cast<uint32_t>(i);
            g_domain_active[i] = true;
            g_rule_count[i] = 0;

            lib::kprintf("[sandbox] domain %u created\n", i);
            return static_cast<sandbox_id_t>(i);
        }
    }

    return static_cast<sandbox_id_t>(-1);
}

void sandbox_destroy(sandbox_id_t id) {
    if (id >= MAX_DOMAINS || !g_domain_active[id]) return;

    g_rule_count[id] = 0;
    g_domain_active[id] = false;
    g_domain_violations[id] = 0;
}

int sandbox_add_rule(sandbox_id_t id, const sandbox_rule* rule) {
    if (id >= MAX_DOMAINS || !g_domain_active[id] || !rule) return -1;
    if (g_rule_count[id] >= MAX_RULES_PER_DOMAIN) return -1;

    g_rules[id][g_rule_count[id]] = *rule;
    g_rule_count[id]++;

    return static_cast<int>(g_rule_count[id] - 1);
}

int sandbox_remove_rule(sandbox_id_t id, uint64_t syscall_num) {
    if (id >= MAX_DOMAINS || !g_domain_active[id]) return -1;

    for (uint32_t i = 0; i < g_rule_count[id]; i++) {
        if (g_rules[id][i].syscall_num == syscall_num) {
            // Shift remaining rules
            for (uint32_t j = i; j < g_rule_count[id] - 1; j++) {
                g_rules[id][j] = g_rules[id][j + 1];
            }
            g_rule_count[id]--;
            return 0;
        }
    }

    return -1;
}

bool sandbox_check(uint64_t syscall_num, uint64_t arg1, uint64_t arg2,
                   uint64_t arg3, uint64_t arg4, uint64_t arg5,
                   sandbox_action* action) {
    // Get current task's domain
    auto* t = task::task_current();
    if (!t) return true;  // No task context, allow

    // Find the task's sandbox domain
    // In a full implementation, task struct would have domain_id
    // For now, use task_id as domain hint
    sandbox_id_t domain_id = static_cast<sandbox_id_t>(t->id % MAX_DOMAINS);

    sandbox_domain* dom = find_domain(domain_id);
    if (!dom || !g_domain_active[domain_id]) return true;  // No domain, allow

    // Check rules
    for (uint32_t i = 0; i < g_rule_count[domain_id]; i++) {
        const sandbox_rule* rule = &g_rules[domain_id][i];
        if (rule_matches(rule, syscall_num, arg1, arg2, arg3, arg4, arg5)) {
            *action = rule->action;

            // Log violation if not allow
            if (rule->action == sandbox_action::SANDBOX_DENY ||
                rule->action == sandbox_action::SANDBOX_KILL) {
                g_domain_violations[domain_id]++;
                if (g_violation_cb) {
                    sandbox_violation v;
                    v.domain_id = domain_id;
                    v.task_id = t->id;
                    v.syscall_num = syscall_num;
                    v.arg1 = arg1;
                    v.arg2 = arg2;
                    v.timestamp = 0;  // Would use cycle counter
                    g_violation_cb(&v);
                }
            }

            return rule->action != sandbox_action::SANDBOX_DENY &&
                   rule->action != sandbox_action::SANDBOX_KILL;
        }
    }

    // Default: allow if domain allows it
    return true;
}

bool sandbox_check_mem(sandbox_id_t id, void* addr, uint64_t size, bool write) {
    if (id >= MAX_DOMAINS || !g_domain_active[id]) return true;

    sandbox_domain* dom = find_domain(id);
    if (!dom) return true;

    // Check memory bounds
    if (dom->max_memory > 0) {
        uint64_t addr_end = reinterpret_cast<uint64_t>(addr) + size;
        if (addr_end > dom->max_memory) return false;
    }

    // Check W^X
    if (write && dom->allow_mmap_exec == false) {
        // In a real implementation, check page permissions
        // For now, just allow
    }

    return true;
}

bool sandbox_check_file(sandbox_id_t id, const char* path, bool write) {
    if (id >= MAX_DOMAINS || !g_domain_active[id]) return true;

    sandbox_domain* dom = find_domain(id);
    if (!dom) return true;

    if (!dom->allow_filesystem) return false;

    return true;
}

bool sandbox_get_config(sandbox_id_t id, sandbox_domain* config) {
    if (id >= MAX_DOMAINS || !g_domain_active[id] || !config) return false;

    *config = g_domains[id];
    return true;
}

void sandbox_register_violation_cb(violation_cb_t cb) {
    g_violation_cb = cb;
}

uint64_t sandbox_get_violations(sandbox_id_t id) {
    if (id >= MAX_DOMAINS || !g_domain_active[id]) return 0;
    return g_domain_violations[id];
}

} // namespace feather::kernel::sandbox
