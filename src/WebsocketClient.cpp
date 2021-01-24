#include "WebsocketClient.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

using NetworkMonitor::WebsocketClient;

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

WebsocketClient::WebsocketClient(
    const std::string& url,
    const std::string& port,
    boost::asio::io_context& ioc
) : url_ {url},
    port_ {port},
    resolver_ {boost::asio::make_strand(ioc)},
    ws_ {boost::asio::make_strand(ioc)}
{
}

WebsocketClient::~WebsocketClient() = default;

void WebsocketClient::Connect(
    std::function<void (boost::system::error_code)> onConnect,
    std::function<void (boost::system::error_code,
                        std::string&&)> onMessage,
    std::function<void (boost::system::error_code)> onDisconnect
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

void WebsocketClient::Send(
        const std::string& message,
        std::function<void (boost::system::error_code)> onSend
)
{
    ws_.async_write(boost::asio::buffer(std::move(message)),
        [this, onSend](auto ec, auto) {
            if(onSend){
                onSend(ec);
            }
        }
    );
}

void WebsocketClient::Close(
        std::function<void (boost::system::error_code)> onClose
)
{
    ws_.async_close(websocket::close_code::none, 
        [this, onClose](auto ec){
            if(onClose){
                onClose(ec);
            }
        }
    );
}

// Private methods

void WebsocketClient::OnResolve(
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

void WebsocketClient::OnConnect(
            const boost::system::error_code& ec
)
{
    if(ec){
        Log("OnConnect", ec);
        if(onConnect_) {
            onConnect_(ec);
        }
        return;
    }

    // the TCP socket is connected, we can reset the timeout to whatever Boost.Beast recommends
    ws_.next_layer().expires_never();
    ws_.set_option(websocket::stream_base::timeout::suggested(
        boost::beast::role_type::client
    ));

    // Attempt a Websocket Handshake
    ws_.async_handshake(url_, "/",
        [this](auto ec){
            OnHandshake(ec);
        }
    );
}

void WebsocketClient::OnHandshake(
            const boost::system::error_code& ec
)
{
    if(ec){
        Log("OnHandshake", ec);
        if(onConnect_) {
            onConnect_(ec);
        }
        return;
    }

    // Tell the Websocket to exchange messages in text format
    ws_.text(true);

    // Set up a recursive asynchronous listener to receive messages
    ListenToIncomingMessage(ec);

    // Dispathc the user callback
    // This call is synchronous and will block the Websocket strand
    if(onConnect_) {
        onConnect_(ec);
    }
}

void WebsocketClient::ListenToIncomingMessage(
            const boost::system::error_code& ec
)
{
    // Stop processing messages if the connection has been aborted 
    if(ec == boost::asio::error::operation_aborted){
        if(onDisconnect_) {
            onDisconnect_(ec);
        }
        return;
    }

    // Read a message asynchronously. On a syccessful read, process the message
    // and recursively call this function again to process the next message
    ws_.async_read(rBuffer_,
        [this](auto ec, auto nBytes) {
            OnRead(ec, nBytes);
            ListenToIncomingMessage(ec);
        }
    );
}

void WebsocketClient::OnRead(
            const boost::system::error_code& ec,
            size_t nBytes
)
{
    // Ignore messages that failed to read
    if (ec) {
        return;
    }

    // Parse the message and forward it to the user callback
    // This call is synchronous and will block the Websocket strand
    std::string message {boost::beast::buffers_to_string(rBuffer_.data())};
    rBuffer_.consume(nBytes);
    if (onMessage_) {
        onMessage_(ec, std::move(message));
    }
}


