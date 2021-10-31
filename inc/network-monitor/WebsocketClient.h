#ifndef NETWORK_MONITOR_WEBSOCKET_CLIENT_H
#define NETWORK_MONITOR_WEBSOCKET_CLIENT_H

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/system/error_code.hpp>

#include <openssl/ssl.h>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>

namespace NetworkMonitor {

/*! \brief Client to connect to a Websocket server over TLS.
 *
 *  \tparam Resolver        The class to resolve the URL to an IP address. It
 *                          must support the same interface of
 *                          boost::asio::ip::tcp::resolver.
 *  \tparam WebsocketStream The Websocket stream class. It must support the
 *                          same interface of boost::beast::websocket::stream.
 */
template <
    typename Resolver,
    typename WebsocketStream
>
class WebsocketClient {
public:
    /*! \brief Construct a Websocket client.
     *
     *  \note This constructor does not initiate a connection.
     *
     *  \param url      The URL of the server.
     *  \param endpoint The endpoint on the server to connect to.
     *                  Example: ltnm.learncppthroughprojects.com/<endpoint>
     *  \param port     The port on the server.
     *  \param ioc      The io_context object. The user takes care of calling
     *                  ioc.run().
     *  \param ctx      The TLS context to setup a TLS socket stream.
     */
    WebsocketClient(
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
        spdlog::info("WebsocketClient: New client for {}:{}{}",
                     url_, port_, endpoint_);
    }

    /*! \brief Destructor.
     */
    ~WebsocketClient() = default;

    /*! \brief Connect to the server.
     *
     *  \param onConnect     Called when the connection fails or succeeds.
     *  \param onMessage     Called only when a message is successfully
     *                       received. The message is an rvalue reference;
     *                       ownership is passed to the receiver.
     *  \param onDisconnect  Called when the connection is closed by the server
     *                       or due to a connection error.
     */
    void Connect(
        std::function<void (boost::system::error_code)> onConnect = nullptr,
        std::function<void (boost::system::error_code,
                            std::string&&)> onMessage = nullptr,
        std::function<void (boost::system::error_code)> onDisconnect = nullptr
    )
    {
        // Save the user callbacks for later use.
        onConnect_ = onConnect;
        onMessage_ = onMessage;
        onDisconnect_ = onDisconnect;

        // Start the chain of asynchronous callbacks.
        closed_ = false;
        spdlog::info("WebsocketClient: Attempting to resolve {}:{}",
                     url_, port_);
        resolver_.async_resolve(url_, port_,
            [this](auto ec, auto resolverIt) {
                OnResolve(ec, resolverIt);
            }
        );
    }

    /*! \brief Send a text message to the Websocket server.
     *
     *  \param message The message to send. The caller must ensure that this
     *                 string stays in scope until the onSend handler is called.
     *  \param onSend  Called when a message is sent successfully or if it
     *                 failed to send.
     */
    void Send(
        const std::string& message,
        std::function<void (boost::system::error_code)> onSend = nullptr
    )
    {
        spdlog::info("WebsocketClient: Sending message");
        ws_.async_write(boost::asio::buffer(message),
            [onSend](auto ec, auto) {
                if (onSend) {
                    onSend(ec);
                }
            }
        );
    }

    /*! \brief Close the Websocket connection.
     *
     *  \param onClose Called when the connection is closed, successfully or
     *                 not.
     */
    void Close(
        std::function<void (boost::system::error_code)> onClose = nullptr
    )
    {
        spdlog::info("WebsocketClient: Closing connection");
        closed_ = true;
        ws_.async_close(
            boost::beast::websocket::close_code::none,
            [onClose](auto ec) {
                if (onClose) {
                    onClose(ec);
                }
            }
        );
    }

private:
    std::string url_ {};
    std::string endpoint_ {};
    std::string port_ {};

    // We leave these uninitialized because they do not support a default
    // constructor.
    Resolver resolver_;
    WebsocketStream ws_;

    boost::beast::flat_buffer rBuffer_ {};

    bool closed_ {true};

    std::function<void (boost::system::error_code)> onConnect_ {nullptr};
    std::function<void (boost::system::error_code,
                        std::string&&)> onMessage_ {nullptr};
    std::function<void (boost::system::error_code)> onDisconnect_ {nullptr};

    void OnResolve(
        const boost::system::error_code& ec,
        boost::asio::ip::tcp::resolver::iterator resolverIt
    )
    {
        if (ec) {
            spdlog::error("WebsocketClient: Could not resolve server URL: {}",
                          ec.message());
            if (onConnect_) {
                onConnect_(ec);
            }
            return;
        }
        spdlog::info("WebsocketClient: Server URL resolved: {}",
                     resolverIt->endpoint().address().to_string());

        // The following timeout only matters for the purpose of connecting to
        // the TCP socket. We will reset the timeout to a sensible default
        // after we are connected.
        // Note: The TCP layer is the lowest layer (Websocket -> TLS -> TCP).
        boost::beast::get_lowest_layer(ws_).expires_after(
            std::chrono::seconds(5)
        );

        // Connect to the TCP socket.
        // Note: The TCP layer is the lowest layer (Websocket -> TLS -> TCP).
        spdlog::info("WebsocketClient: Attempting connection to server");
        boost::beast::get_lowest_layer(ws_).async_connect(*resolverIt,
            [this](auto ec) {
                OnConnect(ec);
            }
        );
    }

    void OnConnect(
        const boost::system::error_code& ec
    )
    {
        if (ec) {
            spdlog::error("WebsocketClient: Could not connect to server: {}",
                          ec.message());
            if (onConnect_) {
                onConnect_(ec);
            }
            return;
        }
        spdlog::info("WebsocketClient: Connected to server");

        // Now that the TCP socket is connected, we can reset the timeout to
        // whatever Boost.Beast recommends.
        // Note: The TCP layer is the lowest layer (Websocket -> TLS -> TCP).
        boost::beast::get_lowest_layer(ws_).expires_never();
        ws_.set_option(
            boost::beast::websocket::stream_base::timeout::suggested(
                boost::beast::role_type::client
            )
        );

        // Some clients require that we set the host name before the TLS
        // handshake or the connection will fail. We use an OpenSSL function
        // for that.
        SSL_set_tlsext_host_name(
            ws_.next_layer().native_handle(),
            url_.c_str()
        );

        // Attempt a TLS handshake.
        // Note: The TLS layer is the next layer (Websocket -> TLS -> TCP).
        spdlog::info("WebsocketClient: Wait for TLS handshake");
        ws_.next_layer().async_handshake(boost::asio::ssl::stream_base::client,
            [this](auto ec) {
                OnTlsHandshake(ec);
            }
        );
    }

    void OnTlsHandshake(
        const boost::system::error_code& ec
    )
    {
        if (ec) {
            spdlog::error(
                "WebsocketClient: Could not complete TLS handshake: {}",
                ec.message()
            );
            if (onConnect_) {
                onConnect_(ec);
            }
            return;
        }
        spdlog::info("WebsocketClient: TLS handshake completed");

        // Attempt a Websocket handshake.
        spdlog::info("WebsocketClient: Wait for Websocket handshake");
        ws_.async_handshake(url_, endpoint_,
            [this](auto ec) {
                OnHandshake(ec);
            }
        );
    }

    void OnHandshake(
        const boost::system::error_code& ec
    )
    {
        if (ec) {
            spdlog::error(
                "WebsocketClient: Could not complete Websocket handshake: {}",
                ec.message()
            );
            if (onConnect_) {
                onConnect_(ec);
            }
            return;
        }
        spdlog::info("WebsocketClient: Websocket handshake completed");

        // Tell the Websocket object to exchange messages in text format.
        ws_.text(true);

        // Now that we are connected, set up a recursive asynchronous listener
        // to receive messages.
        spdlog::info("WebsocketClient: Listening to incoming messages");
        ListenToIncomingMessage(ec);

        // Dispatch the user callback.
        // Note: This call is synchronous and will block the Websocket strand.
        if (onConnect_) {
            onConnect_(ec);
        }
    }

    void ListenToIncomingMessage(
        const boost::system::error_code& ec
    )
    {
        // Stop processing messages if the connection has been aborted.
        if (ec == boost::asio::error::operation_aborted) {
            spdlog::info(
                "WebsocketClient: Stopped listening to incoming messages"
            );
            if (onDisconnect_ && !closed_) {
                onDisconnect_(ec);
            }
            return;
        }

        // Read a message asynchronously. On a successful read, process the
        // message and recursively call this function again to process the next
        // message.
        ws_.async_read(rBuffer_,
            [this](auto ec, auto nBytes) {
                OnRead(ec, nBytes);
                ListenToIncomingMessage(ec);
            }
        );
    }

    void OnRead(
        const boost::system::error_code& ec,
        size_t nBytes
    )
    {
        // We just ignore messages that failed to read.
        if (ec) {
            return;
        }
        spdlog::debug("WebsocketClient: Received {}-byte message", nBytes);

        // Parse the message and forward it to the user callback.
        // Note: This call is synchronous and will block the Websocket strand.
        std::string message {boost::beast::buffers_to_string(rBuffer_.data())};
        rBuffer_.consume(nBytes);
        if (onMessage_) {
            onMessage_(ec, std::move(message));
        }
    }
};

using BoostWebsocketClient = WebsocketClient<
    boost::asio::ip::tcp::resolver,
    boost::beast::websocket::stream<
        boost::beast::ssl_stream<boost::beast::tcp_stream>
    >
>;

} // namespace NetworkMonitor

#endif // NETWORK_MONITOR_WEBSOCKET_CLIENT_H