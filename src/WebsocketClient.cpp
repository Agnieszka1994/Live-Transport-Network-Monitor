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
    onConnect_ = onConnect;
    onMessage_ = onMessage;
    onDisconnect_ = onDisconnect;

    // Start the chain of asynchronous callbacks

    resolver_.async_resolve(url_, port_,
        [this](auto ec, auto endpoint){
            OnResolve(ec, endpoint);
        }
    );

}

void WebSocketClient::Send(
        const std::string& message,
        std::function<void (boost::system::error_code)> onSend = nullptr
)
{
    ws_.async_write(boost::asio::buffer(std::move(message)),
        [this](auto ec, auto) {
            if(onSend){
                onSend(ec);
            }
        });
}

void WebSocketClient::Close(
        std::function<void (boost::system::error_code)> onClose = nullptr
)
{
    ws_.async_close(websocket::close_code::none, [this, onClose](auto ec){
        if(onClose){
            onClose(ec);
        }
    });
}

// Private methods

void WebSocketClient::OnResolve(
            const boost::system::error_code& ec,
            boost::asio::ip::tcp::resolver::iterator endpoint
)
{
    if(ec){
        Log("OnResolve", ec);
        if(onConnect_) {
            onConnect_(ec);
        }
        return;
    }

    // Timeout for the purpose of connecting to the TCP socket
    ws_.next_layer().expires_after(std::chrono::seconds(5));

    // Connect to the TCP socket
    // The socket is embedded in ws_, and we access it through next_layer()
    ws_.next_layer().async_connect(*endpoint,
        [this](auto ec){
            OnConnect(ec);
        }
    );
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


