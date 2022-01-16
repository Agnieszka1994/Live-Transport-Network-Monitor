#include <network-monitor/env.h>
#include <network-monitor/NetworkMonitor.h>
#include <network-monitor/WebsocketClient.h>
#include <network-monitor/WebsocketServer.h>

#include <chrono>
#include <string>

using NetworkMonitor::BoostWebsocketClient;
using NetworkMonitor::GetEnvVar;
using NetworkMonitor::NetworkMonitorError;
using NetworkMonitor::NetworkMonitorConfig;
using NetworkMonitor::BoostWebsocketServer;

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
        "127.0.0.1", // Use the IP as the server hostname because the client
                     // will connect to 127.0.0.1 directly, without host name
                     // resolution.
        "127.0.0.1",
        8042,
        0.1,
        0.1,
        20,
    };

    // Optional run timeout
    // Default: Oms = run indefinitely
    auto timeoutMs {std::stoi(GetEnvVar("LTNM_TIMEOUT_MS", "0"))};

    // Launch the monitor.
    NetworkMonitor::NetworkMonitor<
        BoostWebsocketClient,
        BoostWebsocketServer
    > monitor {};
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