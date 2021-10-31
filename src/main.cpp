#include <network-monitor/env.h>
#include <network-monitor/NetworkMonitor.h>
#include <network-monitor/WebsocketClient.h>

#include <chrono>
#include <string>

using NetworkMonitor::BoostWebsocketClient;
using NetworkMonitor::GetEnvVar;
using NetworkMonitor::NetworkMonitorError;
using NetworkMonitor::NetworkMonitorConfig;

int main()
{
    // Monitor configuration
    NetworkMonitorConfig config {
        GetEnvVar("LTNM_SERVER_URL", "ltnm.learncppthroughprojects.com"),
        GetEnvVar("LTNM_SERVER_PORT", "443"),
        GetEnvVar("LTNM_USERNAME"),
        GetEnvVar("LTNM_PASSWORD"),
        GetEnvVar("LTNM_CACERT_PATH", "cacert.pem"),
        GetEnvVar("LTNM_NETWORK_LAYOUT_FILE_PATH", ""),
    };

    // Optional run timeout
    // Default: Oms = run indefinitely
    auto timeoutMs {std::stoi(GetEnvVar("LTNM_TIMEOUT_MS", "0"))};

    // Launch the monitor.
    NetworkMonitor::NetworkMonitor<BoostWebsocketClient> monitor;
    auto error {monitor.Configure(config)};
    if (error != NetworkMonitorError::kOk) {
        return -1;
    }
    if (timeoutMs == 0) {
        monitor.Run();
    } else {
        monitor.Run(std::chrono::milliseconds(timeoutMs));
    }

    return monitor.GetLastErrorCode() == NetworkMonitorError::kOk ? 0 : -2;
} 