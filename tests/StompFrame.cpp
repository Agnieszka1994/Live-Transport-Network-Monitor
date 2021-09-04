#include <network-monitor/StompFrame.h>

#include <boost/test/unit_test.hpp>

#include <sstream>
#include <string>

using NetworkMonitor::StompCommand;
using NetworkMonitor::StompError;
using NetworkMonitor::StompFrame;
using NetworkMonitor::StompHeader;

using namespace std::string_literals;

BOOST_AUTO_TEST_SUITE(network_monitor);

BOOST_AUTO_TEST_SUITE(stomp_frame);

BOOST_AUTO_TEST_SUITE(enum_class_StompCommand);

BOOST_AUTO_TEST_CASE(ostream)
{
    std::stringstream invalidSs {};
    invalidSs << StompCommand::kInvalid;
    auto invalid {invalidSs.str()};
    for (const auto& command: {
        StompCommand::kAbort,
        StompCommand::kAck,
        StompCommand::kBegin,
        StompCommand::kCommit,
        StompCommand::kConnect,
        StompCommand::kConnected,
        StompCommand::kDisconnect,
        StompCommand::kError,
        StompCommand::kMessage,
        StompCommand::kNack,
        StompCommand::kReceipt,
        StompCommand::kSend,
        StompCommand::kStomp,
        StompCommand::kSubscribe,
        StompCommand::kUnsubscribe,
    }) {
        std::stringstream ss {};
        ss << command;
        BOOST_CHECK(ss.str() != invalid);
    }
}

BOOST_AUTO_TEST_SUITE_END(); // enum_class_StompCommand

BOOST_AUTO_TEST_SUITE(enum_class_StompHeader);

BOOST_AUTO_TEST_CASE(ostream)
{
    std::stringstream invalidSs {};
    invalidSs << StompHeader::kInvalid;
    auto invalid {invalidSs.str()};
    for (const auto& header: {
        StompHeader::kAcceptVersion,
        StompHeader::kAck,
        StompHeader::kContentLength,
        StompHeader::kContentType,
        StompHeader::kDestination,
        StompHeader::kHeartBeat,
        StompHeader::kHost,
        StompHeader::kId,
        StompHeader::kLogin,
        StompHeader::kMessage,
        StompHeader::kMessageId,
        StompHeader::kPasscode,
        StompHeader::kReceipt,
        StompHeader::kReceiptId,
        StompHeader::kSession,
        StompHeader::kSubscription,
        StompHeader::kTransaction,
        StompHeader::kServer,
        StompHeader::kVersion,
    }) {
        std::stringstream ss {};
        ss << header;
        BOOST_CHECK(ss.str() != invalid);
    }
}

BOOST_AUTO_TEST_SUITE_END(); // enum_class_StompHeader

BOOST_AUTO_TEST_SUITE(enum_class_StompError);

BOOST_AUTO_TEST_CASE(ostream)
{
    std::stringstream invalidSs {};
    invalidSs << StompError::kUndefinedError;
    auto invalid {invalidSs.str()};
    for (const auto& error: {
        StompError::kOk,
        StompError::kParsingEmptyHeaderValue,
        StompError::kParsingContentLengthExceedsFrameLength,
        StompError::kParsingInvalidContentLength,
        StompError::kParsingJunkAfterBody,
        StompError::kParsingMissingBlankLineAfterHeaders,
        StompError::kParsingMissingColonInHeader,
        StompError::kParsingMissingEolAfterCommand,
        StompError::kParsingMissingEolAfterHeaderValue,
        StompError::kParsingMissingNullInBody,
        StompError::kParsingUnrecognizedCommand,
        StompError::kParsingUnrecognizedHeader,
        StompError::kValidationContentLengthMismatch,
        StompError::kValidationInvalidCommand,
        StompError::kValidationInvalidContentLength,
        StompError::kValidationMissingHeader,
    }) {
        std::stringstream ss {};
        ss << error;
        BOOST_CHECK(ss.str() != invalid);
    }
}

BOOST_AUTO_TEST_SUITE_END(); // enum_class_StompError

BOOST_AUTO_TEST_SUITE(class_StompFrame);

BOOST_AUTO_TEST_CASE(parse_well_formed)
{
    std::string plain {
        "CONNECT\n"
        "accept-version:42\n"
        "host:host.com\n"
        "\n"
        "Frame body\0"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK_EQUAL(error, StompError::kOk);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kConnect);
    BOOST_CHECK_EQUAL(frame.GetHeaderValue(StompHeader::kAcceptVersion), "42");
    BOOST_CHECK_EQUAL(frame.GetHeaderValue(StompHeader::kHost), "host.com");
    BOOST_CHECK_EQUAL(frame.GetBody(), "Frame body");
}

BOOST_AUTO_TEST_CASE(parse_well_formed_content_length)
{
    std::string plain {
        "CONNECT\n"
        "accept-version:42\n"
        "host:host.com\n"
        "content-length:10\n"
        "\n"
        "Frame body\0"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK_EQUAL(error, StompError::kOk);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kConnect);
    BOOST_CHECK_EQUAL(frame.GetHeaderValue(StompHeader::kAcceptVersion), "42");
    BOOST_CHECK_EQUAL(frame.GetHeaderValue(StompHeader::kHost), "host.com");
    BOOST_CHECK_EQUAL(frame.GetBody(), "Frame body");
}

BOOST_AUTO_TEST_CASE(parse_empty_body)
{
    std::string plain {
        "CONNECT\n"
        "accept-version:42\n"
        "host:host.com\n"
        "\n"
        "\0"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK_EQUAL(error, StompError::kOk);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kConnect);
    BOOST_CHECK_EQUAL(frame.GetBody().size(), 0);
}

BOOST_AUTO_TEST_CASE(parse_empty_body_content_length)
{
    std::string plain {
        "CONNECT\n"
        "accept-version:42\n"
        "host:host.com\n"
        "content-length:0\n"
        "\n"
        "\0"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK_EQUAL(error, StompError::kOk);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kConnect);
    BOOST_CHECK_EQUAL(frame.GetBody().size(), 0);
}

BOOST_AUTO_TEST_CASE(parse_empty_headers)
{
    std::string plain {
        "DISCONNECT\n"
        "\n"
        "Frame body\0"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK_EQUAL(error, StompError::kOk);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kDisconnect);
    BOOST_CHECK_EQUAL(frame.GetBody(), "Frame body");
}

BOOST_AUTO_TEST_CASE(parse_only_command)
{
    std::string plain {
        "DISCONNECT\n"
        "\n"
        "\0"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK_EQUAL(error, StompError::kOk);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kDisconnect);
    BOOST_CHECK_EQUAL(frame.GetBody().size(), 0);
}

BOOST_AUTO_TEST_CASE(parse_bad_command)
{
    std::string plain {
        "CONNECTX\n"
        "accept-version:42\n"
        "host:host.com\n"
        "\n"
        "Frame body\0"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK(error != StompError::kOk);
    BOOST_CHECK_EQUAL(error, StompError::kParsingUnrecognizedCommand);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kInvalid);
}

BOOST_AUTO_TEST_CASE(parse_bad_header)
{
    std::string plain {
        "CONNECT\n"
        "accept-version:42\n"
        "login\n"
        "\n"
        "Frame body\0"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK(error != StompError::kOk);
    BOOST_CHECK_EQUAL(error, StompError::kParsingMissingColonInHeader);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kInvalid);
}

BOOST_AUTO_TEST_CASE(parse_missing_body_newline)
{
    std::string plain {
        "CONNECT\n"
        "accept-version:42\n"
        "host:host.com\n"
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK(error != StompError::kOk);
    BOOST_CHECK_EQUAL(error, StompError::kParsingMissingBlankLineAfterHeaders);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kInvalid);
}

BOOST_AUTO_TEST_CASE(parse_missing_last_header_newline)
{
    std::string plain {
        "CONNECT\n"
        "accept-version:42\n"
        "host:host.com"
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK(error != StompError::kOk);
    BOOST_CHECK_EQUAL(error, StompError::kParsingMissingEolAfterHeaderValue);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kInvalid);
}

BOOST_AUTO_TEST_CASE(parse_unrecognized_header)
{
    std::string plain {
        "CONNECT\n"
        "bad_header:42\n"
        "host:host.com\n"
        "\n"
        "\0"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK(error != StompError::kOk);
    BOOST_CHECK_EQUAL(error, StompError::kParsingUnrecognizedHeader);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kInvalid);
}

BOOST_AUTO_TEST_CASE(parse_empty_header_value)
{
    std::string plain {
        "CONNECT\n"
        "accept-version:\n"
        "host:host.com\n"
        "\n"
        "\0"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK(error != StompError::kOk);
    BOOST_CHECK_EQUAL(error, StompError::kParsingEmptyHeaderValue);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kInvalid);
}

BOOST_AUTO_TEST_CASE(parse_just_command)
{
    std::string plain {
        "CONNECT"
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK(error != StompError::kOk);
    BOOST_CHECK_EQUAL(error, StompError::kParsingMissingEolAfterCommand);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kInvalid);
}

BOOST_AUTO_TEST_CASE(parse_newline_after_command)
{
    std::string plain {
        "DISCONNECT\n"
        "\n"
        "version:42\n"
        "host:host.com\n"
        "\n"
        "Frame body\0"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK_EQUAL(error, StompError::kOk);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kDisconnect);

    // Everything becomes part of the body.
    BOOST_CHECK_EQUAL(frame.GetBody().substr(0, 10), "version:42");
}

BOOST_AUTO_TEST_CASE(parse_double_colon_in_header_line)
{
    std::string plain {
        "CONNECT\n"
        "accept-version:42:43\n"
        "host:host.com\n"
        "\n"
        "Frame body\0"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK_EQUAL(error, StompError::kOk);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kConnect);
    BOOST_CHECK_EQUAL(frame.GetHeaderValue(StompHeader::kAcceptVersion),
                      "42:43");
}

BOOST_AUTO_TEST_CASE(parse_repeated_headers)
{
    std::string plain {
        "CONNECT\n"
        "accept-version:42\n"
        "accept-version:43\n"
        "host:host.com\n"
        "\n"
        "Frame body\0"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK_EQUAL(error, StompError::kOk);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kConnect);
    BOOST_CHECK_EQUAL(frame.GetHeaderValue(StompHeader::kAcceptVersion), "42");
}

BOOST_AUTO_TEST_CASE(parse_repeated_headers_error_in_second)
{
    std::string plain {
        "CONNECT\n"
        "accept-version:42\n"
        "accept-version:\n"
        "\n"
        "Frame body\0"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK(error != StompError::kOk);
    BOOST_CHECK_EQUAL(error, StompError::kParsingEmptyHeaderValue);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kInvalid);
}

BOOST_AUTO_TEST_CASE(parse_unterminated_body)
{
    std::string plain {
        "CONNECT\n"
        "accept-version:42\n"
        "host:host.com\n"
        "\n"
        "Frame body"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK(error != StompError::kOk);
    BOOST_CHECK_EQUAL(error, StompError::kParsingMissingNullInBody);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kInvalid);
}

BOOST_AUTO_TEST_CASE(parse_unterminated_body_content_length)
{
    std::string plain {
        "CONNECT\n"
        "accept-version:42\n"
        "host:host.com\n"
        "content-length:10\n"
        "\n"
        "Frame body"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK(error != StompError::kOk);
    BOOST_CHECK_EQUAL(error, StompError::kParsingMissingNullInBody);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kInvalid);
}

BOOST_AUTO_TEST_CASE(parse_junk_after_body)
{
    std::string plain {
        "CONNECT\n"
        "accept-version:42\n"
        "host:host.com\n"
        "\n"
        "Frame body\0\n\njunk\n"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK(error != StompError::kOk);
    BOOST_CHECK_EQUAL(error, StompError::kParsingJunkAfterBody);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kInvalid);
}

BOOST_AUTO_TEST_CASE(parse_junk_after_body_content_length)
{
    std::string plain {
        "CONNECT\n"
        "accept-version:42\n"
        "host:host.com\n"
        "content-length:10\n"
        "\n"
        "Frame body\0\n\njunk\n"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK(error != StompError::kOk);
    BOOST_CHECK_EQUAL(error, StompError::kParsingJunkAfterBody);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kInvalid);
}

BOOST_AUTO_TEST_CASE(parse_newlines_after_body)
{
    std::string plain {
        "CONNECT\n"
        "accept-version:42\n"
        "host:host.com\n"
        "\n"
        "Frame body\0\n\n\n"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK_EQUAL(error, StompError::kOk);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kConnect);
    BOOST_CHECK_EQUAL(frame.GetBody(), "Frame body");
}

BOOST_AUTO_TEST_CASE(parse_newlines_after_body_content_length)
{
    std::string plain {
        "CONNECT\n"
        "accept-version:42\n"
        "host:host.com\n"
        "content-length:10\n"
        "\n"
        "Frame body\0\n\n\n"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK_EQUAL(error, StompError::kOk);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kConnect);
    BOOST_CHECK_EQUAL(frame.GetBody(), "Frame body");
}

BOOST_AUTO_TEST_CASE(parse_content_length_wrong_number)
{
    std::string plain {
        "CONNECT\n"
        "accept-version:42\n"
        "host:host.com\n"
        "content-length:9\n" // This is one byte off
        "\n"
        "Frame body\0"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK(error != StompError::kOk);
    BOOST_CHECK_EQUAL(error, StompError::kParsingMissingNullInBody);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kInvalid);
}

BOOST_AUTO_TEST_CASE(parse_content_length_exceeding)
{
    std::string plain {
        "CONNECT\n"
        "accept-version:42\n"
        "host:host.com\n"
        "content-length:15\n" // Way above the actual body length
        "\n"
        "Frame body\0"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_CHECK(error != StompError::kOk);
    BOOST_CHECK_EQUAL(error, StompError::kParsingContentLengthExceedsFrameLength);
    BOOST_CHECK_EQUAL(frame.GetCommand(), StompCommand::kInvalid);
}

BOOST_AUTO_TEST_CASE(parse_required_headers)
{
    StompError error;
    {
        std::string plain {
            "CONNECT\n"
            "\n"
            "\0"s
        };
        StompFrame frame {error, std::move(plain)};
        BOOST_CHECK(error != StompError::kOk);
        BOOST_CHECK_EQUAL(error, StompError::kValidationMissingHeader);
    }
    {
        std::string plain {
            "CONNECT\n"
            "accept-version:42\n"
            "\n"
            "\0"s
        };
        StompFrame frame {error, std::move(plain)};
        BOOST_CHECK(error != StompError::kOk);
        BOOST_CHECK_EQUAL(error, StompError::kValidationMissingHeader);
    }
    {
        std::string plain {
            "CONNECT\n"
            "accept-version:42\n"
            "host:host.com\n"
            "\n"
            "\0"s
        };
        StompFrame frame {error, std::move(plain)};
        BOOST_CHECK_EQUAL(error, StompError::kOk);
    }
}

BOOST_AUTO_TEST_CASE(constructors)
{
    std::string plain {
        "CONNECT\n"
        "accept-version:42\n"
        "host:host.com\n"
        "\n"
        "Frame body\0"s
    };
    StompError error;
    StompFrame frame {error, std::move(plain)};
    BOOST_REQUIRE(error == StompError::kOk);
    BOOST_REQUIRE(frame.GetCommand() == StompCommand::kConnect);
    BOOST_REQUIRE_EQUAL(frame.GetHeaderValue(StompHeader::kAcceptVersion),
                        "42");
    BOOST_REQUIRE_EQUAL(frame.GetHeaderValue(StompHeader::kHost), "host.com");
    BOOST_REQUIRE_EQUAL(frame.GetBody(), "Frame body");

    {
        // Copy constructor
        const StompFrame copied(frame);
        BOOST_CHECK(copied.GetCommand() == StompCommand::kConnect);
        BOOST_CHECK_EQUAL(copied.GetHeaderValue(StompHeader::kAcceptVersion),
                          "42");
        BOOST_CHECK_EQUAL(copied.GetHeaderValue(StompHeader::kHost),
                          "host.com");
        BOOST_CHECK_EQUAL(copied.GetBody(), "Frame body");

        // Copy assignment
        auto assigned = copied;
        BOOST_CHECK(assigned.GetCommand() == StompCommand::kConnect);
        BOOST_CHECK_EQUAL(assigned.GetHeaderValue(StompHeader::kAcceptVersion),
                          "42");
        BOOST_CHECK_EQUAL(assigned.GetHeaderValue(StompHeader::kHost),
                          "host.com");
        BOOST_CHECK_EQUAL(assigned.GetBody(), "Frame body");
    }

    // Check that the std::string_view's stay valid when copied object goes out
    // of scope.
    {
        StompFrame assigned;
        {
            std::string plain {
                "CONNECT\n"
                "accept-version:42\n"
                "host:host.com\n"
                "\n"
                "Frame body\0"s
            };
            StompError error;
            StompFrame newFrame(error, std::move(plain));
            BOOST_REQUIRE(error == StompError::kOk);
            assigned = newFrame;
        }
        BOOST_CHECK(assigned.GetCommand() == StompCommand::kConnect);
        BOOST_CHECK_EQUAL(assigned.GetHeaderValue(StompHeader::kAcceptVersion),
                          "42");
        BOOST_CHECK_EQUAL(assigned.GetHeaderValue(StompHeader::kHost),
                          "host.com");
        BOOST_CHECK_EQUAL(assigned.GetBody(), "Frame body");
    }

    // The move tests are performed last.
    {
        // Move constructor
        StompFrame moved(std::move(frame));
        BOOST_REQUIRE(moved.GetCommand() == StompCommand::kConnect);
        BOOST_REQUIRE_EQUAL(moved.GetHeaderValue(StompHeader::kAcceptVersion),
                            "42");
        BOOST_REQUIRE_EQUAL(moved.GetHeaderValue(StompHeader::kHost),
                            "host.com");
        BOOST_REQUIRE_EQUAL(moved.GetBody(), "Frame body");

        // Move assignment
        auto assigned = std::move(moved);
        BOOST_CHECK(assigned.GetCommand() == StompCommand::kConnect);
        BOOST_CHECK_EQUAL(assigned.GetHeaderValue(StompHeader::kAcceptVersion),
                          "42");
        BOOST_CHECK_EQUAL(assigned.GetHeaderValue(StompHeader::kHost),
                          "host.com");
        BOOST_CHECK_EQUAL(assigned.GetBody(), "Frame body");
    }
}

BOOST_AUTO_TEST_CASE(constructor_from_components_full)
{
    StompError error;
    StompFrame frame {
        error,
        StompCommand::kConnect,
        {
            {StompHeader::kAcceptVersion, "42"},
            {StompHeader::kHost, "host.com"},
        },
        "Frame body"
    };
    BOOST_CHECK(error == StompError::kOk);
    BOOST_CHECK(frame.GetCommand() == StompCommand::kConnect);
    BOOST_CHECK_EQUAL(frame.GetHeaderValue(StompHeader::kAcceptVersion), "42");
    BOOST_CHECK_EQUAL(frame.GetHeaderValue(StompHeader::kHost), "host.com");
    BOOST_CHECK_EQUAL(frame.GetBody(), "Frame body");
}

BOOST_AUTO_TEST_CASE(constructor_from_components_only_command)
{
    StompError error;
    StompFrame frame {
        error,
        StompCommand::kDisconnect
    };
    BOOST_CHECK(error == StompError::kOk);
    BOOST_CHECK(frame.GetCommand() == StompCommand::kDisconnect);
}

BOOST_AUTO_TEST_CASE(constructor_from_components_empty_headers)
{
    StompError error;
    StompFrame frame {
        error,
        StompCommand::kDisconnect,
        {},
        "Frame body"
    };
    BOOST_CHECK(error == StompError::kOk);
    BOOST_CHECK(frame.GetCommand() == StompCommand::kDisconnect);
    BOOST_CHECK_EQUAL(frame.GetBody(), "Frame body");
}

BOOST_AUTO_TEST_CASE(constructor_from_components_empty_body)
{
    StompError error;
    StompFrame frame {
        error,
        StompCommand::kConnect,
        {
            {StompHeader::kAcceptVersion, "42"},
            {StompHeader::kHost, "host.com"},
        }
    };
    BOOST_CHECK(error == StompError::kOk);
    BOOST_CHECK(frame.GetCommand() == StompCommand::kConnect);
    BOOST_CHECK_EQUAL(frame.GetHeaderValue(StompHeader::kAcceptVersion), "42");
    BOOST_CHECK_EQUAL(frame.GetHeaderValue(StompHeader::kHost), "host.com");
}

BOOST_AUTO_TEST_CASE(to_string)
{
    const std::string plain {
        "CONNECT\n"
        "accept-version:42\n"
        "host:host.com\n"
        "\n"
        "Frame body\0"s
    };
    StompError error;
    StompFrame frame {error, plain};
    BOOST_REQUIRE(error == StompError::kOk);
    BOOST_CHECK_EQUAL(plain, frame.ToString());
}

BOOST_AUTO_TEST_SUITE_END(); // class_StompFrame

BOOST_AUTO_TEST_SUITE_END(); // stomp_frame

BOOST_AUTO_TEST_SUITE_END(); // network_monitor