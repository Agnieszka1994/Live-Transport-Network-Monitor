#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/system/error_code.hpp>
#include <functional>
#include <string>
#include <iomanip>
#include <iostream>
#include <string>
#include <chrono>

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
        *                  Example: echo.websocket.org/<endpoint>
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

        /*! \brief Send a text message to the Websocket server.
        *
        *  \param message The message to send.
        *  \param onSend  Called when a message is sent successfully or if it
        *                 failed to send.
        */
        void Send(
                const std::string& message,
                std::function<void (boost::system::error_code)> onSend = nullptr
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

        /*! \brief Close the Websocket connection.
        *
        *  \param onClose Called when the connection is closed, successfully or
        *                 not.
        */
        void Close(
                std::function<void (boost::system::error_code)> onClose = nullptr
        )
        {
            ws_.async_close(boost::beast::websocket::close_code::none, 
                [this, onClose](auto ec){
                    if(onClose){
                        onClose(ec);
                    }
                }
            );
        }

    private:
        std::string url_ {};
        std::string endpoint_ {};
        std::string port_ {};

        // Leaving these uninitialized because they do not support a default constructor
        Resolver resolver_;

        WebsocketStream ws_;
        boost::beast::flat_buffer rBuffer_ {};

        std::function<void (boost::system::error_code)> onConnect_ {nullptr};

        std::function<void (boost::system::error_code,
                        std::string&&)> onMessage_ {nullptr};

        std::function<void (boost::system::error_code)> onDisconnect_ {nullptr};
        
        static void Log(const std::string& where, boost::system::error_code ec)
        {
            std::cerr << "[" << std::setw(20) << where << "] "
                    << (ec ? "Error: " : "OK")
                    << (ec ? ec.message() : "")
                    << std::endl;
        }

        void OnResolve(
                    const boost::system::error_code& ec,
                    boost::asio::ip::tcp::resolver::iterator resolverIt
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
            // Note: The TCP layer is the lowest layer (Websocket -> TLS -> TCP).
            boost::beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(5));

            // Connect to the TCP socket.
            // Note: The TCP layer is the lowest layer (Websocket -> TLS -> TCP).
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
            if(ec){
                Log("OnConnect", ec);
                if(onConnect_) {
                    onConnect_(ec);
                }
                return;
            }

            // Now that the TCP socket is connected, we can reset the timeout to
            // whatever Boost.Beast recommends.
            // Note: The TCP layer is the lowest layer (Websocket -> TLS -> TCP).
            boost::beast::get_lowest_layer(ws_).expires_never();
            ws_.set_option(boost::beast::websocket::stream_base::timeout::suggested(
                boost::beast::role_type::client
            ));

            // Some clients require that we set the host name before the TLS handshake
            // or the connection will fail. We use an OpenSSL function for that.
            SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), url_.c_str());

            // Attempt a TLS handshake.
            // Note: The TLS layer is the next layer (Websocket -> TLS -> TCP).
            ws_.next_layer().async_handshake(boost::asio::ssl::stream_base::client,
                [this](auto ec) {
                    OnTlsHandshake(ec);
                }
            );
        }

        void OnHandshake(
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

        void OnTlsHandshake(
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

            // Attempt a Websocket handshake.
            ws_.async_handshake(url_, endpoint_,
                [this](auto ec) {
                    OnHandshake(ec);
                }
            );
        }

        void ListenToIncomingMessage(
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

        void OnRead(
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
    };

    using BoostWebsocketClient = WebsocketClient<
        boost::asio::ip::tcp::resolver,
        boost::beast::websocket::stream<
            boost::beast::ssl_stream<boost::beast::tcp_stream>
        >
    >;

} // namespace NetworkMonitor
#endif // WEBSOCKET_CLIENT_H