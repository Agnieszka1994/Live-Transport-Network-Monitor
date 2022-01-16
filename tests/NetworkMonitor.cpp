#include "WebsocketClientMock.h"
#include "WebsocketServerMock.h"

#include <network-monitor/env.h>
#include <network-monitor/FileDownloader.h>
#include <network-monitor/NetworkMonitor.h>
#include <network-monitor/WebsocketClient.h>
#include <network-monitor/StompClient.h>
#include <network-monitor/WebsocketServer.h>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>
#include <exception>
#include <thread>

using NetworkMonitor::BoostWebsocketClient;
using NetworkMonitor::BoostWebsocketServer;
using NetworkMonitor::GetEnvVar;
using NetworkMonitor::GetMockSendFrame;
using NetworkMonitor::GetMockStompFrame;
using NetworkMonitor::Id;
using NetworkMonitor::MockWebsocketClientForStomp;
using NetworkMonitor::MockWebsocketEvent;
using NetworkMonitor::MockWebsocketServerForStomp;
using NetworkMonitor::NetworkMonitorConfig;
using NetworkMonitor::NetworkMonitorError;
using NetworkMonitor::ParseJsonFile;
using NetworkMonitor::StompClient;
using NetworkMonitor::StompClientError;
using NetworkMonitor::TravelRoute;

// Use this to set a timeout on tests that may hang or suffer from a slow
// connection.
using timeout = boost::unit_test::timeout;

// This fixture is used to re-initialize all mock properties before a test.
struct NetworkMonitorTestFixture {
    NetworkMonitorTestFixture()
    {
        MockWebsocketClientForStomp::endpoint = "/passengers";
        MockWebsocketClientForStomp::username = "some_username";
        MockWebsocketClientForStomp::password = "some_password_123";
        MockWebsocketClientForStomp::connectEc = {};
        MockWebsocketClientForStomp::sendEc = {};
        MockWebsocketClientForStomp::closeEc = {};
        MockWebsocketClientForStomp::triggerDisconnection = false;
        MockWebsocketClientForStomp::subscriptionMessages = {};

        MockWebsocketServerForStomp::triggerDisconnection = false;
        MockWebsocketServerForStomp::runEc = {};
        MockWebsocketServerForStomp::mockEvents = {};
    }
};

BOOST_AUTO_TEST_SUITE(network_monitor);

BOOST_AUTO_TEST_SUITE(network_monitor);

BOOST_AUTO_TEST_SUITE(enum_class_NetworkMonitorError);

BOOST_AUTO_TEST_CASE(ostream)
{
    std::stringstream invalidSs;
    invalidSs << NetworkMonitorError::kUndefinedError;
    auto invalid {invalidSs.str()};
    for (const auto& error: {
        NetworkMonitorError::kOk,
        NetworkMonitorError::kCouldNotConnectToStompClient,
        NetworkMonitorError::kCouldNotParsePassengerEvent,
        NetworkMonitorError::kCouldNotParseQuietRouteRequest,
        NetworkMonitorError::kCouldNotRecordPassengerEvent,
        NetworkMonitorError::kCouldNotStartStompServer,
        NetworkMonitorError::kCouldNotSubscribeToPassengerEvents,
        NetworkMonitorError::kFailedNetworkLayoutFileDownload,
        NetworkMonitorError::kFailedNetworkLayoutFileParsing,
        NetworkMonitorError::kFailedTransportNetworkConstruction,
        NetworkMonitorError::kMissingCaCertFile,
        NetworkMonitorError::kMissingNetworkLayoutFile,
        NetworkMonitorError::kStompClientDisconnected,
    }) {
        std::stringstream ss;
        ss << error;
        BOOST_CHECK(ss.str() != invalid);
    }
}

BOOST_AUTO_TEST_SUITE_END(); // enum_class_NetworkMonitorError

BOOST_FIXTURE_TEST_SUITE(class_NetworkMonitor, NetworkMonitorTestFixture);

BOOST_AUTO_TEST_SUITE(Configure);

BOOST_AUTO_TEST_CASE(ok)
{
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        TESTS_NETWORK_LAYOUT_JSON,
    };
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_CHECK_EQUAL(ec, NetworkMonitorError::kOk);
}

BOOST_AUTO_TEST_CASE(ok_download_file, *timeout {3})
{
    // Note: In this test we use a mock but we download the file for real.
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        "", // Empty network layout file path. Will download
    };
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_CHECK_EQUAL(ec, NetworkMonitorError::kOk);
}

BOOST_AUTO_TEST_CASE(missing_cacert_file)
{
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        std::filesystem::temp_directory_path() / "nonexistent_cacert.pem",
        TESTS_NETWORK_LAYOUT_JSON,
    };
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_CHECK_EQUAL(ec, NetworkMonitorError::kMissingCaCertFile);
}

BOOST_AUTO_TEST_CASE(missing_network_layout_file)
{
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        std::filesystem::temp_directory_path() / "nonexistent_nw_file.json",
    };
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_CHECK_EQUAL(ec, NetworkMonitorError::kMissingNetworkLayoutFile);
}

BOOST_AUTO_TEST_CASE(download_file_fail)
{
    NetworkMonitorConfig config {
        "ltnm-fail.learncppthroughprojects.com", // It will fail to download
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        "", // Empty network layout file path. Will try to download
    };
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_CHECK_EQUAL(ec, NetworkMonitorError::kFailedNetworkLayoutFileDownload);
}

BOOST_AUTO_TEST_CASE(fail_to_parse_network_layout_file)
{
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        std::filesystem::path(TEST_DATA) / "bad_json_file.json"
    };
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_CHECK_EQUAL(ec, NetworkMonitorError::kFailedNetworkLayoutFileParsing);
}

BOOST_AUTO_TEST_CASE(fail_to_construct_transport_network)
{
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        std::filesystem::path(TEST_DATA) / "bad_network_layout_file.json"
    };
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_CHECK_EQUAL(
        ec,
        NetworkMonitorError::kFailedTransportNetworkConstruction
    );
}

BOOST_AUTO_TEST_CASE(fail_to_launch_stomp_server)
{
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        TESTS_NETWORK_LAYOUT_JSON,
        "localhost",
        "127.0.0.1",
        8042,
    };

    // Setup the mock.
    MockWebsocketServerForStomp::runEc = boost::asio::error::access_denied;

    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_CHECK_EQUAL(
        ec,
        NetworkMonitorError::kCouldNotStartStompServer
    );
}

BOOST_AUTO_TEST_SUITE_END(); // Configure

BOOST_AUTO_TEST_SUITE(Run);

BOOST_AUTO_TEST_CASE(fail_to_connect_ws, *timeout {1})
{
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        TESTS_NETWORK_LAYOUT_JSON,
    };

    // Setup the mock.
    namespace error = boost::asio::ssl::error;
    MockWebsocketClientForStomp::connectEc = error::stream_truncated;

    // We need to set a timeout otherwise the network monitor will run forever.
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_REQUIRE_EQUAL(ec, NetworkMonitorError::kOk);
    monitor.Run(std::chrono::milliseconds(150));

    // When we arrive here, the Run() function ran out of things to do.
    BOOST_CHECK_EQUAL(
        monitor.GetLastErrorCode(),
        NetworkMonitorError::kCouldNotConnectToStompClient
    );
}

BOOST_AUTO_TEST_CASE(fail_to_connect_auth, *timeout {1})
{
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "wrong_password_123", // We will fail to authenticate
        TESTS_CACERT_PEM,
        TESTS_NETWORK_LAYOUT_JSON,
    };

    // We need to set a timeout otherwise the network monitor will run forever.
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_REQUIRE_EQUAL(ec, NetworkMonitorError::kOk);
    monitor.Run(std::chrono::milliseconds(150));

    // When we arrive here, the Run() function ran out of things to do.
    BOOST_CHECK_EQUAL(
        monitor.GetLastErrorCode(),
        NetworkMonitorError::kStompClientDisconnected
    );
}

BOOST_AUTO_TEST_CASE(fail_to_subscribe, *timeout {1})
{
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        TESTS_NETWORK_LAYOUT_JSON,
    };

    // Setup the mock.
    // Note: Our mock does not support a random subscription failure, so we
    //       trigger it by expecting a different subscription endpoint.
    MockWebsocketClientForStomp::endpoint = "/not-passengers";

    // We need to set a timeout otherwise the network monitor will run forever.
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_REQUIRE_EQUAL(ec, NetworkMonitorError::kOk);
    monitor.Run(std::chrono::milliseconds(150));

    // When we arrive here, the Run() function ran out of things to do.
    BOOST_CHECK_EQUAL(
        monitor.GetLastErrorCode(),
        NetworkMonitorError::kStompClientDisconnected
    );
}

BOOST_AUTO_TEST_CASE(fail_to_parse_passenger_event, *timeout {1})
{
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        TESTS_NETWORK_LAYOUT_JSON,
    };

    // Setup the mock.
    MockWebsocketClientForStomp::subscriptionMessages = {
        "Not a valid JSON payload {}[]--.",
    };

    // We need to set a timeout otherwise the network monitor will run forever.
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_REQUIRE_EQUAL(ec, NetworkMonitorError::kOk);
    monitor.Run(std::chrono::milliseconds(150));

    // When we arrive here, the Run() function ran out of things to do.
    BOOST_CHECK_EQUAL(
        monitor.GetLastErrorCode(),
        NetworkMonitorError::kCouldNotParsePassengerEvent
    );
}

BOOST_AUTO_TEST_CASE(fail_to_record_passenger_event, *timeout {1})
{
    // In this test we load a very simple network and then try to process a
    // passenger event for a station outside of the network.
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        std::filesystem::path(TEST_DATA) / "from_json_1line_1route.json",
    };

    // Setup the mock.
    nlohmann::json event {
        {"datetime", "2020-11-01T07:18:50.234000Z"},
        {"passenger_event", "in"},
        {"station_id", "station_42"}, // This station is not in the network
    };
    MockWebsocketClientForStomp::subscriptionMessages = {
        event.dump(),
    };

    // We need to set a timeout otherwise the network monitor will run forever.
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_REQUIRE_EQUAL(ec, NetworkMonitorError::kOk);
    monitor.Run(std::chrono::milliseconds(150));

    // When we arrive here, the Run() function ran out of things to do.
    BOOST_CHECK_EQUAL(
        monitor.GetLastErrorCode(),
        NetworkMonitorError::kCouldNotRecordPassengerEvent
    );
}

BOOST_AUTO_TEST_CASE(record_1_passenger_event, *timeout {1})
{
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        std::filesystem::path(TEST_DATA) / "from_json_1line_1route.json",
    };

    // Setup the mock.
    nlohmann::json event {
        {"datetime", "2020-11-01T07:18:50.234000Z"},
        {"passenger_event", "in"},
        {"station_id", "station_0"},
    };
    MockWebsocketClientForStomp::subscriptionMessages = {
        event.dump(),
    };

    // We need to set a timeout otherwise the network monitor will run forever.
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_REQUIRE_EQUAL(ec, NetworkMonitorError::kOk);
    monitor.Run(std::chrono::milliseconds(150));

    // When we arrive here, the Run() function ran out of things to do.
    BOOST_CHECK_EQUAL(monitor.GetLastErrorCode(), NetworkMonitorError::kOk);
    BOOST_CHECK_EQUAL(
        monitor.GetNetworkRepresentation().GetPassengerCount("station_0"),
        1
    );
    BOOST_CHECK_EQUAL(
        monitor.GetNetworkRepresentation().GetPassengerCount("station_1"),
        0
    );
}

BOOST_AUTO_TEST_CASE(record_2_passenger_events_same_station, *timeout {1})
{
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        std::filesystem::path(TEST_DATA) / "from_json_1line_1route.json",
    };

    // Setup the mock.
    nlohmann::json event0 {
        {"datetime", "2020-11-01T07:18:50.234000Z"},
        {"passenger_event", "in"},
        {"station_id", "station_0"},
    };
    nlohmann::json event1 {
        {"datetime", "2020-11-01T07:18:51.234000Z"},
        {"passenger_event", "in"},
        {"station_id", "station_0"},
    };
    MockWebsocketClientForStomp::subscriptionMessages = {
        event0.dump(),
        event1.dump(),
    };

    // We need to set a timeout otherwise the network monitor will run forever.
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_REQUIRE_EQUAL(ec, NetworkMonitorError::kOk);
    monitor.Run(std::chrono::milliseconds(150));

    // When we arrive here, the Run() function ran out of things to do.
    BOOST_CHECK_EQUAL(monitor.GetLastErrorCode(), NetworkMonitorError::kOk);
    BOOST_CHECK_EQUAL(
        monitor.GetNetworkRepresentation().GetPassengerCount("station_0"),
        2
    );
    BOOST_CHECK_EQUAL(
        monitor.GetNetworkRepresentation().GetPassengerCount("station_1"),
        0
    );
}

BOOST_AUTO_TEST_CASE(record_2_passenger_events_different_station, *timeout {1})
{
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        std::filesystem::path(TEST_DATA) / "from_json_1line_1route.json",
    };

    // Setup the mock.
    nlohmann::json event0 {
        {"datetime", "2020-11-01T07:18:50.234000Z"},
        {"passenger_event", "in"},
        {"station_id", "station_0"},
    };
    nlohmann::json event1 {
        {"datetime", "2020-11-01T07:18:51.234000Z"},
        {"passenger_event", "in"},
        {"station_id", "station_1"},
    };
    MockWebsocketClientForStomp::subscriptionMessages = {
        event0.dump(),
        event1.dump(),
    };

    // We need to set a timeout otherwise the network monitor will run forever.
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_REQUIRE_EQUAL(ec, NetworkMonitorError::kOk);
    monitor.Run(std::chrono::milliseconds(150));

    // When we arrive here, the Run() function ran out of things to do.
    BOOST_CHECK_EQUAL(monitor.GetLastErrorCode(), NetworkMonitorError::kOk);
    BOOST_CHECK_EQUAL(
        monitor.GetNetworkRepresentation().GetPassengerCount("station_0"),
        1
    );
    BOOST_CHECK_EQUAL(
        monitor.GetNetworkRepresentation().GetPassengerCount("station_1"),
        1
    );
}

BOOST_AUTO_TEST_CASE(record_passenger_events_from_file, *timeout {3})
{
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        TESTS_NETWORK_LAYOUT_JSON,
    };

    // Setup the mock.
    auto events = ParseJsonFile(
        std::filesystem::path(TEST_DATA) / "passenger_events.json"
    ).get<std::vector<nlohmann::json>>();
    std::vector<std::string> messages {};
    messages.reserve(events.size());
    for (const auto& event: events) {
        messages.emplace_back(event.dump());
    }
    MockWebsocketClientForStomp::subscriptionMessages = std::move(messages);

    // Load the expected results.
    auto counts = ParseJsonFile(
        std::filesystem::path(TEST_DATA) / "passenger_events_count.json"
    ).get<std::unordered_map<std::string, long long int>>();

    // We need to set a timeout otherwise the network monitor will run forever.
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_REQUIRE_EQUAL(ec, NetworkMonitorError::kOk);
    monitor.Run(std::chrono::milliseconds(1000));

    // When we arrive here, the Run() function ran out of things to do.
    BOOST_CHECK_EQUAL(monitor.GetLastErrorCode(), NetworkMonitorError::kOk);
    const auto& network {monitor.GetNetworkRepresentation()};
    for (const auto& [stationId, passengerCount]: counts) {
        BOOST_CHECK_EQUAL(network.GetPassengerCount(stationId), passengerCount);
    }
}

BOOST_AUTO_TEST_CASE(failed_incoming_connection, *timeout {1})
{
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        TESTS_NETWORK_LAYOUT_JSON,
        "localhost",
        "127.0.0.1",
        8042,
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
            "invalid stomp frame" // Will fail the STOMP connection
        },
    }};

    // We need to set a timeout otherwise the network monitor will run forever.
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_REQUIRE_EQUAL(ec, NetworkMonitorError::kOk);
    monitor.Run(std::chrono::milliseconds(150));

    // When we arrive here, the Run() function ran out of things to do.
    BOOST_CHECK_EQUAL(monitor.GetConnectedClients().size(), 0);
}

BOOST_AUTO_TEST_CASE(incoming_connection, *timeout {1})
{
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        TESTS_NETWORK_LAYOUT_JSON,
        "localhost",
        "127.0.0.1",
        8042,
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
            GetMockStompFrame("localhost")
        },
    }};

    // We need to set a timeout otherwise the network monitor will run forever.
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_REQUIRE_EQUAL(ec, NetworkMonitorError::kOk);
    monitor.Run(std::chrono::milliseconds(150));

    // When we arrive here, the Run() function ran out of things to do.
    BOOST_CHECK_EQUAL(monitor.GetConnectedClients().size(), 1);
}

BOOST_AUTO_TEST_CASE(quiet_route_fail_bad_msg, *timeout {1})
{
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        TESTS_NETWORK_LAYOUT_JSON,
        "localhost",
        "127.0.0.1",
        8042,
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
            GetMockStompFrame("localhost")
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            GetMockSendFrame("req0", "/quiet-route", nlohmann::json {
                {"start_station_id", "station_211"},
                // No end_station_id!
            }.dump())
        },
    }};

    // We need to set a timeout otherwise the network monitor will run forever.
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_REQUIRE_EQUAL(ec, NetworkMonitorError::kOk);
    monitor.Run(std::chrono::milliseconds(150));

    // When we arrive here, the Run() function ran out of things to do.
    BOOST_CHECK_EQUAL(monitor.GetConnectedClients().size(), 0);
}

BOOST_AUTO_TEST_CASE(quiet_route_bad_stations, *timeout {1})
{
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        TESTS_NETWORK_LAYOUT_JSON,
        "localhost",
        "127.0.0.1",
        8042,
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
            GetMockStompFrame("localhost")
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            GetMockSendFrame("req0", "/quiet-route", nlohmann::json {
                {"start_station_id", "station_211"},
                {"end_station_id", "station_XXX"},
            }.dump())
        },
    }};

    // We need to set a timeout otherwise the network monitor will run forever.
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_REQUIRE_EQUAL(ec, NetworkMonitorError::kOk);
    monitor.Run(std::chrono::milliseconds(150));

    // When we arrive here, the Run() function ran out of things to do.
    BOOST_CHECK_EQUAL(monitor.GetConnectedClients().size(), 1);
    auto travelRoute {monitor.GetLastTravelRoute()};
    BOOST_CHECK_EQUAL(travelRoute.startStationId, "");
    BOOST_CHECK_EQUAL(travelRoute.endStationId, "");
    BOOST_CHECK_EQUAL(travelRoute.totalTravelTime, 0);
    BOOST_CHECK_EQUAL(travelRoute.steps.size(), 0);
}

BOOST_AUTO_TEST_CASE(quiet_route, *timeout {5})
{
    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        TESTS_NETWORK_LAYOUT_JSON,
        "localhost",
        "127.0.0.1",
        8042,
        0.1,
        0.1,
        20,
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
            GetMockStompFrame("localhost")
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            GetMockSendFrame("req0", "/quiet-route", nlohmann::json {
                {"start_station_id", "station_211"},
                {"end_station_id", "station_119"},
            }.dump())
        },
    }};

    // We need to set a timeout otherwise the network monitor will run forever.
    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_REQUIRE_EQUAL(ec, NetworkMonitorError::kOk);
    monitor.Run(std::chrono::milliseconds(150));

    // When we arrive here, the Run() function ran out of things to do.
    BOOST_CHECK_EQUAL(monitor.GetConnectedClients().size(), 1);
    auto travelRoute {monitor.GetLastTravelRoute()};
    BOOST_CHECK_EQUAL(travelRoute.startStationId, "station_211");
    BOOST_CHECK_EQUAL(travelRoute.endStationId, "station_119");
    BOOST_CHECK_EQUAL(travelRoute.totalTravelTime, 29);
    BOOST_CHECK_EQUAL(travelRoute.steps.size(), 19);
}

BOOST_AUTO_TEST_CASE(quiet_route_ltc_quiet2, *timeout {5})
{
    // This test is based on the same network, passenger events, and travel
    // routes of the ltc_quiet2 test for the TransportNetwork class.

    NetworkMonitorConfig config {
        "ltnm.learncppthroughprojects.com",
        "443",
        "some_username",
        "some_password_123",
        TESTS_CACERT_PEM,
        TESTS_NETWORK_LAYOUT_JSON,
        "localhost",
        "127.0.0.1",
        8042,
        0.1, // These configurations make route 049 the most quiet.
        0.1,
        20,
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
            GetMockStompFrame("localhost")
        },
        MockWebsocketEvent {
            "connection0",
            MockWebsocketEvent::Type::kMessage,
            {}, // Succeeds
            GetMockSendFrame("req0", "/quiet-route", nlohmann::json {
                {"start_station_id", "station_211"},
                {"end_station_id", "station_119"},
            }.dump())
        },
    }};

    NetworkMonitor::NetworkMonitor<
        MockWebsocketClientForStomp,
        MockWebsocketServerForStomp
    > monitor {};
    auto ec {monitor.Configure(config)};
    BOOST_REQUIRE_EQUAL(ec, NetworkMonitorError::kOk);

    // Record a set of passenger counts. We do not go through mock passenger
    // events as we'd have to rely on the right timing between a quiet-route
    // request and the passenger events themselves.
    std::unordered_map<Id, int> passengerCounts {};
    try {
        passengerCounts = ParseJsonFile(
            std::filesystem::path(TEST_DATA) / "ltc_quiet2.counts.json"
        ).get<std::unordered_map<Id, int>>();
    } catch (...) {
        BOOST_FAIL("Failed to parse passenger counts file");
    }
    monitor.SetNetworkCrowding(passengerCounts);

    // We need to set a timeout otherwise the network monitor will run forever.
    monitor.Run(std::chrono::milliseconds(150));

    // When we arrive here, the Run() function ran out of things to do.
    BOOST_CHECK_EQUAL(monitor.GetConnectedClients().size(), 1);
    auto travelRoute {monitor.GetLastTravelRoute()};
    BOOST_CHECK_EQUAL(travelRoute.startStationId, "station_211");
    BOOST_CHECK_EQUAL(travelRoute.endStationId, "station_119");
    BOOST_CHECK_EQUAL(travelRoute.totalTravelTime, 30);
    BOOST_CHECK_EQUAL(travelRoute.steps.size(), 17);
    auto travelRouteJson = ParseJsonFile(
        std::filesystem::path(TEST_DATA) / "ltc_quiet2.result.route_049.json"
    );
    TravelRoute golden {};
    try {
        golden = travelRouteJson.get<TravelRoute>();
    } catch (...) {
        BOOST_FAIL(std::string("Failed to parse result JSON file"));
    }
    BOOST_CHECK_EQUAL(travelRoute, golden);
}

BOOST_AUTO_TEST_CASE(live, *timeout {5})
{
    // This test starts a live NetworkMonitor instance and then constructs a
    // local StompClient instance to connect to it. We perform one quiet-route
    // request and check the format of the response we receive.

    NetworkMonitorConfig config {
        GetEnvVar("LTNM_SERVER_URL", "ltnm.learncppthroughprojects.com"),
        GetEnvVar("LTNM_SERVER_PORT", "443"),
        GetEnvVar("LTNM_USERNAME"),
        GetEnvVar("LTNM_PASSWORD"),
        TESTS_CACERT_PEM,
        GetEnvVar("LTNM_NETWORK_LAYOUT_FILE_PATH", TESTS_NETWORK_LAYOUT_JSON),
        "127.0.0.1", // We use the IP as the server hostname because the client
                     // will connect to 127.0.0.1 directly, without host name
                     // resolution.
        "127.0.0.1",
        8042,
        0.1,
        0.1,
        20,
    };

    // The NetworkMonitor and the StompClient instances will run in their own
    // threads. The current thread is only used to validate the test results.

    // NetworkMonitor
    NetworkMonitor::NetworkMonitor<
        BoostWebsocketClient,
        BoostWebsocketServer
    > monitor {};
    std::thread monitorThread {[&]() {
        auto ec {monitor.Configure(config)};
        BOOST_REQUIRE_EQUAL(ec, NetworkMonitorError::kOk);

        // We run for an indefinite amount of time: We let the client drive the
        // test flow.
        monitor.Run();
    }};

    // STOMP client
    bool clientDidConnect {false};
    bool clientDidSendReq {false};
    bool clientDidReceiveResp {false};
    bool clientDidClose {false};
    TravelRoute quietRoute {};
    std::thread clientThread {[&]() {
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
        ctx.load_verify_file(TESTS_CACERT_PEM);
        StompClient<BoostWebsocketClient> client {
            url,
            endpoint,
            port,
            ioc,
            ctx
        };

        // Client callbacks
        auto onSend {[&clientDidSendReq](auto ec, auto&& id) {
            BOOST_REQUIRE_EQUAL(ec, StompClientError::kOk);
            BOOST_REQUIRE(id.size() > 0);
            clientDidSendReq = true;
            spdlog::info("TestStompClient: /quiet-route request sent");
        }};
        auto onConnect {[&clientDidConnect, &client, &onSend](auto ec) {
            BOOST_REQUIRE_EQUAL(ec, StompClientError::kOk);
            clientDidConnect = true;
            spdlog::info("TestStompClient: Connected");

            // Make the quiet-route request.
            spdlog::info("TestStompClient: Sending /quiet-route request");
            client.Send("/quiet-route", nlohmann::json {
                {"start_station_id", "station_211"},
                {"end_station_id", "station_119"},
            }.dump(), onSend);
        }};
        auto onClose {[&clientDidClose, &monitor](auto ec) {
            BOOST_REQUIRE_EQUAL(ec, StompClientError::kOk);
            clientDidClose = true;
            spdlog::info("TestStompClient: Client connection closed");
            monitor.Stop();
        }};
        auto onMessage {[
            &clientDidReceiveResp,
            &client,
            &quietRoute,
            onClose
        ](auto ec, auto dst, auto&& msg) {
            BOOST_REQUIRE_EQUAL(ec, StompClientError::kOk);
            clientDidReceiveResp = true;
            spdlog::info("TestStompClient: Received /quiet-route response");
            try {
                quietRoute = nlohmann::json::parse(msg);
            } catch (const std::exception& e) {
                spdlog::error("TestStompClient: Failed to parse response: {}",
                              e.what());
                spdlog::error("TestStompClient: Response content:\n{}",
                              nlohmann::json::parse(msg));
                BOOST_REQUIRE(false);
            }
            spdlog::info("TestStompClient: Closing the client connection");
            client.Close(onClose);
        }};

        spdlog::info("TestStompClient: Connecting");
        client.Connect(username, password, onConnect, onMessage, onClose);

        ioc.run();

        spdlog::info("TestStompClient: No work left to do");
    }};

    // Let's wait for both the NetworkMonitor and StompClient threads to run
    // out of work to do.
    // Note: The client thread must be joined first as it's the one that drives
    //       the test flow.
    clientThread.join();
    monitorThread.join();

    BOOST_CHECK_EQUAL(monitor.GetLastErrorCode(), NetworkMonitorError::kOk);
    BOOST_REQUIRE(clientDidConnect);
    BOOST_REQUIRE(clientDidSendReq);
    BOOST_REQUIRE(clientDidReceiveResp);
    BOOST_REQUIRE(clientDidClose);

    // We can only check the quiet-route response format rather than the actual
    // itinerary. The travel route may be affected by the current level of
    // crowding. We do know that a route exists though, so we expect to have a
    // non-null travel time and a non-empty itinerary.
    BOOST_CHECK_EQUAL(quietRoute.startStationId, "station_211");
    BOOST_CHECK_EQUAL(quietRoute.endStationId, "station_119");
    BOOST_CHECK(quietRoute.totalTravelTime > 0);
    BOOST_REQUIRE(quietRoute.steps.size() > 0);
}

BOOST_AUTO_TEST_SUITE_END(); // Run

BOOST_AUTO_TEST_SUITE_END(); // class_NetworkMonitor

BOOST_AUTO_TEST_SUITE_END(); // network_monitor

BOOST_AUTO_TEST_SUITE_END(); // network_monitor