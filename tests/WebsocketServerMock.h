#ifndef NETWORK_MONITOR_TESTS_WEBSOCKET_SERVER_MOCK_H
#define NETWORK_MONITOR_TESTS_WEBSOCKET_SERVER_MOCK_H

#include <network-monitor/StompFrame.h>
#include <network-monitor/WebsocketServer.h>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <utility>

namespace NetworkMonitor {

/*! \brief Craft a mock STOMP frame.
 */
std::string GetMockStompFrame(
    const std::string& host
);

/*! \brief Craft a mock SEND frame.
 */
std::string GetMockSendFrame(
    const std::string& id,
    const std::string& destination,
    const std::string& payload
);

/*! \brief Mock a Websocket event.
 *
 *  A test can specify a sequence of mock events for a given Websocket session.
 */
struct MockWebsocketEvent {
    /*! \brief The Websocket event type.
     */
    enum class Type {
        kConnect,
        kMessage,
        kDisconnect,
    };

    std::string id {};
    Type type {Type::kConnect};
    boost::system::error_code ec {};
    std::string message {};
};

/*! \brief Mock the WebsocketServer class.
 *
 *  We do not mock all available methods — only the ones we are interested in
 *  for testing.
 */
class MockWebsocketSession: public std::enable_shared_from_this<
    MockWebsocketSession
> {
public:
    // Use these static members in a test to set the error codes returned by
    // the mock.
    static boost::system::error_code sendEc;

    /*! \brief Mock handler type for MockWebsocketSession.
     */
    using Handler = std::function<
        void (
            boost::system::error_code,
            std::shared_ptr<MockWebsocketSession>
        )
    >;

    /*! \brief Mock message handler type for MockWebsocketSession.
     */
    using MsgHandler = std::function<
        void (
            boost::system::error_code,
            std::shared_ptr<MockWebsocketSession>,
            std::string&&
        )
    >;

    /*! \brief Mock constructor. This is the only method that has a different
     *         signature from the original one.
     */
    MockWebsocketSession(
        boost::asio::io_context& ioc
    );

    /*! \brief Mock destructor.
     */
    ~MockWebsocketSession();

    /*! \brief Send a mock message.
     */
    void Send(
        const std::string& message,
        std::function<void (boost::system::error_code)> onSend = nullptr
    );

    /*! \brief Mock close.
     */
    void Close(
        std::function<void (boost::system::error_code)> onClose = nullptr
    );

private:
    // This strand handles all the user callbacks.
    // We leave it uninitialized because it does not support a default
    // constructor.
    boost::asio::strand<boost::asio::io_context::executor_type> context_;

    bool connected_ {false};

    void MockIncomingMessages(
        std::function<void (boost::system::error_code,
                            std::string&&)> onMessage = nullptr,
        std::function<void (boost::system::error_code)> onDisconnect = nullptr
    );
};

/*! \brief Mock the WebsocketServer class.
 *
 *  We do not mock all available methods — only the ones we are interested in
 *  for testing.
 */
class MockWebsocketServer {
public:
    // Use these static members in a test to set the error codes returned by
    // the mock.
    static bool triggerDisconnection;
    static boost::system::error_code runEc;
    static std::queue<MockWebsocketEvent> mockEvents;

    /*! \brief Mock session type.
     */
    using Session = MockWebsocketSession;

    /*! \brief Mock constructor.
     */
    MockWebsocketServer(
        const std::string& ip,
        const unsigned short port,
        boost::asio::io_context& ioc,
        boost::asio::ssl::context& ctx
    );

    /*! \brief Mock destructor.
     */
    virtual ~MockWebsocketServer();

    /*! \brief Mock run.
     */
    boost::system::error_code Run(
        Session::Handler onSessionConnect = nullptr,
        Session::MsgHandler onSessionMessage = nullptr,
        Session::Handler onSessionDisconnect = nullptr,
        std::function<void (boost::system::error_code)> onDisconnect = nullptr
    );

    /*! \brief Mock stop.
     */
    void Stop();

private:
    // We leave it uninitialized because it does not support a default
    // constructor.
    boost::asio::io_context& ioc_;

    // This strand handles all the user callbacks.
    boost::asio::strand<boost::asio::io_context::executor_type> context_;

    std::unordered_map<
        std::string,
        std::shared_ptr<MockWebsocketSession>
    > connections_ {};

    bool started_ {false};
    bool stopped_ {false};

    void ListenToMockConnections(
        Session::Handler onSessionConnect,
        Session::MsgHandler onSessionMessage,
        Session::Handler onSessionDisconnect,
        std::function<void (boost::system::error_code)> onDisconnect
    );
};

/*! \brief Mock the WebsocketServer class to connect to a STOMP client.
 *
 *  We do not mock all available methods — only the ones we are interested in
 *  for testing.
 */
class MockWebsocketServerForStomp: public MockWebsocketServer {
    using MockWebsocketServer::MockWebsocketServer;
};

} // namespace NetworkMonitor

#endif // NETWORK_MONITOR_TESTS_BOOST_MOCK_H