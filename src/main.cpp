#include "server/HttpServer.hpp"
#include "http/Response.hpp"

#include <iostream>


int main() 
{
    try 
    {
        server::HttpServer app;
        
        // Регистрируем главную страницу
        app.get("/", [](const http::Request& /*req*/) 
            {
                http::Response res;
                res.status_code = http::StatusCode::OK;
                res.body = "<h1>Welcome to cpp-http-server!</h1>";
                res.headers["Content-Length"] = std::to_string(res.body.size());
                res.headers["Content-Type"] = "text/html";
                return res;
            });

        // Регистрируем API эндпоинт
        app.get("/api/users", [](const http::Request& /*req*/) 
            {
                http::Response res;
                res.status_code = http::StatusCode::OK;
                res.body = "{\"users\": [\"Henrik\", \"Arshavir\"]}";
                res.headers["Content-Length"] = std::to_string(res.body.size());
                res.headers["Content-Type"] = "application/json";
                return res;
            });

        // Run server
        app.listen(8080)
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}