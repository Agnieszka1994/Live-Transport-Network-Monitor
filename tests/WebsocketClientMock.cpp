#include "WebsocketClientMock.h"
#include <network-monitor/StompFrame.h>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include <functional>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

using NetworkMonitor::StompFrame;
using NetworkMonitor::MockWebsocketClient;
using NetworkMonitor::MockWebsocketClientForStomp;

// MockWebsocketClient

// Static member variables definition.
boost::system::error_code MockWebsocketClient::connectEc = {};
boost::system::error_code MockWebsocketClient::sendEc = {};
boost::system::error_code MockWebsocketClient::closeEc = {};
bool MockWebsocketClient::triggerDisconnection = false;
std::queue<std::string> MockWebsocketClient::messageQueue = {};
std::function<
    void (const std::string&)
> MockWebsocketClient::respondToSend = [](auto msg) {
    return false;
};

MockWebsocketClient::MockWebsocketClient(
    const std::string& url,
    const std::string& endpoint,
    const std::string& port,
    boost::asio::io_context& ioc,
    boost::asio::ssl::context& ctx
) : context_ {boost::asio::make_strand(ioc)}
{
    // No need to save anything apart from the strand.
}

MockWebsocketClient::~MockWebsocketClient() = default;

void MockWebsocketClient::Connect(
    std::function<void (boost::system::error_code)> onConnect,
    std::function<void (boost::system::error_code,
                        std::string&&)> onMessage,
    std::function<void (boost::system::error_code)> onDisconnect
)
{
    if (connectEc) {
        // Mock an error.
        boost::asio::post(
            context_,
            [this, onConnect]() {
                connected_ = false;
                if (onConnect) {
                    onConnect(connectEc);
                }
            }
        );
    } else {
        // Mock a successful connect.
        boost::asio::post(
            context_,
            [this, onConnect]() {
                connected_ = true;
                if (onConnect) {
                    onConnect(connectEc);
                }
            }
        );
        boost::asio::post(
            context_,
            [this, onMessage, onDisconnect]() {
                MockIncomingMessages(onMessage, onDisconnect);
            }
        );
    }
}

void MockWebsocketClient::Send(
    const std::string& message,
    std::function<void (boost::system::error_code)> onSend
)
{
    // Mock a send callback.
    if (connected_) {
        boost::asio::post(
            context_,
            [this, onSend, message]() {
                if (onSend) {
                    onSend(sendEc);
                    respondToSend(message);
                }
            }
        );
    } else {
        boost::asio::post(
            context_,
            [onSend]() {
                if (onSend) {
                    onSend(boost::asio::error::operation_aborted);
                }
            }
        );
    }
}

void MockWebsocketClient::Close(
    std::function<void (boost::system::error_code)> onClose
)
{
    // Mock a close callback.
    if (connected_) {
        boost::asio::post(
            context_,
            [this, onClose]() {
                connected_ = false;
                closed_ = true;
                triggerDisconnection = true;
                if (onClose) {
                    onClose(closeEc);
                }
            }
        );
    } else {
        boost::asio::post(
            context_,
            [onClose]() {
                if (onClose) {
                    onClose(boost::asio::error::operation_aborted);
                }
            }
        );
    }
}

// Private methods

void MockWebsocketClient::MockIncomingMessages(
    std::function<void (boost::system::error_code,
                        std::string&&)> onMessage,
    std::function<void (boost::system::error_code)> onDisconnect
)
{
    if (!connected_ || triggerDisconnection) {
        triggerDisconnection = false;
        boost::asio::post(
            context_,
            [onDisconnect, closed = closed_]() {
                if (onDisconnect && !closed) {
                    onDisconnect(boost::asio::error::operation_aborted);
                }
            }
        );
        return;
    }

    // Recurse.
    boost::asio::post(
        context_,
        [this, onMessage, onDisconnect]() {
            if (!messageQueue.empty()) {
                auto message {messageQueue.front()};
                messageQueue.pop();
                if (onMessage) {
                    onMessage({}, std::move(message));
                }
            }
            MockIncomingMessages(onMessage, onDisconnect);
        }
    );
}

// MockWebsocketClientForStomp

// Static member variables definition.
std::string MockWebsocketClientForStomp::endpoint = "";
std::string MockWebsocketClientForStomp::username = "";
std::string MockWebsocketClientForStomp::password = "";
std::vector<std::string> MockWebsocketClientForStomp::subscriptionMessages = {};

// Public methods

MockWebsocketClientForStomp::MockWebsocketClientForStomp(
    const std::string& url,
    const std::string& endpoint,
    const std::string& port,
    boost::asio::io_context& ioc,
    boost::asio::ssl::context& ctx
) : MockWebsocketClient(url, endpoint, port, ioc, ctx)
{
    // Mock the responses a STOMP server would send in reaction to the client
    // messages.
    respondToSend = [this](auto msg) {
        OnMessage(msg);
    };
}

std::string MockWebsocketClientForStomp::GetMockSendFrame(
    const std::string& destination,
    const std::string& messageContent
)
{
    static const long long int counter {0};

    auto messageSize {messageContent.size()};

    StompError error;
    StompFrame frame {
        error,
        StompCommand::kSend,
        {
            {StompHeader::kId, std::to_string(counter)},
            {StompHeader::kDestination, destination},
            {StompHeader::kContentType, "application/json"},
            {StompHeader::kContentLength, std::to_string(messageSize)},
        },
        messageContent
    };
    if (error != StompError::kOk) {
        throw std::runtime_error("Unexpected: Invalid mock STOMP frame: " +
                                 ToString(error));
    }
    return frame.ToString();
}

// Private methods

StompFrame MockWebsocketClientForStomp::MakeConnectedFrame()
{
    StompError error;
    StompFrame frame {
        error,
        StompCommand::kConnected,
        {
            {StompHeader::kVersion, "1.2"},
            {StompHeader::kSession, "42"},
        }
    };
    if (error != StompError::kOk) {
        throw std::runtime_error("Unexpected: Invalid mock STOMP frame: " +
                                 ToString(error));
    }
    return frame;
}

StompFrame MockWebsocketClientForStomp::MakeReceiptFrame(
    const std::string& id
)
{
    StompError error;
    StompFrame frame {
        error,
        StompCommand::kReceipt,
        {
            {StompHeader::kReceiptId, id},
        }
    };
    if (error != StompError::kOk) {
        throw std::runtime_error("Unexpected: Invalid mock STOMP frame: " +
                                 ToString(error));
    }
    return frame;
}

StompFrame MockWebsocketClientForStomp::MakeErrorFrame(
    const std::string& msg
)
{
    StompError error;
    StompFrame frame {
        error,
        StompCommand::kError,
        {
            {StompHeader::kVersion, "1.2"},
            {StompHeader::kContentLength, std::to_string(msg.size())},
            {StompHeader::kContentType, "text/plain"},
        },
        msg
    };
    if (error != StompError::kOk) {
        throw std::runtime_error("Unexpected: Invalid mock STOMP frame: " +
                                 ToString(error));
    }
    return frame;
}

StompFrame MockWebsocketClientForStomp::MakeMessageFrame(
    const std::string& destination,
    const std::string& subscriptionId,
    const std::string& message
)
{
    static const long long int counter {0};

    StompError error;
    StompFrame frame {
        error,
        StompCommand::kMessage,
        {
            {StompHeader::kSubscription, subscriptionId},
            {StompHeader::kMessageId, std::to_string(counter)},
            {StompHeader::kDestination, destination},
            {StompHeader::kContentLength,
             std::to_string(message.size())},
            {StompHeader::kContentType, "application/json"},
        },
        message
    };
    if (error != StompError::kOk) {
        throw std::runtime_error("Unexpected: Invalid mock STOMP frame: " +
                                 ToString(error));
    }
    return frame;
}

bool MockWebsocketClientForStomp::CheckConnection(
    const StompFrame& frame
)
{
    bool ok {true};
    ok &= frame.HasHeader(StompHeader::kLogin);
    ok &= frame.HasHeader(StompHeader::kPasscode);
    if (!ok) {
        return false;
    }
    bool checkAuth {
        frame.GetHeaderValue(StompHeader::kLogin) == username
            &&
        frame.GetHeaderValue(StompHeader::kPasscode) == password
    };
    return checkAuth;
}

std::pair<
    std::string,
    std::string
> MockWebsocketClientForStomp::CheckSubscription(
    const StompFrame& frame
)
{
    bool ok {true};
    ok &= frame.GetHeaderValue(StompHeader::kDestination) == endpoint;
    if (!ok) {
        return {"", ""};
    }
    return {
        std::string(frame.GetHeaderValue(StompHeader::kReceipt)),
        std::string(frame.GetHeaderValue(StompHeader::kId)),
    };
}

void MockWebsocketClientForStomp::OnMessage(const std::string& msg)
{
    StompError error;
    StompFrame frame {error, msg};
    if (error != StompError::kOk) {
        triggerDisconnection = true;
        return;
    }
    spdlog::info("MockStompServer: OnMessage: {}", frame.GetCommand());
    switch (frame.GetCommand()) {
        case StompCommand::kStomp:
        case StompCommand::kConnect: {
            if (CheckConnection(frame)) {
                spdlog::info("MockStompServer: OnMessage: Connected");
                messageQueue.push(MakeConnectedFrame().ToString());
            } else {
                spdlog::info("MockStompServer: OnMessage: Error: Connect");
                messageQueue.push(MakeErrorFrame("Connect").ToString());
                triggerDisconnection = true;
            }
            break;
        }
        case StompCommand::kSubscribe: {
            auto [receiptId, subscriptionId] = CheckSubscription(frame);
            if (subscriptionId != "") {
                if (receiptId != "") {
                    spdlog::info("MockStompServer: OnMessage: Send receipt");
                    messageQueue.push(MakeReceiptFrame(receiptId).ToString());
                }
                spdlog::info(
                    "MockStompServer: OnMessage: About to send {} "
                    "subscription messages",
                    subscriptionMessages.size()
                );
                for (const auto& message: subscriptionMessages) {
                    messageQueue.push(MakeMessageFrame(
                        endpoint,
                        subscriptionId,
                        message
                    ).ToString());
                }
            } else {
                spdlog::info("MockStompServer: OnMessage: Error: Subscribe");
                messageQueue.push(MakeErrorFrame("Subscribe").ToString());
                triggerDisconnection = true;
            }
            break;
        }
        default: {
            break;
        }
    }
}