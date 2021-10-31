#ifndef NETWORK_MONITOR_NETWORK_MONITOR_H
#define NETWORK_MONITOR_NETWORK_MONITOR_H

#include <network-monitor/FileDownloader.h>
#include <network-monitor/StompClient.h>
#include <network-monitor/TransportNetwork.h>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <spdlog/fmt/chrono.h>
#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>

namespace NetworkMonitor {

/*! \brief Configuration structure for the Live Transport Network Monitor
 *         process.
 */
struct NetworkMonitorConfig {
    std::string networkEventsUrl {};
    std::string networkEventsPort {};
    std::string networkEventsUsername {};
    std::string networkEventsPassword {};
    std::filesystem::path caCertFile {};
    std::filesystem::path networkLayoutFile {};
};

/*! \brief Error codes for the Live Transport Network Monitor process.
 */
enum class NetworkMonitorError {
    kOk = 0,
    kUndefinedError,
    kCouldNotConnectToStompClient,
    kCouldNotParsePassengerEvent,
    kCouldNotRecordPassengerEvent,
    kCouldNotSubscribeToPassengerEvents,
    kFailedNetworkLayoutFileDownload,
    kFailedNetworkLayoutFileParsing,
    kFailedTransportNetworkConstruction,
    kMissingCaCertFile,
    kMissingNetworkLayoutFile,
    kStompClientDisconnected,
};

/*! \brief Print operator for the `NetworkMonitorError` class.
 */
std::ostream& operator<<(
    std::ostream& os,
    const NetworkMonitorError& m
);

/*! \brief Convert `NetworkMonitorError` to string.
 */
std::string ToString(
    const NetworkMonitorError& m
);

/*! \brief Live Transport Network Monitor
 *
 *  \tparam WsClient Type compatible with WebsocketClient.
 */
template <typename WsClient>
class NetworkMonitor {
public:
    /*! \brief Default constructor.
     */
    NetworkMonitor() = default;

    /*! \brief Destructor.
     */
    ~NetworkMonitor() = default;

    /*! \brief Setup the Live Transport Network Monitor.
     *
     *  This function only sets up the connection and performs error checks.
     *  It does not run the STOMP client.
     */
    NetworkMonitorError Configure(
        const NetworkMonitorConfig& config
    )
    {
        spdlog::info("NetworkMonitor: Configure network monitor");

        // Sanity checks
        spdlog::info("NetworkMonitor: Running sanity checks");
        if (!std::filesystem::exists(config.caCertFile)) {
            spdlog::error("NetworkMonitor: Could not find {}. Exiting",
                          config.caCertFile);
            return NetworkMonitorError::kMissingCaCertFile;
        }
        if (!config.networkLayoutFile.empty() &&
                !std::filesystem::exists(config.networkLayoutFile)) {
            spdlog::error("NetworkMonitor: Could not find {}. Exiting",
                          config.networkLayoutFile);
            return NetworkMonitorError::kMissingNetworkLayoutFile;
        }

        // Download the network-layout.json file if the config does not contain
        // a local filename, then parse the file.
        auto networkLayoutFile {config.networkLayoutFile.empty() ?
            std::filesystem::temp_directory_path() / "network-layout.json" :
            config.networkLayoutFile
        };
        if (config.networkLayoutFile.empty()) {
            spdlog::info(
                "NetworkMonitor: Downloading the network layout file to {}",
                networkLayoutFile
            );
            const std::string fileUrl {
                "https://" + config.networkEventsUrl + networkLayoutEndpoint_
            };
            bool downloaded {DownloadFile(
                fileUrl,
                networkLayoutFile,
                config.caCertFile
            )};
            if (!downloaded) {
                spdlog::error("NetworkMonitor: Could not download {}. Exiting",
                              fileUrl);
                return NetworkMonitorError::kFailedNetworkLayoutFileDownload;
            }
        }
        spdlog::info("NetworkMonitor: Loading the network layout file");
        auto parsed = ParseJsonFile(networkLayoutFile);
        if (parsed.empty()) {
            spdlog::error("NetworkMonitor: Could not parse {}. Exiting",
                          networkLayoutFile);
            return NetworkMonitorError::kFailedNetworkLayoutFileParsing;
        }

        // Network representation
        spdlog::info("NetworkMonitor: Constructing the network representation");
        try {
            bool networkLoaded {network_.FromJson(std::move(parsed))};
            if (!networkLoaded) {
                spdlog::error("NetworkMonitor: Could not construct the "
                              "TransportNetwork. Exiting");
                return NetworkMonitorError::kFailedTransportNetworkConstruction;
            }
        } catch (const std::exception& e) {
            spdlog::error("NetworkMonitor: Exception while constructing the "
                          "TransportNetwork: {}. Exiting",
                          e.what());
            return NetworkMonitorError::kFailedTransportNetworkConstruction;
        }

        // STOMP client
        spdlog::info("NetworkMonitor: Constructing the STOMP client: {}:{}{}",
                     config.networkEventsUrl, config.networkEventsPort,
                     networkEventsEndpoint_);
        clientCtx_.load_verify_file(config.caCertFile.string());
        client_ = std::make_shared<StompClient<WsClient>>(
            config.networkEventsUrl,
            networkEventsEndpoint_,
            config.networkEventsPort,
            ioc_,
            clientCtx_
        );
        client_->Connect(
            config.networkEventsUsername,
            config.networkEventsPassword,
            [this](auto ec) {
                OnNetworkEventsConnect(ec);
            },
            [this](auto ec) {
                OnNetworkEventsDisconnect(ec);
            }
        );

        // Note: At this stage nothing runs until someone calls the run()
        //       function on the I/O context object.
        spdlog::info("NetworkMonitor: Successfully configured");
        config_ = config;
        return NetworkMonitorError::kOk;
    }

    /*! \brief Run the I/O context.
     *
     *  This function runs the I/O context in the current thread.
     */
    void Run()
    {
        spdlog::info("NetworkMonitor: Running");
        lastErrorCode_ = NetworkMonitorError::kOk;
        ioc_.run();
    }

    /*! \brief Run the I/O context for a maximum amount of time.
     *
     *  This function runs the I/O context in the current thread.
     *
     *  \param runFor   A time duration after which the I/O context stops, even
     *                  if it has outstanding work to dispatch.
     */
    template <typename DurationRep, typename DurationRatio>
    void Run(
        std::chrono::duration<DurationRep, DurationRatio> runFor
    )
    {
        spdlog::info("NetworkMonitor: Running for {}", runFor);
        lastErrorCode_ = NetworkMonitorError::kOk;
        ioc_.run_for(runFor);
    }

    /*! \brief Stop any computation.
     *
     *  This function causes the I/O context run function to cancel any
     *  outstanding work. This may leave some connections dangling or some
     *  messages not completely sent or parsed.
     */
    void Stop()
    {
        // Here we do not set lastErrorCode_ because the caller may want to
        // know what was the last error code before the network monitor was
        // stoppped.
        spdlog::info("NetworkMonitor: Stopping");
        ioc_.stop();
    }

    /*! \brief Get the latest recorder error.
     *
     *  This is the last error code recorded before the I/O context run function
     *  run out of work to do.
     */
    NetworkMonitorError GetLastErrorCode() const
    {
        return lastErrorCode_;
    }

    /*! \brief Access the internal network representation.
     *
     *  \returns a reference to the internal `TransportNetwork` object instance.
     *           The object has the same lifetime as the `NetworkMonitor` class.
     */
    const TransportNetwork& GetNetworkRepresentation() const
    {
        return network_;
    }

private:
    // We maintain our own instance of the I/O and TLS contexts.
    boost::asio::io_context ioc_ {};
    boost::asio::ssl::context clientCtx_ {
        boost::asio::ssl::context::tlsv12_client
    };

    // We keep the client and server as shared pointers to allow a default
    // constructor.
    std::shared_ptr<StompClient<WsClient>> client_ {nullptr};

    NetworkMonitorConfig config_ {};

    TransportNetwork network_ {};

    NetworkMonitorError lastErrorCode_ {NetworkMonitorError::kUndefinedError};

    // Remote endpoints
    const std::string networkEventsEndpoint_ {"/network-events"};
    const std::string networkLayoutEndpoint_ {"/network-layout.json"};
    const std::string subscriptionDestination_ {"/passengers"};

    // Handlers

    void OnNetworkEventsConnect(
        StompClientError ec
    )
    {
        using Error = NetworkMonitorError;
        if (ec != StompClientError::kOk) {
            spdlog::error("NetworkMonitor: STOMP client connection failed: {}",
                          ec);
            lastErrorCode_ = Error::kCouldNotConnectToStompClient;
            client_->Close();
            return;
        }
        spdlog::info("NetworkMonitor: STOMP client connected");

        // Subscribe to the passenger events
        spdlog::info("NetworkMonitor: Subscribing to {}",
                     subscriptionDestination_);
        auto id {client_->Subscribe(
            subscriptionDestination_,
            [this](auto ec, auto&& id) {
                OnSubscribe(ec, std::move(id));
            },
            [this](auto ec, auto&& msg) {
                OnNetworkEventsMessage(ec, std::move(msg));
            }
        )};
        if (id.empty()) {
            spdlog::error(
                "NetworkMonitor: STOMP client subscription failed: {}", ec
            );
            lastErrorCode_ = Error::kCouldNotSubscribeToPassengerEvents;
            client_->Close();
        }
        lastErrorCode_ = Error::kOk;
    }

    void OnNetworkEventsDisconnect(
        StompClientError ec
    )
    {
        spdlog::error("NetworkMonitor: STOMP client disconnected: {}", ec);
        lastErrorCode_ = NetworkMonitorError::kStompClientDisconnected;
    }

    void OnSubscribe(
        StompClientError ec,
        std::string&& subscriptionId
    )
    {
        using Error = NetworkMonitorError;
        if (ec != StompClientError::kOk) {
            spdlog::error("NetworkMonitor: Unable to subscribe to {}",
                          subscriptionDestination_);
            lastErrorCode_ = Error::kCouldNotSubscribeToPassengerEvents;
        } else {
            spdlog::info("NetworkMonitor: STOMP client subscribed to {}",
                         subscriptionDestination_);
        }
        lastErrorCode_ = Error::kOk;
    }

    void OnNetworkEventsMessage(
        StompClientError ec,
        std::string&& msg
    )
    {
        using Error = NetworkMonitorError;
        PassengerEvent event {};
        try {
            event = nlohmann::json::parse(msg);
        } catch (...) {
            spdlog::error(
                "NetworkMonitor: Could not parse passenger event:\n{}{}",
                std::setw(4), msg
            );
            lastErrorCode_ = Error::kCouldNotParsePassengerEvent;
            return;
        }
        auto ok {network_.RecordPassengerEvent(event)};
        spdlog::debug("NetworkMonitor: Message:\n{}{}", std::setw(4), msg);
        if (!ok) {
            spdlog::error(
                "NetworkMonitor: Could not record new passenger event:\n{}{}",
                std::setw(4), msg
            );
            lastErrorCode_ = Error::kCouldNotRecordPassengerEvent;
            return;
        }
        spdlog::debug(
            "NetworkMonitor: New event: {}",
            boost::posix_time::to_iso_extended_string(event.timestamp)
        );
        lastErrorCode_ = Error::kOk;
    }
};

} // namespace NetworkMonitor

#endif // NETWORK_MONITOR_NETWORK_MONITOR_H