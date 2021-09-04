#include "BoostMock.h"

#include <network-monitor/WebsocketClient.h>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>

using NetworkMonitor::BoostWebsocketClient;

using NetworkMonitor::MockResolver;
using NetworkMonitor::MockTcpStream;
using NetworkMonitor::MockTlsStream;
using NetworkMonitor::MockTlsWebsocketStream;
using NetworkMonitor::TestWebsocketClient;

// Re-initialize all mock properties before a test
struct WebsocketClientTestFixture {
    WebsocketClientTestFixture()
    {
        MockResolver::resolveEc = {};
        MockTcpStream::connectEc = {};
        MockTlsStream::handshakeEc = {};
        MockTlsWebsocketStream::handshakeEc = {};
        MockTlsWebsocketStream::readEc = {};
        MockTlsWebsocketStream::readBuffer = "";
        MockTlsWebsocketStream::writeEc = {};
        MockTlsWebsocketStream::closeEc = {};
    }
};

// Use this to set a timeout on tests that may hang or suffer from a slow
// connection.
using timeout = boost::unit_test::timeout;

BOOST_AUTO_TEST_SUITE(network_monitor);

BOOST_AUTO_TEST_SUITE(class_WebsocketClient);

BOOST_AUTO_TEST_CASE(cacert_pem)
{
    BOOST_CHECK(std::filesystem::exists(TESTS_CACERT_PEM));
}

BOOST_FIXTURE_TEST_SUITE(Connect, WebsocketClientTestFixture);

BOOST_AUTO_TEST_CASE(fail_resolve, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string url {"some.echo-server.com"};
    const std::string endpoint {"/"};
    const std::string port {"443"};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    boost::asio::io_context ioc {};

    // Set the expected error codes.
    MockResolver::resolveEc = boost::asio::error::host_not_found;

    TestWebsocketClient client {url, endpoint, port, ioc, ctx};
    bool calledOnConnect {false};
    auto onConnect {[&calledOnConnect](auto ec) {
        calledOnConnect = true;
        BOOST_CHECK_EQUAL(ec, boost::asio::error::host_not_found);
    }};
    client.Connect(onConnect);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(calledOnConnect);
}

BOOST_AUTO_TEST_CASE(fail_socket_connect, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string url {"some.echo-server.com"};
    const std::string endpoint {"/"};
    const std::string port {"443"};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    boost::asio::io_context ioc {};

    // Set the expected error codes.
    MockTcpStream::connectEc = boost::asio::error::connection_refused;

    TestWebsocketClient client {url, endpoint, port, ioc, ctx};
    bool calledOnConnect {false};
    auto onConnect {[&calledOnConnect](auto ec) {
        calledOnConnect = true;
        BOOST_CHECK_EQUAL(ec, boost::asio::error::connection_refused);
    }};
    client.Connect(onConnect);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(calledOnConnect);
}

BOOST_AUTO_TEST_CASE(fail_tls_handshake, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string url {"some.echo-server.com"};
    const std::string endpoint {"/"};
    const std::string port {"443"};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    boost::asio::io_context ioc {};

    // Set the expected error codes.
    namespace error = boost::asio::ssl::error;
    MockTlsStream::handshakeEc = error::stream_truncated;

    TestWebsocketClient client {url, endpoint, port, ioc, ctx};
    bool calledOnConnect {false};
    auto onConnect {[&calledOnConnect](auto ec) {
        calledOnConnect = true;
        BOOST_CHECK_EQUAL(ec, error::stream_truncated);
    }};
    client.Connect(onConnect);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(calledOnConnect);
}

BOOST_AUTO_TEST_CASE(fail_websocket_handshake, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string url {"some.echo-server.com"};
    const std::string endpoint {"/"};
    const std::string port {"443"};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    boost::asio::io_context ioc {};

    // Set the expected error codes.
    using error = boost::beast::websocket::error;
    MockTlsWebsocketStream::handshakeEc = error::upgrade_declined;

    TestWebsocketClient client {url, endpoint, port, ioc, ctx};
    bool calledOnConnect {false};
    auto onConnect {[&calledOnConnect](auto ec) {
        calledOnConnect = true;
        BOOST_CHECK(ec == error::upgrade_declined);
    }};
    client.Connect(onConnect);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(calledOnConnect);
}

BOOST_AUTO_TEST_CASE(successful_nothing_to_read, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string url {"some.echo-server.com"};
    const std::string endpoint {"/"};
    const std::string port {"443"};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    boost::asio::io_context ioc {};

    // We don't set any error code because we expect the connection to succeed.

    TestWebsocketClient client {url, endpoint, port, ioc, ctx};
    bool calledOnConnect {false};
    auto onConnect {[&calledOnConnect, &client](auto ec) {
        calledOnConnect = true;
        BOOST_CHECK(!ec);

        // This test assumes that Close() works.
        client.Close();
    }};
    client.Connect(onConnect);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(calledOnConnect);
}

BOOST_AUTO_TEST_CASE(successful_no_connecthandler, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string url {"some.echo-server.com"};
    const std::string endpoint {"/"};
    const std::string port {"443"};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    boost::asio::io_context ioc {};

    // We don't set any error code because we expect the connection to succeed.

    // Because in this test we do not use the onConnect handler, we need to
    // close the connection after some time.
    TestWebsocketClient client {url, endpoint, port, ioc, ctx};
    bool didTimeout {false};
    boost::asio::high_resolution_timer timer(ioc);
    timer.expires_after(std::chrono::milliseconds(250));
    timer.async_wait([&didTimeout, &client](auto ec) {
        didTimeout = true;
        BOOST_CHECK(!ec);

        // This test assumes that Close() works.
        client.Close();
    });
    client.Connect();
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(didTimeout);
}

BOOST_AUTO_TEST_SUITE_END(); // Connect

BOOST_FIXTURE_TEST_SUITE(onMessage, WebsocketClientTestFixture);

BOOST_AUTO_TEST_CASE(one_message, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string url {"some.echo-server.com"};
    const std::string endpoint {"/"};
    const std::string port {"443"};
    const std::string message {"Hello Websocket"};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    boost::asio::io_context ioc {};

    // We don't set any error code because we expect the connection to succeed.
    MockTlsWebsocketStream::readBuffer = message;

    TestWebsocketClient client {url, endpoint, port, ioc, ctx};
    bool calledOnMessage {false};
    auto onMessage {[&calledOnMessage, &message, &client](auto ec, auto msg) {
        calledOnMessage = true;
        BOOST_CHECK(!ec);
        BOOST_CHECK_EQUAL(msg, message);

        // This test assumes that Close() works.
        client.Close();
    }};
    client.Connect(nullptr, onMessage);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(calledOnMessage);
}

BOOST_AUTO_TEST_CASE(two_messages, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string url {"some.echo-server.com"};
    const std::string endpoint {"/"};
    const std::string port {"443"};
    const std::string message {"Hello Websocket"};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    boost::asio::io_context ioc {};

    // We don't set any error code because we expect the connection to succeed.
    MockTlsWebsocketStream::readBuffer = message;

    TestWebsocketClient client {url, endpoint, port, ioc, ctx};
    size_t calledOnMessage {0};
    auto onMessage {[&calledOnMessage, &message, &client](auto ec, auto msg) {
        ++calledOnMessage;
        BOOST_CHECK(!ec);
        BOOST_CHECK_EQUAL(msg, message);

        // Place a new message in the buffer.
        MockTlsWebsocketStream::readBuffer = message;

        // This test assumes that Close() works.
        if (calledOnMessage == 2) {
            client.Close();
        }
    }};
    client.Connect(nullptr, onMessage);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK_EQUAL(calledOnMessage, 2);
}

BOOST_AUTO_TEST_CASE(fail, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string url {"some.echo-server.com"};
    const std::string endpoint {"/"};
    const std::string port {"443"};
    const std::string message {"Hello Websocket"};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    boost::asio::io_context ioc {};

    // Set the expected error codes.
    using error = boost::beast::websocket::error;
    MockTlsWebsocketStream::readBuffer = message;
    MockTlsWebsocketStream::readEc = error::bad_data_frame;

    // Because in this test we do not expect the onMessage handler to be called,
    // we need to close the connection after some time.
    TestWebsocketClient client {url, endpoint, port, ioc, ctx};
    boost::asio::high_resolution_timer timer(ioc);
    timer.expires_after(std::chrono::milliseconds(250));
    timer.async_wait([&client](auto ec) {
        // This test assumes that Close() works.
        client.Close();
    });
    bool calledOnMessage {false};
    auto onMessage {[&calledOnMessage, &message, &client](auto ec, auto msg) {
        // We expect onMessage not to be called on a failing read.
        calledOnMessage = true;
        BOOST_CHECK(false);
    }};
    client.Connect(nullptr, onMessage);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    // We expect onMessage not to be called on a failing read.
    BOOST_CHECK(!calledOnMessage);
}

BOOST_AUTO_TEST_CASE(no_handler, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string url {"some.echo-server.com"};
    const std::string endpoint {"/"};
    const std::string port {"443"};
    const std::string message {"Hello Websocket"};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    boost::asio::io_context ioc {};

    // We don't set any error code because we expect the connection to succeed.
    MockTlsWebsocketStream::readBuffer = message;

    // Because in this test we do not expect the onMessage handler to be called,
    // we need to close the connection after some time.
    TestWebsocketClient client {url, endpoint, port, ioc, ctx};
    boost::asio::high_resolution_timer timer(ioc);
    timer.expires_after(std::chrono::milliseconds(250));
    timer.async_wait([&client](auto ec) {
        // This test assumes that Close() works.
        client.Close();
    });
    client.Connect(nullptr, nullptr);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    // We should get here safely even with nullptr callbacks.
    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_SUITE_END(); // onMessage

BOOST_FIXTURE_TEST_SUITE(Send, WebsocketClientTestFixture);

BOOST_AUTO_TEST_CASE(send_before_connect, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string url {"some.echo-server.com"};
    const std::string endpoint {"/"};
    const std::string port {"443"};
    const std::string message {"Hello Websocket"};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    boost::asio::io_context ioc {};

    TestWebsocketClient client {url, endpoint, port, ioc, ctx};
    bool calledOnSend {false};
    auto onSend {[&calledOnSend, &message](auto ec) {
        calledOnSend = true;
        BOOST_CHECK(ec == boost::asio::error::operation_aborted);
    }};
    client.Send(message, onSend);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(calledOnSend);
}

BOOST_AUTO_TEST_CASE(one_message, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string url {"some.echo-server.com"};
    const std::string endpoint {"/"};
    const std::string port {"443"};
    const std::string message {"Hello Websocket"};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    boost::asio::io_context ioc {};

    // We don't set any error code because we expect the connection to succeed.

    TestWebsocketClient client {url, endpoint, port, ioc, ctx};
    bool calledOnSend {false};
    auto onSend {[&calledOnSend, &message, &client](auto ec) {
        calledOnSend = true;
        BOOST_CHECK(!ec);

        // This test assumes that Close() works.
        client.Close();
    }};
    client.Connect([&client, &message, &onSend](auto ec) {
        BOOST_REQUIRE(!ec);
        client.Send(message, onSend);
    });
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(calledOnSend);
}

BOOST_AUTO_TEST_CASE(fail, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string url {"some.echo-server.com"};
    const std::string endpoint {"/"};
    const std::string port {"443"};
    const std::string message {"Hello Websocket"};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    boost::asio::io_context ioc {};

    // Set the expected error codes.
    using error = boost::beast::websocket::error;
    MockTlsWebsocketStream::writeEc = error::bad_data_frame;

    TestWebsocketClient client {url, endpoint, port, ioc, ctx};
    bool calledOnSend {false};
    auto onSend {[&calledOnSend, &message, &client](auto ec) {
        calledOnSend = true;
        BOOST_CHECK(ec == error::bad_data_frame);

        // This test assumes that Close() works.
        client.Close();
    }};
    client.Connect([&client, &message, &onSend](auto ec) {
        BOOST_REQUIRE(!ec);
        client.Send(message, onSend);
    });
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(calledOnSend);
}

BOOST_AUTO_TEST_SUITE_END(); // Send

BOOST_FIXTURE_TEST_SUITE(Close, WebsocketClientTestFixture);

BOOST_AUTO_TEST_CASE(close, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string url {"some.echo-server.com"};
    const std::string endpoint {"/"};
    const std::string port {"443"};
    const std::string message {"Hello Websocket"};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    boost::asio::io_context ioc {};

    // We don't set any error code because we expect the connection to succeed.

    TestWebsocketClient client {url, endpoint, port, ioc, ctx};
    bool calledOnConnect {false};
    bool calledOnClose {false};
    auto onClose {[&calledOnClose](auto ec) {
        calledOnClose = true;
        BOOST_CHECK(!ec);
    }};
    auto onConnect {[&calledOnConnect, &client, &onClose](auto ec) {
        calledOnConnect = true;
        BOOST_REQUIRE(!ec);
        client.Close(onClose);
    }};
    client.Connect(onConnect);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(calledOnConnect);
    BOOST_CHECK(calledOnClose);
}

BOOST_AUTO_TEST_CASE(close_before_connect, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string url {"some.echo-server.com"};
    const std::string endpoint {"/"};
    const std::string port {"443"};
    const std::string message {"Hello Websocket"};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    boost::asio::io_context ioc {};

    // We don't set any error code because we expect the connection to succeed.

    TestWebsocketClient client {url, endpoint, port, ioc, ctx};
    bool calledOnClose {false};
    auto onClose {[&calledOnClose](auto ec) {
        calledOnClose = true;
        BOOST_CHECK(ec == boost::asio::error::operation_aborted);
    }};
    client.Close(onClose);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(calledOnClose);
}

BOOST_AUTO_TEST_CASE(close_no_disconnect, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string url {"some.echo-server.com"};
    const std::string endpoint {"/"};
    const std::string port {"443"};
    const std::string message {"Hello Websocket"};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    boost::asio::io_context ioc {};

    // We don't set any error code because we expect the connection to succeed.

    TestWebsocketClient client {url, endpoint, port, ioc, ctx};
    bool calledOnConnect {false};
    bool calledOnClose {false};
    auto onClose {[&calledOnClose](auto ec) {
        calledOnClose = true;
        BOOST_CHECK(!ec);
    }};
    auto onConnect {[&calledOnConnect, &client, &onClose](auto ec) {
        calledOnConnect = true;
        BOOST_REQUIRE(!ec);
        client.Close(onClose);
    }};
    auto onDisconnect {[](auto) {
        // onDisconnect should never be called if it was the user to end the
        // connection.
        BOOST_CHECK(false);
    }};
    client.Connect(onConnect);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(calledOnConnect);
    BOOST_CHECK(calledOnClose);
}

BOOST_AUTO_TEST_SUITE_END(); // Close

BOOST_AUTO_TEST_SUITE(live);

// Commented out as the Server echo.websocket.org 
//is inactive so the test will always fail
// BOOST_AUTO_TEST_CASE(echo, *timeout {20})
// {
//     // Connection targets
//     const std::string url {"echo.websocket.org"};
//     const std::string endpoint {"/"};
//     const std::string port {"443"};
//     const std::string message {"Hello Websocket"};

//     // TLS context
//     boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
//     ctx.load_verify_file(TESTS_CACERT_PEM);

//     // Always start with an I/O context object.
//     boost::asio::io_context ioc {};

//     // The class under test
//     BoostWebsocketClient client {url, endpoint, port, ioc, ctx};

//     // We use these flags to check that the connection, send, receive functions
//     // work as expected.
//     bool connected {false};
//     bool messageSent {false};
//     bool messageReceived {false};
//     bool disconnected {false};
//     std::string echo {};

//     // Our own callbacks
//     auto onSend {[&messageSent](auto ec) {
//         messageSent = !ec;
//     }};
//     auto onConnect {[&client, &connected, &onSend, &message](auto ec) {
//         connected = !ec;
//         if (!ec) {
//             client.Send(message, onSend);
//         }
//     }};
//     auto onClose {[&disconnected](auto ec) {
//         disconnected = !ec;
//     }};
//     auto onReceive {[&client,
//                       &onClose,
//                       &messageReceived,
//                       &echo](auto ec, auto received) {
//         messageReceived = !ec;
//         echo = std::move(received);
//         client.Close(onClose);
//     }};

//     // We must call io_context::run for asynchronous callbacks to run.
//     client.Connect(onConnect, onReceive);
//     ioc.run();

//     // When we get here, the io_context::run function has run out of work to do.
//     BOOST_CHECK(connected);
//     BOOST_CHECK(messageSent);
//     BOOST_CHECK(messageReceived);
//     BOOST_CHECK(disconnected);
//     BOOST_CHECK_EQUAL(message, echo);
// }

bool CheckResponse(const std::string& response)
{
    // We do not parse the whole message. We only check that it contains some
    // expected items.
    bool ok {true};
    ok &= response.find("ERROR") != std::string::npos;
    ok &= response.find("ValidationInvalidAuth") != std::string::npos;
    return ok;
}

BOOST_AUTO_TEST_CASE(network_events, *timeout {3})
{
    // Connection targets
    const std::string url {"ltnm.learncppthroughprojects.com"};
    const std::string endpoint {"/network-events"};
    const std::string port {"443"};

    // STOMP frame
    const std::string username {"fake_username"};
    const std::string password {"fake_password"};
    std::stringstream ss {};
    ss << "STOMP" << std::endl
       << "accept-version:1.2" << std::endl
       << "host:transportforlondon.com" << std::endl
       << "login:" << username << std::endl
       << "passcode:" << password << std::endl
       << std::endl // Headers need to be followed by a blank line.
       << '\0'; // The body (even if absent) must be followed by a NULL octet.
    const std::string message {ss.str()};

    // TLS context
    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_client};
    ctx.load_verify_file(TESTS_CACERT_PEM);

    // Always start with an I/O context object.
    boost::asio::io_context ioc {};

    // The class under test
    BoostWebsocketClient client {url, endpoint, port, ioc, ctx};

    // We use these flags to check that the connection, send, receive functions
    // work as expected.
    bool connected {false};
    bool messageSent {false};
    bool messageReceived {false};
    bool disconnected {false};
    std::string response {};

    // Our own callbacks
    auto onSend {[&messageSent](auto ec) {
        messageSent = !ec;
    }};
    auto onConnect {[&client, &connected, &onSend, &message](auto ec) {
        connected = !ec;
        if (!ec) {
            client.Send(message, onSend);
        }
    }};
    auto onClose {[&disconnected](auto ec) {
        disconnected = !ec;
    }};
    auto onReceive {[&client,
                     &onClose,
                     &messageReceived,
                     &response](auto ec, auto received) {
        messageReceived = !ec;
        response = std::move(received);
        client.Close(onClose);
    }};

    // We must call io_context::run for asynchronous callbacks to run.
    client.Connect(onConnect, onReceive);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(connected);
    BOOST_CHECK(messageSent);
    BOOST_CHECK(messageReceived);
    BOOST_CHECK(disconnected);
    BOOST_CHECK(CheckResponse(response));
}

BOOST_AUTO_TEST_SUITE_END(); // live

BOOST_AUTO_TEST_SUITE_END(); // class_WebsocketClient

BOOST_AUTO_TEST_SUITE_END(); // network_monitor