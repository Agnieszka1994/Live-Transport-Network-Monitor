#include <network-monitor/NetworkMonitor.h>

#include <boost/bimap.hpp>

#include <initializer_list>
#include <ostream>
#include <string>
#include <string_view>

using NetworkMonitor::NetworkMonitorError;

// Utility function to generate a boost::bimap.
template <typename L, typename R>
static boost::bimap<L, R> MakeBimap(
    std::initializer_list<typename boost::bimap<L, R>::value_type> list
)
{
    return boost::bimap<L, R>(list.begin(), list.end());
}

// NetworkMonitorError

static const auto gNetworkMonitorErrorStrings {
    MakeBimap<NetworkMonitorError, std::string_view>({
        {NetworkMonitorError::kOk                                ,
                              "Ok"                                },
        {NetworkMonitorError::kUndefinedError                    ,
                              "UndefinedError"                    },
        {NetworkMonitorError::kCouldNotConnectToStompClient      ,
                              "CouldNotConnectToStompClient"      },
        {NetworkMonitorError::kCouldNotParsePassengerEvent       ,
                              "CouldNotParsePassengerEvent"       },
        {NetworkMonitorError::kCouldNotParseQuietRouteRequest    ,
                              "CouldNotParseQuietRouteRequest"    },
        {NetworkMonitorError::kCouldNotRecordPassengerEvent      ,
                              "CouldNotRecordPassengerEvent"      },
        {NetworkMonitorError::kCouldNotStartStompServer          ,
                              "CouldNotStartStompServer"          },
        {NetworkMonitorError::kCouldNotSubscribeToPassengerEvents,
                              "CouldNotSubscribeToPassengerEvents"},
        {NetworkMonitorError::kFailedNetworkLayoutFileDownload   ,
                              "FailedNetworkLayoutFileDownload"   },
        {NetworkMonitorError::kFailedNetworkLayoutFileParsing    ,
                              "FailedNetworkLayoutFileParsing"    },
        {NetworkMonitorError::kFailedTransportNetworkConstruction,
                              "FailedTransportNetworkConstruction"},
        {NetworkMonitorError::kMissingCaCertFile                 ,
                              "MissingCaCertFile"                 },
        {NetworkMonitorError::kMissingNetworkLayoutFile          ,
                              "MissingNetworkLayoutFile"          },
        {NetworkMonitorError::kStompClientDisconnected           ,
                              "StompClientDisconnected"           },
        {NetworkMonitorError::kStompServerClientDisconnected     ,
                              "StompServerClientDisconnected"     },
        {NetworkMonitorError::kStompServerDisconnected           ,
                              "StompServerDisconnected"           },
    })
};

std::ostream& NetworkMonitor::operator<<(
    std::ostream& os,
    const NetworkMonitorError& error
)
{
    auto errorIt {gNetworkMonitorErrorStrings.left.find(error)};
    if (errorIt == gNetworkMonitorErrorStrings.left.end()) {
        os << gNetworkMonitorErrorStrings.left.at(
            NetworkMonitorError::kUndefinedError
        );
    }
    return os << errorIt->second;
}

std::string NetworkMonitor::ToString(
    const NetworkMonitorError& error
)
{
    static const auto undefinedError {std::string(
        gNetworkMonitorErrorStrings.left.at(NetworkMonitorError::kUndefinedError)
    )};
    auto errorIt {gNetworkMonitorErrorStrings.left.find(error)};
    if (errorIt == gNetworkMonitorErrorStrings.left.end()) {
        return undefinedError;
    }
    return std::string(errorIt->second);
}