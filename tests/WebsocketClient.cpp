#include <network-monitor/WebsocketClient.h>

#include "BoostMock.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <filesystem>
#include <string>
#include <mutex>

using NetworkMonitor::BoostWebsocketClient;
using NetworkMonitor::MockResolver;
using NetworkMonitor::MockTcpStream;
using NetworkMonitor::TestWebsocketClient;

// Re-initialize all mock properties before a test.
struct WebsocketClientTestFixture {
    WebsocketClientTestFixture()
    {
        MockResolver::resolveEc = {};
        MockTcpStream::connectEc = {};
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

BOOST_AUTO_TEST_SUITE_END(); // Connect

BOOST_AUTO_TEST_SUITE(live);

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