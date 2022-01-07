#ifndef NETWORK_MONITOR_STOMP_CLIENT_H
#define NETWORK_MONITOR_STOMP_CLIENT_H

#include <network-monitor/StompFrame.h>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include <functional>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace NetworkMonitor {

/*! \brief Error codes for the STOMP client.
 */
enum class StompClientError {
    kOk = 0,
    kUndefinedError,
    kCouldNotCloseWebsocketConnection,
    kCouldNotConnectToWebsocketServer,
    kCouldNotParseMessageAsStompFrame,
    kCouldNotSendMessage,
    kCouldNotSendStompFrame,
    kCouldNotSendSubscribeFrame,
    kUnexpectedCouldNotCreateValidFrame,
    kUnexpectedMessageContentType,
    kUnexpectedSubscriptionMismatch,
    kWebsocketServerDisconnected,
};

/*! \brief Print operator for the StompClientError class.
 */
std::ostream& operator<<(std::ostream& os, const StompClientError& m);

/*! \brief Convert StompClientError to string.
 */
std::string ToString(const StompClientError& m);

/*! \brief STOMP client implementing the subset of commands needed by the
 *         network-events service.
 *
 *  \tparam WsClient    Websocket client class. This type must have the same
 *                      interface of WebsocketClient.
 */
template <typename WsClient>
class StompClient {
public:
    /*! \brief Construct a STOMP client connecting to a remote URL/port through
     *         a secure Websocket connection.
     *
     *  \note This constructor does not initiate a connection.
     *
     *  \param url      The URL of the server.
     *  \param endpoint The endpoint on the server to connect to.
     *                  Example: ltnm.learncppthroughprojects.com/<endpoint>
     *  \param port     The port on the server.
     *  \param ioc      The io_context object. The user takes care of calling
     *                  ioc.run().
     *  \param ctx      The TLS context to setup a TLS socket stream.
     */
    StompClient(
        const std::string& url,
        const std::string& endpoint,
        const std::string& port,
        boost::asio::io_context& ioc,
        boost::asio::ssl::context& ctx
    ) : ws_ {url, endpoint, port, ioc, ctx},
        url_ {url},
        context_ {boost::asio::make_strand(ioc)}
    {
        spdlog::info("StompClient: Creating STOMP client for {}:{}{}",
                     url, port, endpoint);
    }

    /*! \brief The copy constructor is deleted.
     */
    StompClient(const StompClient& other) = delete;

    /*! \brief Move constructor.
     */
    StompClient(StompClient&& other) = default;

    /*! \brief The copy assignment operator is deleted.
     */
    StompClient& operator=(const StompClient& other) = delete;

    /*! \brief Move assignment operator.
     */
    StompClient& operator=(StompClient&& other) = default;

    /*! \brief Connect to the STOMP server.
     *
     *  This method first connects to the Websocket server, then tries to
     *  establish a STOMP connection with the user credentials.
     *
     *  \param username     Username.
     *  \param password     Password.
     *  \param onConnect    This handler is called when the STOMP connection
     *                      is setup correctly. It will also be called with an
     *                      error if there is any failure before a successful
     *                      connection.
     *  \param onMessage    This handler is called when the connected client
     *                      or server sends us a message as a SEND frame. The
     *                      handler contains an error code, the message
     *                      destination and content. Ownership of the
     *                      message content is passed to the receiver. It is
     *                      assumed that the message is received with
     *                      application/json content type.
     *  \param onDisconnect This handler is called when the STOMP or the
     *                      Websocket connection is suddenly closed. In the
     *                      STOMP protocol, this may happen also in response to
     *                      bad inputs (authentication, subscription).
     *
     *  All handlers run in a separate I/O execution context from the Websocket
     *  one.
     */
    void Connect(
        const std::string& username,
        const std::string& password,
        std::function<void (StompClientError)> onConnect = nullptr,
        std::function<
            void (StompClientError, const std::string&, std::string&&)
        > onMessage = nullptr,
        std::function<void (StompClientError)> onDisconnect = nullptr
    )
    {
        spdlog::info("StompClient: Connecting to STOMP server");
        username_ = username;
        password_ = password;
        onConnect_ = onConnect;
        onMessage_ = onMessage;
        onDisconnect_ = onDisconnect;
        ws_.Connect(
            [this](auto ec) {
                OnWsConnect(ec);
            },
            [this](auto ec, auto&& msg) {
                OnWsMessage(ec, std::move(msg));
            },
            [this](auto ec) {
                OnWsDisconnect(ec);
            }
        );
    }

    /*! \brief Close the STOMP and Websocket connection.
     *
     *  \param onClose This handler is called when the connection has been
     *                 closed.
     *
     *  All handlers run in a separate I/O execution context from the Websocket
     *  one.
     */
    void Close(
        std::function<void (StompClientError)> onClose = nullptr
    )
    {
        spdlog::info("StompClient: Closing connection to STOMP server");
        subscriptions_.clear();
        ws_.Close(
            [this, onClose](auto ec) {
                OnWsClose(ec, onClose);
            }
        );
    }

    /*! \brief Subscribe to a STOMP enpoint.
     *
     *  \returns The subscription ID, if the subscription process was started
     *  successfully; otherwise, an empty string.
     *
     *  \param destination  The subscription topic.
     *  \param onSubscribe  This handler is called when the subscription is
     *                      setup correctly. The handler receives an error code
     *                      and the subscription ID. Note: On failure, this
     *                      callback is only called if the failure happened at
     *                      the Websocket level, not at the STOMP level. This
     *                      is due to the fact that the STOMP server
     *                      automatically closes the Websocket connection on a
     *                      STOMP protocol failure.
     *  \param onMessage    This handler is called on every new message from the
     *                      subscription destination. It is assumed that the
     *                      message is received with application/json content
     *                      type.
     *
     *  All handlers run in a separate I/O execution context from the Websocket
     *  one.
     */
    std::string Subscribe(
        const std::string& destination,
        std::function<void (StompClientError, std::string&&)> onSubscribe,
        std::function<void (StompClientError, std::string&&)> onMessage
    )
    {
        spdlog::info("StompClient: Subscribing to {}", destination);
        auto subscriptionId {GenerateId()};
        Subscription subscription {
            destination,
            onSubscribe,
            onMessage,
        };

        // Assemble the SUBSCRIBE frame.
        // We use the subscription ID to also request a receipt, so the server
        // will confirm if we are subscribed.
        StompError error {};
        StompFrame frame {
            error,
            StompCommand::kSubscribe,
            {
                {StompHeader::kId, subscriptionId},
                {StompHeader::kDestination, destination},
                {StompHeader::kAck, "auto"},
                {StompHeader::kReceipt, subscriptionId},
            }
        };
        if (error != StompError::kOk) {
            spdlog::error("StompClient: Could not create a valid frame: {}",
                          error);
            return "";
        }

        // Send the Websocket message.
        ws_.Send(
            frame.ToString(),
            [
                this,
                subscriptionId,
                subscription = std::move(subscription)
            ](auto ec) mutable {
                OnWsSendSubscribe(
                    ec,
                    std::move(subscriptionId),
                    std::move(subscription)
                );
            }
        );
        return subscriptionId;
    }

    /*! \brief Send a JSON message to a STOMP destination enpoint.
     *
     *  \returns The request ID. If empty, we failed to send the message.
     *
     *  \param destination      The message destination.
     *  \param messageContent   A string containing the message content. We do
     *                          not check if this string is compatible with the
     *                          content type. We assume the content type is
     *                          application/json. The caller must ensure that
     *                          this string stays in scope until the onSend
     *                          handler is called.
     *  \param onSend           This handler is called when the Websocket
     *                          client terminates the Send operation. On
     *                          success, we cannot guarantee that the message
     *                          reached the STOMP server, only that is was
     *                          correctly sent at the Websocket level. The
     *                          handler contains an error code and the message
     *                          request ID.
     *
     *  All handlers run in a separate I/O execution context from the Websocket
     *  one.
     */
    std::string Send(
        const std::string& destination,
        const std::string& messageContent,
        std::function<void (StompClientError, std::string&&)> onSend = nullptr
    )
    {
        spdlog::info("StompClient: Sending message to {}", destination);

        auto requestId {GenerateId()};
        auto messageSize {messageContent.size()};

        // Assemble the SEND frame.
        StompError error {};
        StompFrame frame {
            error,
            StompCommand::kSend,
            {
                {StompHeader::kId, requestId},
                {StompHeader::kDestination, destination},
                {StompHeader::kContentType, "application/json"},
                {StompHeader::kContentLength, std::to_string(messageSize)},
            },
            messageContent,
        };
        if (error != StompError::kOk) {
            spdlog::error("StompClient: Could not create a valid frame: {}",
                          error);
            return "";
        }

        // Send the Websocket message.
        if (onSend == nullptr) {
            ws_.Send(frame.ToString());
        } else {
            ws_.Send(
                frame.ToString(),
                [requestId, onSend](auto ec) mutable {
                    auto error {ec ? StompClientError::kCouldNotSendMessage :
                                     StompClientError::kOk};
                    onSend(error, std::move(requestId));
                }
            );
        }
        return requestId;
    }

private:
    // This strand handles all the STOMP subscription messages. These operations
    // are decoupled from the Websocket operations.
    // We leave it uninitialized because it does not support a default
    // constructor.
    boost::asio::strand<boost::asio::io_context::executor_type> context_;

    std::string url_ {};

    // We leave this uninitialized because it does not support a default
    // constructor.
    WsClient ws_;

    std::function<void (StompClientError)> onConnect_ {nullptr};
    std::function<
        void (StompClientError, const std::string&, std::string&&)
    > onMessage_ {nullptr};
    std::function<void (StompClientError)> onDisconnect_ {nullptr};
    std::string username_ {};
    std::string password_ {};

    struct Subscription {
        std::string destination {};
        std::function<void (
            StompClientError,
            std::string&&
        )> onSubscribe {nullptr};
        std::function<void (
            StompClientError,
            std::string&&
        )> onMessage {nullptr};
    };

    // We store subscriptions in a map so we can retrieve the message
    // handler for the right subscription when a message arrives.
    std::unordered_map<std::string, Subscription> subscriptions_ {};

    void OnWsConnect(
        boost::system::error_code ec
    )
    {
        using Error = StompClientError;

        // We cannot continue if the connection was not established correctly.
        if (ec) {
            spdlog::error("StompClient: Could not connect to server: {}",
                          ec.message());
            if (onConnect_) {
                boost::asio::post(
                    context_,
                    [onConnect = onConnect_]() {
                        onConnect(Error::kCouldNotConnectToWebsocketServer);
                    }
                );
            }
            return;
        }

        // Assemble and send the STOMP frame.
        StompError error {};
        StompFrame frame {
            error,
            StompCommand::kStomp,
            {
                {StompHeader::kAcceptVersion, "1.2"},
                {StompHeader::kHost, url_},
                {StompHeader::kLogin, username_},
                {StompHeader::kPasscode, password_},
            }
        };
        if (error != StompError::kOk) {
            spdlog::error("StompClient: Could not create a valid frame: {}",
                          error);
            if (onConnect_) {
                boost::asio::post(
                    context_,
                    [onConnect = onConnect_]() {
                        onConnect(Error::kUnexpectedCouldNotCreateValidFrame);
                    }
                );
            }
            return;
        }
        ws_.Send(
            frame.ToString(),
            [this](auto ec) {
                OnWsSendStomp(ec);
            }
        );
    }

    void OnWsSendStomp(
        boost::system::error_code ec
    )
    {
        // If we got here, it only means that we correctly sent the STOMP
        // frame, not that we are authenticated. OnWsMessage is the function
        // that handles the response from the server.
        if (ec) {
            spdlog::error("StompClient: Could not send STOMP frame: {}",
                          ec.message());
            if (onConnect_) {
                boost::asio::post(
                    context_,
                    [onConnect = onConnect_]() {
                        onConnect(StompClientError::kCouldNotSendStompFrame);
                    }
                );
            }
        }
    }

    void OnWsSendSubscribe(
        boost::system::error_code ec,
        std::string&& subscriptionId,
        Subscription&& subscription
    )
    {
        // At this stage we can only know if we successfully sent the SUBSCRIBE
        // command, but not if our subscription was acknowledged.
        if (!ec) {
            // Save the subscription.
            subscriptions_.emplace(
                subscriptionId,
                std::move(subscription)
            );
        } else {
            // Notify the user.
            spdlog::error("StompClient: Could not subscribe to {}: {}",
                          subscription.destination, ec.message());
            if (subscription.onSubscribe) {
                boost::asio::post(
                    context_,
                    [onSubscribe = subscription.onSubscribe]() {
                        onSubscribe(
                            StompClientError::kCouldNotSendSubscribeFrame,
                            ""
                        );
                    }
                );
            }
        }
    }

    void OnWsMessage(
        boost::system::error_code ec,
        std::string&& msg
    )
    {
        // Parse the message.
        StompError error {};
        StompFrame frame {error, std::move(msg)};
        if (error != StompError::kOk) {
            spdlog::error(
                "StompClient: Could not parse message as STOMP frame: {}",
                error
            );
            if (onConnect_) {
                boost::asio::post(
                    context_,
                    [onConnect = onConnect_]() {
                        onConnect(
                            StompClientError::kCouldNotParseMessageAsStompFrame
                        );
                    }
                );
            }
            return;
        }

        // Decide what to do based on the STOMP command.
        spdlog::debug("StompClient: Received {}", frame.GetCommand());
        switch (frame.GetCommand()) {
            case StompCommand::kConnected: {
                HandleConnected(std::move(frame));
                break;
            }
            case StompCommand::kError: {
                HandleError(std::move(frame));
                break;
            }
            case StompCommand::kMessage: {
                HandleSubscriptionMessage(std::move(frame));
                break;
            }
            case StompCommand::kReceipt: {
                HandleSubscriptionReceipt(std::move(frame));
                break;
            }
            case StompCommand::kSend: {
                HandleMessage(std::move(frame));
                break;
            }
            default: {
                spdlog::error("StompClient: Unexpected STOMP command: {}",
                              frame.GetCommand());
                break;
            }
        }
    }

    void OnWsDisconnect(
        boost::system::error_code ec
    )
    {
        // Notify the user.
        spdlog::info("StompClient: Websocket connection disconnected: {}",
                     ec.message());
        if (onDisconnect_) {
            auto error {ec ? StompClientError::kWebsocketServerDisconnected :
                             StompClientError::kOk};
            boost::asio::post(
                context_,
                [onDisconnect = onDisconnect_, error]() {
                    onDisconnect(error);
                }
            );
        }
    }

    void OnWsClose(
        boost::system::error_code ec,
        std::function<void (StompClientError)> onClose = nullptr
    )
    {
        // Notify the user.
        if (onClose) {
            auto error {ec ?
                StompClientError::kCouldNotCloseWebsocketConnection :
                StompClientError::kOk
            };
            boost::asio::post(
                context_,
                [onClose, error]() {
                    onClose(error);
                }
            );
        }
    }

    void HandleConnected(
        StompFrame&& frame
    )
    {
        // Notify the user of the susccessful connection.
        spdlog::info("StompClient: Successfully connected to STOMP server");
        if (onConnect_) {
            boost::asio::post(
                context_,
                [onConnect = onConnect_]() {
                    onConnect(StompClientError::kOk);
                }
            );
        }
    }

    void HandleError(
        StompFrame&& frame
    )
    {
        // At the moment we do not handle errors, we only print a message.
        spdlog::error("StompClient: The STOMP server returned an error: {}",
                      frame.GetBody());
    }

    void HandleSubscriptionMessage(
        StompFrame&& frame
    )
    {
        // Find the subscription.
        auto subscriptionId {frame.GetHeaderValue(StompHeader::kSubscription)};
        auto subscriptionIt {subscriptions_.find(std::string(subscriptionId))};
        if (subscriptionIt == subscriptions_.end()) {
            spdlog::error("StompClient: Cannot find subscription {}",
                          subscriptionId);
            return;
        }
        const auto& subscription {subscriptionIt->second};

        // Check the subscription destination.
        auto destination {frame.GetHeaderValue(StompHeader::kDestination)};
        if (destination != subscription.destination) {
            spdlog::error("StompClient: Destination mismatch {} / {}",
                          destination, subscription.destination);
            if (subscription.onMessage) {
                boost::asio::post(
                    context_,
                    [onMessage = subscription.onMessage]() {
                        onMessage(
                            StompClientError::kUnexpectedSubscriptionMismatch,
                            {}
                        );
                    }
                );
            }
            return;
        }

        // Send the message to the user handler.
        if (subscription.onMessage) {
            boost::asio::post(
                context_,
                [
                    onMessage = subscription.onMessage,
                    message = std::string(frame.GetBody())
                ]() mutable {
                    onMessage(StompClientError::kOk, std::move(message));
                }
            );
        }
    }

    void HandleSubscriptionReceipt(
        StompFrame&& frame
    )
    {
        // Find the subscription.
        // When we send the SUBSCRIBE frame, we request a receipt with the same
        // ID of the subscription so that it's easier to retrieve it here.
        auto subscriptionId {frame.GetHeaderValue(StompHeader::kReceiptId)};
        auto subscriptionIt {subscriptions_.find(std::string(subscriptionId))};
        if (subscriptionIt == subscriptions_.end()) {
            spdlog::error("StompClient: Cannot find subscription {}",
                          subscriptionId);
            return;
        }
        const auto& subscription {subscriptionIt->second};

        // Notify the user of the susccessful subscription.
        spdlog::info("StompClient: Successfully subscribed to {}",
                     subscriptionId);
        if (subscription.onSubscribe) {
            boost::asio::post(
                context_,
                [
                    onSubscribe = subscription.onSubscribe,
                    subscriptionId = std::string(subscriptionId)
                ]() mutable {
                    onSubscribe(StompClientError::kOk,
                                std::move(subscriptionId));
                }
            );
        }
    }

    void HandleMessage(
        StompFrame&& frame
    )
    {
        // Call the user callback.
        if (onMessage_) {
            boost::asio::post(
                context_,
                [
                    onMessage = onMessage_,
                    destination = std::string(frame.GetHeaderValue(
                        StompHeader::kDestination
                    )),
                    message = std::string(frame.GetBody())
                ]() mutable {
                    onMessage(
                        StompClientError::kOk,
                        destination,
                        std::move(message)
                    );
                }
            );
        }
    }

    std::string GenerateId()
    {
        std::stringstream ss {};
        ss << boost::uuids::random_generator()();
        return ss.str();
    }
};

} // namespace NetworkMonitor

#endif // NETWORK_MONITOR_STOMP_CLIENT_H