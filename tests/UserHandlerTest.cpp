#include <gtest/gtest.h>
#include "handlers/UserHandler.hpp"

#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

namespace
{

    http::Request make_request(http::Method method, std::string_view uri, std::string_view body = "")
    {
        http::Request req;
        req.method = method;
        req.uri = std::pmr::string(uri);
        req.body = std::pmr::string(body);
        return req;
    }

    json parse_body(const http::Response& res)
    {
        return json::parse(std::string(res.body.begin(), res.body.end()));
    }

} // namespace

// ==========================================================
// GET /api/users
// ==========================================================

TEST(UserHandlerTest, GetUsersReturnsOkJson)
{
    http::Response res = handlers::get_users(make_request(http::Method::GET, "/api/users"));

    EXPECT_EQ(res.status_code, http::StatusCode::OK);
    EXPECT_EQ(res.headers["Content-Type"], "application/json");
    EXPECT_EQ(res.headers["Content-Length"], std::pmr::string(std::to_string(res.body.size())));

    json body = parse_body(res);
    EXPECT_EQ(body["status"], "success");
    ASSERT_TRUE(body["users"].is_array());
    EXPECT_EQ(body["users"].size(), 2u);
    EXPECT_EQ(body["users"][0]["name"], "Henrik");
}

// ==========================================================
// POST /api/users
// ==========================================================

TEST(UserHandlerTest, CreateUserWithValidJson)
{
    const std::string payload = R"({"name":"Alice","role":"tester"})";
    http::Response res = handlers::create_user(make_request(http::Method::POST, "/api/users", payload));

    EXPECT_EQ(res.status_code, http::StatusCode::OK);
    EXPECT_EQ(res.headers["Content-Type"], "application/json");

    json body = parse_body(res);
    EXPECT_EQ(body["status"], "created");
    EXPECT_EQ(body["user"]["name"], "Alice");
    EXPECT_EQ(body["user"]["role"], "tester");
}

TEST(UserHandlerTest, CreateUserMissingNameIsBadRequest)
{
    http::Response res = handlers::create_user(
        make_request(http::Method::POST, "/api/users", R"({"role":"tester"})"));

    EXPECT_EQ(res.status_code, http::StatusCode::BAD_REQUEST);
    json body = parse_body(res);
    EXPECT_EQ(body["error"], "Bad Request");
}

TEST(UserHandlerTest, CreateUserMissingRoleIsBadRequest)
{
    http::Response res = handlers::create_user(
        make_request(http::Method::POST, "/api/users", R"({"name":"Alice"})"));

    EXPECT_EQ(res.status_code, http::StatusCode::BAD_REQUEST);
}

TEST(UserHandlerTest, CreateUserWithInvalidJsonIsBadRequest)
{
    http::Response res = handlers::create_user(
        make_request(http::Method::POST, "/api/users", "{not valid json"));

    EXPECT_EQ(res.status_code, http::StatusCode::BAD_REQUEST);
    json body = parse_body(res);
    EXPECT_EQ(body["error"], "Invalid JSON format");
}

TEST(UserHandlerTest, CreateUserWithEmptyBodyIsBadRequest)
{
    http::Response res = handlers::create_user(
        make_request(http::Method::POST, "/api/users", ""));

    EXPECT_EQ(res.status_code, http::StatusCode::BAD_REQUEST);
}

TEST(UserHandlerTest, ContentLengthMatchesBody)
{
    http::Response res = handlers::create_user(
        make_request(http::Method::POST, "/api/users", R"({"name":"A","role":"B"})"));

    EXPECT_EQ(res.headers["Content-Length"], std::pmr::string(std::to_string(res.body.size())));
}
