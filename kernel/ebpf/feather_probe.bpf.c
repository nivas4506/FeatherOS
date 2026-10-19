// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
/*
 * FeatherOS In-Kernel eBPF Probe
 * Bridges Linux Kernel Base Layer events directly to FeatherOS Ambient Perception
 */

#include <linux/bpf.h>
#include <linux/ptrace.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

struct event {
    __u32 pid;
    __u32 ppid;
    __u32 uid;
    char comm[16];
    char filename[256];
    __u32 event_type; // 1: exec, 2: unlink, 3: rename, 4: security violation
};

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);
} feather_events SEC(".maps");

SEC("tracepoint/syscalls/sys_enter_execve")
int tracepoint__syscalls__sys_enter_execve(struct trace_event_raw_sys_enter* ctx) {
    struct event *e;
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    __u32 pid = pid_tgid >> 32;

    e = bpf_ringbuf_reserve(&feather_events, sizeof(*e), 0);
    if (!e) return 0;

    e->pid = pid;
    e->uid = bpf_get_current_uid_gid() & 0xFFFFFFFF;
    e->event_type = 1;
    bpf_get_current_comm(&e->comm, sizeof(e->comm));
    
    // Copy target binary path from first argument
    const char *filename_ptr = (const char *)ctx->args[0];
    bpf_probe_read_user_str(&e->filename, sizeof(e->filename), filename_ptr);

    bpf_ringbuf_submit(e, 0);
    return 0;
}

SEC("tracepoint/syscalls/sys_enter_unlinkat")
int tracepoint__syscalls__sys_enter_unlinkat(struct trace_event_raw_sys_enter* ctx) {
    struct event *e;
    __u64 pid_tgid = bpf_get_current_pid_tgid();
    __u32 pid = pid_tgid >> 32;

    e = bpf_ringbuf_reserve(&feather_events, sizeof(*e), 0);
    if (!e) return 0;

    e->pid = pid;
    e->uid = bpf_get_current_uid_gid() & 0xFFFFFFFF;
    e->event_type = 2; // file deletion
    bpf_get_current_comm(&e->comm, sizeof(e->comm));

    const char *filename_ptr = (const char *)ctx->args[1];
    bpf_probe_read_user_str(&e->filename, sizeof(e->filename), filename_ptr);

    bpf_ringbuf_submit(e, 0);
    return 0;
}
