#include "BoostMock.h"
#include <network-monitor/WebsocketServer.h>
#include <network-monitor/TestServerCertificate.h>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/test/unit_test.hpp>

#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>

using NetworkMonitor::BoostWebsocketClient;
using NetworkMonitor::BoostWebsocketServer;
using NetworkMonitor::LoadTestServerCertificate;
using NetworkMonitor::MockAcceptor;
using NetworkMonitor::MockTcpStream;
using NetworkMonitor::MockTlsStream;
using NetworkMonitor::MockTlsWebsocketStream;
using NetworkMonitor::TestWebsocketServer;
using NetworkMonitor::WebsocketSession;

// This fixture is used to re-initialize all mock properties before a test.
struct WebsocketServerTestFixture {
    WebsocketServerTestFixture()
    {
        MockAcceptor::openEc = {};
        MockAcceptor::bindEc = {};
        MockAcceptor::listenEc = {};
        MockAcceptor::acceptEc = {};
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

BOOST_AUTO_TEST_SUITE(class_WebsocketServer);

BOOST_FIXTURE_TEST_SUITE(Run, WebsocketServerTestFixture);

BOOST_AUTO_TEST_CASE(fail_run_cant_open, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    LoadTestServerCertificate(ctx);
    boost::asio::io_context ioc {};

    // Set the expected error codes.
    MockAcceptor::openEc = boost::asio::error::address_family_not_supported;

    TestWebsocketServer server {ip, port, ioc, ctx};
    auto onConnect {[&server](auto, auto) {
        // We should never get here.
        BOOST_TEST(false);
        server.Stop();
    }};
    auto ec {server.Run(onConnect)};
    BOOST_CHECK_EQUAL(ec, MockAcceptor::openEc);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
}

BOOST_AUTO_TEST_CASE(fail_run_cant_bind, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    LoadTestServerCertificate(ctx);
    boost::asio::io_context ioc {};

    // Set the expected error codes.
    MockAcceptor::bindEc = boost::asio::error::access_denied;

    TestWebsocketServer server {ip, port, ioc, ctx};
    auto onConnect {[&server](auto, auto) {
        // We should never get here.
        BOOST_TEST(false);
        server.Stop();
    }};
    auto ec {server.Run(onConnect)};
    BOOST_CHECK_EQUAL(ec, MockAcceptor::bindEc);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
}

BOOST_AUTO_TEST_CASE(fail_run_cant_listen, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    LoadTestServerCertificate(ctx);
    boost::asio::io_context ioc {};

    // Set the expected error codes.
    MockAcceptor::listenEc = boost::asio::error::access_denied;

    TestWebsocketServer server {ip, port, ioc, ctx};
    auto onConnect {[&server](auto, auto) {
        // We should never get here.
        BOOST_TEST(false);
        server.Stop();
    }};
    auto ec {server.Run(onConnect)};
    BOOST_CHECK_EQUAL(ec, MockAcceptor::listenEc);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
}

BOOST_AUTO_TEST_CASE(fail_run_cant_accept, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    LoadTestServerCertificate(ctx);
    boost::asio::io_context ioc {};

    // Set the expected error codes.
    MockAcceptor::acceptEc = std::queue<boost::system::error_code> {
        {boost::asio::error::access_denied}
    };

    TestWebsocketServer server {ip, port, ioc, ctx};
    auto onConnect {[&server](auto, auto) {
        // We should never get here.
        // The server will be listening to incoming connections but none should
        // be accepted.
        BOOST_TEST(false);
        server.Stop();
    }};
    auto ec {server.Run(onConnect)};
    BOOST_REQUIRE(!ec);

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

BOOST_AUTO_TEST_CASE(listen_live, *timeout {1})
{
    // Note: This test runs live.
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    LoadTestServerCertificate(ctx);
    boost::asio::io_context ioc {};

    BoostWebsocketServer server {ip, port, ioc, ctx};
    auto onConnect {[&server](auto, auto) {
        // We should never get here.
        // The server will be listening to incoming connections but none should
        // arrive.
        BOOST_TEST(false);
        server.Stop();
    }};
    auto ec {server.Run(onConnect)};
    BOOST_REQUIRE(!ec);

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

BOOST_AUTO_TEST_SUITE_END(); // Run

BOOST_FIXTURE_TEST_SUITE(Session, WebsocketServerTestFixture);

BOOST_AUTO_TEST_CASE(fail_tls_handshake, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    LoadTestServerCertificate(ctx);
    boost::asio::io_context ioc {};

    // Set the expected error codes.
    MockAcceptor::acceptEc = std::queue<boost::system::error_code> {{
        boost::system::error_code {}, // One successful connection
    }};
    namespace error = boost::asio::ssl::error;
    MockTlsStream::handshakeEc = error::stream_truncated;

    TestWebsocketServer server {ip, port, ioc, ctx};
    size_t nSessions {0};
    auto onConnect {[&server, &nSessions](auto ec, auto session) {
        BOOST_CHECK_EQUAL(ec, MockTlsStream::handshakeEc);
        BOOST_REQUIRE(session != nullptr);
        ++nSessions;

        // This test assumes that Close() and Stop() work.
        session->Close();
        if (MockAcceptor::acceptEc.size() == 0) {
            server.Stop();
        }
    }};
    auto ec {server.Run(onConnect)};
    BOOST_REQUIRE(!ec);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK_EQUAL(nSessions, 1);
}

BOOST_AUTO_TEST_CASE(fail_tls_handshake_two_connections, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    LoadTestServerCertificate(ctx);
    boost::asio::io_context ioc {};

    // Set the expected error codes.
    MockAcceptor::acceptEc = std::queue<boost::system::error_code> {{
        boost::system::error_code {}, // One successful connection
        boost::system::error_code {}, // One successful connection
    }};
    namespace error = boost::asio::ssl::error;
    MockTlsStream::handshakeEc = error::stream_truncated;

    TestWebsocketServer server {ip, port, ioc, ctx};
    size_t nSessions {0};
    auto onConnect {[&server, &nSessions](auto ec, auto session) {
        BOOST_CHECK_EQUAL(ec, MockTlsStream::handshakeEc);
        BOOST_REQUIRE(session != nullptr);
        ++nSessions;

        // This test assumes that Close() and Stop() work.
        session->Close();
        if (MockAcceptor::acceptEc.size() == 0) {
            server.Stop();
        }
    }};
    auto ec {server.Run(onConnect)};
    BOOST_REQUIRE(!ec);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK_EQUAL(nSessions, 2);
}

BOOST_AUTO_TEST_CASE(fail_websocket_handshake, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    LoadTestServerCertificate(ctx);
    boost::asio::io_context ioc {};

    // Set the expected error codes.
    MockAcceptor::acceptEc = std::queue<boost::system::error_code> {{
        boost::system::error_code {}, // One successful connection
    }};
    using error = boost::beast::websocket::error;
    MockTlsWebsocketStream::handshakeEc = error::upgrade_declined;

    TestWebsocketServer server {ip, port, ioc, ctx};
    size_t nSessions {0};
    auto onConnect {[&server, &nSessions](auto ec, auto session) {
        BOOST_CHECK_EQUAL(ec, MockTlsWebsocketStream::handshakeEc);
        BOOST_REQUIRE(session != nullptr);
        ++nSessions;

        // This test assumes that Close() and Stop() work.
        session->Close();
        if (MockAcceptor::acceptEc.size() == 0) {
            server.Stop();
        }
    }};
    auto ec {server.Run(onConnect)};
    BOOST_REQUIRE(!ec);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK_EQUAL(nSessions, 1);
}

BOOST_AUTO_TEST_CASE(Close, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    LoadTestServerCertificate(ctx);
    boost::asio::io_context ioc {};

    // We don't set any error code because we expect the connection to succeed.
    MockAcceptor::acceptEc = std::queue<boost::system::error_code> {{
        boost::system::error_code {}, // One successful connection
    }};

    TestWebsocketServer server {ip, port, ioc, ctx};
    size_t nSessions {0};
    bool closed {false};
    auto onConnect {[&server, &nSessions, &closed](auto ec, auto session) {
        BOOST_CHECK(!ec);
        BOOST_CHECK(session != nullptr);
        ++nSessions;
        session->Close([&server, &closed](auto ec) {
            BOOST_CHECK(!ec);
            closed = true;
            server.Stop();
        });
    }};
    auto ec {server.Run(onConnect)};
    BOOST_REQUIRE(!ec);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK_EQUAL(nSessions, 1);
    BOOST_CHECK(closed);
}

BOOST_AUTO_TEST_CASE(Close_after_Stop, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    LoadTestServerCertificate(ctx);
    boost::asio::io_context ioc {};

    // We don't set any error code because we expect the connection to succeed.
    MockAcceptor::acceptEc = std::queue<boost::system::error_code> {{
        boost::system::error_code {}, // One successful connection
    }};

    TestWebsocketServer server {ip, port, ioc, ctx};
    size_t nSessions {0};
    std::shared_ptr<
        WebsocketSession<MockTlsWebsocketStream>
    > wsSession {nullptr};
    auto onConnect {[&server, &nSessions, &wsSession](auto ec, auto session) {
        BOOST_REQUIRE(!ec);
        BOOST_REQUIRE(session != nullptr);
        wsSession = session;
        ++nSessions;
        server.Stop();
    }};
    auto ec {server.Run(onConnect)};
    BOOST_REQUIRE(!ec);

    // We stop the server but leave one session alive, which is closed after a
    // timeout.
    bool didTimeout {false};
    bool closed {false};
    boost::asio::high_resolution_timer timer(ioc);
    timer.expires_after(std::chrono::milliseconds(250));
    timer.async_wait([&didTimeout, &closed, &wsSession](auto ec) {
        didTimeout = true;
        BOOST_REQUIRE(!ec);
        BOOST_REQUIRE(wsSession != nullptr);
        wsSession->Close([&closed](auto ec) {
            BOOST_CHECK(!ec);
            closed = true;
        });
    });

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK_EQUAL(nSessions, 1);
    BOOST_CHECK(didTimeout);
    BOOST_CHECK(closed);
}

BOOST_AUTO_TEST_CASE(Send, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    const std::string message {"Hello Websocket"};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    LoadTestServerCertificate(ctx);
    boost::asio::io_context ioc {};

    // We don't set any error code because we expect the connection to succeed.
    MockAcceptor::acceptEc = std::queue<boost::system::error_code> {{
        boost::system::error_code {}, // One successful connection
    }};

    TestWebsocketServer server {ip, port, ioc, ctx};
    bool calledOnSend {false};
    auto onConnect {[&server, &calledOnSend, &message](auto ec, auto session) {
        BOOST_REQUIRE(!ec);
        BOOST_REQUIRE(session != nullptr);
        session->Send(message, [session, &server, &calledOnSend](auto ec) {
            BOOST_CHECK(!ec);
            calledOnSend = true;

            // This test assumes that Close() and Stop() work.
            session->Close([&server](auto ec) {
                BOOST_CHECK(!ec);
                server.Stop();
            });
        });
    }};
    auto ec {server.Run(onConnect)};
    BOOST_REQUIRE(!ec);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(calledOnSend);
}

BOOST_AUTO_TEST_CASE(Send_fail, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    const std::string message {"Hello Websocket"};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    LoadTestServerCertificate(ctx);
    boost::asio::io_context ioc {};

    // Set the expected error codes.
    MockAcceptor::acceptEc = std::queue<boost::system::error_code> {{
        boost::system::error_code {}, // One successful connection
    }};
    using error = boost::beast::websocket::error;
    MockTlsWebsocketStream::writeEc = error::bad_data_frame;

    TestWebsocketServer server {ip, port, ioc, ctx};
    bool calledOnSend {false};
    auto onConnect {[&server, &calledOnSend, &message](auto ec, auto session) {
        BOOST_REQUIRE(!ec);
        BOOST_REQUIRE(session != nullptr);
        session->Send(message, [session, &server, &calledOnSend](auto ec) {
            BOOST_CHECK(ec);
            calledOnSend = true;

            // This test assumes that Close() and Stop() work.
            session->Close([&server](auto ec) {
                BOOST_CHECK(!ec);
                server.Stop();
            });
        });
    }};
    auto ec {server.Run(onConnect)};
    BOOST_REQUIRE(!ec);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(calledOnSend);
}

BOOST_AUTO_TEST_CASE(OnMessage, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    const std::string message {"Hello Websocket"};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    LoadTestServerCertificate(ctx);
    boost::asio::io_context ioc {};

    // We don't set any error code because we expect the connection to succeed.
    MockAcceptor::acceptEc = std::queue<boost::system::error_code> {{
        boost::system::error_code {}, // One successful connection
    }};
    MockTlsWebsocketStream::readBuffer = message;

    TestWebsocketServer server {ip, port, ioc, ctx};
    bool receivedMessage {false};
    auto onConnect {[](auto ec, auto session) {
        BOOST_REQUIRE(!ec);
        BOOST_REQUIRE(session != nullptr);
    }};
    auto onMessage {
        [&server, &receivedMessage, &message](
            auto ec,
            auto session,
            auto&& msg
        ) {
            BOOST_REQUIRE(!ec);
            BOOST_REQUIRE(session != nullptr);
            BOOST_CHECK_EQUAL(message, msg);
            receivedMessage = true;

            // This test assumes that Close() and Stop() work.
            session->Close([&server](auto ec) {
                BOOST_CHECK(!ec);
                server.Stop();
            });
        }
    };
    auto ec {server.Run(onConnect, onMessage)};
    BOOST_REQUIRE(!ec);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK(receivedMessage);
}

BOOST_AUTO_TEST_CASE(OnMessage_three_sessions, *timeout {1})
{
    // We use the mock client so we don't really connect to the target.
    const std::string ip {"127.0.0.1"};
    const unsigned short port {8042};
    const std::string message {"Hello Websocket"};

    boost::asio::ssl::context ctx {boost::asio::ssl::context::tlsv12_server};
    ctx.load_verify_file(TESTS_CACERT_PEM);
    LoadTestServerCertificate(ctx);
    boost::asio::io_context ioc {};

    // We don't set any error code because we expect the connection to succeed.
    MockAcceptor::acceptEc = std::queue<boost::system::error_code> {{
        boost::system::error_code {}, // 1st successful connection
        boost::system::error_code {}, // 2nd successful connection
        boost::system::error_code {}, // 3rd successful connection
    }};

    TestWebsocketServer server {ip, port, ioc, ctx};
    size_t nMessages {0};
    auto onConnect {[&message](auto ec, auto session) {
        BOOST_REQUIRE(!ec);
        BOOST_REQUIRE(session != nullptr);

        // This will be the first message.
        MockTlsWebsocketStream::readBuffer = message;
    }};
    auto onMessage {
        [&server, &nMessages, &message](
            auto ec,
            auto session,
            auto&& msg
        ) {
            BOOST_REQUIRE(!ec);
            BOOST_REQUIRE(session != nullptr);
            BOOST_CHECK_EQUAL(message, msg);
            ++nMessages;

            // For the next message.
            MockTlsWebsocketStream::readBuffer = message;

            // This test assumes that Close() and Stop() work.
            session->Close([&server, &nMessages](auto ec) {
                BOOST_CHECK(!ec);
                if (nMessages == 3) {
                    server.Stop();
                }
            });
        }
    };
    auto ec {server.Run(onConnect, onMessage)};
    BOOST_REQUIRE(!ec);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK_EQUAL(nMessages, 3);
}

BOOST_AUTO_TEST_SUITE_END(); // Session

BOOST_AUTO_TEST_SUITE(live);

BOOST_AUTO_TEST_CASE(client_and_server, *timeout {2})
{
    const std::string ip {"127.0.0.1"};
    const std::string endpoint {"/"};
    const unsigned short port {8042};

    using boost::asio::ssl::context;

    boost::asio::io_context ioc {};

    // Server
    boost::asio::ssl::context serverCtx {context::tlsv12_server};
    serverCtx.load_verify_file(TESTS_CACERT_PEM);
    LoadTestServerCertificate(serverCtx);
    BoostWebsocketServer server {ip, port, ioc, serverCtx};
    const std::string serverMessageSent {"server"};
    std::string serverMessageReceived {};
    {
        auto onConnect {[&serverMessageSent](auto ec, auto session) {
            BOOST_REQUIRE(!ec);
            BOOST_REQUIRE(session != nullptr);
            session->Send(serverMessageSent);
        }};
        auto onReceive {
            [
                &server,
                &serverMessageReceived
            ](auto ec, auto session, auto&& received) {
                BOOST_CHECK(!ec);
                BOOST_REQUIRE(session != nullptr);
                serverMessageReceived = std::move(received);
                session->Close([&server](auto ec) {
                    BOOST_CHECK(!ec);
                    server.Stop();
                });
            }
        };
        auto ec {server.Run(onConnect, onReceive)};
        BOOST_REQUIRE(!ec);
    }

    // Client
    // The client connects to an IPv4 address directly to prevent the case
    // where `localhost` is resolved to an IPv6 address.
    boost::asio::ssl::context clientCtx {context::tlsv12_client};
    clientCtx.load_verify_file(TESTS_CACERT_PEM);
    BoostWebsocketClient client {
        ip, endpoint, std::to_string(port), ioc, clientCtx
    };
    const std::string clientMessageSent {"client"};
    std::string clientMessageReceived {};
    {
        auto onConnect {[&client, &clientMessageSent](auto ec) {
            BOOST_CHECK(!ec);
            if (!ec) {
                client.Send(clientMessageSent);
            }
        }};
        auto onReceive {
            [&client, &clientMessageReceived](auto ec, auto&& received) {
                BOOST_CHECK(!ec);
                clientMessageReceived = std::move(received);
                client.Close();
            }
        };
        client.Connect(onConnect, onReceive);
    }

    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    BOOST_CHECK_EQUAL(clientMessageReceived, serverMessageSent);
    BOOST_CHECK_EQUAL(serverMessageReceived, clientMessageSent);
}

BOOST_AUTO_TEST_SUITE_END(); // live

BOOST_AUTO_TEST_SUITE_END(); // class_WebsocketServer

BOOST_AUTO_TEST_SUITE_END(); // network_monitor