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

void OnHandshake(
    // Start of shared data
    websocket::stream<boost::beast::tcp_stream>& ws,
    const boost::asio::const_buffer& wBuffer,
    boost::beast::flat_buffer& rBuffer,
    // End of shared data
    const boost::system::error_code& ec
)
{
    if(ec){
        Log("OnHandshake", ec);
        return;
    }

    // Tell the WebSocket to exchange messages in text format
    ws.text(true);

    // Send a message to the connected WebSocket server
    ws.async_write(wBuffer,
        [&ws, &rBuffer](auto ec, auto nBytesWritten){
            OnSend(ws, rBuffer, ec);
        }
    );
}

void OnConnect(
    // Start of shared data
    websocket::stream<boost::beast::tcp_stream>& ws,
    const std::string& url,
    const boost::asio::const_buffer& wBuffer,
    boost::beast::flat_buffer& rBuffer,
    // End of shared data
    const boost::system::error_code& ec
)
{
    if(ec){
        Log("OnConnect", ec);
        return;
    }

    // Attempt a WebSocket Handshake
    ws.async_handshake(url, "/",
        [&ws, &wBuffer, &rBuffer](auto ec){
            OnHandshake(ws, wBuffer, rBuffer, ec);
        }
    );
}

void OnResolve(
    // Start of shared data
    websocket::stream<boost::beast::tcp_stream>& ws,
    const std::string& url,
    const boost::asio::const_buffer& wBuffer,
    boost::beast::flat_buffer& rBuffer,
    // End of shared data
    const boost::system::error_code& ec,
    tcp::resolver::iterator endpoint
)
{
    if(ec){
        Log("OnResolve", ec);
        return;
    }

    // Connect to the TCP socket
    // Instead of constructing the socket and the ws objects separately,
    // the socket is now embedded in ws, and we access it through next_layer()
    ws.next_layer().async_connect(*endpoint,
        [&ws, &url, &wBuffer, &rBuffer](auto ec){
            OnConnect(ws, url, wBuffer, rBuffer, ec);
        }
    );
}
int main()
{
    // Connection targets
    const std::string url {"echo.websocket.org"};
    const std::string port {"80"};
    const std::string message {"Hello Websocket"};

    // start with an I/O context object
    boost::asio::io_context ioc {};

    // Create the objects that will be shared by the connection callbacks
    websocket::stream<boost::beast::tcp_stream> ws {ioc};
    boost::asio::const_buffer wBuffer {message.c_str(), message.size()};
    boost::beast::flat_buffer rBuffer {};

    // Resolve the endpoint
    tcp::resolver resolver {ioc};
    resolver.async_resolve(url, port,
        [&ws, &url, &wBuffer, &rBuffer](auto ec, auto endpoint){
            OnResolve(ws, url, wBuffer, rBuffer, ec, endpoint);
        }
    );

    // call io_context::run for asynchronous callbacks to run
    ioc.run();

    return 0;
}