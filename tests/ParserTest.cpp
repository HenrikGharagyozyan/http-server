#include <gtest/gtest.h>
#include "http/Parser.hpp"

#include <MemCore/MallocUpstream.hpp>
#include <MemCore/LinearAllocator.hpp>
#include <MemCore/PmrAdapter.hpp>

TEST(ParserTest, ParsesBasicGetRequest) 
{
    MemCore::MallocUpstream upstream;
    MemCore::Block chunk = upstream.allocate(1024, alignof(std::max_align_t));
    
    {
        // Создаем арену внутри блока видимости
        MemCore::LinearAllocator arena(chunk);
        MemCore::PmrAdapter pmr(arena);

        std::string raw_request = "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";

        http::Request req = http::parse_request(raw_request, &pmr);

        EXPECT_EQ(req.method, http::Method::GET);
        EXPECT_EQ(req.uri, "/index.html");
        EXPECT_EQ(req.headers["Host"], "localhost");
    } // <- Здесь объект req уничтожается ПЕРЕД тем, как мы освобождаем chunk!

    upstream.deallocate(chunk.ptr, chunk.size);
}