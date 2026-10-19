// FeatherOS OS Layer Demo — PORTAL Kernel Backend (Minimal)
// Demonstrates HAL components working together

#include "feather/hal.hpp"
#include "feather/ipc.hpp"
#include "feather/watch.hpp"

#include <iostream>
#include <filesystem>
#include <chrono>
#include <thread>

int main() {
    std::ios::sync_with_stdio(false);
    std::cout << std::unitbuf; // Enable auto-flush
    
    std::cout << "=== FeatherOS OS Layer Demo ===\n\n";

    using namespace feather::hal;

    // 1. System Telemetry
    std::cout << "--- System Telemetry ---\n";
    auto telemetry = make_telemetry_provider();
    std::cout << "  Kernel: " << telemetry->kernel_version() << "\n";
    std::cout << "  Arch:   " << telemetry->architecture() << "\n";
    
    auto t_result = telemetry->sample_telemetry();
    if (t_result.has_value()) {
        const auto& t = t_result.value();
        std::cout << "  CPU: " << t.cpu_cores << " cores, " << t.cpu_usage_pct << "%\n";
        std::cout << "  Mem: " << t.mem_available_mb << "/" << t.mem_total_mb << " MB\n";
    } else {
        std::cout << "  Error: " << t_result.error() << "\n";
    }

    // 2. Virtual File System
    std::cout << "\n--- Virtual File System ---\n";
    auto vfs = make_virtual_filesystem();
    std::filesystem::path test_file = std::filesystem::temp_directory_path() / "feather_demo.txt";
    
    std::string content = "Hello from FeatherOS!\n";
    auto w_result = vfs->write_atomic(test_file, 
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(content.data()), content.size()));
    if (w_result.has_error()) {
        std::cout << "  Write error: " << w_result.error() << "\n";
    } else {
        std::cout << "  Write: OK\n";
    }
    
    auto r_result = vfs->read_to_string(test_file);
    if (r_result.has_value()) {
        std::cout << "  Read: " << r_result.value();
    } else {
        std::cout << "  Read error: " << r_result.error() << "\n";
    }

    // 3. Process Manager (simulated)
    std::cout << "\n--- Process Manager ---\n";
    auto pm = make_process_manager();
    auto sandbox = make_sandbox_driver();
    std::cout << "  Sandbox: " << (sandbox->detect_capability() == SandboxKind::UniversalRestricted ? "UniversalRestricted" : "Other") << "\n";
    std::cout << "  (Execution requires PORTAL kernel)\n";

    // 4. IPC
    std::cout << "\n--- IPC Transport ---\n";
    std::cout << "  Endpoint: featherd\n";
    std::cout << "  Protocol: JSON-RPC 2.0 over Portal IPC\n";

    // 5. Watcher
    std::cout << "\n--- Filesystem Watcher ---\n";
    auto watcher = feather::watch::create_workspace_watcher(
        [](const feather::watch::FileEvent& ev) {
            std::cout << "  [Event] " << (int)ev.kind << ": " << ev.path << "\n";
        });
    std::cout << "  Watcher created\n";
    watcher->stop();
    std::cout << "  Watcher stopped\n";

    std::cout << "\n=== Demo Complete ===\n";
    return 0;
}