#ifndef NETWORK_MONITOR_TESTS_BOOST_MOCK_H
#define NETWORK_MONITOR_TESTS_BOOST_MOCK_H

#include <network-monitor/WebsocketClient.h>
#include <network-monitor/WebsocketServer.h>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/utility/string_view.hpp>

#include <queue>
#include <string>

namespace NetworkMonitor {

/*! \brief Mock the DNS resolver from Boost.Asio.
 */
class MockResolver {
public:

    /*! \brief Use this static member in a test to set the error code returned
     *         by async_resolve.
     */
    static boost::system::error_code resolveEc;

    /*! \brief Mock for the resolver constructor
     */
    template <typename ExecutionContext>
    explicit MockResolver(
        ExecutionContext&& context
    ) : context_ {context}
    {
    }

    /*! \brief Mock for resolver::async_resolve
     */
    template <typename ResolveHandler>
    void async_resolve(
        boost::string_view host,
        boost::string_view service,
        ResolveHandler&& handler
    )
    {
        using resolver = boost::asio::ip::tcp::resolver;
        return boost::asio::async_initiate<
            ResolveHandler,
            void (const boost::system::error_code&, resolver::results_type)
        >(
            [](auto&& handler, auto resolver, auto host, auto service) {
                if (MockResolver::resolveEc) {
                    // Failing branch
                    boost::asio::post(
                        resolver->context_,
                        boost::beast::bind_handler(
                            std::move(handler),
                            MockResolver::resolveEc,
                            resolver::results_type {} // No resolved endpoints
                        )
                    );
                } else {
                    // Successful branch.
                    boost::asio::post(
                        resolver->context_,
                        boost::beast::bind_handler(
                            std::move(handler),
                            MockResolver::resolveEc,
                            // Create a valid resolved endpoint
                            resolver::results_type::create(
                                boost::asio::ip::tcp::endpoint {
                                    boost::asio::ip::make_address(
                                        "127.0.0.1"
                                    ),
                                    443
                                },
                                host,
                                service
                            )
                        )
                    );
                }
            },
            handler,
            this,
            host.to_string(),
            service.to_string()
        );
    }

private:
    boost::asio::strand<boost::asio::io_context::executor_type> context_;
};

// Out-of-line static member initialization
inline boost::system::error_code MockResolver::resolveEc {};

/*! \brief Mock the TCP acceptor from Boost.Asio.
 *
 *  We do not mock all available methods — only the ones we are interested in
 *  for testing.
 */
class MockAcceptor {
public:
    /*! \brief Use this static member in a test to set the error code returned
     *         by acceptor::open.
     */
    static boost::system::error_code openEc;

    /*! \brief Use this static member in a test to set the error code returned
     *         by acceptor::bind.
     */
    static boost::system::error_code bindEc;

    /*! \brief Use this static member in a test to set the error code returned
     *         by acceptor::listen.
     */
    static boost::system::error_code listenEc;

    /*! \brief Use this static member in a test to set the error codes returned
     *         by acceptor::async_accept. You can use the queue to trigger as
     *         many connections as you want.
     */
    static std::queue<boost::system::error_code> acceptEc;

    /*! \brief Mock for the resolver constructor
     */
    template <typename ExecutionContext>
    explicit MockAcceptor(
        ExecutionContext&& context
    ) : context_ {context}
    {
    }

    /*! \brief Mock for acceptor::open
     */
    template <typename ProtocolType>
    void open(
        const ProtocolType& protocol,
        boost::system::error_code& ec
    )
    {
        ec = openEc;
    }

    /*! \brief Mock for acceptor::set_option
     */
    template <typename SettableSocketOption>
    void set_option(
        const SettableSocketOption& option,
        boost::system::error_code& ec
    )
    {
        ec = {}; // We always succeed.
    }

    /*! \brief Mock for acceptor::bind
     */
    template <typename EndpointType>
    void bind(
        const EndpointType& protocol,
        boost::system::error_code& ec
    )
    {
        ec = bindEc;
    }

    /*! \brief Mock for acceptor::listen
     */
    void listen(
        int backlog,
        boost::system::error_code& ec
    )
    {
        ec = listenEc;
    }

    /*! \brief Mock for acceptor::listen
     */
    void close()
    {
        // On close, schedule the next accept error code.
        acceptEc.push(boost::asio::error::operation_aborted);
    }

    /*! \brief Mock for acceptor::async_accept
     */
    template <typename ExecutionContext, typename AcceptHandler>
    void async_accept(
        const ExecutionContext& context,
        AcceptHandler&& handler
    )
    {
        return boost::asio::async_initiate<
            AcceptHandler,
            void (boost::system::error_code, boost::asio::ip::tcp::socket&&)
        >(
            [this](auto&& handler, auto context) {
                if (acceptEc.size() > 0) {
                    auto ec {acceptEc.front()};
                    acceptEc.pop();

                    // Call the user callback.
                    boost::asio::post(
                        context,
                        boost::beast::bind_handler(
                            handler,
                            ec,
                            boost::asio::ip::tcp::socket(context)
                        )
                    );
                }
            },
            handler,
            context
        );
    }
private:
    // We leave this uninitialized because it does not support a default
    // constructor.
    boost::asio::strand<boost::asio::io_context::executor_type> context_;
};

// Out-of-line static member initialization
inline boost::system::error_code MockAcceptor::openEc {};
inline boost::system::error_code MockAcceptor::bindEc {};
inline boost::system::error_code MockAcceptor::listenEc {};
inline std::queue<boost::system::error_code> MockAcceptor::acceptEc {};


/*! \brief Mock the TCP socket stream from Boost.Beast.
 */
class MockTcpStream: public boost::beast::tcp_stream {
public:
    /*! \brief Inherit all constructors from the parent class.
     */
    using boost::beast::tcp_stream::tcp_stream;

    /*! \brief Used in a test to set the error code returned
     * by async_connect.
     */
    static boost::system::error_code connectEc;

    /*! \brief Mock for tcp_stream::async_connect
     */
    template <typename ConnectHandler>
    void async_connect(
        endpoint_type type,
        ConnectHandler&& handler
    )
    {
        return boost::asio::async_initiate<
            ConnectHandler,
            void (boost::system::error_code)
        >(
            [](auto&& handler, auto stream) {
                // Call the user callback.
                boost::asio::post(
                    stream->get_executor(),
                    boost::beast::bind_handler(
                        std::move(handler),
                        MockTcpStream::connectEc
                    )
                );
            },
            handler,
            this
        );
    }
};

// Out-of-line static member initialization
inline boost::system::error_code MockTcpStream::connectEc {};

// This overload is required by Boost.Beast when you define a custom stream.
template <typename TeardownHandler>
void async_teardown(
    boost::beast::role_type role,
    MockTcpStream& socket,
    TeardownHandler&& handler
)
{
    return;
}

/*! \brief Mock the SSL stream from Boost.Beast.
 */
template <typename TcpStream>
class MockSslStream: public boost::beast::ssl_stream<TcpStream> {
public:
    /*! \brief Inherit all constructors from the parent class
     */
    using boost::beast::ssl_stream<TcpStream>::ssl_stream;

    /* \brief Use this static member in a test to set the error code returned by
     * async_handshake
     */
    static boost::system::error_code handshakeEc;

    /*! \brief Mock for ssl_stream::async_handshake
     */
    template <typename HandshakeHandler>
    void async_handshake(
        boost::asio::ssl::stream_base::handshake_type type,
        HandshakeHandler&& handler
    )
    {
        return boost::asio::async_initiate<
            HandshakeHandler,
            void (boost::system::error_code)
        >(
            [](auto&& handler, auto stream) {
                // Call the user callback
                boost::asio::post(
                    stream->get_executor(),
                    boost::beast::bind_handler(
                        std::move(handler),
                        MockSslStream::handshakeEc
                    )
                );
            },
            handler,
            this
        );
    }
};

// Out-of-line static member initialization
template <typename TcpStream>
boost::system::error_code MockSslStream<TcpStream>::handshakeEc = {};

// This overload is required by Boost.Beast when you define a custom stream.
template <typename TeardownHandler>
void async_teardown(
    boost::beast::role_type role,
    MockSslStream<MockTcpStream>& socket,
    TeardownHandler&& handler
)
{
    return;
}

/*! \brief Mock the Websockets stream from Boost.Beast.
 */
template <typename TransportStream>
class MockWebsocketStream: public boost::beast::websocket::stream<
    TransportStream
> {
public:
    /*! \brief Inherit all constructors from the parent class.
     */
    using boost::beast::websocket::stream<TransportStream>::stream;

    /* \brief Use this static member in a test to set the error code returned by
     * async_handshake  (client) and async_accept (server).
     */
    static boost::system::error_code handshakeEc;

    /* \brief Use this static member in a test to set the error code returned by
     *        async_read.
     */
    static boost::system::error_code readEc;

    /* \brief Use this static member in a test to set the buffer content read by
     *        async_read.
     */
    static std::string readBuffer;

    /* \brief Use this static member in a test to set the error code returned by
     *        async_write.
     */
    static boost::system::error_code writeEc;

    /* \brief Use this static member in a test to set the error code returned by
     *        async_close.
     */
    static boost::system::error_code closeEc;

    /*! \brief Mock for websocket::stream::async_handshake
     */
    template <typename HandshakeHandler>
    void async_handshake(
        boost::string_view host,
        boost::string_view target,
        HandshakeHandler&& handler
    )
    {
        return boost::asio::async_initiate<
            HandshakeHandler,
            void (boost::system::error_code)
        >(
            [](auto&& handler, auto stream, auto host, auto target) {
                stream->closed_ = false;

                // Call the user callback.
                boost::asio::post(
                    stream->get_executor(),
                    boost::beast::bind_handler(
                        std::move(handler),
                        MockWebsocketStream::handshakeEc
                    )
                );
            },
            handler,
            this,
            host.to_string(),
            target.to_string()
        );
    }
    
    /*! 
     * \brief Mock for websocket::stream::async_accept
     */
    template <typename HandshakeHandler>
    void async_accept(
        HandshakeHandler&& handler
    )
    {
        return boost::asio::async_initiate<
            HandshakeHandler,
            void (boost::system::error_code)
        >(
            [](auto&& handler, auto stream) {
                stream->closed_ = false;

                // Call the user callback.
                boost::asio::post(
                    stream->get_executor(),
                    boost::beast::bind_handler(
                        std::move(handler),
                        MockWebsocketStream::handshakeEc
                    )
                );
            },
            handler,
            this
        );
    }

    /*! \brief Mock for websocket::stream::async_read
     */
    template <typename DynamicBuffer, typename ReadHandler>
    void async_read(
        DynamicBuffer& buffer,
        ReadHandler&& handler
    )
    {
        return boost::asio::async_initiate<
            ReadHandler,
            void (boost::system::error_code, size_t)
        >(
            [this](auto&& handler, auto& buffer) {
                // Call a recursive function that mocks a series of reads from
                // the Websockets.
                RecursiveRead(handler, buffer);
            },
            handler,
            buffer
        );
    }

    /*! \brief Mock for websocket::stream::async_write
     */
    template <typename ConstBufferSequence, typename WriteHandler>
    void async_write(
        const ConstBufferSequence& buffers,
        WriteHandler&& handler
    )
    {
        return boost::asio::async_initiate<
            WriteHandler,
            void (boost::system::error_code, size_t)
        >(
            [](auto&& handler, auto stream, auto& buffers) {
                if (stream->closed_) {
                    // If the connection has been closed, the write operation
                    // aborts.
                    boost::asio::post(
                        stream->get_executor(),
                        boost::beast::bind_handler(
                            std::move(handler),
                            boost::asio::error::operation_aborted,
                            0
                        )
                    );
                } else {
                    // Call the user callback.
                    boost::asio::post(
                        stream->get_executor(),
                        boost::beast::bind_handler(
                            std::move(handler),
                            MockWebsocketStream::writeEc,
                            MockWebsocketStream::writeEc ? 0 : buffers.size()
                        )
                    );
                }
            },
            handler,
            this,
            buffers
        );
    }

    /*! \brief Mock for websocket::stream::async_close
     */
    template <typename CloseHandler>
    void async_close(
        const boost::beast::websocket::close_reason& cr,
        CloseHandler&& handler
    )
    {
        return boost::asio::async_initiate<
            CloseHandler,
            void (boost::system::error_code)
        >(
            [](auto&& handler, auto stream) {
                // The Websockets must be connected to begin with.
                if (stream->closed_) {
                    boost::asio::post(
                        stream->get_executor(),
                        boost::beast::bind_handler(
                            std::move(handler),
                            boost::asio::error::operation_aborted
                        )
                    );
                } else {
                    if (!MockWebsocketStream::closeEc) {
                        stream->closed_ = true;
                    }

                    // Call the user callback.
                    boost::asio::post(
                        stream->get_executor(),
                        boost::beast::bind_handler(
                            std::move(handler),
                            MockWebsocketStream::closeEc
                        )
                    );
                }
            },
            handler,
            this
        );
    }

private:
    // We use this in the other methods to check if we can proceed with a
    // successful response.
    bool closed_ {true};

    // This function mimicks a socket reading messages. It's the function we
    // call from async_read.
    template <typename DynamicBuffer, typename ReadHandler>
    void RecursiveRead(
        ReadHandler&& handler,
        DynamicBuffer& buffer
    )
    {
        if (closed_) {
            // If the connection has been closed, the read operation aborts.
            boost::asio::post(
                this->get_executor(),
                boost::beast::bind_handler(
                    std::move(handler),
                    boost::asio::error::operation_aborted,
                    0
                )
            );
        } else {
            // Read the buffer. This may be empty
            size_t nRead;
            nRead = MockWebsocketStream::readBuffer.size();
            nRead = boost::asio::buffer_copy(
                buffer.prepare(nRead),
                boost::asio::buffer(MockWebsocketStream::readBuffer)
            );
            buffer.commit(nRead);

            // Clear the mock buffer for the next read.
            MockWebsocketStream::readBuffer = "";

            if (nRead == 0) {
                // If there was nothing to read, recursively go and wait for
                // a new message.
                boost::asio::post(
                    this->get_executor(),
                    [this, handler = std::move(handler), &buffer]() {
                        RecursiveRead(handler, buffer);
                    }
                );
            } else {
                // On a legitimate message, call the async_read original
                // handler.
                boost::asio::post(
                    this->get_executor(),
                    boost::beast::bind_handler(
                        std::move(handler),
                        MockWebsocketStream::readEc,
                        nRead
                    )
                );
            }
        }
    }
};

// Out-of-line static member initialization

template <typename TransportStream>
boost::system::error_code MockWebsocketStream<TransportStream>::handshakeEc = {};

template <typename TransportStream>
boost::system::error_code MockWebsocketStream<TransportStream>::readEc = {};

template <typename TransportStream>
std::string MockWebsocketStream<TransportStream>::readBuffer = "";

template <typename TransportStream>
boost::system::error_code MockWebsocketStream<TransportStream>::writeEc = {};

template <typename TransportStream>
boost::system::error_code MockWebsocketStream<TransportStream>::closeEc = {};

/*! \brief Type alias for the mocked ssl_stream.
 */
using MockTlsStream = MockSslStream<MockTcpStream>;

/*! \brief Type alias for the mocked websocket::stream.
 */
using MockTlsWebsocketStream = MockWebsocketStream<MockTlsStream>;

/*! \brief Type alias for the mocked WebsocketClient.
 */
using TestWebsocketClient = WebsocketClient<
    MockResolver,
    MockTlsWebsocketStream
>;

/*! \brief Type alias for the mocked WebsocketServer.
 */
using TestWebsocketServer = WebsocketServer<
    MockAcceptor,
    MockTlsWebsocketStream
>;

} // namespace NetworkMonitor

#endif // NETWORK_MONITOR_TESTS_BOOST_MOCK_H