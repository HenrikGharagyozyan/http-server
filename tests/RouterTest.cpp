#include <gtest/gtest.h>
#include "server/Router.hpp"

#include <string>
#include <tuple>

namespace
{

    http::Request make_request(http::Method method, std::string_view uri)
    {
        http::Request req; // Default resource is fine for router tests
        req.method = method;
        req.uri = std::pmr::string(uri);
        return req;
    }

    http::Response ok_response(std::string_view body)
    {
        http::Response res;
        res.status_code = http::StatusCode::OK;
        res.body = std::pmr::string(body);
        res.headers["Content-Length"] = std::pmr::string(std::to_string(body.size()));
        return res;
    }

} // namespace

TEST(RouterTest, GetRouteHit)
{
    server::Router router;
    router.get("/hello", [](const http::Request&) { return ok_response("hello"); });

    http::Response res = router.route(make_request(http::Method::GET, "/hello"));

    EXPECT_EQ(res.status_code, http::StatusCode::OK);
    EXPECT_EQ(res.body, "hello");
}

TEST(RouterTest, PostRouteHit)
{
    server::Router router;
    router.post("/submit", [](const http::Request& req)
    {
        return ok_response(std::string("got:") + std::string(req.body));
    });

    http::Request req = make_request(http::Method::POST, "/submit");
    req.body = "payload";
    http::Response res = router.route(req);

    EXPECT_EQ(res.status_code, http::StatusCode::OK);
    EXPECT_EQ(res.body, "got:payload");
}

TEST(RouterTest, HandlerReceivesTheRequest)
{
    server::Router router;
    http::Method seen_method{};
    std::string seen_uri;

    router.get("/inspect", [&](const http::Request& req)
    {
        seen_method = req.method;
        seen_uri.assign(req.uri.begin(), req.uri.end());
        return ok_response("");
    });

    std::ignore = router.route(make_request(http::Method::GET, "/inspect"));

    EXPECT_EQ(seen_method, http::Method::GET);
    EXPECT_EQ(seen_uri, "/inspect");
}

TEST(RouterTest, MissReturns404WithHeaders)
{
    server::Router router;
    router.get("/exists", [](const http::Request&) { return ok_response("x"); });

    http::Response res = router.route(make_request(http::Method::GET, "/does-not-exist"));

    EXPECT_EQ(res.status_code, http::StatusCode::NOT_FOUND);
    EXPECT_EQ(res.body, "404 Not Found\n");
    EXPECT_EQ(res.headers["Content-Length"], "14");
    EXPECT_EQ(res.headers["Content-Type"], "text/plain");
    EXPECT_EQ(res.headers["Connection"], "close");
}

TEST(RouterTest, EmptyRouterReturns404)
{
    server::Router router;

    http::Response res = router.route(make_request(http::Method::GET, "/anything"));

    EXPECT_EQ(res.status_code, http::StatusCode::NOT_FOUND);
}

TEST(RouterTest, DefaultHandlerCalledOnUriMiss)
{
    server::Router router;
    router.get("/exists", [](const http::Request&) { return ok_response("x"); });
    router.set_default_handler([](const http::Request&)
    {
        http::Response res;
        res.status_code = http::StatusCode::OK;
        res.body = "default";
        return res;
    });

    http::Response res = router.route(make_request(http::Method::GET, "/other"));

    EXPECT_EQ(res.status_code, http::StatusCode::OK);
    EXPECT_EQ(res.body, "default");
}

TEST(RouterTest, RegisteredRouteWinsOverDefaultHandler)
{
    server::Router router;
    router.get("/hello", [](const http::Request&) { return ok_response("specific"); });
    router.set_default_handler([](const http::Request&)
    {
        http::Response res;
        res.body = "default";
        return res;
    });

    http::Response res = router.route(make_request(http::Method::GET, "/hello"));

    EXPECT_EQ(res.body, "specific");
}

TEST(RouterTest, SameUriDifferentMethodsAreDistinct)
{
    server::Router router;
    router.get("/api/users", [](const http::Request&) { return ok_response("list"); });
    router.post("/api/users", [](const http::Request&) { return ok_response("create"); });

    EXPECT_EQ(router.route(make_request(http::Method::GET, "/api/users")).body, "list");
    EXPECT_EQ(router.route(make_request(http::Method::POST, "/api/users")).body, "create");
}

TEST(RouterTest, UnregisteredMethodOnKnownUriReturns405)
{
    server::Router router;
    router.get("/index.html", [](const http::Request&) { return ok_response("page"); });

    http::Response res = router.route(make_request(http::Method::DELETE, "/index.html"));

    EXPECT_EQ(res.status_code, http::StatusCode::METHOD_NOT_ALLOWED);
    EXPECT_EQ(res.headers["Allow"], "GET");
}

// Method-fallthrough (from the Phase 2 plan): a URI registered only for GET,
// with a static-file default handler installed (as main.cpp does), must answer
// DELETE /index.html with 405 Method Not Allowed — never by serving the file.
TEST(RouterTest, DeleteOnGetOnlyRouteMustNotFallThroughToDefaultHandler)
{
    server::Router router;
    router.get("/index.html", [](const http::Request&) { return ok_response("<html>page</html>"); });

    // Simulates handlers::handle_static_request, which serves the cached file
    router.set_default_handler([](const http::Request&) { return ok_response("<html>page</html>"); });

    http::Response res = router.route(make_request(http::Method::DELETE, "/index.html"));

    EXPECT_NE(res.status_code, http::StatusCode::OK)
        << "DELETE on a GET-only route was served by the default (static) handler";
    EXPECT_EQ(res.status_code, http::StatusCode::METHOD_NOT_ALLOWED);
}

TEST(RouterTest, AllowHeaderListsEveryMethodServingTheUri)
{
    server::Router router;
    router.get("/api/users", [](const http::Request&) { return ok_response("list"); });
    router.post("/api/users", [](const http::Request&) { return ok_response("create"); });

    http::Response res = router.route(make_request(http::Method::DELETE, "/api/users"));

    EXPECT_EQ(res.status_code, http::StatusCode::METHOD_NOT_ALLOWED);
    // Map iteration order is unspecified — accept either order
    EXPECT_TRUE(res.headers["Allow"] == "GET, POST" || res.headers["Allow"] == "POST, GET")
        << "Allow: " << res.headers["Allow"];
}
