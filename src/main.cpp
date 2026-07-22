#include "server/TcpServer.hpp"
#include "http/Parser.hpp"
#include "http/Response.hpp"
#include <iostream>

int main() 
{
    try 
    {
        server::TcpServer server;
        uint16_t port = 8080;
        
        std::cout << "Starting server on port " << port << "...\n";
        server.start(port);
        std::cout << "Server is listening. Press Ctrl+C to stop.\n\n";

        while (true) 
        {
            std::cout << "Waiting for connection...\n";
            server::Socket client = server.accept_connection();
            std::cout << "Client connected! Client fd: " << client.get() << "\n";
            
            std::string raw_request = client.recv();
            if (raw_request.empty()) 
                continue; // Protection against empty connections

            try 
            {
                // Parse raw text into our structure
                http::Request req = http::parse_request(raw_request);
                
                std::cout << "--- Parsed Request ---\n";
                std::cout << "URI: " << req.uri << "\n";
                std::cout << "Host: " << req.headers["Host"] << "\n";
                std::cout << "----------------------\n";

                // Build a nice typed response
                http::Response res;
                res.status_code = http::StatusCode::OK;
                res.headers["Content-Type"] = "text/html";
                res.headers["Connection"] = "close";
                res.body = "<h1>Welcome to cpp-http-server!</h1><p>You requested URI: " + req.uri + "</p>";
                
                // Content-Length is required, calculate it dynamically
                res.headers["Content-Length"] = std::to_string(res.body.size());

                client.send(res.serialize());
                std::cout << "Response sent.\n\n";
                
            } 
            catch (const std::exception& e) 
            {
                std::cerr << "Parse error: " << e.what() << "\n";
                
                // If someone sent garbage, return 400 Bad Request
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