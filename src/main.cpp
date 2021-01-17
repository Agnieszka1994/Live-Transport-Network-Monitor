#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/system/error_code.hpp>

#include <iomanip>
#include <iostream>
#include <string>

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

void Log(const std::string& where, boost::system::error_code ec)
{
    std::cerr << "[" << std::setw(20) << where << "] "
              << (ec ? "Error: " : "OK")
              << (ec ? ec.message() : "")
              << std::endl;
}

void OnReceive(
    // Start of shared data
    boost::beast::flat_buffer& rBuffer,
    // End of shared data
    const boost::system::error_code& ec
)
{
    if(ec){
        Log("OnReceive", ec);
        return;
    }

    // Print the echoed message

    std::cout << "ECHO: "
                << boost::beast::make_printable(rBuffer.data())
                << std::endl;
}

void OnSend(
    // Start of shared data
    websocket::stream<boost::beast::tcp_stream>& ws,
    boost::beast::flat_buffer& rBuffer,
    // End of shared data
    const boost::system::error_code& ec
)
{
    if(ec){
        Log("OnSend", ec);
        return;
    }
    // Read the echoed message back
    ws.async_read(rBuffer,
        [&rBuffer](auto ec, auto nBytesRead){
            OnReceive(rBuffer, ec);
        }
    );
}
void callback(const boost::system::error_code& error)
{
    Log(std::string(), error);
};
int main()
{
    // Connection targets
    const std::string url {"echo.websocket.org"};
    const std::string port {"80"};
    const std::string message {"Hello WebSocket"};

    // Always start with an I/O context object.
    boost::asio::io_context ioc {};

    // Under the hood, socket.connect uses I/O context to talk to the socket
    // and get a response back. The response is saved in ec.
    boost::system::error_code ec {};

    // Resolve the endpoint.
    tcp::resolver resolver {ioc};
    auto endpoint {resolver.resolve(url, port, ec)};
    if (ec) {
        Log("resolver.resolve", ec);
        return -1;
    }

    // Connect the TCP socket.
    tcp::socket socket {ioc};

    socket.async_connect(*endpoint, callback);
    if (ec) {
        Log("socket.connect", ec);
        return -2;
    }

    // Tie the socket object to the WebSocket stream and attempt an handshake.
    websocket::stream<boost::beast::tcp_stream> ws {std::move(socket)};
    ws.handshake(url, "/", ec);
    if (ec) {
        Log("ws.handshake", ec);
        return -3;
    }

    // Tell the WebSocket object to exchange messages in text format.
    ws.text(true);

    // Send a message to the connected WebSocket server.
    boost::asio::const_buffer wbuffer {message.c_str(), message.size()};

    ws.write(wbuffer, ec);
    if (ec) {
        Log("ws.write", ec);
        return -4;
    }

    // Read the echoed message back.
    boost::beast::flat_buffer rbuffer {};

    ws.read(rbuffer, ec);
    if (ec) {
        Log("ws.read", ec);
        return 51;
    }

    // Print the echoed message.
    std::cout << "ECHO: "
              << boost::beast::make_printable(rbuffer.data())
              << std::endl;

    Log("returning", ec);
    return 0;
}