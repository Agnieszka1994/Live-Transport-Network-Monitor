#ifndef NETWORK_MONITOR_STOMP_FRAME_H
#define NETWORK_MONITOR_STOMP_FRAME_H

#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace NetworkMonitor {

/*! \brief Available STOMP commands, from the STOMP protocol v1.2.
 */
enum class StompCommand {
    kInvalid,
    kAbort,
    kAck,
    kBegin,
    kCommit,
    kConnect,
    kDisconnect,
    kError,
    kMessage,
    kNack,
    kReceipt,
    kSend,
    kStomp,
    kSubscribe,
    kUnsubscribe,
};

/*! \brief Print operator for the stompCommand class
 */
std::ostream operator<<(std::ostream& os, const StompCommand& command);

/*! \brief Convert StompCommand to string
 */
std::string ToString(const StompCommand& command);

/*! \brief Available STOMP headers, from the STOMP protocol v1.2.
 */
enum class StompHeader {
    kInvalid,
    kAcceptVersion,
    kAck,
    kContentLength,
    kContentType,
    kDestination,
    kHeartBeat,
    kHost,
    kId,
    kLogin,
    kMessage,
    kMessageId,
    kPasscode,
    kReceipt,
    kReceiptId,
    kSession,
    kSubscription,
    kTransaction,
    kServer,
    kVersion
};

/*! \brief Print operator for the StompHeader class
 */
std::ostream operator<<(std::ostream& os, const StompHeader& header);

/*! \brief Convert StompHeader to string
 */
std::string ToString(const StompHeader& header);

/*! \brief Error codes for the STOMP protocol
 */
enum class StompError {
    kOk = 0,
    kUndefinedError,
    kParsingEmptyHeaderValue,
    kParsingContentLengthExceedsFrameLength,
    kParsingInvalidContentLength,
    kParsingJunkAfterBody,
    kParsingMissingBlankLineAfterHeaders,
    kParsingMissingColonInHeader,
    kParsingMissingEolAfterCommand,
    kParsingMissingEolAfterHeaderValue,
    kParsingMissingNullInBody,
    kParsingUnrecognizedCommand,
    kParsingUnrecognizedHeader,
    kValidationContentLengthMismatch,
    kValidationInvalidCommand,
    kValidationInvalidContentLength,
    kValidationMissingHeader,
};

/*! \brief Print operator for the StompError class
 */
std::ostream operator<<(std::ostream& os, const StompError& error);

/*! \brief Convert StompError to string
 */
std::string ToString(const StompError& error);

/* \brief STOMP frame representation, supporting STOMP v1.2.
 */
class StompFrame {
public:

    using Headers = std::unordered_map<StompHeader, std::string_view>;

    /*! \brief Default constructor. Corresponds to an empty, invalid STOMP
     *         frame.
     */
    StompFrame();

    /*! \brief Construct the STOMP frame from a string. The string is copied.
     *
     *  The result of the operation is stored in the error code.
     */
    StompFrame(
        StompError& ec,
        const std::string& frame
    );

    /*! \brief Construct the STOMP frame from a string. The string is moved into
     *         the object.
     *
     *  The result of the operation is stored in the error code.
     */
    StompFrame(
        StompError& ec,
        std::string&& frame
    );

    /*! \brief Copy constructor.
     */
    StompFrame(const StompFrame& other);

    /*! \brief Move constructor.
     */
    StompFrame(StompFrame&& other);

    /*! \brief Copy assignment operator.
     */
    StompFrame& operator=(const StompFrame& other);

    /*! \brief Move assignment operator.
     */
    StompFrame& operator=(StompFrame&& other);

    /*! \brief Get the STOMP command.
     */
    StompCommand GetCommand() const;

    /*! \brief Check if the frame has a specified header.
     */
    const bool HasHeader(const StompHeader& header) const;

    /*! \brief Get the value for the specified header.
     *
     *  \returns An empty std::string_view if the header is not in the frame.
     */
    const std::string_view& GetHeaderValue(const StompHeader& header) const;

    /*! \brief Get the frame body.
     */
    const std::string_view& GetBody() const;

    /*! \brief Dump the frame to string.
     */
    std::string ToString() const;

private:
    std::string plain_ {};

    StompCommand command_ {StompCommand::kInvalid};
    Headers headers_ {};
    std::string_view body_ {};

    // Helper function to parse and validate a STOMP frame
    StompError ParseAndValidateFrame(const std::strig_view& frame);

    // Parse string_view into a separate items: 
    // A command, a map of headers and a body view
    StompError ParseFrame(const std::string_view& frame);

    // Validate the STOMP frame based on the parsed date
    // (command, headers, map, body view)
    StompError ValidateFrame();
};

} // namespace NetworkMonitor

#endif // NETWORK_MONITOR_STOMP_FRAME_H