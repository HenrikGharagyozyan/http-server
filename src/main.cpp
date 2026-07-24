#include "server/HttpServer.hpp"
#include "http/Response.hpp"
#include "utils/FileSystem.hpp"

#include <iostream>
#include <chrono>
#include <thread>


http::Response handle_static_request(const http::Request& req) 
{
    http::Response res;
    
    // Protect against escaping the public folder (directory traversal)
    if (req.uri.find("..") != std::string_view::npos) 
    {
        res.status_code = http::StatusCode::FORBIDDEN;
        res.body = "<h1>403 Forbidden</h1>";
        res.headers["Content-Type"] = "text/html";
        res.headers["Content-Length"] = std::to_string(res.body.size());
        return res;
    }

    // Build the file path
    // (Use std::string(req.uri) because uri is now std::string_view)
    std::string filepath = "../public"; // "../" because we run from build/
    if (req.uri == "/") 
    {
        filepath += "/index.html";
    } 
    else 
    {
        filepath += std::string(req.uri); 
    }

    std::string file_content;
    if (utils::read_file(filepath, file_content)) 
    {
        res.status_code = http::StatusCode::OK;
        res.body = std::move(file_content); // std::move for optimization!
        res.headers["Content-Type"] = utils::get_mime_type(filepath);
    } 
    else 
    {
        // If the file is not found
        res.status_code = http::StatusCode::NOT_FOUND;
        res.body = "<h1>404 - File Not Found</h1>";
        res.headers["Content-Type"] = "text/html";
    }

    res.headers["Content-Length"] = std::to_string(res.body.size());
    return res;
}


int main() 
{
    try 
    {
        server::HttpServer app;
        
        // Our JSON API
        app.get("/api/users", [](const http::Request& /*req*/) 
            {
                http::Response res;
                res.status_code = http::StatusCode::OK;
                res.body = "{\"users\": [\"Henrik\", \"Arshavir\"]}";
                res.headers["Content-Length"] = std::to_string(res.body.size());
                res.headers["Content-Type"] = "application/json";
                return res;
            });

        // Test POST route
        app.post("/api/echo", [](const http::Request& req) 
            {
                http::Response res;
                
                // Print to the server console what the client sent us
                std::cout << "[POST] Received body: " << req.body << "\n";

                res.status_code = http::StatusCode::OK;
                // Return to the client what it sent us (echo server)
                res.body = "Server received your data:\n" + req.body;
                res.headers["Content-Length"] = std::to_string(res.body.size());
                res.headers["Content-Type"] = "text/plain";
                
                return res;
            });

        // Static file handler (fallback)
        app.set_default_handler(handle_static_request);

        // Run server
        app.listen(8080);
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}