#include <gtest/gtest.h>
#include "http/Parser.hpp"

#include <MemCore/MallocUpstream.hpp>
#include <MemCore/LinearAllocator.hpp>
#include <MemCore/PmrAdapter.hpp>
#include <optional>

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

TEST_F(ParserTestFixture, ParsesBasicGetRequest) 
{
    std::string raw_request = "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
    http::Request req = http::parse_request(raw_request, &(*pmr));

    EXPECT_EQ(req.method, http::Method::GET);
    EXPECT_EQ(req.uri, "/index.html");
    EXPECT_EQ(req.headers["Host"], "localhost");
}

TEST_F(ParserTestFixture, ParsesPostRequestWithBody) 
{
    std::string raw_request = "POST /api/users HTTP/1.1\r\nHost: example.com\r\nContent-Length: 13\r\n\r\n{\"name\":\"John\"}";
    http::Request req = http::parse_request(raw_request, &(*pmr));

    EXPECT_EQ(req.method, http::Method::POST);
    EXPECT_EQ(req.uri, "/api/users");
    EXPECT_EQ(req.headers["Content-Length"], "13");
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