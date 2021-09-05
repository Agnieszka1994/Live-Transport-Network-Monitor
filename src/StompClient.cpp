#include <network-monitor/StompClient.h>

#include <boost/bimap.hpp>
#include <initializer_list>
#include <ostream>
#include <string>
#include <string_view>

using NetworkMonitor::StompClientError;

// Utility function to generate a boost::bimap.
template <typename L, typename R>
static boost::bimap<L, R> MakeBimap(
    std::initializer_list<typename boost::bimap<L, R>::value_type> list
)
{
    return boost::bimap<L, R>(list.begin(), list.end());
}

// StompClientError

static const auto gStompClientErrorStrings {
    MakeBimap<StompClientError, std::string_view>({
        {StompClientError::kOk                                , "Ok"                                },
        {StompClientError::kUndefinedError                    , "UndefinedError"                    },
        {StompClientError::kCouldNotCloseWebsocketConnection  , "CouldNotCloseWebsocketConnection"  },
        {StompClientError::kCouldNotConnectToWebsocketServer  , "CouldNotConnectToWebsocketServer"  },
        {StompClientError::kCouldNotParseMessageAsStompFrame  , "CouldNotParseMessageAsStompFrame"  },
        {StompClientError::kCouldNotSendStompFrame            , "CouldNotSendStompFrame"            },
        {StompClientError::kCouldNotSendSubscribeFrame        , "CouldNotSendSubscribeFrame"        },
        {StompClientError::kUnexpectedCouldNotCreateValidFrame, "UnexpectedCouldNotCreateValidFrame"},
        {StompClientError::kUnexpectedMessageContentType      , "UnexpectedMessageContentType"      },
        {StompClientError::kUnexpectedSubscriptionMismatch    , "UnexpectedSubscriptionMismatch"    },
        {StompClientError::kWebsocketServerDisconnected       , "WebsocketServerDisconnected"       },
    })
};

std::ostream& NetworkMonitor::operator<<(
    std::ostream& os,
    const StompClientError& error
)
{
    auto errorIt {gStompClientErrorStrings.left.find(error)};
    if (errorIt == gStompClientErrorStrings.left.end()) {
        os << gStompClientErrorStrings.left.at(
            StompClientError::kUndefinedError
        );
    } else {
        os << errorIt->second;
    }
    return os;
}

std::string NetworkMonitor::ToString(const StompClientError& error)
{
    static const auto undefinedError {std::string(
        gStompClientErrorStrings.left.at(StompClientError::kUndefinedError)
    )};
    auto errorIt {gStompClientErrorStrings.left.find(error)};
    if (errorIt == gStompClientErrorStrings.left.end()) {
        return undefinedError;
    }
    return std::string(errorIt->second);
}