#ifndef NETWORK_MONITOR_STOMP_SERVER_H
#define NETWORK_MONITOR_STOMP_SERVER_H

#include <network-monitor/StompFrame.h>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include <iomanip>
#include <iostream>
#include <functional>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace NetworkMonitor {

/*! \brief Error codes for the STOMP client.
 */
enum class StompServerError {
    kOk = 0,
    kUndefinedError,
    kClientCannotReconnect,
    kCouldNotCloseClientConnection,
    kCouldNotParseFrame,
    kCouldNotSendMessage,
    kCouldNotStartWebsocketServer,
    kInvalidHeaderValueAcceptVersion,
    kInvalidHeaderValueHost,
    kUnsupportedFrame,
    kWebsocketSessionDisconnected,
    kWebsocketServerDisconnected,
};

/*! \brief Print operator for the `StompServerError` class.
 */
std::ostream& operator<<(std::ostream& os, const StompServerError& m);

/*! \brief Convert `StompServerError` to string.
 */
std::string ToString(const StompServerError& m);

/*! \brief STOMP server implementing the subset of commands needed by the
 *         quiet-route service.
 *
 *  \tparam WsServer    Websocket server class. This type must have the same
 *                      interface of `WebsocketServer`.
 */
template <typename WsServer>
class StompServer {
public:
    /*! \brief Callback type for the server, with an attached error code.
     */
    using ServerHandler = std::function<void (StompServerError ec)>;

    /*! \brief Callback type for a connection, with an attached error code.
     */
    using ClientHandler = std::function<
        void (
            StompServerError ec,
            const std::string& clientConnectionId
        )
    >;

    /*! \brief Message callback type for a connection, with an attached error
     *         code.
     *
     *  The user receives:
     *  - The ID of the connection that received the message.
     *  - The message destination endpoint.
     *  - The message request ID (optional, non-standard). The user may re-use
     *    this request ID to send a response back to the client.
     *  - The message content.
     *  Owenship of the message content is passed to the caller. The assumption is
     *  that the message content type is application/json.
     */
    using ClientMsgHandler = std::function<
        void (
            StompServerError ec,
            const std::string& clientConnectionId,
            const std::string& destination,
            const std::string& requestId,
            std::string&& msgContent
        )
    >;

    /*! \brief Construct a STOMP server accepting connections on a specific
     *         port through a secure Websocket connection.
     *
     *  \note This constructor does not automatically listen to incoming
     *        connections.
     *
     *  \param host The hostname of the STOMP server.
     *  \param ip   The IP of the server.
     *  \param port The port of the server.
     *  \param ioc  The io_context object. The user takes care of calling
     *              ioc.run().
     *  \param ctx  The TLS context to setup a TLS socket stream.
     */
    StompServer(
        const std::string& host,
        const std::string& ip,
        const unsigned short port,
        boost::asio::io_context& ioc,
        boost::asio::ssl::context& ctx
    ) : kHost_ {host},
        ws_ {ip, port, ioc, ctx},
        context_ {boost::asio::make_strand(ioc)}
    {
        spdlog::info("StompServer: New server on port {}", port);
    }

    /*! \brief The copy constructor is deleted.
     */
    StompServer(const StompServer& other) = delete;

    /*! \brief Move constructor.
     */
    StompServer(StompServer&& other) = default;

    /*! \brief The copy assignment operator is deleted.
     */
    StompServer& operator=(const StompServer& other) = delete;

    /*! \brief Move assignment operator.
     */
    StompServer& operator=(StompServer&& other) = default;

    /*! \brief Start the STOMP server.
     *
     *  This method starts the Websocket server. Every new incoming connection
     *  is validated through the STOMP protocol before being considered a valid
     *  STOMP connection.
     *
     *  The user does not see incoming connections before they are proper STOMP
     *  connections.
     *
     *  \returns StompServerError::kOk if the Websocket server was started
     *  successfully.
     *
     *  \param onClientConnect      Called when a new STOMP connection is
     *                              established. The user does not receive
     *                              callbacks for failed or incomplete STOMP
     *                              connections.
     *  \param onClientMessage      Called when a connected STOMP client sends
     *                              us a message. The message content type is
     *                              assumed to be application/json.
     *  \param onClientDisconnect   Called when a connected STOMP client
     *                              disconnects on its own or is disconnected by
     *                              us as a result of a STOMP protocol error.
     *  \param onDisconnect         Called when the STOMP server itself is
     *                              disconnected. All connected clients are
     *                              disconnected automatically.
     *
     *  All handlers run in a separate I/O execution context from the Websocket
     *  one.
     */
    StompServerError Run(
        ClientHandler onClientConnect = nullptr,
        ClientMsgHandler onClientMessage = nullptr,
        ClientHandler onClientDisconnect = nullptr,
        ServerHandler onDisconnect = nullptr
    )
    {
        onClientConnect_ = onClientConnect;
        onClientMessage_ = onClientMessage;
        onClientDisconnect_ = onClientDisconnect;
        onDisconnect_ = onDisconnect;
        auto ec {ws_.Run(
            [this](auto ec, auto wsSession) {
                OnWsSessionConnect(ec, wsSession);
            },
            [this](auto ec, auto wsSession, auto&& msg) {
                OnWsSessionMessage(ec, wsSession, std::move(msg));
            },
            [this](auto ec, auto wsSession) {
                OnWsSessionDisconnect(ec, wsSession);
            },
            [this](auto ec) {
                OnWsDisconnect(ec);
            }
        )};
        if (ec) {
            spdlog::error("StompServer: Could not start Websocket server: {}",
                          ec.message());
            return StompServerError::kCouldNotStartWebsocketServer;
        } else {
            spdlog::info("StompServer: Websocket server started");
            return StompServerError::kOk;
        }
    }

    /*! \brief Send a JSON message to a connected STOMP client.
     *
     *  \returns The request ID. If empty, we failed to send the message.
     *
     *  \param connectionId     The STOMP connection ID. This ID has been
     *                          previously provided by the onClientConnect
     *                          callback.
     *  \param destination      The message destination.
     *  \param messageContent   A string containing the message content. Do
     *                          not check if this string is compatible with the
     *                          content type. The assumption is that the content 
     *                          type is application/json. The caller must ensure 
     *                          that this string stays in scope until the onSend
     *                          handler is called.
     *  \param onSend           This handler is called when the Websocket
     *                          client terminates the Send operation. On
     *                          success, we cannot guarantee that the message
     *                          reached the STOMP client, only that is was
     *                          correctly sent at the Websocket level. The
     *                          handler contains an error code and the message
     *                          request ID.
     *  \param userRequestId    An optional user-defined request ID. A user
     *                          may want to use a custom request ID when
     *                          replying to a previous client request.
     *
     *  All handlers run in a separate I/O execution context from the Websocket
     *  one.
     */
    std::string Send(
        const std::string& connectionId,
        const std::string& destination,
        const std::string& messageContent,
        std::function<void (StompServerError, std::string&&)> onSend = nullptr,
        const std::string& userRequestId = ""
    )
    {
        // The connection should exist to begin with.
        auto sessionIt {sessions_.find(connectionId)};
        if (sessionIt == sessions_.end()) {
            spdlog::error("StompServer: Unrecognized STOMP connection: {}",
                          connectionId);
            return "";
        }
        auto& wsSession {sessionIt->second};
        auto connectionIt {connections_.find(wsSession)};
        if (connectionIt == connections_.end()) {
            spdlog::error("StompServer: Unrecognized Websocket connection: {}",
                          wsSession);
            // Close the Websocket connection here, as this is not a
            // valid STOMP connection.
            wsSession->Close();
            return "";
        }
        const auto& connection {connectionIt->second};

        // The client must be connected.
        if (connection.status != ConnectionStatus::kConnected) {
            spdlog::error("StompServer: [{}] Could not send message: "
                          "STOMP not yet connected",
                          connectionId);
            return "";
        }

        auto requestId {userRequestId.empty() ? GenerateId() : userRequestId};
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
            spdlog::error("StompServer: Could not create a valid frame: {}",
                          error);
            return "";
        }

        // Send the Websocket message.
        spdlog::info("StompServer: [{}] Sending message to {}",
                     connectionId, destination);
        if (onSend == nullptr) {
            wsSession->Send(frame.ToString());
        } else {
            wsSession->Send(
                frame.ToString(),
                [requestId, onSend](auto ec) mutable {
                    auto error {ec ? StompServerError::kCouldNotSendMessage :
                                     StompServerError::kOk};
                    onSend(error, std::move(requestId));
                }
            );
        }
        return requestId;
    }

    /*! \brief Close a connection.
     *
     *  This method closes an individual client connection asynchronously.
     *
     *  \param onClientClose    If provided, this handler is called after the
     *                          client connection has been closed.
     */
    void Close(
        const std::string& connectionId,
        ClientHandler onClientClose = nullptr
    )
    {
        // The connection should exist to begin with.
        auto sessionIt {sessions_.find(connectionId)};
        if (sessionIt == sessions_.end()) {
            spdlog::error("StompServer: Unrecognized STOMP connection: {}",
                          connectionId);
            return;
        }
        auto& wsSession {sessionIt->second};
        auto connectionIt {connections_.find(wsSession)};
        if (connectionIt == connections_.end()) {
            spdlog::error("StompServer: Unrecognized Websocket connection: {}",
                          wsSession);
            // Close the Websocket connection here, as this is not a
            // valid STOMP connection.
            if (onClientClose) {
                wsSession->Close(
                    [onClientClose, connectionId](auto ec) {
                        StompServerError error {ec ?
                            StompServerError::kCouldNotCloseClientConnection :
                            StompServerError::kOk
                        };
                        onClientClose(error, connectionId);
                    }
                );
            } else {
                wsSession->Close();
            }
            return;
        }
        const auto& connection {connectionIt->second};

        // Close the connection without reason.
        if (onClientClose) {
            CloseConnection(
                connection,
                wsSession,
                StompServerError::kUndefinedError,
                [onClientClose, connectionId](auto ec) {
                    StompServerError error {ec ?
                        StompServerError::kCouldNotCloseClientConnection :
                        StompServerError::kOk
                    };
                    onClientClose(error, connectionId);
                }
            );
        } else {
            CloseConnection(connection, wsSession);
        }
    }

    /*! \brief Stop listening to new incoming connections.
     *
     *  This method stops the server synchronously, with immediate effect. All
     *  active connections are also closed, but asynchronously.
     *
     *  \note This method does not trigger any of the user callbacks,
     *        onClientDisconnect and onDisconnect.
     */
    void Stop()
    {
        spdlog::info("StompServer: Stopping server");
        ws_.Stop();
        for (auto& [wsSession, _]: connections_) {
            wsSession->Close();
        }
        connections_.clear();
        sessions_.clear();
    }

private:
    enum class ConnectionStatus {
        kInvalid,
        kPending,
        kConnected,
    };

    struct Connection {
        std::string id {};
        ConnectionStatus status {ConnectionStatus::kInvalid};
    };

    const std::string kVersion_ {"1.2"};
    const std::string kHost_ {""};

    // This strand handles all the STOMP-specific callbacks. These operations
    // are decoupled from the Websocket operations.
    // Leave it uninitialized because it does not support a default
    // constructor.
    boost::asio::strand<boost::asio::io_context::executor_type> context_;

    // Leave this uninitialized because it does not support a default
    // constructor.
    WsServer ws_;

    ClientHandler onClientConnect_ {nullptr};
    ClientMsgHandler onClientMessage_ {nullptr};
    ClientHandler onClientDisconnect_ {nullptr};
    ServerHandler onDisconnect_ {nullptr};

    // Maintain two maps of all active sessions to make sure that they are
    // properly connected according to the STOMP protocol.
    // Unfortunately we need to keep a mapping in both directions:
    // connections <--> sessions
    std::unordered_map<
        std::shared_ptr<typename WsServer::Session>,
        Connection
    > connections_ {};
    std::unordered_map<
        std::string,
        std::shared_ptr<typename WsServer::Session>
    > sessions_ {};

    void OnWsSessionConnect(
        boost::system::error_code ec,
        std::shared_ptr<typename WsServer::Session> wsSession
    )
    {
        // On error, we close the connection.
        if (ec) {
            spdlog::error(
                "StompServer: [{}] Could not open new Websocket connection: {}",
                wsSession, ec.message()
            );
            // Close the Websocket connection here, as this is not a
            // valid STOMP connection.
            wsSession->Close();
            return;
        }

        // A new Websocket connection only becomes a STOMP connection after a
        // successful STOMP frame from the client. Save the new connection
        // as pending.
        Connection connection {
            GenerateId(),
            ConnectionStatus::kPending,
        };
        spdlog::info("StompServer: [{}] STOMP status: Pending",
                     connection.id);
        sessions_[connection.id] = wsSession;
        connections_[wsSession] = std::move(connection);
    }

    void OnWsSessionMessage(
        boost::system::error_code ec,
        std::shared_ptr<typename WsServer::Session> wsSession,
        std::string&& msg
    )
    {
        // The connection should exist to begin with.
        auto connectionIt {connections_.find(wsSession)};
        if (connectionIt == connections_.end()) {
            spdlog::error("StompServer: Unrecognized Websocket connection: {}",
                          wsSession);
            // Close the Websocket connection here, as this is not a
            // valid STOMP connection.
            wsSession->Close();
            return;
        }
        auto& connection {connectionIt->second};

        // On error (Websockets)
        if (ec) {
            spdlog::error("StompServer: [{}] Invalid Websocket message",
                          connection.id);
            return;
        }

        // Parse the message.
        StompError error {};
        StompFrame frame {error, std::move(msg)};
        if (error != StompError::kOk) {
            CloseConnection(
                connection,
                wsSession,
                StompServerError::kCouldNotParseFrame
            );
            return;
        }

        // Decide what to do based on the STOMP command.
        auto command {frame.GetCommand()};
        spdlog::info("StompServer: [{}] Received {} frame",
                     connection.id, command);
        switch (command) {
            case StompCommand::kStomp: {
                HandleStomp(wsSession, connection, std::move(frame));
                break;
            }
            case StompCommand::kSend: {
                HandleSend(wsSession, connection, std::move(frame));
                break;
            }
            default: {
                CloseConnection(
                    connection,
                    wsSession,
                    StompServerError::kUnsupportedFrame
                );
                return;
            }
        }
    }

    void OnWsSessionDisconnect(
        boost::system::error_code ec,
        std::shared_ptr<typename WsServer::Session> wsSession
    )
    {
        // The connection should exist to begin with.
        auto connectionIt {connections_.find(wsSession)};
        if (connectionIt == connections_.end()) {
            spdlog::error("StompServer: [{}] Unrecognized Websocket connection",
                          wsSession);
            return;
        }
        const auto id {connectionIt->second.id};
        const auto status {connectionIt->second.status};

        // Call the user callback, but only if the STOMP connection
        // was successfully established.
        spdlog::info("StompServer:: [{}] Disconnected: {}", id, ec.message());
        sessions_.erase(id);
        connections_.erase(wsSession);
        if (status == ConnectionStatus::kConnected && onClientDisconnect_) {
            auto error {ec ? StompServerError::kWebsocketSessionDisconnected :
                             StompServerError::kOk};
            boost::asio::post(
                context_,
                [onClientDisconnect = onClientDisconnect_, error, id]() {
                    onClientDisconnect(error, id);
                }
            );
        }
    }

    void OnWsDisconnect(
        boost::system::error_code ec
    )
    {
        // Do not terminate the connections, we simply stop listening to new ones.

        // Call the user callback.
        spdlog::info("StompServer:: WebsocketServer disconnected: {}",
                     ec.message());
        if (onDisconnect_) {
            auto error {ec ? StompServerError::kWebsocketServerDisconnected :
                             StompServerError::kOk};
            boost::asio::post(
                context_,
                [onDisconnect = onDisconnect_, error]() {
                    onDisconnect(error);
                }
            );
        }
    }

    void CloseConnection(
        const Connection& connection,
        std::shared_ptr<typename WsServer::Session> wsSession,
        const StompServerError error = StompServerError::kUndefinedError,
        std::function<void (boost::system::error_code)> onClose = nullptr
    )
    {
        spdlog::info(
            "StompServer: [{}] Closing connection{}{}",
            connection.id,
            error == StompServerError::kUndefinedError ? "" : ": ",
            error == StompServerError::kUndefinedError ? "" : ToString(error)
        );
        sessions_.erase(connection.id);
        connections_.erase(wsSession);
        if (error != StompServerError::kUndefinedError) {
            wsSession->Send(MakeErrorFrame(
                StompServerError::kUnsupportedFrame
            ).ToString());
        }
        wsSession->Close(onClose);
    }

    void HandleStomp(
        std::shared_ptr<typename WsServer::Session> wsSession,
        Connection& connection,
        StompFrame&& frame
    )
    {
        // Check the expected values in the frame.
        auto version {frame.GetHeaderValue(StompHeader::kAcceptVersion)};
        if (version != kVersion_) {
            CloseConnection(
                connection,
                wsSession,
                StompServerError::kInvalidHeaderValueAcceptVersion
            );
            return;
        }
        auto host {frame.GetHeaderValue(StompHeader::kHost)};
        if (host != kHost_) {
            CloseConnection(
                connection,
                wsSession,
                StompServerError::kInvalidHeaderValueHost
            );
            return;
        }

        // Do not support a re-connection.
        if (connection.status != ConnectionStatus::kPending) {
            spdlog::error("StompServer: [{}] Connection was not pending",
                          connection.id);
            CloseConnection(
                connection,
                wsSession,
                StompServerError::kClientCannotReconnect
            );
            return;
        }

        // Officially connected.
        spdlog::info("StompServer: [{}] STOMP status: Connected",
                     connection.id);
        connection.status = ConnectionStatus::kConnected;

        // Send a CONNECTED frame.
        StompError error {};
        StompFrame response {
            error,
            StompCommand::kConnected,
            {
                {StompHeader::kVersion, "1.2"},
                {StompHeader::kSession, connection.id},
            }
        };
        if (error != StompError::kOk) {
            spdlog::error(
                "StompServer: [{}] Unexpected: Could not create frame: {}",
                error
            );
            return;
        }
        wsSession->Send(response.ToString());

        // Call the user callback.
        if (onClientConnect_) {
            boost::asio::post(
                context_,
                [onClientConnect = onClientConnect_, id = connection.id]() {
                    onClientConnect(StompServerError::kOk, id);
                }
            );
        }
    }

    void HandleSend(
        std::shared_ptr<typename WsServer::Session> wsSession,
        Connection& connection,
        StompFrame&& frame
    )
    {
        // The client must be connected.
        if (connection.status != ConnectionStatus::kConnected) {
            spdlog::error("StompServer: [{}] Received SEND frame from "
                          "invalid STOMP connection",
                          connection.id);
            // Do not notify the user here, as this is not a valid STOMP
            // connection.
            CloseConnection(
                connection,
                wsSession
            );
            return;
        }

        // Call the user callback.
        if (onClientMessage_) {
            boost::asio::post(
                context_,
                [
                    onClientMessage = onClientMessage_,
                    id = connection.id,
                    destination = std::string(frame.GetHeaderValue(
                        StompHeader::kDestination
                    )),
                    requestId = std::string(frame.GetHeaderValue(
                        StompHeader::kId
                    )),
                    message = std::string(frame.GetBody())
                ]() mutable {
                    onClientMessage(
                        StompServerError::kOk,
                        id,
                        destination,
                        requestId,
                        std::move(message)
                    );
                }
            );
        }
    }

    StompFrame MakeErrorFrame(
        const StompServerError error
    )
    {
        StompError frameError {};
        StompFrame frame {
            frameError,
            StompCommand::kError,
            {
                {StompHeader::kContentType, "text/plain"},
                {StompHeader::kVersion, kVersion_},
            },
            ToString(error)
        };
        if (frameError != StompError::kOk) {
            spdlog::error(
                "StompServer: [{}] Unexpected: Could not create frame: {}",
                error
            );
        }
        return frame;
    }

    std::string GenerateId()
    {
        std::stringstream ss {};
        ss << boost::uuids::random_generator()();
        return ss.str();
    }
};

} // namespace NetworkMonitor

#endif // NETWORK_MONITOR_STOMP_SERVER_H