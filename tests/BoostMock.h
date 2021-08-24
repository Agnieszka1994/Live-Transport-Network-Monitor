#ifndef NETWORK_MONITOR_BOOST_MOCK_H
#define NETWORK_MONITOR_BOOST_MOCK_H

#include <network-monitor/WebsocketClient.h>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/utility/string_view.hpp>

namespace NetworkMonitor {
    /*!
    *
    */
    class MockResolver {
        public:

        /*! \brief Use this static member in a test to set the error code returned
        *         by async_resolve.
        */
        static boost::system::error_code resolveEc;

        /*!
        * Mock for the resolver constructor
        */
        template<typename ExecutionContext>
        explicit MockResolver(
            ExecutionContext&& ctx
        ) : context_(ctx)
        {
        }

        /*!
        * Mock for resolver::async_resolve
        */
        template<typename ResolveHandler>
        void async_resolve(
            boost::string_view host,
            boost::string_view service,
            ResolveHandler&& handler
        )
        {
            using resolver = boost::asio::ip::tcp::resolver;
            return boost::asio::async_initiate<
                ResolveHandler,
                void(const boost::system::error_code&, resolver::results_type)
            >(
                [](auto&& handler, auto resolver, auto host, auto service){
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

        /*! \brief Mock the TCP socket stream from Boost.Beast.
    *
    *  We do not mock all available methods — only the ones we are interested in
    *  for testing.
    */
    class MockTcpStream: public boost::beast::tcp_stream {
    public:
        /*! \brief Inherit all constructors from the parent class.
        */
        using boost::beast::tcp_stream::tcp_stream;

        /*! \brief Used in a test to set the error code returned
        *         by async_connect.
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

    /*! \brief Type alias for the mocked WebsocketClient.
    */
    using TestWebsocketClient = WebsocketClient<
        MockResolver,
        boost::beast::websocket::stream<
            boost::beast::ssl_stream<MockTcpStream>
        >
    >;
} // namespace NetworkMonitor

#endif // NETWORK_MONITOR_BOOST_MOCK_H