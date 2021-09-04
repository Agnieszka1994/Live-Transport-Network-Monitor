#include <network-monitor/StompFrame.h>
#include <boost/bimap.hpp>
#include <charconv>
#include <initializer_list>
#include <ostream>
#include <string_view>
#include <string_view>
#include <system_error>
#include <utility>

using NetworkMonitor::StompCommand;
using NetworkMonitor::StompError;
using NetworkMonitor::StompFrame;
using NetworkMonitor::StompHeader;

using Headers = StompFrame::Headers;

// Generate a boost::bimap.
template <typename L, typename R>
static boost::bimap<L, R> MakeBimap(
    std::initializer_list<typename boost::bimap<L, R>::value_type> list
)
{
    return boost::bimap<L, R>(list.begin(), list.end());
}

// Convert a std::string_view to a number.
// Returns false if the conversion failed.
static bool StoI(const std::string_view string, size_t& result)
{
    auto conversionResult {std::from_chars(
        string.data(),
        string.data() + string.size(),
        result
    )};
    return conversionResult.ec != std::errc::invalid_argument;
}

// Stomp Command - TO DO 