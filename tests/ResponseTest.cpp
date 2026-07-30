#include <gtest/gtest.h>
#include "http/Response.hpp"
#include "http/HttpStatus.hpp"

#include <string>

// ==========================================================
// Status line
// ==========================================================

TEST(ResponseSerializeTest, StatusLine200)
{
    http::Response res;
    res.status_code = http::StatusCode::OK;

    std::string out = res.serialize();

    EXPECT_TRUE(out.starts_with("HTTP/1.1 200 OK\r\n")) << out;
}

TEST(ResponseSerializeTest, StatusLineForEveryCode)
{
    struct Case { http::StatusCode code; const char* line; };
    const Case cases[] = {
        {http::StatusCode::OK,                         "HTTP/1.1 200 OK\r\n"},
        {http::StatusCode::CREATED,                    "HTTP/1.1 201 Created\r\n"},
        {http::StatusCode::BAD_REQUEST,                "HTTP/1.1 400 Bad Request\r\n"},
        {http::StatusCode::FORBIDDEN,                  "HTTP/1.1 403 Forbidden\r\n"},
        {http::StatusCode::NOT_FOUND,                  "HTTP/1.1 404 Not Found\r\n"},
        {http::StatusCode::METHOD_NOT_ALLOWED,         "HTTP/1.1 405 Method Not Allowed\r\n"},
        {http::StatusCode::REQUEST_TIMEOUT,            "HTTP/1.1 408 Request Timeout\r\n"},
        {http::StatusCode::PAYLOAD_TOO_LARGE,          "HTTP/1.1 413 Payload Too Large\r\n"},
        {http::StatusCode::URI_TOO_LONG,               "HTTP/1.1 414 URI Too Long\r\n"},
        {http::StatusCode::HEADER_FIELDS_TOO_LARGE,    "HTTP/1.1 431 Request Header Fields Too Large\r\n"},
        {http::StatusCode::INTERNAL_SERVER_ERROR,      "HTTP/1.1 500 Internal Server Error\r\n"},
        {http::StatusCode::NOT_IMPLEMENTED,            "HTTP/1.1 501 Not Implemented\r\n"},
        {http::StatusCode::SERVICE_UNAVAILABLE,        "HTTP/1.1 503 Service Unavailable\r\n"},
        {http::StatusCode::HTTP_VERSION_NOT_SUPPORTED, "HTTP/1.1 505 HTTP Version Not Supported\r\n"},
    };

    for (const auto& c : cases)
    {
        http::Response res;
        res.status_code = c.code;
        EXPECT_TRUE(res.serialize().starts_with(c.line))
            << "expected: " << c.line << "got: " << res.serialize();
    }
}

// ==========================================================
// Full message structure
// ==========================================================

TEST(ResponseSerializeTest, EmptyResponseIsStatusLinePlusBlankLine)
{
    http::Response res;

    EXPECT_EQ(res.serialize(), "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
}

TEST(ResponseSerializeTest, SingleHeaderAndBodyExactFormat)
{
    http::Response res;
    res.status_code = http::StatusCode::OK;
    res.body = "hello";
    res.headers["Content-Length"] = "5";

    EXPECT_EQ(res.serialize(), "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello");
}

TEST(ResponseSerializeTest, ContentLengthHeaderIsEmitted)
{
    http::Response res;
    res.body = "some body text";
    res.headers["Content-Length"] = std::pmr::string(std::to_string(res.body.size()));

    std::string out = res.serialize();

    EXPECT_NE(out.find("Content-Length: 14\r\n"), std::string::npos) << out;
}

TEST(ResponseSerializeTest, AllHeadersEmittedOnceWithBlankLineSeparator)
{
    http::Response res;
    res.body = "{}";
    res.headers["Content-Type"] = "application/json";
    res.headers["Content-Length"] = "2";
    res.headers["Connection"] = "keep-alive";

    std::string out = res.serialize();

    // Header order in an unordered_map is unspecified — check presence, not order
    EXPECT_NE(out.find("Content-Type: application/json\r\n"), std::string::npos);
    EXPECT_NE(out.find("Content-Length: 2\r\n"), std::string::npos);
    EXPECT_NE(out.find("Connection: keep-alive\r\n"), std::string::npos);

    // Exactly one blank line, placed right before the body
    const std::string separator = "\r\n\r\n";
    size_t sep_pos = out.find(separator);
    ASSERT_NE(sep_pos, std::string::npos);
    EXPECT_EQ(out.find(separator, sep_pos + 1), std::string::npos);
    EXPECT_EQ(out.substr(sep_pos + separator.size()), "{}");
}

TEST(ResponseSerializeTest, BinaryBodyPreserved)
{
    http::Response res;
    res.body = std::pmr::string("\x00\x01\xff\r\n\x02", 6);
    res.headers["Content-Length"] = "6";

    std::string out = res.serialize();
    const std::string expected_tail("\x00\x01\xff\r\n\x02", 6);

    ASSERT_GE(out.size(), expected_tail.size());
    EXPECT_EQ(out.substr(out.size() - expected_tail.size()), expected_tail);
}

// ==========================================================
// Status message lookup
// ==========================================================

TEST(HttpStatusTest, KnownCodesHaveMessages)
{
    EXPECT_EQ(http::get_status_message(http::StatusCode::OK), "OK");
    EXPECT_EQ(http::get_status_message(http::StatusCode::NOT_FOUND), "Not Found");
    EXPECT_EQ(http::get_status_message(http::StatusCode::INTERNAL_SERVER_ERROR), "Internal Server Error");
}

TEST(HttpStatusTest, UnknownCodeFallsBackToUnknown)
{
    EXPECT_EQ(http::get_status_message(static_cast<http::StatusCode>(999)), "Unknown");
}
