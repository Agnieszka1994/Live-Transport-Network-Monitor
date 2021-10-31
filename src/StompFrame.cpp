#include <network-monitor/StompFrame.h>

#include <boost/bimap.hpp>

#include <charconv>
#include <initializer_list>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

using NetworkMonitor::StompCommand;
using NetworkMonitor::StompError;
using NetworkMonitor::StompFrame;
using NetworkMonitor::StompHeader;

using Headers = StompFrame::Headers;

// Utility function to generate a boost::bimap.
template <typename L, typename R>
static boost::bimap<L, R> MakeBimap(
    std::initializer_list<typename boost::bimap<L, R>::value_type> list
)
{
    return boost::bimap<L, R>(list.begin(), list.end());
}

// Utility function to convert a std::string_view to a number.
// Returns false if the conversion failed.
static bool StoI(const std::string_view string, size_t& result)
{
    auto conversionResult {std::from_chars(
        string.data(),
        string.data() + string.size(),
        result
    )};
    return conversionResult.ec != std::errc::invalid_argument;
}

// StompCommand

static const auto gStompCommandStrings {
    MakeBimap<StompCommand, std::string_view>({
        {StompCommand::kAbort      , "ABORT"      },
        {StompCommand::kAck        , "ACK"        },
        {StompCommand::kBegin      , "BEGIN"      },
        {StompCommand::kCommit     , "COMMIT"     },
        {StompCommand::kConnect    , "CONNECT"    },
        {StompCommand::kConnected  , "CONNECTED"  },
        {StompCommand::kDisconnect , "DISCONNECT" },
        {StompCommand::kError      , "ERROR"      },
        {StompCommand::kMessage    , "MESSAGE"    },
        {StompCommand::kNack       , "NACK"       },
        {StompCommand::kReceipt    , "RECEIPT"    },
        {StompCommand::kSend       , "SEND"       },
        {StompCommand::kStomp      , "STOMP"      },
        {StompCommand::kSubscribe  , "SUBSCRIBE"  },
        {StompCommand::kUnsubscribe, "UNSUBSCRIBE"},
    })
};

std::ostream& NetworkMonitor::operator<<(
    std::ostream& os,
    const StompCommand& command
)
{
    auto commandIt {gStompCommandStrings.left.find(command)};
    if (commandIt == gStompCommandStrings.left.end()) {
        os << "StompCommand::kInvalid";
    } else {
        os << commandIt->second;
    }
    return os;
}

std::string NetworkMonitor::ToString(const StompCommand& command)
{
    auto commandIt {gStompCommandStrings.left.find(command)};
    if (commandIt == gStompCommandStrings.left.end()) {
        return "StompCommand::kInvalid";
    }
    return std::string(commandIt->second);
}

static const StompCommand& ToCommand(const std::string_view command)
{
    static const auto invalidCommand {StompCommand::kInvalid};
    auto commandIt {gStompCommandStrings.right.find(command)};
    if (commandIt == gStompCommandStrings.right.end()) {
        return invalidCommand;
    }
    return commandIt->second;
}

// StompHeader

static const auto gStompHeaderStrings {
    MakeBimap<StompHeader, std::string_view>({
        {StompHeader::kAcceptVersion, "accept-version"},
        {StompHeader::kAck          , "ack"           },
        {StompHeader::kContentLength, "content-length"},
        {StompHeader::kContentType  , "content-type"  },
        {StompHeader::kDestination  , "destination"   },
        {StompHeader::kHeartBeat    , "heart-beat"    },
        {StompHeader::kHost         , "host"          },
        {StompHeader::kId           , "id"            },
        {StompHeader::kLogin        , "login"         },
        {StompHeader::kMessage      , "message"       },
        {StompHeader::kMessageId    , "message-id"    },
        {StompHeader::kPasscode     , "passcode"      },
        {StompHeader::kReceipt      , "receipt"       },
        {StompHeader::kReceiptId    , "receipt-id"    },
        {StompHeader::kSession      , "session"       },
        {StompHeader::kSubscription , "subscription"  },
        {StompHeader::kTransaction  , "transaction"   },
        {StompHeader::kServer       , "server"        },
        {StompHeader::kVersion      , "version"       },
    })
};

std::ostream& NetworkMonitor::operator<<(
    std::ostream& os,
    const StompHeader& header
)
{
    auto headerIt {gStompHeaderStrings.left.find(header)};
    if (headerIt == gStompHeaderStrings.left.end()) {
        os << "StompHeader::kInvalid";
    } else {
        os << headerIt->second;
    }
    return os;
}

std::string NetworkMonitor::ToString(const StompHeader& header)
{
    auto headerIt {gStompHeaderStrings.left.find(header)};
    if (headerIt == gStompHeaderStrings.left.end()) {
        return "StompHeader::kInvalid";
    }
    return std::string(headerIt->second);
}

static const StompHeader& ToHeader(const std::string_view header)
{
    static const auto invalidHeader {StompHeader::kInvalid};
    auto headerIt {gStompHeaderStrings.right.find(header)};
    if (headerIt == gStompHeaderStrings.right.end()) {
        return invalidHeader;
    }
    return headerIt->second;
}

// StompError

static const auto gStompErrorStrings {
    MakeBimap<StompError, std::string_view>({
        {StompError::kOk                                    ,
                     "Ok"                                    },
        {StompError::kUndefinedError                        ,
                     "UndefinedError"                        },
        {StompError::kParsingEmptyHeaderValue               ,
                     "ParsingEmptyHeaderValue"               },
        {StompError::kParsingContentLengthExceedsFrameLength,
                     "ParsingContentLengthExceedsFrameLength"},
        {StompError::kParsingInvalidContentLength           ,
                     "ParsingInvalidContentLength"           },
        {StompError::kParsingJunkAfterBody                  ,
                     "ParsingJunkAfterBody"                  },
        {StompError::kParsingMissingBlankLineAfterHeaders   ,
                     "ParsingMissingBlankLineAfterHeaders   "},
        {StompError::kParsingMissingColonInHeader           ,
                     "ParsingMissingColonInHeader"           },
        {StompError::kParsingMissingEolAfterCommand         ,
                     "ParsingMissingEolAfterCommand"         },
        {StompError::kParsingMissingEolAfterHeaderValue     ,
                     "ParsingMissingEolAfterHeaderValue"     },
        {StompError::kParsingMissingNullInBody              ,
                     "ParsingMissingNullInBody"              },
        {StompError::kParsingUnrecognizedCommand            ,
                     "ParsingUnrecognizedCommand"            },
        {StompError::kParsingUnrecognizedHeader             ,
                     "ParsingUnrecognizedHeader"             },
        {StompError::kValidationContentLengthMismatch       ,
                     "ValidationContentLengthMismatch"       },
        {StompError::kValidationInvalidCommand              ,
                     "ValidationInvalidCommand"              },
        {StompError::kValidationInvalidContentLength        ,
                     "ValidationInvalidContentLength"        },
        {StompError::kValidationMissingHeader               ,
                     "ValidationMissingHeader"               },
    })
};

std::ostream& NetworkMonitor::operator<<(
    std::ostream& os,
    const StompError& error
)
{
    static const auto undefinedError {
        gStompErrorStrings.left.at(StompError::kUndefinedError)
    };
    auto errorIt {gStompErrorStrings.left.find(error)};
    if (errorIt == gStompErrorStrings.left.end()) {
        os << undefinedError;
    } else {
        os << errorIt->second;
    }
    return os;
}

std::string NetworkMonitor::ToString(const StompError& error)
{
    static const auto undefinedError {std::string(
        gStompErrorStrings.left.at(StompError::kUndefinedError)
    )};
    auto errorIt {gStompErrorStrings.left.find(error)};
    if (errorIt == gStompErrorStrings.left.end()) {
        return undefinedError;
    }
    return std::string(errorIt->second);
}

static const StompError& StringToError(const std::string_view error)
{
    static const auto invalidError {StompError::kUndefinedError};
    auto errorIt {gStompErrorStrings.right.find(error)};
    if (errorIt == gStompErrorStrings.right.end()) {
        return invalidError;
    }
    return errorIt->second;
}

// StompFrame — Public methods

StompFrame::StompFrame() = default;

StompFrame::StompFrame(
    StompError& ec,
    const std::string& frame
)
{
    plain_ = frame;
    ec = ParseAndValidateFrame(plain_);
}

StompFrame::StompFrame(
    StompError& ec,
    std::string&& frame
)
{
    plain_ = std::move(frame);
    ec = ParseAndValidateFrame(plain_);
}

StompFrame::StompFrame(
    StompError& ec,
    const StompCommand& command,
    const std::unordered_map<StompHeader, std::string>& headers,
    const std::string& body
)
{
    using namespace std::string_literals;

    // Construct the frame as a string.
    std::string plain {""};
    plain += NetworkMonitor::ToString(command);
    plain += "\n";
    for (const auto& [header, value]: headers) {
        plain += NetworkMonitor::ToString(header);
        plain += ":";
        plain += value;
        plain += "\n";
    }
    plain += "\n";
    plain += body;
    plain += "\0"s;
    plain_ = std::move(plain);

    // This may be wasteful, but we re-parse the frame to make sure it is a
    // valid one. We may optimize this later.
    ec = ParseAndValidateFrame(plain_);
}

// The copy constructor cannot copy the string views. Instead, we copy the
// original plain-text frame and re-parse it.
StompFrame::StompFrame(const StompFrame& other)
{
    plain_ = other.plain_;
    ParseAndValidateFrame(plain_);
}

StompFrame::StompFrame(StompFrame&& other) = default;

// The copy assignment operator cannot copy the string views. Instead, we copy
// the original plain-text frame and re-parse it.
StompFrame& StompFrame::operator=(const StompFrame& other)
{
    if (this != &other) {
        plain_ = other.plain_;
        ParseAndValidateFrame(plain_);
    }
    return *this;
}

StompFrame& StompFrame::operator=(StompFrame&& other) = default;

StompCommand StompFrame::GetCommand() const
{
    return command_;
}

const bool StompFrame::HasHeader(const StompHeader& header) const
{
    return headers_.find(header) != headers_.end();
}

const std::string_view& StompFrame::GetHeaderValue(
    const StompHeader& header
) const
{
    static const std::string_view emptyHeaderValue {""};
    auto headerValueIt {headers_.find(header)};
    if (headerValueIt == headers_.end()) {
        return emptyHeaderValue;
    }
    return headerValueIt->second;
}

const std::string_view& StompFrame::GetBody() const
{
    return body_;
}

std::string StompFrame::ToString() const
{
    return plain_;
}

// StompFrame — Private methods

StompError StompFrame::ParseAndValidateFrame(const std::string_view frame)
{
    auto ec {ParseFrame(frame)};
    if (ec != StompError::kOk) {
        return ec;
    }
    return ValidateFrame();
}

StompError StompFrame::ParseFrame(const std::string_view frame)
{
    const std::string_view plain {frame};

    // Frame delimiters
    static const char null {'\0'};
    static const char colon {':'};
    static const char newLine {'\n'};

    // Command
    size_t commandStart {0};
    size_t commandEnd {plain.find(newLine, commandStart)};
    if (commandEnd == std::string::npos) {
        return StompError::kParsingMissingEolAfterCommand;
    }
    auto command {ToCommand(
        plain.substr(commandStart, commandEnd - commandStart)
    )};
    if (command == StompCommand::kInvalid) {
        return StompError::kParsingUnrecognizedCommand;
    }

    // Headers
    size_t headerLineStart {commandEnd + 1};
    Headers headers {};
    while (headerLineStart < plain.size() &&
           plain.at(headerLineStart) != newLine) {
        size_t headerStart {headerLineStart};
        size_t headerEnd {plain.find(colon, headerStart)};
        if (headerEnd == std::string::npos) {
            return StompError::kParsingMissingColonInHeader;
        }
        auto header {ToHeader(
            plain.substr(headerStart, headerEnd - headerStart)
        )};
        if (header == StompHeader::kInvalid) {
            return StompError::kParsingUnrecognizedHeader;
        };
        size_t valueStart {headerEnd + 1};
        if (plain.at(valueStart) == newLine) {
            return StompError::kParsingEmptyHeaderValue;
        }
        size_t valueEnd {plain.find(newLine, valueStart)};
        if (valueEnd == std::string::npos) {
            return StompError::kParsingMissingEolAfterHeaderValue;
        }
        auto value {plain.substr(valueStart, valueEnd - valueStart)};

        // Skip this header value if the header is already in the header map.
        if (headers.find(header) == headers.end()) {
            headers.emplace(std::make_pair(
                std::move(header),
                std::move(value))
            );
        }

        // Prepare for next line;
        headerLineStart = valueEnd + 1;
    }

    // Blank line between headers and body
    size_t newLineBeforeBody {headerLineStart};
    if (newLineBeforeBody >= plain.size() ||
        plain.at(newLineBeforeBody) != newLine) {
        return StompError::kParsingMissingBlankLineAfterHeaders;
    }

    // Body
    // Everything else that's left is potentially part of the body.
    size_t bodyStart {newLineBeforeBody + 1};
    size_t bodyEnd {0}; // The NULL octet
    size_t bodyLength {0};
    auto contentLengthIt {headers.find(StompHeader::kContentLength)};
    if (contentLengthIt != headers.end()) {
        // If the content-length header is present, we need to read the
        // specified number of bytes.
        auto ok {StoI(contentLengthIt->second, bodyLength)};
        if (!ok) {
            return StompError::kParsingInvalidContentLength;
        }
        if (bodyLength == (plain.size() - bodyStart)) {
            return StompError::kParsingMissingNullInBody;
        }
        if (bodyLength > (plain.size() - bodyStart)) {
            return StompError::kParsingContentLengthExceedsFrameLength;
        }
        bodyEnd = bodyStart + bodyLength;
        if (plain.at(bodyEnd) != null) {
            return StompError::kParsingMissingNullInBody;
        }
    } else {
        // If the content-length header is not present, we need to look for the
        // first NULL octet as a body delimiter.
        bodyEnd = plain.find(null, bodyStart);
        if (bodyEnd == std::string::npos) {
            return StompError::kParsingMissingNullInBody;
        }
        bodyLength = bodyEnd - bodyStart;
    }
    for (size_t idx {bodyEnd + 1}; idx < plain.size(); ++idx) {
        if (plain.at(idx) != newLine) {
            return StompError::kParsingJunkAfterBody;
        }
    }
    auto body {plain.substr(bodyStart, bodyLength)};

    command_ = std::move(command);
    headers_ = std::move(headers);
    body_ = std::move(body);
    return StompError::kOk;
}

// We validate the headers in the frame based on the protocol specification.
StompError StompFrame::ValidateFrame()
{
    bool hasAllHeaders {true};
    switch (command_) {
        case StompCommand::kConnect:
        case StompCommand::kStomp:
            hasAllHeaders &= HasHeader(StompHeader::kAcceptVersion);
            hasAllHeaders &= HasHeader(StompHeader::kHost);
            break;
        case StompCommand::kConnected:
            hasAllHeaders &= HasHeader(StompHeader::kVersion);
            break;
        case StompCommand::kSend:
            hasAllHeaders &= HasHeader(StompHeader::kDestination);
            break;
        case StompCommand::kSubscribe:
            hasAllHeaders &= HasHeader(StompHeader::kDestination);
            hasAllHeaders &= HasHeader(StompHeader::kId);
            break;
        case StompCommand::kUnsubscribe:
            hasAllHeaders &= HasHeader(StompHeader::kId);
            break;
        case StompCommand::kAck:
        case StompCommand::kNack:
            hasAllHeaders &= HasHeader(StompHeader::kId);
            break;
        case StompCommand::kBegin:
        case StompCommand::kCommit:
        case StompCommand::kAbort:
            hasAllHeaders &= HasHeader(StompHeader::kTransaction);
            break;
        case StompCommand::kDisconnect:
            break;
        case StompCommand::kMessage:
            hasAllHeaders &= HasHeader(StompHeader::kDestination);
            hasAllHeaders &= HasHeader(StompHeader::kMessageId);
            hasAllHeaders &= HasHeader(StompHeader::kSubscription);
            break;
        case StompCommand::kReceipt:
            hasAllHeaders &= HasHeader(StompHeader::kReceiptId);
            break;
        case StompCommand::kError:
            break;
        case StompCommand::kInvalid:
        default:
            return StompError::kValidationInvalidCommand;
    }
    if (!hasAllHeaders) {
        return StompError::kValidationMissingHeader;
    }

    if (HasHeader(StompHeader::kContentLength)) {
        size_t length;
        auto ok {StoI(GetHeaderValue(StompHeader::kContentLength), length)};
        if (!ok) {
            return StompError::kValidationInvalidContentLength;
        }
        if (length != body_.size()) {
            return StompError::kValidationContentLengthMismatch;
        }
    }

    return StompError::kOk;
}