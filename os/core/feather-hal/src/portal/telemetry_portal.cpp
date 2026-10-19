/*
 * Portal SystemTelemetryProvider Backend
 * Implements feather::hal::SystemTelemetryProvider using portal_get_telemetry syscall.
 */

#include "feather/hal.hpp"
#include <portal/portal.h>
#include <string>
#include <mutex>
#include <cstring>

namespace feather::hal {

class PortalTelemetry final : public SystemTelemetryProvider {
public:
    PortalTelemetry() = default;

    Expected<HardwareTelemetry> sample_telemetry() override {
        portal_telemetry_t t{};
        int ret = portal_get_telemetry(&t);
        if (ret < 0) {
            return Expected<HardwareTelemetry>::err(std::string("portal_get_telemetry failed: ") + std::strerror(-ret));
        }

        return HardwareTelemetry{
            .arch = t.architecture,
            .kernel_version = t.kernel_version,
            .cpu_cores = t.cpu_cores,
            .cpu_usage_pct = t.cpu_usage_pct,
            .mem_total_mb = t.mem_total_mb,
            .mem_available_mb = t.mem_available_mb,
            .disk_available_gb = t.disk_available_gb
        };
    }

    std::string kernel_version() const noexcept override {
        // Cached version — fast path
        static std::string cached;
        static std::once_flag flag;
        std::call_once(flag, [this] {
            portal_telemetry_t t{};
            if (portal_get_telemetry(&t) == 0) {
                cached = t.kernel_version;
            } else {
                cached = "Portal Kernel (unknown)";
            }
        });
        return cached;
    }

    std::string architecture() const noexcept override {
        static std::string cached;
        static std::once_flag flag;
        std::call_once(flag, [this] {
            portal_telemetry_t t{};
            if (portal_get_telemetry(&t) == 0) {
                cached = t.architecture;
            } else {
                cached = "unknown";
            }
        });
        return cached;
    }
};

std::unique_ptr<SystemTelemetryProvider> create_telemetry_provider() {
    return std::make_unique<PortalTelemetry>();
}

} // namespace feather::hal