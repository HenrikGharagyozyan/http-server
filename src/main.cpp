#include "server/TcpServer.hpp"
#include "server/Router.hpp"
#include "http/Parser.hpp"
#include "http/Response.hpp"
#include <iostream>

int main() 
{
    try 
    {
        // 1. Set up the router (declarative approach)
        server::Router router;

        router.get("/", [](const http::Request& /*req*/) {
            http::Response res;
            res.status_code = http::StatusCode::OK;
            res.body = "<h1>Welcome to cpp-http-server!</h1>";
            res.headers["Content-Length"] = std::to_string(res.body.size());
            res.headers["Content-Type"] = "text/html";
            return res;
        });

        // Create a JSON API endpoint to showcase the router's power
        router.get("/api/users", [](const http::Request& /*req*/) {
            http::Response res;
            res.status_code = http::StatusCode::OK;
            // Simulate a response from the database
            res.body = "{\"users\": [\"Henrik\", \"Arshavir\"]}";
            res.headers["Content-Length"] = std::to_string(res.body.size());
            res.headers["Content-Type"] = "application/json";
            return res;
        });

        // 2. Start the server
        server::TcpServer server;
        uint16_t port = 8080;
        
        std::cout << "Starting server on port " << port << "...\n";
        server.start(port);
        std::cout << "Server is listening. Press Ctrl+C to stop.\n\n";

        // 3. Main loop
        while (true) 
        {
            server::Socket client = server.accept_connection();
            std::string raw_request = client.recv();
            if (raw_request.empty()) continue;

            try 
            {
                http::Request req = http::parse_request(raw_request);
                
                // All business logic is now hidden inside the route() call!
                http::Response res = router.route(req);
                
                client.send(res.serialize());
            } 
            catch (const std::exception& e) 
            {
                std::cerr << "Parse error: " << e.what() << "\n";
                http::Response res;
                res.status_code = http::StatusCode::BAD_REQUEST;
                res.body = "Bad Request";
                res.headers["Content-Length"] = std::to_string(res.body.size());
                client.send(res.serialize());
            }
        }
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}