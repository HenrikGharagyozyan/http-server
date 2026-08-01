#include <gtest/gtest.h>
#include "handlers/StaticHandler.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <memory>

namespace fs = std::filesystem;

namespace
{

    const std::string kIndexHtml = "<html><body>Test Index</body></html>";
    const std::string kStyleCss = "body { color: red; }";

    http::Request make_request(std::string_view uri)
    {
        http::Request req;
        req.method = http::Method::GET;
        req.uri = std::pmr::string(uri);
        return req;
    }

    void write_file(const fs::path& p, const std::string& content)
    {
        fs::create_directories(p.parent_path());
        std::ofstream out(p, std::ios::binary);
        out << content;
    }

} // namespace

// The static file cache is populated once for the whole suite
// using a static unique_ptr to the StaticHandler instance.
class StaticHandlerTest : public ::testing::Test
{
protected:
    static std::unique_ptr<handlers::StaticHandler> s_handler;

    static void SetUpTestSuite()
    {
        fs::path public_dir = fs::path(::testing::TempDir()) / "static_test_public";
        fs::remove_all(public_dir);

        write_file(public_dir / "index.html", kIndexHtml);
        write_file(public_dir / "css" / "style.css", kStyleCss);

        // Instantiate the handler, which loads the cache via its constructor
        s_handler = std::make_unique<handlers::StaticHandler>(public_dir.string());
    }

    static void TearDownTestSuite()
    {
        s_handler.reset();
    }
};

// Define the static member
std::unique_ptr<handlers::StaticHandler> StaticHandlerTest::s_handler = nullptr;

TEST_F(StaticHandlerTest, ServesCachedHtmlFile)
{
    http::Response res = s_handler->handle(make_request("/index.html"));

    EXPECT_EQ(res.status_code, http::StatusCode::OK);
    EXPECT_EQ(res.body, kIndexHtml.c_str());
    EXPECT_EQ(res.headers["Content-Type"], "text/html");

    // Content-Length is auto-generated during serialization
    std::string out(res.serialize());
    EXPECT_NE(out.find("Content-Length: " + std::to_string(kIndexHtml.size()) + "\r\n"), std::string::npos) << out;
}

TEST_F(StaticHandlerTest, RootIsAliasForIndexHtml)
{
    http::Response res = s_handler->handle(make_request("/"));

    EXPECT_EQ(res.status_code, http::StatusCode::OK);
    EXPECT_EQ(res.body, kIndexHtml.c_str());
}

TEST_F(StaticHandlerTest, ServesNestedFileWithCorrectMime)
{
    http::Response res = s_handler->handle(make_request("/css/style.css"));

    EXPECT_EQ(res.status_code, http::StatusCode::OK);
    EXPECT_EQ(res.body, kStyleCss.c_str());
    EXPECT_EQ(res.headers["Content-Type"], "text/css");
}

TEST_F(StaticHandlerTest, MissingFileReturns404)
{
    http::Response res = s_handler->handle(make_request("/nope.html"));

    EXPECT_EQ(res.status_code, http::StatusCode::NOT_FOUND);
    EXPECT_EQ(res.headers["Content-Type"], "text/html");
}

TEST_F(StaticHandlerTest, NonGetMethodIsNotAllowed)
{
    http::Request req;
    req.method = http::Method::DELETE;
    req.uri = std::pmr::string("/index.html");

    http::Response res = s_handler->handle(req);

    EXPECT_EQ(res.status_code, http::StatusCode::METHOD_NOT_ALLOWED);
    EXPECT_EQ(res.headers["Allow"], "GET");
}

TEST_F(StaticHandlerTest, PathTraversalIsForbidden)
{
    http::Response res = s_handler->handle(make_request("/../etc/passwd"));

    EXPECT_EQ(res.status_code, http::StatusCode::FORBIDDEN);
}

TEST_F(StaticHandlerTest, EmbeddedDotDotIsForbidden)
{
    http::Response res = s_handler->handle(make_request("/css/../../secret.txt"));

    EXPECT_EQ(res.status_code, http::StatusCode::FORBIDDEN);
}