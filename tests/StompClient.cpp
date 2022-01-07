#include "WebsocketClientMock.h"

#include <network-monitor/env.h>
#include <network-monitor/StompClient.h>
#include <network-monitor/WebsocketClient.h>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/test/unit_test.hpp>

#include <nlohmann/json.hpp>

#include <string>

using NetworkMonitor::BoostWebsocketClient;
using NetworkMonitor::GetEnvVar;
using NetworkMonitor::MockWebsocketClientForStomp;
using NetworkMonitor::StompClient;
using NetworkMonitor::StompClientError;
using NetworkMonitor::StompCommand;
using NetworkMonitor::StompError;
using NetworkMonitor::StompFrame;

using namespace std::string_literals;

// Use this to set a timeout on tests that may hang or suffer from a slow
// connection.
using timeout = boost::unit_test::timeout;

// This fixture is used to re-initialize all mock properties before a test.
struct StompClientTestFixture {
    StompClientTestFixture()
    {
        MockWebsocketClientForStomp::endpoint = "/passengers";
        MockWebsocketClientForStomp::username = "some_username";
        MockWebsocketClientForStomp::password = "some_password_123";
        MockWebsocketClientForStomp::connectEc = {};
        MockWebsocketClientForStomp::sendEc = {};
        MockWebsocketClientForStomp::closeEc = {};
        MockWebsocketClientForStomp::triggerDisconnection = false;
        MockWebsocketClientForStomp::messageQueue = {};
        MockWebsocketClientForStomp::subscriptionMessages = {};
    }
};

BOOST_AUTO_TEST_SUITE(network_monitor);

BOOST_AUTO_TEST_SUITE(stomp_client);

BOOST_AUTO_TEST_SUITE(enum_class_StompClientError);

BOOST_AUTO_TEST_CASE(ostream)
{
    std::stringstream invalidSs;
    invalidSs << StompClientError::kUndefinedError;
    auto invalid {invalidSs.str()};
    for (const auto& error: {
        StompClientError::kOk,
        StompClientError::kCouldNotCloseWebsocketConnection,
        StompClientError::kCouldNotConnectToWebsocketServer,
        StompClientError::kCouldNotParseMessageAsStompFrame,
        StompClientError::kCouldNotSendMessage,
        StompClientError::kCouldNotSendStompFrame,
        StompClientError::kCouldNotSendSubscribeFrame,
        StompClientError::kUnexpectedCouldNotCreateValidFrame,
        StompClientError::kUnexpectedMessageContentType,
        StompClientError::kUnexpectedSubscriptionMismatch,
        StompClientError::kWebsocketServerDisconnected,
    }) {
        std::stringstream ss {};
        ss << error;
        BOOST_CHECK(ss.str() != invalid);
    }
}

BOOST_AUTO_TEST_SUITE_END(); // enum_class_StompClientError

BOOST_FIXTURE_TEST_SUITE(class_StompClient, StompClientTestFixture);

BOOST_AUTO_TEST_CASE(connect, *timeout {1})
{
    // Since we use the mock, we do not actually connect to this remote.
    const std::string url {"ltnm.learncppthroughprojects.com"};
    const std::string endpoint {"/network-events"};
    const std::string port {"443"};
    const std::string username {"some_username"};
    const std::string password {"some_password_123"};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    StompClient<MockWebsocketClientForStomp> client {
        url,
        endpoint,
        port,
        ioc,
        ctx
    };
    bool connected {false};
    auto onConnect {[&client, &connected](auto ec) {
        connected = true;
        BOOST_CHECK_EQUAL(ec, StompClientError::kOk);
        client.Close([](auto ec) {});
    }};
    client.Connect(username, password, onConnect);
    ioc.run();
    BOOST_CHECK(connected);
}

BOOST_AUTO_TEST_CASE(connect_nullptr, *timeout {1})
{
    // Since we use the mock, we do not actually connect to this remote.
    const std::string url {"ltnm.learncppthroughprojects.com"};
    const std::string endpoint {"/network-events"};
    const std::string port {"443"};
    const std::string username {"some_username"};
    const std::string password {"some_password_123"};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    // Because in this test we do not use the onConnect handler, we need to
    // close the connection after some time.
    // Note: This test does not actually check that we connect, only that we do
    //       not fail because of the nullptr callback.
    StompClient<MockWebsocketClientForStomp> client {
        url,
        endpoint,
        port,
        ioc,
        ctx
    };
    client.Connect(username, password, nullptr);
    bool didTimeout {false};
    boost::asio::high_resolution_timer timer {ioc};
    timer.expires_after(std::chrono::milliseconds {250});
    timer.async_wait([&didTimeout, &client](auto ec) {
        didTimeout = true;
        BOOST_CHECK(!ec);
        client.Close([](auto ec) {});
    });
    ioc.run();
    BOOST_CHECK(didTimeout);
}

BOOST_AUTO_TEST_CASE(fail_to_connect_ws, *timeout {1})
{
    // Since we use the mock, we do not actually connect to this remote.
    const std::string url {"ltnm.learncppthroughprojects.com"};
    const std::string endpoint {"/network-events"};
    const std::string port {"443"};
    const std::string username {"some_username"};
    const std::string password {"some_password_123"};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    // Setup the mock.
    namespace error = boost::asio::ssl::error;
    MockWebsocketClientForStomp::connectEc = error::stream_truncated;

    StompClient<MockWebsocketClientForStomp> client {
        url,
        endpoint,
        port,
        ioc,
        ctx
    };
    bool calledOnConnect {false};
    auto onConnect {[&calledOnConnect](auto ec) {
        calledOnConnect = true;
        BOOST_CHECK_EQUAL(
            ec,
            StompClientError::kCouldNotConnectToWebsocketServer
        );
    }};
    client.Connect(username, password, onConnect);
    ioc.run();
    BOOST_CHECK(calledOnConnect);
}

BOOST_AUTO_TEST_CASE(fail_to_connect_auth, *timeout {1})
{
    // Since we use the mock, we do not actually connect to this remote.
    const std::string url {"ltnm.learncppthroughprojects.com"};
    const std::string endpoint {"/network-events"};
    const std::string port {"443"};
    const std::string username {"some_username"};
    const std::string password {"some_bad_password_123"}; // Bad password
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    // When we fail to authenticate, the serve closes our connection.
    StompClient<MockWebsocketClientForStomp> client {
        url,
        endpoint,
        port,
        ioc,
        ctx
    };
    auto onConnect {[](auto ec) {
        // We should never get here.
        BOOST_CHECK(false);
    }};
    bool calledOnDisconnect {false};
    auto onDisconnect {[&calledOnDisconnect](auto ec) {
        calledOnDisconnect = true;
        BOOST_CHECK_EQUAL(
            ec,
            StompClientError::kWebsocketServerDisconnected
        );
    }};
    client.Connect(username, password, onConnect, nullptr, onDisconnect);
    ioc.run();
    BOOST_CHECK(calledOnDisconnect);
}

BOOST_AUTO_TEST_CASE(close, *timeout {1})
{
    // Since we use the mock, we do not actually connect to this remote.
    const std::string url {"ltnm.learncppthroughprojects.com"};
    const std::string endpoint {"/network-events"};
    const std::string port {"443"};
    const std::string username {"some_username"};
    const std::string password {"some_password_123"};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    StompClient<MockWebsocketClientForStomp> client {
        url,
        endpoint,
        port,
        ioc,
        ctx
    };
    bool closed {false};
    auto onClose {[&closed](auto ec) {
        closed = true;
        BOOST_CHECK_EQUAL(ec, StompClientError::kOk);
    }};
    auto onConnect {[&client, &onClose](auto ec) {
        BOOST_REQUIRE_EQUAL(ec, StompClientError::kOk);
        client.Close(onClose);
    }};
    client.Connect(username, password, onConnect);
    ioc.run();
    BOOST_CHECK(closed);
}

BOOST_AUTO_TEST_CASE(close_nullptr, *timeout {1})
{
    // Since we use the mock, we do not actually connect to this remote.
    const std::string url {"ltnm.learncppthroughprojects.com"};
    const std::string endpoint {"/network-events"};
    const std::string port {"443"};
    const std::string username {"some_username"};
    const std::string password {"some_password_123"};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    StompClient<MockWebsocketClientForStomp> client {
        url,
        endpoint,
        port,
        ioc,
        ctx
    };
    bool connected {false};
    auto onConnect {[&client, &connected](auto ec) {
        connected = true;
        BOOST_REQUIRE_EQUAL(ec, StompClientError::kOk);
        client.Close(nullptr);
    }};
    client.Connect(username, password, onConnect);
    ioc.run();
    // If we got here the Close() worked.
    BOOST_CHECK(connected);
}

BOOST_AUTO_TEST_CASE(close_before_connect, *timeout {1})
{
    // Since we use the mock, we do not actually connect to this remote.
    const std::string url {"ltnm.learncppthroughprojects.com"};
    const std::string endpoint {"/network-events"};
    const std::string port {"443"};
    const std::string username {"some_username"};
    const std::string password {"some_password_123"};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    StompClient<MockWebsocketClientForStomp> client {
        url,
        endpoint,
        port,
        ioc,
        ctx
    };
    bool closed {false};
    auto onClose {[&closed](auto ec) {
        closed = true;
        BOOST_CHECK_EQUAL(
            ec,
            StompClientError::kCouldNotCloseWebsocketConnection
        );
    }};
    client.Close(onClose);
    ioc.run();
    BOOST_CHECK(closed);
}

BOOST_AUTO_TEST_CASE(subscribe, *timeout {1})
{
    // Since we use the mock, we do not actually connect to this remote.
    const std::string url {"ltnm.learncppthroughprojects.com"};
    const std::string endpoint {"/network-events"};
    const std::string port {"443"};
    const std::string username {"some_username"};
    const std::string password {"some_password_123"};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    StompClient<MockWebsocketClientForStomp> client {
        url,
        endpoint,
        port,
        ioc,
        ctx
    };
    bool calledOnSubscribe {false};
    auto onSubscribe {[&calledOnSubscribe, &client](auto ec, auto&& id) {
        calledOnSubscribe = true;
        BOOST_CHECK_EQUAL(ec, StompClientError::kOk);
        BOOST_CHECK(id != "");
        client.Close([](auto ec) {});
    }};
    auto onMessage {[](auto ec, auto&& msg) {
    }};
    auto onConnect {[&client, &onSubscribe, &onMessage](auto ec) {
        BOOST_REQUIRE_EQUAL(ec, StompClientError::kOk);
        auto id {client.Subscribe("/passengers", onSubscribe, onMessage)};
        BOOST_REQUIRE(id != "");
    }};
    client.Connect(username, password, onConnect);
    ioc.run();
    BOOST_CHECK(calledOnSubscribe);
}

BOOST_AUTO_TEST_CASE(subscribe_onSubscribe_nullptr, *timeout {1})
{
    // Since we use the mock, we do not actually connect to this remote.
    const std::string url {"ltnm.learncppthroughprojects.com"};
    const std::string endpoint {"/network-events"};
    const std::string port {"443"};
    const std::string username {"some_username"};
    const std::string password {"some_password_123"};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    // Setup the mock.
    MockWebsocketClientForStomp::subscriptionMessages = {
        "{counter: 1}",
    };

    // This test requires the subscription to send a valid message for us to
    // say: "yes, we did subscribe".
    StompClient<MockWebsocketClientForStomp> client {
        url,
        endpoint,
        port,
        ioc,
        ctx
    };
    bool subscribed {false};
    auto onMessage {[&subscribed, &client](auto ec, auto&& msg) {
        BOOST_CHECK_EQUAL(ec, StompClientError::kOk);
        subscribed = true;
        client.Close([](auto ec) {});
    }};
    auto onConnect {[&client, &onMessage](auto ec) {
        BOOST_REQUIRE_EQUAL(ec, StompClientError::kOk);
        client.Subscribe("/passengers", nullptr, onMessage);
    }};
    client.Connect(username, password, onConnect);
    ioc.run();
    BOOST_CHECK(subscribed);
}

BOOST_AUTO_TEST_CASE(subscribe_onMessage_nullptr, *timeout {1})
{
    // Since we use the mock, we do not actually connect to this remote.
    const std::string url {"ltnm.learncppthroughprojects.com"};
    const std::string endpoint {"/network-events"};
    const std::string port {"443"};
    const std::string username {"some_username"};
    const std::string password {"some_password_123"};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    StompClient<MockWebsocketClientForStomp> client {
        url,
        endpoint,
        port,
        ioc,
        ctx
    };
    bool calledOnSubscribe {false};
    auto onSubscribe {[&calledOnSubscribe, &client](auto ec, auto&& id) {
        calledOnSubscribe = true;
        BOOST_CHECK_EQUAL(ec, StompClientError::kOk);
        BOOST_CHECK(id != "");
        client.Close([](auto ec) {});
    }};
    auto onConnect {[&client, &onSubscribe](auto ec) {
        BOOST_REQUIRE_EQUAL(ec, StompClientError::kOk);
        client.Subscribe("/passengers", onSubscribe, nullptr);
    }};
    client.Connect(username, password, onConnect);
    ioc.run();
    BOOST_CHECK(calledOnSubscribe);
}

BOOST_AUTO_TEST_CASE(subscribe_get_message, *timeout {1})
{
    // Since we use the mock, we do not actually connect to this remote.
    const std::string url {"ltnm.learncppthroughprojects.com"};
    const std::string endpoint {"/network-events"};
    const std::string port {"443"};
    const std::string username {"some_username"};
    const std::string password {"some_password_123"};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    // Setup the mock.
    MockWebsocketClientForStomp::subscriptionMessages = {
        "{counter: 1}",
    };

    StompClient<MockWebsocketClientForStomp> client {
        url,
        endpoint,
        port,
        ioc,
        ctx
    };
    bool messageReceived {false};
    auto onMessage {[&messageReceived, &client](auto ec, auto&& msg) {
        messageReceived = true;
        BOOST_CHECK_EQUAL(ec, StompClientError::kOk);
        client.Close([](auto ec) {});
    }};
    auto onConnect {[&client, &onMessage](auto ec) {
        BOOST_REQUIRE_EQUAL(ec, StompClientError::kOk);
        client.Subscribe("/passengers", nullptr, onMessage);
    }};
    client.Connect(username, password, onConnect);
    ioc.run();
    BOOST_CHECK(messageReceived);
}

BOOST_AUTO_TEST_CASE(subscribe_before_connect, *timeout {1})
{
    // Since we use the mock, we do not actually connect to this remote.
    const std::string url {"ltnm.learncppthroughprojects.com"};
    const std::string endpoint {"/network-events"};
    const std::string port {"443"};
    const std::string username {"some_username"};
    const std::string password {"some_password_123"};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    StompClient<MockWebsocketClientForStomp> client {
        url,
        endpoint,
        port,
        ioc,
        ctx
    };
    bool calledOnSubscribe {false};
    auto onSubscribe {[&calledOnSubscribe, &client](auto ec, auto&& id) {
        calledOnSubscribe = true;
        BOOST_CHECK_EQUAL(ec, StompClientError::kCouldNotSendSubscribeFrame);
        BOOST_CHECK_EQUAL(id, "");
        client.Close([](auto ec) {});
    }};
    auto onMessage {[](auto ec, auto&& msg) {
        // We should never get here.
        BOOST_CHECK(false);
    }};
    client.Subscribe("/passengers", onSubscribe, onMessage);
    ioc.run();
    BOOST_CHECK(calledOnSubscribe);
}

BOOST_AUTO_TEST_CASE(subscribe_after_close, *timeout {1})
{
    // Since we use the mock, we do not actually connect to this remote.
    const std::string url {"ltnm.learncppthroughprojects.com"};
    const std::string endpoint {"/network-events"};
    const std::string port {"443"};
    const std::string username {"some_username"};
    const std::string password {"some_password_123"};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    StompClient<MockWebsocketClientForStomp> client {
        url,
        endpoint,
        port,
        ioc,
        ctx
    };
    bool calledOnSubscribe {false};
    auto onSubscribe {[&calledOnSubscribe](auto ec, auto&& id) {
        calledOnSubscribe = true;
        BOOST_CHECK_EQUAL(ec, StompClientError::kCouldNotSendSubscribeFrame);
        BOOST_CHECK_EQUAL(id, "");
    }};
    auto onClose {[&client, &onSubscribe](auto ec) {
        BOOST_REQUIRE_EQUAL(ec, StompClientError::kOk);
        client.Subscribe("/passengers", onSubscribe, nullptr);
    }};
    auto onConnect {[&client, &onClose](auto ec) {
        BOOST_REQUIRE_EQUAL(ec, StompClientError::kOk);
        client.Close(onClose);
    }};
    client.Connect(username, password, onConnect);
    ioc.run();
    BOOST_CHECK(calledOnSubscribe);
}

BOOST_AUTO_TEST_CASE(send, *timeout {1})
{
    // Since we use the mock, we do not actually connect to this remote.
    const std::string url {"ltnm.learncppthroughprojects.com"};
    const std::string endpoint {"/network-events"};
    const std::string port {"443"};
    const std::string username {"some_username"};
    const std::string password {"some_password_123"};
    const nlohmann::json message {
        {"msg", "Hello world"},
    };
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    StompClient<MockWebsocketClientForStomp> client {
        url,
        endpoint,
        port,
        ioc,
        ctx
    };
    bool calledOnSend {false};
    std::string requestId {};
    auto onSend {[
        &calledOnSend,
        &requestId,
        &client
    ](auto ec, auto&& id) {
        calledOnSend = true;
        BOOST_CHECK_EQUAL(ec, StompClientError::kOk);
        BOOST_CHECK(id.size() > 0);
        BOOST_CHECK_EQUAL(id, requestId);
        client.Close();
    }};
    auto onConnect {[&client, &requestId, &message, &onSend](auto ec) {
        BOOST_REQUIRE_EQUAL(ec, StompClientError::kOk);
        requestId = client.Send("/quiet-route", message.dump(), onSend);
        BOOST_CHECK(requestId.size() > 0);
    }};
    bool calledOnDisconnect {false};
    auto onDisconnect {[&calledOnDisconnect](auto ec) {
        calledOnDisconnect = true;
    }};
    client.Connect(username, password, onConnect, nullptr, onDisconnect);
    ioc.run();
    BOOST_CHECK(calledOnSend);
    BOOST_CHECK(!calledOnDisconnect);
}

BOOST_AUTO_TEST_CASE(send_fail, *timeout {1})
{
    // Since we use the mock, we do not actually connect to this remote.
    const std::string url {"ltnm.learncppthroughprojects.com"};
    const std::string endpoint {"/network-events"};
    const std::string port {"443"};
    const std::string username {"some_username"};
    const std::string password {"some_password_123"};
    const nlohmann::json message {
        {"msg", "Hello world"},
    };
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    StompClient<MockWebsocketClientForStomp> client {
        url,
        endpoint,
        port,
        ioc,
        ctx
    };
    bool calledOnSend {false};
    std::string requestId {};
    auto onSend {[
        &calledOnSend,
        &requestId,
        &client
    ](auto ec, auto&& id) {
        calledOnSend = true;
        BOOST_CHECK_EQUAL(ec, StompClientError::kCouldNotSendMessage);
        BOOST_CHECK(id.size() > 0);
        BOOST_CHECK_EQUAL(id, requestId);
        client.Close();
    }};
    auto onConnect {[&client, &requestId, &message, &onSend](auto ec) {
        BOOST_REQUIRE_EQUAL(ec, StompClientError::kOk);

        // We should not set the mock error code earlier, as this would affect
        // the initial STOMP frame used to establish the connection.
        MockWebsocketClientForStomp::sendEc = boost::asio::error::message_size;

        requestId = client.Send("/quiet-route", message.dump(), onSend);
        BOOST_CHECK(requestId.size() > 0);
    }};
    bool calledOnDisconnect {false};
    auto onDisconnect {[&calledOnDisconnect](auto ec) {
        calledOnDisconnect = true;
    }};
    client.Connect(username, password, onConnect, nullptr, onDisconnect);
    ioc.run();
    BOOST_CHECK(calledOnSend);
    BOOST_CHECK(!calledOnDisconnect);
}

BOOST_AUTO_TEST_CASE(receive_message, *timeout {1})
{
    // Since we use the mock, we do not actually connect to this remote.
    const std::string url {"ltnm.learncppthroughprojects.com"};
    const std::string endpoint {"/network-events"};
    const std::string port {"443"};
    const std::string username {"some_username"};
    const std::string password {"some_password_123"};
    const std::string destination {"/msg-destination"};
    const nlohmann::json message {
        {"msg", "Hello world"},
    };
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    StompClient<MockWebsocketClientForStomp> client {
        url,
        endpoint,
        port,
        ioc,
        ctx
    };
    bool calledOnMessage {false};
    std::string requestId {};
    auto onMessage {[
        &calledOnMessage,
        &destination,
        &message,
        &client
    ](auto ec, auto&& dst, auto&& msg) {
        calledOnMessage = true;
        BOOST_CHECK_EQUAL(ec, StompClientError::kOk);
        BOOST_CHECK_EQUAL(dst, destination);
        BOOST_CHECK_EQUAL(msg, message.dump());
        client.Close();
    }};
    auto onConnect {[&destination, &message](auto ec) {
        BOOST_REQUIRE_EQUAL(ec, StompClientError::kOk);

        // Trigger the message from the server.
        MockWebsocketClientForStomp::messageQueue.push(
            MockWebsocketClientForStomp::GetMockSendFrame(
                destination,
                message.dump()
            )
        );
    }};
    bool calledOnDisconnect {false};
    auto onDisconnect {[&calledOnDisconnect](auto ec) {
        calledOnDisconnect = true;
    }};
    client.Connect(username, password, onConnect, onMessage, onDisconnect);
    ioc.run();
    BOOST_CHECK(calledOnMessage);
    BOOST_CHECK(!calledOnDisconnect);
}

BOOST_AUTO_TEST_CASE(subscribe_to_invalid_endpoint, *timeout {1})
{
    // Since we use the mock, we do not actually connect to this remote.
    const std::string url {"ltnm.learncppthroughprojects.com"};
    const std::string endpoint {"/network-events"};
    const std::string port {"443"};
    const std::string username {"some_username"};
    const std::string password {"some_password_123"};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    StompClient<MockWebsocketClientForStomp> client {
        url,
        endpoint,
        port,
        ioc,
        ctx
    };
    auto onSubscribe {[](auto ec, auto&& id) {
        // We should never get here.
        BOOST_CHECK(false);
    }};
    bool calledOnDisconnect {false};
    auto onDisconnect {[&calledOnDisconnect](auto ec) {
        calledOnDisconnect = true;
        BOOST_CHECK_EQUAL(
            ec,
            StompClientError::kWebsocketServerDisconnected
        );
    }};
    auto onConnect {[&client, &onSubscribe](auto ec) {
        BOOST_REQUIRE_EQUAL(ec, StompClientError::kOk);
        client.Subscribe("/invalid", onSubscribe, nullptr);
    }};
    client.Connect(username, password, onConnect, nullptr, onDisconnect);
    ioc.run();
    BOOST_CHECK(calledOnDisconnect);
}

BOOST_AUTO_TEST_CASE(live, *timeout {3})
{
    const std::string url {GetEnvVar(
        "LTNM_SERVER_URL",
        "ltnm.learncppthroughprojects.com"
    )};
    const std::string endpoint {"/network-events"};
    const std::string port {GetEnvVar("LTNM_SERVER_PORT", "443")};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    const std::string username {GetEnvVar("LTNM_USERNAME")};
    const std::string password {GetEnvVar("LTNM_PASSWORD")};

    StompClient<BoostWebsocketClient> client {
        url,
        endpoint,
        port,
        ioc,
        ctx
    };

    bool calledOnClose {false};
    auto onClose {[&calledOnClose](auto ec) {
        calledOnClose = true;
        BOOST_REQUIRE_EQUAL(ec, StompClientError::kOk);
    }};

    // We cannot guarantee that we will get a message, so we close the
    // connection on a successful subscription.
    bool calledOnSubscribe {false};
    auto onSubscribe {[
        &calledOnSubscribe,
        &client,
        &onClose
    ](auto ec, auto&& id) {
        calledOnSubscribe = true;
        BOOST_REQUIRE_EQUAL(ec, StompClientError::kOk);
        BOOST_REQUIRE(id != "");
        client.Close(onClose);
    }};

    // Receiving messages from the live service is not guaranteed, as it depends
    // on the time of the day. If we do receive a message, we check that it is
    // valid.
    auto onMessage {[](auto ec, auto&& msg) {
        BOOST_CHECK_EQUAL(ec, StompClientError::kOk);
    }};

    bool calledOnConnect {false};
    auto onConnect {[
        &calledOnConnect,
        &client,
        &onSubscribe,
        &onMessage
    ](auto ec) {
        calledOnConnect = true;
        BOOST_REQUIRE_EQUAL(ec, StompClientError::kOk);
        auto id {client.Subscribe("/passengers", onSubscribe, onMessage)};
        BOOST_REQUIRE(id != "");
    }};

    client.Connect(username, password, onConnect);

    ioc.run();

    BOOST_CHECK(calledOnConnect);
    BOOST_CHECK(calledOnSubscribe);
    BOOST_CHECK(calledOnClose);
}

BOOST_AUTO_TEST_SUITE_END(); // class_StompClient

BOOST_AUTO_TEST_SUITE_END(); // stomp_client

BOOST_AUTO_TEST_SUITE_END(); // network_monitor