/*
 * feather-hal factory dispatch — routes to selected backend
 * 
 * This file is compiled into the feather-hal library and provides
 * the create_* functions that return backend-specific implementations.
 */

#include "feather/hal.hpp"

namespace feather::hal {

/* ──────────────────────────────────────────────────────────────
 * Portal Backend
 * ────────────────────────────────────────────────────────────── */
#if defined(FEATHER_HAL_PORTAL_BACKEND)
    // Forward declarations from portal backend
    std::unique_ptr<ProcessManager> create_process_manager();
    std::unique_ptr<VirtualFileSystem> create_virtual_filesystem();
    std::unique_ptr<SystemTelemetryProvider> create_telemetry_provider();
    std::unique_ptr<SandboxDriver> create_sandbox_driver();

/* ──────────────────────────────────────────────────────────────
 * Linux Backend (existing)
 * ────────────────────────────────────────────────────────────── */
#elif defined(FEATHER_HAL_LINUX_BACKEND)
    std::unique_ptr<ProcessManager> create_process_manager();
    std::unique_ptr<VirtualFileSystem> create_virtual_filesystem();
    std::unique_ptr<SystemTelemetryProvider> create_telemetry_provider();
    std::unique_ptr<SandboxDriver> create_sandbox_driver();

/* ──────────────────────────────────────────────────────────────
 * Windows Backend
 * ────────────────────────────────────────────────────────────── */
#elif defined(FEATHER_HAL_WINDOWS_BACKEND)
    std::unique_ptr<ProcessManager> create_process_manager();
    std::unique_ptr<VirtualFileSystem> create_virtual_filesystem();
    std::unique_ptr<SystemTelemetryProvider> create_telemetry_provider();
    std::unique_ptr<SandboxDriver> create_sandbox_driver();

#else
    #error "No HAL backend selected"
#endif

} // namespace feather::hal