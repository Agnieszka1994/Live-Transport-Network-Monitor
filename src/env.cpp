#include <network-monitor/env.h>

#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>

std::string NetworkMonitor::GetEnvVar(
    const std::string& envVar,
    const std::optional<std::string>& defaultValue
)
{
    const char* value {std::getenv(envVar.c_str())};
    if (value == nullptr && defaultValue == std::nullopt) {
        throw std::runtime_error("Could not find environment variable: " +
                                 envVar);
    }
    return value != nullptr ? value : *defaultValue;
}