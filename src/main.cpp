#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>

#include <iomanip>
#include <iostream>
#include <thread>

using tcp = boost::asio::ip::tcp;

void Log(boost::system::error_code ec)
{
    std::cerr << "[" << std::setw(14) << std::this_thread::get_id() << "] "
              << (ec ? "Error: " : "OK")
              << (ec ? ec.message() : "")
              << std::endl;
}

void OnConnect(boost::system::error_code ec)
{
    Log(ec);
}
int main()
{
    std::cerr << "[" << std::setw(14) << std::this_thread::get_id() << "] main"
              << std::endl;

    // Always start with an I/O context object.         
    boost::asio::io_context ioc {};

    // boost::asio i/o object API needs an io_context as the first parameter
    tcp::socket socket{boost::asio::make_strand(ioc)};

    size_t nThreads {4};

    boost::system::error_code ec {};

    tcp::resolver resolver {ioc};

    auto endpoint {resolver.resolve("google.com", "80", ec)};
    //auto address {boost::asio::ip::address::from_string("1.1.1.1")};
    //tcp::endpoint endpoint {address, 80};

    if (ec) {
        Log(ec);
        return -1;
    }
    for (size_t idx {0}; idx < nThreads; ++idx) {
        socket.async_connect(*endpoint, OnConnect);
    }
    // call io_context::run for asynchronous callbacks to run
    std::vector<std::thread> threads {};
    threads.reserve(nThreads);
    for (size_t idx {0}; idx < nThreads; ++idx) {
        threads.emplace_back([&ioc]() {
            ioc.run();
        });
    }
    for (size_t idx {0}; idx < nThreads; ++idx) {
        threads[idx].join();
    }

    return 0;
}