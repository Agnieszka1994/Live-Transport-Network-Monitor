#include "WebsocketServerMock.h"

#include <network-monitor/StompFrame.h>
#include <network-monitor/WebsocketServer.h>

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

using NetworkMonitor::MockWebsocketEvent;
using NetworkMonitor::MockWebsocketServer;
using NetworkMonitor::MockWebsocketServerForStomp;
using NetworkMonitor::MockWebsocketSession;
using NetworkMonitor::StompFrame;

// Free functions

std::string NetworkMonitor::GetMockStompFrame(
    const std::string& host
)
{
    StompError error;
    StompFrame frame {
        error,
        StompCommand::kStomp,
        {
            {StompHeader::kAcceptVersion, "1.2"},
            {StompHeader::kHost, host},
        }
    };
    if (error != StompError::kOk) {
        throw std::runtime_error("Unexpected: Invalid mock STOMP frame: " +
                                 ToString(error));
    }
    return frame.ToString();
}

std::string NetworkMonitor::GetMockSendFrame(
    const std::string& id,
    const std::string& destination,
    const std::string& payload
)
{
    StompError error;
    StompFrame frame {
        error,
        StompCommand::kSend,
        {
            {StompHeader::kId, id},
            {StompHeader::kDestination, destination},
            {StompHeader::kContentType, "application/json"},
            {StompHeader::kContentLength, std::to_string(payload.size())},
        },
        payload
    };
    if (error != StompError::kOk) {
        throw std::runtime_error("Unexpected: Invalid mock STOMP frame: " +
                                 ToString(error));
    }
    return frame.ToString();
}

// MockWebsocketSession

// Static member variables definition.
boost::system::error_code MockWebsocketSession::sendEc = {};

MockWebsocketSession::MockWebsocketSession(
    boost::asio::io_context& ioc
) : context_ {boost::asio::make_strand(ioc)}
{
    // We don't need to save anything apart from the strand.
}

MockWebsocketSession::~MockWebsocketSession() = default;

void MockWebsocketSession::Send(
    const std::string& message,
    std::function<void (boost::system::error_code)> onSend
)
{
    spdlog::info("MockWebsocketSession::Send");
    if (onSend) {
        boost::asio::post(
            context_,
            [onSend]() {
                onSend(sendEc);
            }
        );
    }
}

void MockWebsocketSession::Close(
    std::function<void (boost::system::error_code)> onClose
)
{
    spdlog::info("MockWebsocketSession::Close");
    if (onClose) {
        boost::asio::post(
            context_,
            [onClose]() {
                onClose({});
            }
        );
    }
}

// MockWebsocketServer

// Static member variables definition.
bool MockWebsocketServer::triggerDisconnection = false;
boost::system::error_code MockWebsocketServer::runEc = {};
std::queue<MockWebsocketEvent> MockWebsocketServer::mockEvents = {};

MockWebsocketServer::MockWebsocketServer(
    const std::string& ip,
    const unsigned short port,
    boost::asio::io_context& ioc,
    boost::asio::ssl::context& ctx
) : ioc_(ioc), context_ {boost::asio::make_strand(ioc)}
{
    // We don't need to save anything apart from the strand.
}

MockWebsocketServer::~MockWebsocketServer() = default;

boost::system::error_code MockWebsocketServer::Run(
    MockWebsocketServer::Session::Handler onSessionConnect,
    MockWebsocketServer::Session::MsgHandler onSessionMessage,
    MockWebsocketServer::Session::Handler onSessionDisconnect,
    std::function<void (boost::system::error_code)> onDisconnect
)
{
    if (!runEc) {
        started_ = true;
        stopped_ = false;
        ListenToMockConnections(
            onSessionConnect,
            onSessionMessage,
            onSessionDisconnect,
            onDisconnect
        );
    }
    return runEc;
}

void MockWebsocketServer::Stop()
{
    stopped_ = true;
}

void MockWebsocketServer::ListenToMockConnections(
    MockWebsocketServer::Session::Handler onSessionConnect,
    MockWebsocketServer::Session::MsgHandler onSessionMessage,
    MockWebsocketServer::Session::Handler onSessionDisconnect,
    std::function<void (boost::system::error_code)> onDisconnect
)
{
    if (!started_ || stopped_ || triggerDisconnection) {
        triggerDisconnection = false;
        boost::asio::post(
            context_,
            [onDisconnect, stopped = stopped_]() {
                if (onDisconnect && !stopped) {
                    onDisconnect(boost::asio::error::operation_aborted);
                }
            }
        );
        return;
    }

    // Process one event.
    if (mockEvents.size() > 0) {
        auto event {mockEvents.front()};
        mockEvents.pop();
        switch (event.type) {
            case MockWebsocketEvent::Type::kConnect: {
                auto connection {std::make_shared<MockWebsocketSession>(ioc_)};
                connections_[event.id] = connection;
                if (onSessionConnect) {
                    boost::asio::post(
                        context_,
                        [onSessionConnect, ec = event.ec, connection]() {
                            onSessionConnect(ec, connection);
                        }
                    );
                }
                break;
            }
            case MockWebsocketEvent::Type::kMessage: {
                auto connectionIt {connections_.find(event.id)};
                if (connectionIt == connections_.end()){
                    throw std::runtime_error(
                        "MockWebsocketSession: Invalid connection " + event.id
                    );
                }
                const auto& connection {connectionIt->second};
                if (onSessionMessage) {
                    boost::asio::post(
                        context_,
                        [onSessionMessage, ec = event.ec, msg = event.message,
                         connection]() mutable {
                            onSessionMessage(ec, connection, std::move(msg));
                        }
                    );
                }
                break;
            }
            case MockWebsocketEvent::Type::kDisconnect: {
                auto connectionIt {connections_.find(event.id)};
                if (connectionIt == connections_.end()){
                    throw std::runtime_error(
                        "MockWebsocketSession: Invalid connection " + event.id
                    );
                }
                const auto& connection {connectionIt->second};
                if (onSessionDisconnect) {
                    boost::asio::post(
                        context_,
                        [onSessionDisconnect, ec = event.ec, connection]() {
                            onSessionDisconnect(ec, connection);
                        }
                    );
                }
                break;
            }
            default:
                throw std::runtime_error(
                    "MockWebsocketSession: Unknown mock event type"
                );
                break;
        }
    }

    // Recurse.
    boost::asio::post(
        context_,
        [
            this,
            onSessionConnect,
            onSessionMessage,
            onSessionDisconnect,
            onDisconnect
        ]() {
            ListenToMockConnections(
                onSessionConnect,
                onSessionMessage,
                onSessionDisconnect,
                onDisconnect
            );
        }
    );
} 