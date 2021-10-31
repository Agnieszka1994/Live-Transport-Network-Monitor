#ifndef NETWORK_MONITOR_ENV_H
#define NETWORK_MONITOR_ENV_H

#include <optional>
#include <string>

namespace NetworkMonitor {

/*! \brief Get an environment variable, or return a default value.
 *
 *  \throws std::runtime_error if the environment variable cannot be found.
 */
std::string GetEnvVar(
    const std::string& envVar,
    const std::optional<std::string>& defaultValue = std::nullopt
);

} // namespace NetworkMonitor

#endif // NETWORK_MONITOR_ENV_H