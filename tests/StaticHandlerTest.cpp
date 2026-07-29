#include <gtest/gtest.h>
#include "handlers/StaticHandler.hpp"

#include <filesystem>
#include <fstream>
#include <string>

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

// The static file cache is a process-wide singleton populated once,
// so the whole suite shares one initialization.
class StaticHandlerTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        fs::path public_dir = fs::path(::testing::TempDir()) / "static_test_public";
        fs::remove_all(public_dir);

        write_file(public_dir / "index.html", kIndexHtml);
        write_file(public_dir / "css" / "style.css", kStyleCss);

        handlers::init_static_cache(public_dir.string());
    }
};

TEST_F(StaticHandlerTest, ServesCachedHtmlFile)
{
    http::Response res = handlers::handle_static_request(make_request("/index.html"));

    EXPECT_EQ(res.status_code, http::StatusCode::OK);
    EXPECT_EQ(res.body, kIndexHtml.c_str());
    EXPECT_EQ(res.headers["Content-Type"], "text/html");
    EXPECT_EQ(res.headers["Content-Length"], std::pmr::string(std::to_string(kIndexHtml.size())));
}

TEST_F(StaticHandlerTest, RootIsAliasForIndexHtml)
{
    http::Response res = handlers::handle_static_request(make_request("/"));

    EXPECT_EQ(res.status_code, http::StatusCode::OK);
    EXPECT_EQ(res.body, kIndexHtml.c_str());
}

TEST_F(StaticHandlerTest, ServesNestedFileWithCorrectMime)
{
    http::Response res = handlers::handle_static_request(make_request("/css/style.css"));

    EXPECT_EQ(res.status_code, http::StatusCode::OK);
    EXPECT_EQ(res.body, kStyleCss.c_str());
    EXPECT_EQ(res.headers["Content-Type"], "text/css");
}

TEST_F(StaticHandlerTest, MissingFileReturns404)
{
    http::Response res = handlers::handle_static_request(make_request("/nope.html"));

    EXPECT_EQ(res.status_code, http::StatusCode::NOT_FOUND);
    EXPECT_EQ(res.headers["Content-Type"], "text/html");
    EXPECT_EQ(res.headers["Content-Length"], std::pmr::string(std::to_string(res.body.size())));
}

TEST_F(StaticHandlerTest, NonGetMethodIsNotAllowed)
{
    http::Request req;
    req.method = http::Method::DELETE;
    req.uri = std::pmr::string("/index.html");

    http::Response res = handlers::handle_static_request(req);

    EXPECT_EQ(res.status_code, http::StatusCode::METHOD_NOT_ALLOWED);
    EXPECT_EQ(res.headers["Allow"], "GET");
}

TEST_F(StaticHandlerTest, PathTraversalIsForbidden)
{
    http::Response res = handlers::handle_static_request(make_request("/../etc/passwd"));

    EXPECT_EQ(res.status_code, http::StatusCode::FORBIDDEN);
}

TEST_F(StaticHandlerTest, EmbeddedDotDotIsForbidden)
{
    http::Response res = handlers::handle_static_request(make_request("/css/../../secret.txt"));

    EXPECT_EQ(res.status_code, http::StatusCode::FORBIDDEN);
}
