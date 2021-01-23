#include "WebsocketClient.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

using NetworkMonitor::WebSocketClient;

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

// Static functions
static void Log(const std::string& where, boost::system::error_code ec)
{
    std::cerr << "[" << std::setw(20) << where << "] "
              << (ec ? "Error: " : "OK")
              << (ec ? ec.message() : "")
              << std::endl;
}

WebSocketClient::WebSocketClient(
    const std::string& url,
    const std::string& port,
    boost::asio::io_context& ioc
) : url_ {url},
    port_ {port},
    resolver_ {boost::asio::make_strand(ioc)},
    ws_ {boost::asio::make_strand(ioc)}
{
}

WebSocketClient::~WebSocketClient() = default;

void WebSocketClient::Connect(
    std::function<void (boost::system::error_code)> onConnect = nullptr,
    std::function<void (boost::system::error_code,
                        std::string&&)> onMessage = nullptr,
    std::function<void (boost::system::error_code)> onDisconnect = nullptr
)
{
    // Save the user callbacks for later use 
    // TODO

}

void WebSocketClient::Send(
        const std::string& message,
        std::function<void (boost::system::error_code)> onSend = nullptr
)
{
    // TODO
}

void WebSocketClient::Close(
        std::function<void (boost::system::error_code)> onClose = nullptr
)
{
    // TODO
}

// Private methods

void WebSocketClient::OnResolve(
            const boost::system::error_code& ec,
            boost::asio::ip::tcp::resolver::iterator endpoint
)
{
    // TODO
}

void WebSocketClient::OnConnect(
            const boost::system::error_code& ec
)
{
    // TODO
}

void WebSocketClient::OnHandshake(
            const boost::system::error_code& ec
)
{
    // TODO
}

void WebSocketClient::ListenToIncomingMessage(
            const boost::system::error_code& ec
)
{
    // TODO
}

void WebSocketClient::OnRead(
            const boost::system::error_code& ec,
            size_t nBytes
)
{
    // TODO 
}


