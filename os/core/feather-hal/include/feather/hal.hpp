/*
 * FeatherOS Hardware & Kernel Abstraction Layer (HAL) — C++20 Contracts
 * 
 * Platform-independent interfaces for ProcessManager, VirtualFileSystem,
 * SystemTelemetryProvider, and SandboxDriver.
 * 
 * Kernel-specific backends implement these abstract classes.
 */

#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <cstddef>
#include <span>
#include "feather/result.hpp"

namespace feather::hal {

/* ──────────────────────────────────────────────────────────────
 * Sandbox kinds — extend for your kernel's isolation primitives
 * ────────────────────────────────────────────────────────────── */
enum class SandboxKind {
    UniversalRestricted,  // Default: restricted portal namespace
    CustomCompartment,    // Future: CHERI/WASM/memory domain
    CustomVM              // Future: micro-VM
};

/* ──────────────────────────────────────────────────────────────
 * Process execution result
 * ────────────────────────────────────────────────────────────── */
struct ProcessOutput {
    int exit_code;
    std::string stdout_data;
    std::string stderr_data;
    std::uint64_t duration_ms;
};

/* ──────────────────────────────────────────────────────────────
 * Hardware telemetry snapshot
 * ────────────────────────────────────────────────────────────── */
struct HardwareTelemetry {
    std::string arch;
    std::string kernel_version;
    std::size_t cpu_cores;
    float cpu_usage_pct;
    std::uint64_t mem_total_mb;
    std::uint64_t mem_available_mb;
    std::uint64_t disk_available_gb;
};

/* ──────────────────────────────────────────────────────────────
 * ProcessManager — Spawn, signal, wait on processes
 * ────────────────────────────────────────────────────────────── */
class ProcessManager {
public:
    virtual ~ProcessManager() = default;

    /**
     * Execute a command with optional sandbox and timeout.
     * @param cmd      Shell command line (e.g., "ls -la /tmp")
     * @param cwd      Working directory
     * @param sandbox  Sandbox isolation level
     * @param timeout  Optional timeout (wall-clock)
     * @return ProcessOutput on success, error string on failure
     */
    virtual Expected<ProcessOutput>
    execute(std::string_view cmd,
            const std::filesystem::path& cwd,
            SandboxKind sandbox,
            std::optional<std::chrono::seconds> timeout) = 0;

    /**
     * Terminate a process by PID.
     * @param pid  Process ID from execute()
     */
    virtual ExpectedVoid terminate(std::uint32_t pid) = 0;
};

/* ──────────────────────────────────────────────────────────────
 * VirtualFileSystem — Atomic file operations, path resolution
 * ────────────────────────────────────────────────────────────── */
class VirtualFileSystem {
public:
    virtual ~VirtualFileSystem() = default;

    /** Read entire file as UTF-8 string */
    virtual Expected<std::string>
    read_to_string(const std::filesystem::path& path) = 0;

    /** Atomic write: write to temp, then rename over target */
    virtual ExpectedVoid
    write_atomic(const std::filesystem::path& path, std::span<const std::byte> content) = 0;

    /** Remove a file */
    virtual ExpectedVoid remove_file(const std::filesystem::path& path) = 0;

    /** List directory entries (non-recursive) */
    virtual Expected<std::vector<std::filesystem::path>>
    list_directory(const std::filesystem::path& path) = 0;

    /** Check existence (non-throwing, synchronous) */
    virtual bool exists(const std::filesystem::path& path) const noexcept = 0;
};

/* ──────────────────────────────────────────────────────────────
 * SystemTelemetryProvider — Cross-platform metrics sampling
 * ────────────────────────────────────────────────────────────── */
class SystemTelemetryProvider {
public:
    virtual ~SystemTelemetryProvider() = default;

    /** Sample current system telemetry */
    virtual Expected<HardwareTelemetry> sample_telemetry() = 0;

    /** Kernel version string (cached, fast) */
    virtual std::string kernel_version() const noexcept = 0;

    /** Architecture string (cached, fast) */
    virtual std::string architecture() const noexcept = 0;
};

/* ──────────────────────────────────────────────────────────────
 * SandboxDriver — Command wrapping for isolation
 * ────────────────────────────────────────────────────────────── */
class SandboxDriver {
public:
    virtual ~SandboxDriver() = default;

    /**
     * Wrap a command for sandboxed execution.
     * @param original_cmd  Raw command line
     * @param working_dir   Working directory
     * @param kind          Sandbox type
     * @return Wrapped command string (e.g., "portal_spawn_restricted -- ...")
     */
    virtual std::string wrap_command(std::string_view original_cmd,
                                     const std::filesystem::path& working_dir,
                                     SandboxKind kind) const = 0;

    /** Detect best available sandbox on this system */
    virtual SandboxKind detect_capability() const noexcept = 0;
};

/* ──────────────────────────────────────────────────────────────
 * Factory functions — Backend selection at runtime
 * ────────────────────────────────────────────────────────────── */

// Defined in each backend's implementation file
std::unique_ptr<ProcessManager> create_process_manager();
std::unique_ptr<VirtualFileSystem> create_virtual_filesystem();
std::unique_ptr<SystemTelemetryProvider> create_telemetry_provider();
std::unique_ptr<SandboxDriver> create_sandbox_driver();

// Convenience functions
inline std::unique_ptr<ProcessManager> make_process_manager() { return create_process_manager(); }
inline std::unique_ptr<VirtualFileSystem> make_virtual_filesystem() { return create_virtual_filesystem(); }
inline std::unique_ptr<SystemTelemetryProvider> make_telemetry_provider() { return create_telemetry_provider(); }
inline std::unique_ptr<SandboxDriver> make_sandbox_driver() { return create_sandbox_driver(); }

// Shorthand for HAL access
inline Expected<ProcessOutput> execute_command(std::string_view cmd,
                                               const std::filesystem::path& cwd,
                                               SandboxKind sandbox,
                                               std::optional<std::chrono::seconds> timeout = {}) {
    return make_process_manager()->execute(cmd, cwd, sandbox, timeout);
}

inline ExpectedVoid terminate_process(std::uint32_t pid) {
    return make_process_manager()->terminate(pid);
}

inline Expected<std::string> read_file(const std::filesystem::path& path) {
    return make_virtual_filesystem()->read_to_string(path);
}

inline ExpectedVoid write_file(const std::filesystem::path& path, std::span<const std::byte> content) {
    return make_virtual_filesystem()->write_atomic(path, content);
}

inline ExpectedVoid remove_file(const std::filesystem::path& path) {
    return make_virtual_filesystem()->remove_file(path);
}

inline Expected<std::vector<std::filesystem::path>> list_directory(const std::filesystem::path& path) {
    return make_virtual_filesystem()->list_directory(path);
}

inline bool file_exists(const std::filesystem::path& path) {
    return make_virtual_filesystem()->exists(path);
}

inline Expected<HardwareTelemetry> sample_telemetry() {
    return make_telemetry_provider()->sample_telemetry();
}

inline std::string kernel_version() {
    return make_telemetry_provider()->kernel_version();
}

inline std::string architecture() {
    return make_telemetry_provider()->architecture();
}

} // namespace feather::hal