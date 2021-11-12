#ifndef NETWORK_MONITOR_WEBSOCKET_SERVER_H
#define NETWORK_MONITOR_WEBSOCKET_SERVER_H

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
#include <iostream>
#include <memory>
#include <string>

namespace NetworkMonitor {


/*! \brief Object representing a single connection to a Websocket client.
 *
 *  \tparam WebsocketStream The Websocket stream class. It must support the
 *                          same interface of boost::beast::websocket::stream.
 */
template <
    typename WebsocketStream
>
class WebsocketSession: public std::enable_shared_from_this<
    WebsocketSession<WebsocketStream>
> {

public:
    /*! \brief Callback type for the session, with an attached error code.
     */
    using Handler = std::function<
        void (
            boost::system::error_code,
            std::shared_ptr<WebsocketSession<WebsocketStream>>)>;

    /*! \brief Message callback type for the session, with an attached error
     *         code and message.
     */
    using MsgHandler = std::function<
        void (
            boost::system::error_code,
            std::shared_ptr<WebsocketSession<WebsocketStream>>,
            std::string&&
        )
    >;

    /*! \brief Construct a Websocket session.
     *
     *  \note This constructor does not initiate a Websocket connection, but it
     *        expects the socket to be already connected.
     *
     *  \note This constructor is mostly used by the WebsocketServer class.
     *
     *  \param socket   A socket object with an active TCP connection.
     *  \param ctx      The TLS context to setup a TLS socket stream.
     */
    WebsocketSession(
        boost::asio::ip::tcp::socket&& socket,
        boost::asio::ssl::context& ctx
    ) : ws_ {std::move(socket), ctx}
    {
    }

    /*! \brief Destructor
     */
    ~WebsocketSession() = default;

    /*! \brief Send a text message to the connected Websocket client.
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
        auto self {this->shared_from_this()};
        spdlog::info("WebsocketSession: [{}] Sending message", self);
        ws_.async_write(boost::asio::buffer(message),
            [onSend](auto ec, auto) {
                if (onSend) {
                    onSend(ec);
                }
        });
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
        auto self {this->shared_from_this()};
        spdlog::info("WebsocketSession: [{}] Closing session", self);
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
    template <typename A, typename W> friend class WebsocketServer;

    // We leave this uninitialized because it does not support a default
    // constructor.
    WebsocketStream ws_;

    Handler onConnect_ {nullptr};
    MsgHandler onMessage_ {nullptr};
    Handler onDisconnect_ {nullptr};

    boost::beast::flat_buffer rBuffer_ {};

    bool closed_ {false};

    // The connection method is kept private, because we expect the
    // WebsocketServer class to initiate the connection.
    void Connect(
        Handler onConnect = nullptr,
        MsgHandler onMessage = nullptr,
        Handler onDisconnect = nullptr
    )
    {
        onConnect_ = onConnect;
        onMessage_ = onMessage;
        onDisconnect_ = onDisconnect;

        // We initiate the connection on the Websocket object execution context.
        // Note: Here we copy `self` (std::shared_ptr) into the lambda,
        //       effectively prolonging the lifetime of this object at least
        //       until the asio::dispatch callback is called.
        auto self {this->shared_from_this()};
        spdlog::info("WebsocketSession: [{}] New session created", self);
        closed_ = false;
        boost::asio::dispatch(ws_.get_executor(), [self]() {
            self->OnConnect();
        });
    }

    void OnConnect()
    {
        // The following timeout only matters for the purpose of completing the
        // TLS handshake. We will reset the timeout to a sensible default after
        // we are connected.
        // Note: The TCP layer is the lowest layer (Websocket -> TLS -> TCP).
        boost::beast::get_lowest_layer(ws_).expires_after(
            std::chrono::seconds(30)
        );

        // Perform the TLS handshake.
        // Note: Here we copy `self` (std::shared_ptr) into the lambda,
        //       effectively prolonging the lifetime of this object at least
        //       until the async_handshake callback is called.
        auto self {this->shared_from_this()};
        spdlog::info("WebsocketSession: [{}] Wait for TLS handshake", self);
        ws_.next_layer().async_handshake(
            boost::asio::ssl::stream_base::server,
            [self](auto ec) {
                self->OnTlsHandshake(ec);
            }
        );
    }

    void OnTlsHandshake(
        boost::system::error_code ec
    )
    {
        auto self {this->shared_from_this()};

        // On error
        if (ec) {
            spdlog::error(
                "WebsocketSession: [{}] Could not complete TLS handshake: {}",
                self, ec.message()
            );
            if (onConnect_) {
                onConnect_(ec, self);
            }
            return;
        }
        spdlog::info("WebsocketSession: [{}] TLS handshake completed", self);

        // Now that the TLS connection is established, we can reset the timeout
        // to whatever Boost.Beast recommends.
        boost::beast::get_lowest_layer(ws_).expires_never();
        auto option {boost::beast::websocket::stream_base::timeout::suggested(
            boost::beast::role_type::server
        )};
        ws_.set_option(option);

        // Wait for and accept the Websocket handshake.
        // Note: Here we copy `self` (std::shared_ptr) into the lambda,
        //       effectively prolonging the lifetime of this object at least
        //       until the async_accept callback is called.
        spdlog::info("WebsocketSession: [{}] Wait for Websocket handshake",
                     self);
        ws_.async_accept([self](auto ec) {
            self->OnHandshake(ec);
        });
    }

    void OnHandshake(
        boost::system::error_code ec
    )
    {
        auto self {this->shared_from_this()};

        if (ec) {
            spdlog::error("WebsocketSession: [{}] Could not complete Websocket "
                          "handshake: {}", self, ec.message());
            if (onConnect_) {
                onConnect_(ec, self);
            }
            return;
        }
        spdlog::info("WebsocketSession: [{}] Websocket handshake completed",
                     self);

        // Tell the Websocket object to exchange messages in text format.
        ws_.text(true);

        // Now that we are connected, set up a recursive asynchronous listener
        // to receive messages.
        spdlog::info("WebsocketSession: [{}] Listening to incoming messages",
                     self);
        ListenToIncomingMessage(ec);

        // Dispatch the user callback.
        // Note: This call is synchronous and will block the Websocket strand.
        if (onConnect_) {
            onConnect_(ec, self);
        }
    }

    void ListenToIncomingMessage(
        boost::beast::error_code ec
    )
    {
        auto self {this->shared_from_this()};

        // Stop processing messages if the connection has been aborted.
        if (ec == boost::beast::websocket::error::closed ||
            ec == boost::asio::error::operation_aborted) {
            spdlog::info(
                "WebsocketSession: [{}] Stopped listening to incoming messages",
                self
            );
            if (onDisconnect_ && !closed_) {
                onDisconnect_(ec, self);
            }
            return;
        }

        // Read a message asynchronously. On a successful read, process the
        // message and recursively call this function again to process the next
        // message.
        // Note: Here we copy `self` (std::shared_ptr) into the lambda,
        //       effectively prolonging the lifetime of this object at least
        //       until the async_read callback is called.
        ws_.async_read(rBuffer_, [self](auto ec, auto nBytes) {
            self->OnRead(ec, nBytes);
            self->ListenToIncomingMessage(ec);
        });
    }

    void OnRead(
        boost::system::error_code ec,
        size_t nBytes
    )
    {
        auto self {this->shared_from_this()};

        // We just ignore messages that failed to read.
        if (ec) {
            return;
        }
        spdlog::debug("WebsocketSession: [{}] Received {}-byte message",
                      self, nBytes);

        // Parse the message and forward it to the user callback.
        // Note: This call is synchronous and will block the Websocket strand.
        std::string message {boost::beast::buffers_to_string(rBuffer_.data())};
        rBuffer_.consume(nBytes);
        if (onMessage_) {
            onMessage_(ec, self, std::move(message));
        }
    }
};

/*! \brief Websocket server class
 *
 *  Each individual connection is spawn as a WebsocketSession object and has a
 *  life of its own.
 *
 *  \note Stopping the WebsocketServer from listening to new connections will
 *        not automatically kill all existing connections.
 *
 *  \tparam Acceptor        The class to listen to incoming TCP connections. It
 *                          must support the same interface of
 *                          boost::asio::ip::tcp::acceptor.
 *  \tparam WebsocketStream The Websocket stream class. It must support the
 *                          same interface of boost::beast::websocket::stream.
 */

template <
    typename Acceptor,
    typename WebsocketStream
>
class WebsocketServer: public std::enable_shared_from_this<
    WebsocketServer<Acceptor, WebsocketStream>
> {
public:
    /*! \brief Short-hand for the Websocket session object.
     */
    using Session = WebsocketSession<WebsocketStream>;

    /*! \brief Construct a Websocket server.
     *
     *  \note This constructor does not automatically listen to incoming
     *        connections.
     *
     *  \param ip   The IP of the server.
     *  \param port The port of the server.
     *  \param ioc  The io_context object. The user takes care of calling
     *              ioc.run().
     *  \param ctx  The TLS context to setup a TLS socket stream.
     */
    WebsocketServer(
        const std::string& ip,
        const unsigned short port,
        boost::asio::io_context& ioc,
        boost::asio::ssl::context& ctx
    ) : ip_ {ip},
        port_ {port},
        ioc_(ioc),
        ctx_ {ctx},
        acceptor_ {boost::asio::make_strand(ioc)}
    {
        spdlog::info("WebsocketServer: New server for {}:{}", ip_, port_);
    }

    /*! \brief Destructor
     */
    ~WebsocketServer() = default;

    /*! \brief Start listening to incoming connections.
     *
     *  \returns A falsey error code when no error was encountered.
     *
     *  \param onSessionConnect     Called when the connection to the client
     *                              fails or succeeds. This handler is specific
     *                              to the individual connection to the client.
     *  \param onSessionMessage     Called only when a message is successfully
     *                              received. The message is an rvalue
     *                              reference; ownership is passed to the
     *                              receiver. This handler is specific to the
     *                              individual connection to the client.
     *  \param onSessionDisconnect  Called when the connection is closed by the
     *                              client or due to a connection error. This
     *                              handler is specific to the individual
     *                              connection to the client.
     *  \param onDisconnect         Called when the Websocket server itself (and
     *                              not simply one of the active connections)
     *                              disconnects.
     */
    boost::system::error_code Run(
        typename Session::Handler onSessionConnect = nullptr,
        typename Session::MsgHandler onSessionMessage = nullptr,
        typename Session::Handler onSessionDisconnect = nullptr,
        std::function<void (boost::system::error_code)> onDisconnect = nullptr
    )
    {
        onSessionConnect_ = onSessionConnect;
        onSessionMessage_ = onSessionMessage;
        onSessionDisconnect_ = onSessionDisconnect;
        onDisconnect_ = onDisconnect;

        boost::beast::error_code ec {};

        // Setup the TCP connection acceptor and listen for connections.
        boost::asio::ip::tcp::endpoint endpoint {
            boost::asio::ip::make_address(ip_),
            port_
        };
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            spdlog::error("WebsocketServer: Could not open endpoint: {}",
                          ec.message());
            return ec;
        }
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
        if (ec) {
            spdlog::error(
                "WebsocketServer: Could not set option to reuse address: {}",
                ec.message()
            );
            return ec;
        }
        acceptor_.bind(endpoint, ec);
        if (ec) {
            spdlog::error("WebsocketServer: Could not bind endpoint: {}",
                          ec.message());
            return ec;
        }
        acceptor_.listen(
            boost::asio::socket_base::max_listen_connections,
            ec
        );
        if (ec) {
            spdlog::error(
                "WebsocketServer: Could not listen to new connections: {}",
                ec.message()
            );
            return ec;
        }

        spdlog::info("WebsocketServer: Ready to accept connections");
        stopped_ = false;
        AcceptConnection(ec);
        return ec;
    }

    /*! \brief Stop listening to new incoming connections.
     *
     *  \note This action is synchronous and has immediate effect.
     *
     *  \note This method stops the server but it does not kill any existing
     *        active connection.
     */
    void Stop()
    {
        spdlog::info("WebsocketServer: Stopping accepting connections");
        stopped_ = true;
        acceptor_.close();
    }

private:
    std::string ip_ {};
    unsigned short port_ {};

    // We leave these uninitialized because they do not support a default
    // constructor.
    boost::asio::io_context& ioc_;
    boost::asio::ssl::context& ctx_;
    Acceptor acceptor_;

    bool stopped_ {false};

    typename Session::Handler onSessionConnect_ {nullptr};
    typename Session::MsgHandler onSessionMessage_ {nullptr};
    typename Session::Handler onSessionDisconnect_ {nullptr};
    std::function<void (boost::system::error_code)> onDisconnect_ {nullptr};

    void AcceptConnection(
        boost::system::error_code ec
    )
    {
        // On error
        if (ec) {
            spdlog::error(
                "WebsocketServer: Could not accept new connection: {}",
                ec.message()
            );
            if (onDisconnect_ && !stopped_) {
                onDisconnect_(ec);
            }
            return;
        }

        // On a new connection, process it and recursively wait for the next
        // one.
        acceptor_.async_accept(
            boost::asio::make_strand(ioc_),
            [this](auto ec, auto&& socket) {
                OnConnectionAccepted(ec, std::move(socket));
                AcceptConnection(ec);
            }
        );
    }

    void OnConnectionAccepted(
        boost::system::error_code ec,
        boost::asio::ip::tcp::socket&& socket
    )
    {
        // On error
        if (ec) {
            return;
        }

        // Create a new Websocket session. We pass ownership of the socket to
        // the new Websocket session.
        auto session {std::make_shared<Session>(std::move(socket), ctx_)};
        spdlog::info("WebsocketServer: Creating new session: [{}]", session);
        session->Connect(
            onSessionConnect_,
            onSessionMessage_,
            onSessionDisconnect_
        );
    }
};

using BoostWebsocketServer = WebsocketServer<
    boost::asio::ip::tcp::acceptor,
    boost::beast::websocket::stream<
        boost::beast::ssl_stream<boost::beast::tcp_stream>
    >
>;

} // namespace NetworkMonitor

#endif // NETWORK_MONITOR_WEBSOCKET_SERVER_H