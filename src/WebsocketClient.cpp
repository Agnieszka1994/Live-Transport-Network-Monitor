#include "network-monitor/WebsocketClient.h"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/system/error_code.hpp>

#include <openssl/ssl.h>

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
    const std::string& endpoint,
    const std::string& port,
    boost::asio::io_context& ioc,
    boost::asio::ssl::context& ctx
) : url_ {url},
    endpoint_ {endpoint},
    port_ {port},
    resolver_ {boost::asio::make_strand(ioc)},
    ws_ {boost::asio::make_strand(ioc), ctx}
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
            tcp::resolver::iterator resolverIt
)
{
    if(ec){
        Log("OnResolve", ec);
        if(onConnect_) {
            onConnect_(ec);
        }
        return;
    }

    // The following timeout only matters for the purpose of connecting to the
    // TCP socket. We will reset the timeout to a sensible default after we are
    // connected.
    // Note: The TCP layer is the lowest layer (WebSocket -> TLS -> TCP).
    boost::beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(5));

    // Connect to the TCP socket.
    // Note: The TCP layer is the lowest layer (WebSocket -> TLS -> TCP).
    boost::beast::get_lowest_layer(ws_).async_connect(*resolverIt,
        [this](auto ec) {
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

    // Now that the TCP socket is connected, we can reset the timeout to
    // whatever Boost.Beast recommends.
    // Note: The TCP layer is the lowest layer (WebSocket -> TLS -> TCP).
    boost::beast::get_lowest_layer(ws_).expires_never();
    ws_.set_option(websocket::stream_base::timeout::suggested(
        boost::beast::role_type::client
    ));

    // Some clients require that we set the host name before the TLS handshake
    // or the connection will fail. We use an OpenSSL function for that.
    SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), url_.c_str());

    // Attempt a TLS handshake.
    // Note: The TLS layer is the next layer (WebSocket -> TLS -> TCP).
    ws_.next_layer().async_handshake(boost::asio::ssl::stream_base::client,
        [this](auto ec) {
            OnTlsHandshake(ec);
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

void WebsocketClient::OnTlsHandshake(
            const boost::system::error_code& ec
)
{
    if (ec) {
        Log("OnTlsHandshake", ec);
        if (onConnect_) {
            onConnect_(ec);
        }
        return;
    }

    // Attempt a WebSocket handshake.
    ws_.async_handshake(url_, endpoint_,
        [this](auto ec) {
            OnHandshake(ec);
        }
    );
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


