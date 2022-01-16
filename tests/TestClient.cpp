#include <network-monitor/env.h>
#include <network-monitor/StompClient.h>
#include <network-monitor/TransportNetwork.h>
#include <network-monitor/WebsocketClient.h>

#include <nlohmann/json.hpp>

#include <spdlog/spdlog.h>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <iostream>
#include <string>
#include <thread>

using NetworkMonitor::BoostWebsocketClient;
using NetworkMonitor::GetEnvVar;
using NetworkMonitor::StompClient;
using NetworkMonitor::StompClientError;
using NetworkMonitor::TravelRoute;

void Check(bool predicate)
{
    if (!predicate) {
        throw std::runtime_error("Test failed");
    }
}

template <typename T1, typename T2>
void CheckEqual(const T1& ref, const T2& tgt)
{
    if (ref != tgt) {
        throw std::runtime_error("Test failed");
    }
}

int main()
{
    // We echo any message we receive in the STDIN.
    // This is useful if we are running this test client in parallel with the
    // network monitor: CMake pipes all stdouts together.
    std::thread stdinEcho {[]() {
        for (std::string line {}; std::getline(std::cin, line);) {
            std::cout << line << std::endl;
        }
    }};

    // We sleep waiting for the NetworkMonitor to become available.
    spdlog::info("TestStompClient: Sleeping...");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    spdlog::info("TestStompClient: Awake");

    // Configure the STOMP client.
    // The client connects to an IPv4 address directly to prevent the case
    // where `localhost` is resolved to an IPv6 address.
    const std::string url {"127.0.0.1"};
    const std::string endpoint {"/quiet-route"};
    const std::string port {"8042"};
    const std::string username {"username"};
    const std::string password {"password"};
    boost::asio::io_context ioc {};
    boost::asio::ssl::context ctx {
        boost::asio::ssl::context::tlsv12_client
    };
    ctx.load_verify_file(GetEnvVar("LTNM_CACERT_PEM", "cacert.pem"));
    StompClient<BoostWebsocketClient> client {
        url,
        endpoint,
        port,
        ioc,
        ctx
    };

    bool clientDidConnect {false};
    bool clientDidSendReq {false};
    bool clientDidReceiveResp {false};
    bool clientDidClose {false};
    TravelRoute quietRoute {};

    // Client callbacks
    auto onSend {[&clientDidSendReq](auto ec, auto&& id) {
        CheckEqual(ec, StompClientError::kOk);
        Check(id.size() > 0);
        clientDidSendReq = true;
        spdlog::info("TestStompClient: /quiet-route request sent");
    }};
    auto onConnect {[&clientDidConnect, &client, &onSend](auto ec) {
        CheckEqual(ec, StompClientError::kOk);
        clientDidConnect = true;
        spdlog::info("TestStompClient: Connected");

        // Make the quiet-route request.
        spdlog::info("TestStompClient: Sending /quiet-route request");
        client.Send("/quiet-route", nlohmann::json {
            {"start_station_id", "station_211"},
            {"end_station_id", "station_119"},
        }.dump(), onSend);
    }};
    auto onClose {[&clientDidClose](auto ec) {
        CheckEqual(ec, StompClientError::kOk);
        clientDidClose = true;
        spdlog::info("TestStompClient: Client connection closed");
    }};
    auto onMessage {[
        &clientDidReceiveResp,
        &client,
        &quietRoute,
        onClose
    ](auto ec, auto dst, auto&& msg) {
        CheckEqual(ec, StompClientError::kOk);
        clientDidReceiveResp = true;
        spdlog::info("TestStompClient: Received /quiet-route response");
        try {
            quietRoute = nlohmann::json::parse(msg);
        } catch (const std::exception& e) {
            spdlog::error("TestStompClient: Failed to parse response: {}",
                          e.what());
            spdlog::error("TestStompClient: Response content:\n{}",
                          nlohmann::json::parse(msg));
            Check(false);
        }
        spdlog::info("TestStompClient: Closing the client connection");
        client.Close(onClose);
    }};

    spdlog::info("TestStompClient: Connecting");
    client.Connect(username, password, onConnect, onMessage, onClose);

    ioc.run();
    stdinEcho.join();

    spdlog::info("TestStompClient: No work left to do");

    Check(clientDidConnect);
    Check(clientDidSendReq);
    Check(clientDidReceiveResp);
    Check(clientDidClose);

    // We can only check the quiet-route response format rather than the actual
    // itinerary. The travel route may be affected by the current level of
    // crowding. We do know that a route exists though, so we expect to have a
    // non-null travel time and a non-empty itinerary.
    CheckEqual(quietRoute.startStationId, "station_211");
    CheckEqual(quietRoute.endStationId, "station_119");
    Check(quietRoute.totalTravelTime > 0);
    Check(quietRoute.steps.size() > 0);

    return 0;
} 