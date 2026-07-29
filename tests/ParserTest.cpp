#include <gtest/gtest.h>
#include "http/Parser.hpp"

#include <MemCore/MallocUpstream.hpp>
#include <MemCore/LinearAllocator.hpp>
#include <MemCore/PmrAdapter.hpp>
#include <optional>
#include <stdexcept>
#include <string>

class ParserTestFixture : public ::testing::Test
{
protected:
    MemCore::MallocUpstream upstream;
    MemCore::Block chunk{nullptr, 0};

    // Use std::optional so they can be initialized in SetUp() after memory allocation
    std::optional<MemCore::LinearAllocator> arena;
    std::optional<MemCore::PmrAdapter<MemCore::LinearAllocator>> pmr;

    void SetUp() override
    {
        chunk = upstream.allocate(4096, alignof(std::max_align_t));
        arena.emplace(chunk);
        pmr.emplace(*arena);
    }

    void TearDown() override
    {
        pmr.reset();
        arena.reset();
        upstream.deallocate(chunk.ptr, chunk.size);
    }
};

// ==========================================================
// Valid requests
// ==========================================================

TEST_F(ParserTestFixture, ParsesBasicGetRequest)
{
    std::string raw_request = "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
    http::Request req = http::parse_request(raw_request, &(*pmr));

    EXPECT_EQ(req.method, http::Method::GET);
    EXPECT_EQ(req.uri, "/index.html");
    EXPECT_EQ(req.version, "HTTP/1.1");
    EXPECT_EQ(req.headers["Host"], "localhost");
    EXPECT_TRUE(req.body.empty());
}

TEST_F(ParserTestFixture, ParsesPostRequestWithBody)
{
    std::string raw_request = "POST /api/users HTTP/1.1\r\nHost: example.com\r\nContent-Length: 15\r\n\r\n{\"name\":\"John\"}";
    http::Request req = http::parse_request(raw_request, &(*pmr));

    EXPECT_EQ(req.method, http::Method::POST);
    EXPECT_EQ(req.uri, "/api/users");
    EXPECT_EQ(req.headers["Content-Length"], "15");
    EXPECT_EQ(req.body, "{\"name\":\"John\"}");
}

TEST_F(ParserTestFixture, ParsesMultipleHeaders)
{
    std::string raw_request = "GET /data HTTP/1.1\r\nHost: test.local\r\nAccept: application/json\r\nConnection: keep-alive\r\n\r\n";
    http::Request req = http::parse_request(raw_request, &(*pmr));

    EXPECT_EQ(req.method, http::Method::GET);
    EXPECT_EQ(req.uri, "/data");
    EXPECT_EQ(req.headers["Host"], "test.local");
    EXPECT_EQ(req.headers["Accept"], "application/json");
    EXPECT_EQ(req.headers["Connection"], "keep-alive");
}

TEST_F(ParserTestFixture, ParsesEveryMethod)
{
    struct Case { const char* verb; http::Method expected; };
    const Case cases[] = {
        {"GET",    http::Method::GET},
        {"POST",   http::Method::POST},
        {"PUT",    http::Method::PUT},
        {"DELETE", http::Method::DELETE},
    };

    for (const auto& c : cases)
    {
        std::string raw = std::string(c.verb) + " / HTTP/1.1\r\nHost: x\r\n\r\n";
        http::Request req = http::parse_request(raw, std::pmr::get_default_resource());
        EXPECT_EQ(req.method, c.expected) << "method: " << c.verb;
    }
}

TEST_F(ParserTestFixture, UnrecognizedMethodBecomesUnknown)
{
    std::string raw = "PATCH /thing HTTP/1.1\r\nHost: x\r\n\r\n";
    http::Request req = http::parse_request(raw, &(*pmr));

    EXPECT_EQ(req.method, http::Method::UNKNOWN);
    EXPECT_EQ(req.uri, "/thing");
}

TEST_F(ParserTestFixture, MethodMatchingIsCaseSensitive)
{
    std::string raw = "get / HTTP/1.1\r\nHost: x\r\n\r\n";
    http::Request req = http::parse_request(raw, &(*pmr));

    EXPECT_EQ(req.method, http::Method::UNKNOWN);
}

TEST_F(ParserTestFixture, HeaderValueMayContainColon)
{
    std::string raw = "GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n";
    http::Request req = http::parse_request(raw, &(*pmr));

    EXPECT_EQ(req.headers["Host"], "localhost:8080");
}

TEST_F(ParserTestFixture, TrimsLeadingSpacesFromHeaderValue)
{
    std::string raw = "GET / HTTP/1.1\r\nX-Padded:      value\r\n\r\n";
    http::Request req = http::parse_request(raw, &(*pmr));

    EXPECT_EQ(req.headers["X-Padded"], "value");
}

TEST_F(ParserTestFixture, HeaderWithEmptyValue)
{
    std::string raw = "GET / HTTP/1.1\r\nX-Empty:\r\n\r\n";
    http::Request req = http::parse_request(raw, &(*pmr));

    ASSERT_TRUE(req.headers.contains("X-Empty"));
    EXPECT_EQ(req.headers["X-Empty"], "");
}

TEST_F(ParserTestFixture, BodyPreservedVerbatimIncludingCrlf)
{
    std::string body = "line1\r\nline2\r\n\r\nline3";
    std::string raw = "POST /raw HTTP/1.1\r\nHost: x\r\n\r\n" + body;
    http::Request req = http::parse_request(raw, &(*pmr));

    EXPECT_EQ(req.body, body.c_str());
}

// "GET / HTTP/1.1\r\n\r\n" is a valid HTTP request with zero headers: the
// request line is terminated directly by the blank line
TEST_F(ParserTestFixture, RequestWithNoHeadersParses)
{
    std::string raw = "GET / HTTP/1.1\r\n\r\n";
    http::Request req = http::parse_request(raw, &(*pmr));

    EXPECT_EQ(req.method, http::Method::GET);
    EXPECT_TRUE(req.headers.empty());
}

// ==========================================================
// Malformed requests
// ==========================================================

TEST_F(ParserTestFixture, ThrowsWhenHeaderTerminatorMissing)
{
    EXPECT_THROW(
        http::parse_request("GET / HTTP/1.1\r\nHost: localhost\r\n", &(*pmr)),
        std::invalid_argument);
}

// Every strict prefix of a valid request lacks the \r\n\r\n terminator,
// so the parser must reject all of them. This simulates a request that
// was split at every possible byte boundary and only partially received.
TEST(ParserPrefixTest, RejectsEveryStrictPrefixOfValidRequest)
{
    const std::string full =
        "POST /api/users HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
        "data";

    // Prefixes up to (but not including) the end of "\r\n\r\n" are incomplete
    const size_t headers_complete = full.find("\r\n\r\n") + 4;

    for (size_t len = 0; len < headers_complete; ++len)
    {
        EXPECT_THROW(
            http::parse_request(full.substr(0, len), std::pmr::get_default_resource()),
            std::invalid_argument) << "prefix length: " << len;
    }

    // Once the header terminator has arrived, parsing succeeds even if the
    // body is still incomplete (body framing is the server's responsibility).
    for (size_t len = headers_complete; len <= full.size(); ++len)
    {
        http::Request req = http::parse_request(full.substr(0, len), std::pmr::get_default_resource());
        EXPECT_EQ(req.method, http::Method::POST) << "prefix length: " << len;
        EXPECT_EQ(std::string(req.body.begin(), req.body.end()),
                  full.substr(headers_complete, len - headers_complete));
    }
}

TEST_F(ParserTestFixture, ThrowsOnEmptyInput)
{
    EXPECT_THROW(http::parse_request("", &(*pmr)), std::invalid_argument);
}

TEST_F(ParserTestFixture, ThrowsWhenRequestLineHasNoSpaces)
{
    EXPECT_THROW(http::parse_request("GARBAGE\r\n\r\n", &(*pmr)), std::invalid_argument);
}

TEST_F(ParserTestFixture, ThrowsWhenRequestLineMissingVersion)
{
    EXPECT_THROW(http::parse_request("GET /index.html\r\n\r\n", &(*pmr)), std::invalid_argument);
}

TEST_F(ParserTestFixture, ThrowsWhenRequestLineIsEmpty)
{
    EXPECT_THROW(http::parse_request("\r\n\r\n", &(*pmr)), std::invalid_argument);
}

TEST_F(ParserTestFixture, HeaderLineWithoutColonIsSkipped)
{
    std::string raw = "GET / HTTP/1.1\r\nThisHasNoColon\r\nHost: localhost\r\n\r\n";
    http::Request req = http::parse_request(raw, &(*pmr));

    EXPECT_EQ(req.headers.size(), 1u);
    EXPECT_EQ(req.headers["Host"], "localhost");
}

// ==========================================================
// Large input (parser itself has no limits — those are enforced
// by HttpServer; this verifies the parser stays correct at size)
// ==========================================================

TEST(ParserLargeInputTest, ParsesManyHeaders)
{
    std::string raw = "GET /big HTTP/1.1\r\n";
    for (int i = 0; i < 500; ++i)
    {
        raw += "X-Header-" + std::to_string(i) + ": value-" + std::to_string(i) + "\r\n";
    }
    raw += "\r\n";

    http::Request req = http::parse_request(raw, std::pmr::get_default_resource());

    EXPECT_EQ(req.headers.size(), 500u);
    EXPECT_EQ(req.headers["X-Header-0"], "value-0");
    EXPECT_EQ(req.headers["X-Header-499"], "value-499");
}

TEST(ParserLargeInputTest, ParsesHugeSingleHeaderValue)
{
    const std::string big_value(64 * 1024, 'a');
    std::string raw = "GET / HTTP/1.1\r\nX-Big: " + big_value + "\r\n\r\n";

    http::Request req = http::parse_request(raw, std::pmr::get_default_resource());

    EXPECT_EQ(req.headers["X-Big"].size(), big_value.size());
}

TEST(ParserLargeInputTest, ParsesLargeBody)
{
    const std::string big_body(1024 * 1024, 'b');
    std::string raw = "POST /upload HTTP/1.1\r\nContent-Length: " +
                      std::to_string(big_body.size()) + "\r\n\r\n" + big_body;

    http::Request req = http::parse_request(raw, std::pmr::get_default_resource());

    EXPECT_EQ(req.body.size(), big_body.size());
}
