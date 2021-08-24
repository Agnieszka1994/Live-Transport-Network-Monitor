#ifndef NETWORK_MONITOR_BOOST_MOCK_H
#define NETWORK_MONITOR_BOOST_MOCK_H

#include <network-monitor/WebsocketClient.h>
#include <boost/asio.hpp>
#include <boost/utility/string_view.hpp>

namespace NetworkMonitor {
    /*!
    *
    */
    class MockReslover {
        public:

        /*! \brief Use this static member in a test to set the error code returned
        *         by async_resolve.
        */
        static boost::system::error_code resolveEc;

        /*!
        * Mock for the resolver constructor
        */
        template<typename ExecutionContext>
        explicit MockReslover(
            ExecutionContext& ctx
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
                void(const boost::system::error_code&, resolver::result_type)
            >(
                [](auto&& handler, auto resolver){
                    if (MockResolver::resolveEc) {
                        // Failing branch
                        boost::asio::post(
                            resolver->context_,
                            boost::beast::bind_handler(
                                std::move(handler),
                                MockReslover::resolveEc,
                                resolver::results_type {} // No resolved endpoints
                            )
                        );
                    } else {
                        // TO DO ! Handler successfull branch
                    }
                },
                handler,
                this
            );
        }

        private:
        boost::asio::strand<boost::asio::io_context::executor_type> context_;
    };

    // Out-of-line static member initialization
    inline boost::system::error_code MockResolver::resolveEc {};

    /*! \brief Type alias for the mocked WebSocketClient.
    *
    *  For now we only mock the DNS resolver.
    */
    using TestWebSocketClient = WebsocketClient<
        MockResolver,
        boost::beast::websocket::stream<
            boost::beast::ssl_stream<boost::beast::tcp_stream>
        >
    >;
}

#endif // NETWORK_MONITOR_BOOST_MOCK_H