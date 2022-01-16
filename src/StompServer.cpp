#include <network-monitor/StompServer.h>

#include <boost/bimap.hpp>

#include <initializer_list>
#include <ostream>
#include <string>
#include <string_view>

using NetworkMonitor::StompServerError;

// Utility function to generate a boost::bimap.
template <typename L, typename R>
static boost::bimap<L, R> MakeBimap(
    std::initializer_list<typename boost::bimap<L, R>::value_type> list
)
{
    return boost::bimap<L, R>(list.begin(), list.end());
}

// StompServerError

static const auto gStompServerErrorStrings {
    MakeBimap<StompServerError, std::string_view>({
        {StompServerError::kOk                                ,
                           "Ok"                                },
        {StompServerError::kUndefinedError                    ,
                           "UndefinedError"                    },
        {StompServerError::kClientCannotReconnect             ,
                           "ClientCannotReconnect"             },
        {StompServerError::kCouldNotCloseClientConnection     ,
                           "CouldNotCloseClientConnection"     },
        {StompServerError::kCouldNotParseFrame                ,
                           "CouldNotParseFrame"                },
        {StompServerError::kCouldNotSendMessage               ,
                           "CouldNotSendMessage"               },
        {StompServerError::kCouldNotStartWebsocketServer      ,
                           "CouldNotStartWebsocketServer"      },
        {StompServerError::kInvalidHeaderValueAcceptVersion   ,
                           "InvalidHeaderValueAcceptVersion"   },
        {StompServerError::kInvalidHeaderValueHost            ,
                           "InvalidHeaderValueHost"            },
        {StompServerError::kUnsupportedFrame                  ,
                           "UnsupportedFrame"                  },
        {StompServerError::kWebsocketSessionDisconnected      ,
                           "WebsocketSessionDisconnected"      },
        {StompServerError::kWebsocketServerDisconnected       ,
                           "WebsocketServerDisconnected"       },
    })
};

std::ostream& NetworkMonitor::operator<<(
    std::ostream& os,
    const StompServerError& error
)
{
    auto errorIt {gStompServerErrorStrings.left.find(error)};
    if (errorIt == gStompServerErrorStrings.left.end()) {
        os << gStompServerErrorStrings.left.at(
            StompServerError::kUndefinedError
        );
    } else {
        os << errorIt->second;
    }
    return os;
}

std::string NetworkMonitor::ToString(const StompServerError& error)
{
    static const auto undefinedError {std::string(
        gStompServerErrorStrings.left.at(StompServerError::kUndefinedError)
    )};
    auto errorIt {gStompServerErrorStrings.left.find(error)};
    if (errorIt == gStompServerErrorStrings.left.end()) {
        return undefinedError;
    }
    return std::string(errorIt->second);
}