#include "network-monitor/WebsocketClient.h"

#include <boost/asio.hpp>

#include <iostream>
#include <string>

using NetworkMonitor::WebsocketClient;

int main()
{
    // Connection targets
    const std::string url {"echo.websocket.org"};
    const std::string port {"80"};
    const std::string message {"Hello Websocket"};

    // Always start with an I/O context object.
    boost::asio::io_context ioc {};

    // The class under test
    WebsocketClient client {url, port, ioc};

    // We use these flags to check that the connection, send, receive functions
    // work as expected.
    bool connected {false};
    bool messageSent {false};
    bool messageReceived {false};
    bool messageMatches {false};
    bool disconnected {false};

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
                      &messageMatches,
                      &message](auto ec, auto received) {
        messageReceived = !ec;
        messageMatches = message == received;
        client.Close(onClose);
    }};

    // We must call io_context::run for asynchronous callbacks to run.
    client.Connect(onConnect, onReceive);
    ioc.run();

    // When we get here, the io_context::run function has run out of work to do.
    bool ok {
        connected &&
        messageSent &&
        messageReceived &&
        messageMatches &&
        disconnected
    };
    if (ok) {
        std::cout << "OK" << std::endl;
        return 0;
    } else {
        std::cerr << "Test failed" << std::endl;
        return 1;
    }
}