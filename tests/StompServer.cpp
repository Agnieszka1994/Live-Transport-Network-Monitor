#include "WebsocketServerMock.h"

#include <network-monitor/env.h>
#include <network-monitor/StompClient.h>
#include <network-monitor/StompServer.h>
#include <network-monitor/TestServerCertificate.h>
#include <network-monitor/WebsocketClient.h>
#include <network-monitor/WebsocketServer.h>

#include <nlohmann/json.hpp>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/test/unit_test.hpp>

#include <memory>
#include <string>
#include <vector>

using NetworkMonitor::BoostWebsocketClient;
using NetworkMonitor::BoostWebsocketServer;
using NetworkMonitor::GetEnvVar;
using NetworkMonitor::GetMockSendFrame;
using NetworkMonitor::GetMockStompFrame;
using NetworkMonitor::LoadTestServerCertificate;
using NetworkMonitor::MockWebsocketEvent;
using NetworkMonitor::MockWebsocketServerForStomp;
using NetworkMonitor::MockWebsocketSession;
using NetworkMonitor::StompClient;
using NetworkMonitor::StompClientError;
using NetworkMonitor::StompCommand;
using NetworkMonitor::StompError;
using NetworkMonitor::StompFrame;
using NetworkMonitor::StompServer;
using NetworkMonitor::StompServerError;

using namespace std::string_literals;

// Use this to set a timeout on tests that may hang or suffer from a slow
// connection.
using timeout = boost::unit_test::timeout;

// This fixture is used to re-initialize all mock properties before a test.
struct StompServerTestFixture {
    StompServerTestFixture()
    {
        MockWebsocketSession::sendEc = {};
        MockWebsocketServerForStomp::triggerDisconnection = false;
        MockWebsocketServerForStomp::runEc = {};
        MockWebsocketServerForStomp::mockEvents = {};
    }
};

BOOST_AUTO_TEST_SUITE(network_monitor);

BOOST_AUTO_TEST_SUITE(stomp_server);

BOOST_AUTO_TEST_SUITE(enum_class_StompServerError);

BOOST_AUTO_TEST_CASE(ostream)
{
    std::stringstream invalidSs;
    invalidSs << StompServerError::kUndefinedError;
    auto invalid {invalidSs.str()};
    for (const auto& error: {
        StompServerError::kOk,
        StompServerError::kClientCannotReconnect,
        StompServerError::kCouldNotCloseClientConnection,
        StompServerError::kCouldNotParseFrame,
        StompServerError::kCouldNotSendMessage,
        StompServerError::kCouldNotStartWebsocketServer,
        StompServerError::kInvalidHeaderValueAcceptVersion,
        StompServerError::kInvalidHeaderValueHost,
        StompServerError::kUnsupportedFrame,
        StompServerError::kWebsocketSessionDisconnected,
        StompServerError::kWebsocketServerDisconnected,
    }) {
        std::stringstream ss {};
        ss << error;
        BOOST_CHECK(ss.str() != invalid);
    }
}

BOOST_AUTO_TEST_SUITE_END(); // enum_class_StompServerError

BOOST_FIXTURE_TEST_SUITE(class_StompServer, StompServerTestFixture);

BOOST_AUTO_TEST_CASE(run_no_connections, *timeout {1})
{
    // Since we use the mock, we do not actually launch a server at this port.
    const std::string host {"localhost"};
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    StompServer<MockWebsocketServerForStomp> server {
        host,
        ip,
        port,
        ioc,
        ctx
    };
    auto onDisconnect = [](auto) {
        BOOST_CHECK(false);
    };
    auto ec {server.Run(nullptr, nullptr, nullptr, onDisconnect)};
    BOOST_CHECK_EQUAL(ec, StompServerError::kOk);

    // We let the server listen to incoming connections for 250ms.
    bool didTimeout {false};
    boost::asio::high_resolution_timer timer(ioc);
    timer.expires_after(std::chrono::milliseconds(250));
    timer.async_wait([&didTimeout, &server](auto ec) {
        didTimeout = true;
        BOOST_CHECK(!ec);

        // This test assumes that Stop() works.
        server.Stop();
    });

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(didTimeout);
}

BOOST_AUTO_TEST_CASE(run_fail, *timeout {1})
{
    // Since we use the mock, we do not actually launch a server at this port.
    const std::string host {"localhost"};
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    // Set the expected error codes.
    MockWebsocketServerForStomp::runEc = boost::asio::error::access_denied;

    StompServer<MockWebsocketServerForStomp> server {
        host,
        ip,
        port,
        ioc,
        ctx
    };
    auto onDisconnect = [](auto) {
        BOOST_CHECK(false);
    };
    auto ec {server.Run(nullptr, nullptr, nullptr, onDisconnect)};
    BOOST_CHECK_EQUAL(ec, StompServerError::kCouldNotStartWebsocketServer);

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
}

BOOST_AUTO_TEST_CASE(run_connection_fail_connect, *timeout {1})
{
    // Since we use the mock, we do not actually launch a server at this port.
    const std::string host {"localhost"};
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    // Setup the mock.
    MockWebsocketServerForStomp::mockEvents = std::queue<MockWebsocketEvent> {{
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kConnect,
            boost::asio::error::access_denied,
        },
    }};

    StompServer<MockWebsocketServerForStomp> server {
        host,
        ip,
        port,
        ioc,
        ctx
    };
    auto onDisconnect = [](auto) {
        BOOST_CHECK(false);
    };
    auto ec {server.Run(nullptr, nullptr, nullptr, onDisconnect)};
    BOOST_REQUIRE_EQUAL(ec, StompServerError::kOk);

    // We let the server listen to incoming connections for 250ms.
    bool didTimeout {false};
    boost::asio::high_resolution_timer timer(ioc);
    timer.expires_after(std::chrono::milliseconds(250));
    timer.async_wait([&didTimeout, &server](auto ec) {
        didTimeout = true;
        BOOST_CHECK(!ec);

        // This test assumes that Stop() works.
        server.Stop();
    });

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(didTimeout);
}

BOOST_AUTO_TEST_CASE(run_connection_fail_stomp, *timeout {1})
{
    // Since we use the mock, we do not actually launch a server at this port.
    const std::string host {"localhost"};
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    // Setup the mock.
    MockWebsocketServerForStomp::mockEvents = std::queue<MockWebsocketEvent> {{
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kConnect,
            // Succeeds
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            "invalid stomp frame"
        },
    }};

    StompServer<MockWebsocketServerForStomp> server {
        host,
        ip,
        port,
        ioc,
        ctx
    };
    auto onDisconnect = [](auto) {
        BOOST_CHECK(false);
    };
    auto ec {server.Run(nullptr, nullptr, nullptr, onDisconnect)};
    BOOST_REQUIRE_EQUAL(ec, StompServerError::kOk);

    // We let the server listen to incoming connections for 250ms.
    bool didTimeout {false};
    boost::asio::high_resolution_timer timer(ioc);
    timer.expires_after(std::chrono::milliseconds(250));
    timer.async_wait([&didTimeout, &server](auto ec) {
        didTimeout = true;
        BOOST_CHECK(!ec);

        // This test assumes that Stop() works.
        server.Stop();
    });

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(didTimeout);
}

BOOST_AUTO_TEST_CASE(run_connection_disconnect, *timeout {1})
{
    // Since we use the mock, we do not actually launch a server at this port.
    const std::string host {"localhost"};
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    // Setup the mock.
    MockWebsocketServerForStomp::mockEvents = std::queue<MockWebsocketEvent> {{
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kConnect,
            // Succeeds
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kDisconnect,
            boost::asio::error::interrupted,
        },
    }};

    StompServer<MockWebsocketServerForStomp> server {
        host,
        ip,
        port,
        ioc,
        ctx
    };
    auto onClientConnect = [](auto, auto) {
        BOOST_CHECK(false);
    };
    auto onClientMessage = [](auto, auto, auto, auto, auto&&) {
        BOOST_CHECK(false);
    };
    auto onClientDisconnect = [](auto, auto) {
        BOOST_CHECK(false);
    };
    auto onDisconnect = [](auto) {
        BOOST_CHECK(false);
    };
    auto ec {server.Run(
        onClientConnect,
        onClientMessage,
        onClientDisconnect,
        onDisconnect
    )};
    BOOST_REQUIRE_EQUAL(ec, StompServerError::kOk);

    // We let the server listen to incoming connections for 250ms.
    bool didTimeout {false};
    boost::asio::high_resolution_timer timer(ioc);
    timer.expires_after(std::chrono::milliseconds(250));
    timer.async_wait([&didTimeout, &server](auto ec) {
        didTimeout = true;
        BOOST_CHECK(!ec);

        // This test assumes that Stop() works.
        server.Stop();
    });

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(didTimeout);
}

BOOST_AUTO_TEST_CASE(run_connection_stomp, *timeout {1})
{
    // Since we use the mock, we do not actually launch a server at this port.
    const std::string host {"localhost"};
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    // Setup the mock.
    MockWebsocketServerForStomp::mockEvents = std::queue<MockWebsocketEvent> {{
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kConnect,
            // Succeeds
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            GetMockStompFrame(host)
        },
    }};

    StompServer<MockWebsocketServerForStomp> server {
        host,
        ip,
        port,
        ioc,
        ctx
    };
    bool clientDidConnect {false};
    auto onClientConnect = [&clientDidConnect, &server](auto ec, auto id) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kOk);
        BOOST_CHECK(id.size() > 0);
        clientDidConnect = true;

        // This test assumes that Stop works.
        server.Stop();
    };
    auto onClientMessage = [](auto, auto, auto, auto, auto&&) {
        BOOST_CHECK(false);
    };
    auto onClientDisconnect = [](auto, auto) {
        BOOST_CHECK(false);
    };
    auto onDisconnect = [](auto) {
        BOOST_CHECK(false);
    };
    auto ec {server.Run(
        onClientConnect,
        onClientMessage,
        onClientDisconnect,
        onDisconnect
    )};
    BOOST_REQUIRE_EQUAL(ec, StompServerError::kOk);

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(clientDidConnect);
}

BOOST_AUTO_TEST_CASE(run_connection_stomp_disconnect, *timeout {1})
{
    // Since we use the mock, we do not actually launch a server at this port.
    const std::string host {"localhost"};
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    // Setup the mock.
    MockWebsocketServerForStomp::mockEvents = std::queue<MockWebsocketEvent> {{
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kConnect,
            // Succeeds
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            GetMockStompFrame(host)
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kDisconnect,
            boost::asio::error::interrupted,
        },
    }};

    StompServer<MockWebsocketServerForStomp> server {
        host,
        ip,
        port,
        ioc,
        ctx
    };
    bool clientDidConnect {false};
    auto onClientConnect = [&clientDidConnect](auto ec, auto id) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kOk);
        BOOST_CHECK(id.size() > 0);
        clientDidConnect = true;
    };
    auto onClientMessage = [](auto, auto, auto, auto, auto&&) {
        BOOST_CHECK(false);
    };
    bool clientDidDisconnect {false};
    auto onClientDisconnect = [
        &clientDidDisconnect,
        &server
    ](auto ec, auto id) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kWebsocketSessionDisconnected);
        BOOST_CHECK(id.size() > 0);
        clientDidDisconnect = true;

        // This test assumes that Stop works.
        server.Stop();
    };
    auto onDisconnect = [](auto) {
        BOOST_CHECK(false);
    };
    auto ec {server.Run(
        onClientConnect,
        onClientMessage,
        onClientDisconnect,
        onDisconnect
    )};
    BOOST_REQUIRE_EQUAL(ec, StompServerError::kOk);

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(clientDidConnect);
    BOOST_CHECK(clientDidDisconnect);
}

BOOST_AUTO_TEST_CASE(run_disconnect_server, *timeout {1})
{
    // Since we use the mock, we do not actually launch a server at this port.
    const std::string host {"localhost"};
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    // Setup the mock.
    MockWebsocketServerForStomp::mockEvents = std::queue<MockWebsocketEvent> {{
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kConnect,
            // Succeeds
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            GetMockStompFrame(host)
        },
    }};

    StompServer<MockWebsocketServerForStomp> server {
        host,
        ip,
        port,
        ioc,
        ctx
    };
    bool clientDidConnect {false};
    auto onClientConnect = [&clientDidConnect](auto ec, auto id) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kOk);
        BOOST_CHECK(id.size() > 0);
        clientDidConnect = true;

        // Trigger the server disconnection.
        MockWebsocketServerForStomp::triggerDisconnection = true;
    };
    auto onClientMessage = [](auto, auto, auto, auto, auto&&) {
        BOOST_CHECK(false);
    };
    auto onClientDisconnect = [](auto, auto) {
        BOOST_CHECK(false);
    };
    bool serverDidDisconnect {false};
    auto onDisconnect = [&serverDidDisconnect](auto ec) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kWebsocketServerDisconnected);
        serverDidDisconnect = true;
    };
    auto ec {server.Run(
        onClientConnect,
        onClientMessage,
        onClientDisconnect,
        onDisconnect
    )};
    BOOST_REQUIRE_EQUAL(ec, StompServerError::kOk);

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(clientDidConnect);
    BOOST_CHECK(serverDidDisconnect);
}

BOOST_AUTO_TEST_CASE(on_client_message, *timeout {1})
{
    // Since we use the mock, we do not actually launch a server at this port.
    const std::string host {"localhost"};
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    const std::string destination {"/quiet-route"};
    const nlohmann::json message {
        {"msg", "Hello world"},
    };

    // Setup the mock.
    MockWebsocketServerForStomp::mockEvents = std::queue<MockWebsocketEvent> {{
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kConnect,
            // Succeeds
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            GetMockStompFrame(host)
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            GetMockSendFrame("msg0", destination, message.dump())
        },
    }};

    StompServer<MockWebsocketServerForStomp> server {
        host,
        ip,
        port,
        ioc,
        ctx
    };
    bool clientDidConnect {false};
    auto onClientConnect = [&clientDidConnect](auto ec, auto id) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kOk);
        BOOST_CHECK(id.size() > 0);
        clientDidConnect = true;
    };
    bool messageReceived {false};
    auto onClientMessage = [
        &messageReceived,
        &destination,
        &message,
        &server
    ](auto ec, auto id, auto dst, auto reqId, auto&& msg) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kOk);
        BOOST_CHECK(id.size() > 0);
        BOOST_CHECK_EQUAL(dst, destination);
        BOOST_CHECK_EQUAL(reqId, "msg0");
        BOOST_CHECK_EQUAL(msg, message.dump());
        messageReceived = true;

        // This test assumes that Stop works.
        server.Stop();
    };
    auto onClientDisconnect = [](auto, auto) {
        BOOST_CHECK(false);
    };
    auto onDisconnect = [](auto) {
        BOOST_CHECK(false);
    };
    auto ec {server.Run(
        onClientConnect,
        onClientMessage,
        onClientDisconnect,
        onDisconnect
    )};
    BOOST_REQUIRE_EQUAL(ec, StompServerError::kOk);

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(clientDidConnect);
    BOOST_CHECK(messageReceived);
}

BOOST_AUTO_TEST_CASE(on_client_message_before_connect, *timeout {1})
{
    // Since we use the mock, we do not actually launch a server at this port.
    const std::string host {"localhost"};
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    const std::string destination {"/quiet-route"};
    const nlohmann::json message {
        {"msg", "Hello world"},
    };

    // Setup the mock.
    MockWebsocketServerForStomp::mockEvents = std::queue<MockWebsocketEvent> {{
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kConnect,
            // Succeeds
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            GetMockSendFrame("msg0", destination, message.dump())
        },
    }};

    StompServer<MockWebsocketServerForStomp> server {
        host,
        ip,
        port,
        ioc,
        ctx
    };
    auto onClientConnect = [](auto, auto) {
        BOOST_CHECK(false);
    };
    auto onClientMessage = [](auto, auto, auto, auto, auto&&) {
        // We are especially interested in not receiving this callback since
        // the client is only connected through Websockets and not STOMP.
        BOOST_CHECK(false);
    };
    auto onClientDisconnect = [](auto, auto) {
        BOOST_CHECK(false);
    };
    auto onDisconnect = [](auto) {
        BOOST_CHECK(false);
    };
    auto ec {server.Run(
        onClientConnect,
        onClientMessage,
        onClientDisconnect,
        onDisconnect
    )};
    BOOST_REQUIRE_EQUAL(ec, StompServerError::kOk);

    // We let the server listen to incoming connections for 250ms.
    bool didTimeout {false};
    boost::asio::high_resolution_timer timer(ioc);
    timer.expires_after(std::chrono::milliseconds(250));
    timer.async_wait([&didTimeout, &server](auto ec) {
        didTimeout = true;
        BOOST_CHECK(!ec);

        // This test assumes that Stop() works.
        server.Stop();
    });

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(didTimeout);
}

BOOST_AUTO_TEST_CASE(send, *timeout {1})
{
    // Since we use the mock, we do not actually launch a server at this port.
    const std::string host {"localhost"};
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    const std::string destination {"/quiet-route"};
    const nlohmann::json message {
        {"msg", "Hello world"},
    };

    // Setup the mock.
    MockWebsocketServerForStomp::mockEvents = std::queue<MockWebsocketEvent> {{
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kConnect,
            // Succeeds
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            GetMockStompFrame(host)
        },
    }};

    StompServer<MockWebsocketServerForStomp> server {
        host,
        ip,
        port,
        ioc,
        ctx
    };
    bool messageSent {false};
    auto onSend = [&messageSent, &server](auto ec, auto id) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kOk);
        BOOST_CHECK(id.size() > 0);
        messageSent = true;

        // This test assumes that Stop works.
        server.Stop();
    };
    bool clientDidConnect {false};
    auto onClientConnect = [
        &server,
        &clientDidConnect,
        &destination,
        &message,
        &onSend
    ](auto ec, auto id) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kOk);
        BOOST_CHECK(id.size() > 0);
        clientDidConnect = true;

        MockWebsocketSession::sendEc = {};
        auto reqId {server.Send(id, destination, message.dump(), onSend)};
        BOOST_CHECK(reqId.size() > 0);
    };
    auto onClientMessage = [](auto, auto, auto, auto, auto&&) {
        BOOST_CHECK(false);
    };
    auto onClientDisconnect = [](auto, auto) {
        BOOST_CHECK(false);
    };
    auto onDisconnect = [](auto) {
        BOOST_CHECK(false);
    };
    auto ec {server.Run(
        onClientConnect,
        onClientMessage,
        onClientDisconnect,
        onDisconnect
    )};
    BOOST_REQUIRE_EQUAL(ec, StompServerError::kOk);

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(clientDidConnect);
    BOOST_CHECK(messageSent);
}

BOOST_AUTO_TEST_CASE(send_custom_req_id, *timeout {1})
{
    // Since we use the mock, we do not actually launch a server at this port.
    const std::string host {"localhost"};
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    const std::string destination {"/quiet-route"};
    const nlohmann::json message {
        {"msg", "Hello world"},
    };

    const std::string customReqId {"custom-request-id"};

    // Setup the mock.
    MockWebsocketServerForStomp::mockEvents = std::queue<MockWebsocketEvent> {{
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kConnect,
            // Succeeds
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            GetMockStompFrame(host)
        },
    }};

    StompServer<MockWebsocketServerForStomp> server {
        host,
        ip,
        port,
        ioc,
        ctx
    };
    bool calledOnSend {false};
    auto onSend = [&calledOnSend, &customReqId, &server](auto ec, auto id) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kCouldNotSendMessage);
        BOOST_REQUIRE(id.size() > 0);
        BOOST_CHECK_EQUAL(id, customReqId);
        calledOnSend = true;

        // This test assumes that Stop works.
        server.Stop();
    };
    bool clientDidConnect {false};
    auto onClientConnect = [
        &server,
        &clientDidConnect,
        &destination,
        &message,
        &customReqId,
        &onSend
    ](auto ec, auto id) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kOk);
        BOOST_CHECK(id.size() > 0);
        clientDidConnect = true;

        // Make it fail.
        MockWebsocketSession::sendEc = boost::asio::error::interrupted;
        auto reqId {server.Send(id, destination, message.dump(), onSend,
                                customReqId)};
        BOOST_REQUIRE(reqId.size() > 0);
        BOOST_CHECK_EQUAL(reqId, customReqId);
    };
    auto onClientMessage = [](auto, auto, auto, auto, auto&&) {
        BOOST_CHECK(false);
    };
    auto onClientDisconnect = [](auto ec, auto id) {
        BOOST_CHECK(false);
    };
    auto onDisconnect = [](auto ec) {
        BOOST_CHECK(false);
    };
    auto ec {server.Run(
        onClientConnect,
        onClientMessage,
        onClientDisconnect,
        onDisconnect
    )};
    BOOST_REQUIRE_EQUAL(ec, StompServerError::kOk);

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(clientDidConnect);
    BOOST_CHECK(calledOnSend);
}

BOOST_AUTO_TEST_CASE(send_fail, *timeout {1})
{
    // Since we use the mock, we do not actually launch a server at this port.
    const std::string host {"localhost"};
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    const std::string destination {"/quiet-route"};
    const nlohmann::json message {
        {"msg", "Hello world"},
    };

    // Setup the mock.
    MockWebsocketServerForStomp::mockEvents = std::queue<MockWebsocketEvent> {{
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kConnect,
            // Succeeds
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            GetMockStompFrame(host)
        },
    }};

    StompServer<MockWebsocketServerForStomp> server {
        host,
        ip,
        port,
        ioc,
        ctx
    };
    bool calledOnSend {false};
    auto onSend = [&calledOnSend, &server](auto ec, auto id) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kCouldNotSendMessage);
        BOOST_CHECK(id.size() > 0);
        calledOnSend = true;

        // This test assumes that Stop works.
        server.Stop();
    };
    bool clientDidConnect {false};
    auto onClientConnect = [
        &server,
        &clientDidConnect,
        &destination,
        &message,
        &onSend
    ](auto ec, auto id) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kOk);
        BOOST_CHECK(id.size() > 0);
        clientDidConnect = true;

        // Make it fail.
        MockWebsocketSession::sendEc = boost::asio::error::interrupted;
        auto reqId {server.Send(id, destination, message.dump(), onSend)};
        BOOST_CHECK(reqId.size() > 0);
    };
    auto onClientMessage = [](auto, auto, auto, auto, auto&&) {
        BOOST_CHECK(false);
    };
    auto onClientDisconnect = [](auto, auto) {
        BOOST_CHECK(false);
    };
    auto onDisconnect = [](auto) {
        BOOST_CHECK(false);
    };
    auto ec {server.Run(
        onClientConnect,
        onClientMessage,
        onClientDisconnect,
        onDisconnect
    )};
    BOOST_REQUIRE_EQUAL(ec, StompServerError::kOk);

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(clientDidConnect);
    BOOST_CHECK(calledOnSend);
}

BOOST_AUTO_TEST_CASE(send_disconnected_connection, *timeout {1})
{
    // Since we use the mock, we do not actually launch a server at this port.
    const std::string host {"localhost"};
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    const std::string destination {"/quiet-route"};
    const nlohmann::json message {
        {"msg", "Hello world"},
    };

    // Setup the mock.
    MockWebsocketServerForStomp::mockEvents = std::queue<MockWebsocketEvent> {{
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kConnect,
            // Succeeds
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            GetMockStompFrame(host)
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kDisconnect,
            boost::asio::error::interrupted,
        },
    }};

    StompServer<MockWebsocketServerForStomp> server {
        host,
        ip,
        port,
        ioc,
        ctx
    };
    auto onSend = [](auto, auto) {
        BOOST_CHECK(false);
    };
    bool clientDidConnect {false};
    auto onClientConnect = [
        &server,
        &clientDidConnect
    ](auto ec, auto id) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kOk);
        BOOST_CHECK(id.size() > 0);
        clientDidConnect = true;
    };
    auto onClientMessage = [](auto, auto, auto, auto, auto&&) {
        BOOST_CHECK(false);
    };
    bool clientDidDisconnect {false};
    auto onClientDisconnect = [
        &clientDidDisconnect,
        &server,
        &destination,
        &message,
        &onSend
    ](auto ec, auto id) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kWebsocketSessionDisconnected);
        BOOST_CHECK(id.size() > 0);
        clientDidDisconnect = true;

        // It should never send the message to begin with.
        auto reqId {server.Send(id, destination, message.dump(), onSend)};
        BOOST_REQUIRE_EQUAL(reqId.size(), 0);
        server.Stop();
    };
    auto onDisconnect = [](auto) {
        BOOST_CHECK(false);
    };
    auto ec {server.Run(
        onClientConnect,
        onClientMessage,
        onClientDisconnect,
        onDisconnect
    )};
    BOOST_REQUIRE_EQUAL(ec, StompServerError::kOk);

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(clientDidConnect);
    BOOST_CHECK(clientDidDisconnect);
}

BOOST_AUTO_TEST_CASE(close, *timeout {1})
{
    // Since we use the mock, we do not actually launch a server at this port.
    const std::string host {"localhost"};
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    // Setup the mock.
    MockWebsocketServerForStomp::mockEvents = std::queue<MockWebsocketEvent> {{
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kConnect,
            // Succeeds
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            GetMockStompFrame(host)
        },
    }};

    StompServer<MockWebsocketServerForStomp> server {
        host,
        ip,
        port,
        ioc,
        ctx
    };
    std::string connectionId {};
    bool clientDidClose {false};
    auto onClientClose = [
        &clientDidClose,
        &connectionId,
        &server
    ](auto ec, auto id) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kOk);
        BOOST_CHECK_EQUAL(id, connectionId);
        clientDidClose = true;

        // This test assumes that Stop works.
        server.Stop();
    };
    bool clientDidConnect {false};
    auto onClientConnect = [
        &server,
        &clientDidConnect,
        &connectionId,
        &onClientClose
    ](auto ec, auto id) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kOk);
        BOOST_CHECK(id.size() > 0);
        clientDidConnect = true;
        connectionId = id;
        server.Close(id, onClientClose);
    };
    auto onClientMessage = [](auto, auto, auto, auto, auto&&) {
        BOOST_CHECK(false);
    };
    auto onClientDisconnect = [](auto, auto) {
        BOOST_CHECK(false);
    };
    auto onDisconnect = [](auto) {
        BOOST_CHECK(false);
    };
    auto ec {server.Run(
        onClientConnect,
        onClientMessage,
        onClientDisconnect,
        onDisconnect
    )};
    BOOST_REQUIRE_EQUAL(ec, StompServerError::kOk);

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(clientDidConnect);
}

BOOST_AUTO_TEST_CASE(close_then_send, *timeout {1})
{
    // Since we use the mock, we do not actually launch a server at this port.
    const std::string host {"localhost"};
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    const std::string destination {"/quiet-route"};
    const nlohmann::json message {
        {"msg", "Hello world"},
    };

    // Setup the mock.
    MockWebsocketServerForStomp::mockEvents = std::queue<MockWebsocketEvent> {{
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kConnect,
            // Succeeds
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            GetMockStompFrame(host)
        },
    }};

    StompServer<MockWebsocketServerForStomp> server {
        host,
        ip,
        port,
        ioc,
        ctx
    };
    std::string connectionId {};
    auto onSend = [](auto, auto) {
        BOOST_CHECK(false);
    };
    bool clientDidClose {false};
    auto onClientClose = [
        &clientDidClose,
        &connectionId,
        &server,
        &destination,
        &message,
        &onSend
    ](auto ec, auto id) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kOk);
        BOOST_CHECK_EQUAL(id, connectionId);
        clientDidClose = true;

        // This test assumes that Stop works.
        // It should never send the message to begin with.
        auto reqId {server.Send(id, destination, message.dump(), onSend)};
        BOOST_REQUIRE_EQUAL(reqId.size(), 0);
        server.Stop();
    };
    bool clientDidConnect {false};
    auto onClientConnect = [
        &server,
        &clientDidConnect,
        &connectionId,
        &onClientClose
    ](auto ec, auto id) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kOk);
        BOOST_CHECK(id.size() > 0);
        clientDidConnect = true;
        connectionId = id;
        server.Close(id, onClientClose);
    };
    auto onClientMessage = [](auto, auto, auto, auto, auto&&) {
        BOOST_CHECK(false);
    };
    auto onClientDisconnect = [](auto, auto) {
        BOOST_CHECK(false);
    };
    auto onDisconnect = [](auto) {
        BOOST_CHECK(false);
    };
    auto ec {server.Run(
        onClientConnect,
        onClientMessage,
        onClientDisconnect,
        onDisconnect
    )};
    BOOST_REQUIRE_EQUAL(ec, StompServerError::kOk);

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(clientDidConnect);
}

BOOST_AUTO_TEST_CASE(three_connections, *timeout {1})
{
    // Since we use the mock, we do not actually launch a server at this port.
    const std::string host {"localhost"};
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    const std::string destination {"/quiet-route"};
    const nlohmann::json message {
        {"msg", "Hello world"},
    };

    // Setup the mock.
    MockWebsocketServerForStomp::mockEvents = std::queue<MockWebsocketEvent> {{
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kConnect,
            // Succeeds
        },
        MockWebsocketEvent {
            "connection1",
            MockWebsocketEvent::Type::kConnect,
            // Succeeds
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            GetMockStompFrame(host)
        },
        MockWebsocketEvent {
            "connection2",
            MockWebsocketEvent::Type::kConnect,
            // Succeeds
        },
        MockWebsocketEvent {
            "connection1",
            MockWebsocketEvent::Type::kDisconnect,
            boost::asio::error::interrupted,
        },
        MockWebsocketEvent {
            "connection2",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            GetMockStompFrame(host)
        },
        MockWebsocketEvent {
            "connection2",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            GetMockSendFrame("msg0", destination, message.dump())
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            GetMockSendFrame("msg1", destination, message.dump())
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kDisconnect,
            boost::asio::error::interrupted,
        },
        MockWebsocketEvent {
            "connection2",
            MockWebsocketEvent::Type::kDisconnect,
            boost::asio::error::interrupted,
        },
    }};

    StompServer<MockWebsocketServerForStomp> server {
        host,
        ip,
        port,
        ioc,
        ctx
    };
    size_t connectedClients {0};
    auto onClientConnect = [&connectedClients](auto ec, auto id) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kOk);
        BOOST_CHECK(id.size() > 0);
        ++connectedClients;
    };
    size_t receivedMessages {0};
    auto onClientMessage = [
        &receivedMessages,
        &message
    ](auto ec, auto id, auto dst, auto reqId, auto&& msg) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kOk);
        BOOST_CHECK(id.size() > 0);
        BOOST_CHECK_EQUAL(msg, message.dump());
        ++receivedMessages;
    };
    size_t disconnectedClients {0};
    auto onClientDisconnect = [
        &disconnectedClients,
        &server
    ](auto ec, auto id) {
        BOOST_CHECK_EQUAL(ec, StompServerError::kWebsocketSessionDisconnected);
        BOOST_CHECK(id.size() > 0);
        ++disconnectedClients;

        if (disconnectedClients == 2) {
            // This test assumes that Stop works.
            server.Stop();
        }
    };
    auto onDisconnect = [](auto) {
        BOOST_CHECK(false);
    };
    auto ec {server.Run(
        onClientConnect,
        onClientMessage,
        onClientDisconnect,
        onDisconnect
    )};
    BOOST_REQUIRE_EQUAL(ec, StompServerError::kOk);

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK_EQUAL(connectedClients, 2);
    BOOST_CHECK_EQUAL(disconnectedClients, 2);
    BOOST_CHECK_EQUAL(receivedMessages, 2);
}

BOOST_AUTO_TEST_SUITE_END(); // class_StompServer

BOOST_AUTO_TEST_SUITE(live);

BOOST_AUTO_TEST_CASE(connect, *timeout {2})
{
    const std::string endpoint {"/quiet-route"};
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};

    using boost::asio::ssl::context;

    boost::asio::io_context ioc {};

    // Server
    // We use the IP as the server hostname because the client will connect to
    // 127.0.0.1 directly, without host name resolution.
    boost::asio::ssl::context serverCtx {context::tlsv12_server};
    serverCtx.load_verify_file(TESTS_CACERT_PEM);
    LoadTestServerCertificate(serverCtx);
    StompServer<BoostWebsocketServer> server {ip, ip, port, ioc, serverCtx};
    bool serverDidConnect {false};
    {
        auto onClientConnect {
            [&serverDidConnect, &server](auto ec, auto id) {
                BOOST_REQUIRE_EQUAL(ec, StompServerError::kOk);
                serverDidConnect = true;

                // This test assumes that Stop works.
                server.Stop();
            }
        };
        auto onClientMessage {
            [](auto ec, auto id, auto dst, auto reqId, auto&& msg) {
                BOOST_CHECK(false);
            }
        };
        auto onClientDisconnect {
            [](auto, auto) {
                BOOST_CHECK(false);
            }
        };
        auto onDisconnect {[](auto) {
            BOOST_CHECK(false);
        }};
        server.Run(
            onClientConnect,
            onClientMessage,
            onClientDisconnect,
            onDisconnect
        );
    }

    // Client
    // The client connects to an IPv4 address directly to prevent the case
    // where `localhost` is resolved to an IPv6 address.
    boost::asio::ssl::context clientCtx {context::tlsv12_client};
    clientCtx.load_verify_file(TESTS_CACERT_PEM);
    StompClient<BoostWebsocketClient> client {
        ip, endpoint, std::to_string(port), ioc, clientCtx
    };
    bool clientDidConnect {false};
    bool clientDidDisconnect {false};
    {
        auto onConnect {[&clientDidConnect](auto ec) {
            BOOST_CHECK_EQUAL(ec, StompClientError::kOk);
            clientDidConnect = true;
        }};
        auto onMessage {[](auto, auto&&, auto&&) {
            BOOST_CHECK(false);
        }};
        auto onDisconnect {[&clientDidDisconnect](auto ec) {
            BOOST_CHECK_EQUAL(
                ec,
                StompClientError::kWebsocketServerDisconnected
            );
            clientDidDisconnect = true;
        }};
        client.Connect("user", "pwd", onConnect, onMessage, onDisconnect);
    }

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(clientDidConnect);
    BOOST_CHECK(clientDidDisconnect);
    BOOST_CHECK(serverDidConnect);
}

BOOST_AUTO_TEST_CASE(exchange_messages, *timeout {2})
{
    const std::string endpoint {"/quiet-route"};
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};

    const std::string destination {"/quiet-route"};
    const nlohmann::json message {
        {"msg", "Hello world"},
    };

    using boost::asio::ssl::context;

    boost::asio::io_context ioc {};

    // Server
    // We use the IP as the server hostname because the client will connect to
    // 127.0.0.1 directly, without host name resolution.
    boost::asio::ssl::context serverCtx {context::tlsv12_server};
    serverCtx.load_verify_file(TESTS_CACERT_PEM);
    LoadTestServerCertificate(serverCtx);
    StompServer<BoostWebsocketServer> server {ip, ip, port, ioc, serverCtx};
    bool serverDidConnect {false};
    bool serverReceivedMsg {false};
    {
        auto onClientConnect {
            [&serverDidConnect](auto ec, auto id) {
                BOOST_REQUIRE_EQUAL(ec, StompServerError::kOk);
                serverDidConnect = true;
            }
        };
        auto onClientMessage {
            [
                &destination,
                &message,
                &serverReceivedMsg,
                &server
            ](auto ec, auto id, auto dst, auto reqId, auto&& msg) {
                BOOST_CHECK_EQUAL(ec, StompServerError::kOk);
                BOOST_CHECK_EQUAL(msg, message.dump());
                serverReceivedMsg = true;

                // Reply to the client.
                server.Send(id, destination, message.dump(),
                    [&server](auto ec, auto id) {
                        BOOST_CHECK_EQUAL(ec, StompServerError::kOk);
                        BOOST_CHECK(id.size() > 0);

                        // This test assumes that Stop works.
                        server.Stop();
                    }
                );
            }
        };
        auto onClientDisconnect {
            [](auto, auto) {
                BOOST_CHECK(false);
            }
        };
        auto onDisconnect {[](auto) {
            BOOST_CHECK(false);
        }};
        server.Run(
            onClientConnect,
            onClientMessage,
            onClientDisconnect,
            onDisconnect
        );
    }

    // Client
    // The client connects to an IPv4 address directly to prevent the case
    // where `localhost` is resolved to an IPv6 address.
    boost::asio::ssl::context clientCtx {context::tlsv12_client};
    clientCtx.load_verify_file(TESTS_CACERT_PEM);
    StompClient<BoostWebsocketClient> client {
        ip, endpoint, std::to_string(port), ioc, clientCtx
    };
    bool clientDidConnect {false};
    bool clientReceivedMsg {false};
    bool clientDidDisconnect {false};
    {
        auto onConnect {[
            &clientDidConnect,
            &destination,
            &message,
            &client
        ](auto ec) {
            BOOST_CHECK_EQUAL(ec, StompClientError::kOk);
            clientDidConnect = true;
            client.Send(destination, message.dump());
        }};
        auto onMessage {[
            &destination,
            &message,
            &clientReceivedMsg
        ](auto ec, auto&& dst, auto&& msg) {
            BOOST_REQUIRE_EQUAL(ec, StompClientError::kOk);
            BOOST_CHECK_EQUAL(dst, destination);
            BOOST_CHECK_EQUAL(msg, message.dump());
            clientReceivedMsg = true;
        }};
        auto onDisconnect {[&clientDidDisconnect](auto ec) {
            BOOST_CHECK_EQUAL(
                ec,
                StompClientError::kWebsocketServerDisconnected
            );
            clientDidDisconnect = true;
        }};
        client.Connect("user", "pwd", onConnect, onMessage, onDisconnect);
    }

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(clientDidConnect);
    BOOST_CHECK(clientReceivedMsg);
    BOOST_CHECK(clientDidDisconnect);
    BOOST_CHECK(serverDidConnect);
    BOOST_CHECK(serverDidConnect);
}

BOOST_AUTO_TEST_CASE(three_connections, *timeout {2})
{
    const std::string endpoint {"/quiet-route"};
    const std::string destination {"/quiet-route"};
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};

    const nlohmann::json message {
        {"msg", "Hello world"},
    };

    using boost::asio::ssl::context;

    boost::asio::io_context ioc {};

    const size_t nClients {3};

    size_t connectedClients {0};
    size_t disconnectedClients {0};
    size_t receivedMessages {0};

    // Server
    // We use the IP as the server hostname because the client will connect to
    // 127.0.0.1 directly, without host name resolution.
    boost::asio::ssl::context serverCtx {context::tlsv12_server};
    serverCtx.load_verify_file(TESTS_CACERT_PEM);
    LoadTestServerCertificate(serverCtx);
    StompServer<BoostWebsocketServer> server {ip, ip, port, ioc, serverCtx};
    auto onClientConnect {
        [&connectedClients](auto ec, auto id) {
            BOOST_CHECK_EQUAL(ec, StompServerError::kOk);
            ++connectedClients;
        }
    };
    auto onClientMessage {
        [
            &message,
            &receivedMessages
        ](auto ec, auto id, auto dst, auto reqId, auto&& msg) {
            BOOST_CHECK_EQUAL(ec, StompServerError::kOk);
            BOOST_CHECK_EQUAL(msg, message.dump());
            ++receivedMessages;
        }
    };
    auto onClientDisconnect {
        [&disconnectedClients, &server, nClients](auto ec, auto id) {
            BOOST_CHECK_EQUAL(
                ec,
                StompServerError::kWebsocketSessionDisconnected
            );
            ++disconnectedClients;

            if (disconnectedClients == nClients) {
                server.Stop();
            }
        }
    };
    auto onServerDisconnect {[](auto) {
        BOOST_CHECK(false);
    }};
    server.Run(
        onClientConnect,
        onClientMessage,
        onClientDisconnect,
        onServerDisconnect
    );

    // Clients
    // The client connects to an IPv4 address directly to prevent the case
    // where `localhost` is resolved to an IPv6 address.
    boost::asio::ssl::context clientCtx {context::tlsv12_client};
    clientCtx.load_verify_file(TESTS_CACERT_PEM);
    std::vector<std::shared_ptr<StompClient<BoostWebsocketClient>>> clients {};
    clients.reserve(nClients);
    for (size_t idx {0}; idx < nClients; ++idx) {
        clients.emplace_back(
            std::make_shared<StompClient<BoostWebsocketClient>>(
                ip,
                endpoint,
                std::to_string(port),
                ioc,
                clientCtx
            )
        );
        auto& client {clients.back()};
        auto onConnect {[&destination, &message, &client](auto ec) {
            BOOST_CHECK_EQUAL(ec, StompClientError::kOk);
            client->Send(destination, message.dump());
            client->Close();
        }};
        auto onMessage {[](auto, auto&&, auto&&) {
            BOOST_CHECK(false);
        }};
        auto onDisconnect {[](auto ec) {
            BOOST_CHECK_EQUAL(
                ec,
                StompClientError::kWebsocketServerDisconnected
            );
        }};
        client->Connect("user", "pwd", onConnect, onMessage, onDisconnect);
    }

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK_EQUAL(connectedClients, nClients);
    BOOST_CHECK_EQUAL(disconnectedClients, nClients);
    BOOST_CHECK_EQUAL(receivedMessages, nClients);
}

BOOST_AUTO_TEST_SUITE_END(); // live

BOOST_AUTO_TEST_SUITE_END(); // stomp_server

BOOST_AUTO_TEST_SUITE_END(); // network_monitor