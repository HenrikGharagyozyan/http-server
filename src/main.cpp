#include "server/HttpServer.hpp"
#include "http/Response.hpp"
#include "utils/FileSystem.hpp"

#include <iostream>
#include <chrono>
#include <thread>


int main() 
{
    try 
    {
        server::HttpServer app;
        
        // Наш JSON API
        app.get("/api/users", [](const http::Request& /*req*/) 
            {
                http::Response res;
                res.status_code = http::StatusCode::OK;
                res.body = "{\"users\": [\"Henrik\", \"Arshavir\"]}";
                res.headers["Content-Length"] = std::to_string(res.body.size());
                res.headers["Content-Type"] = "application/json";
                return res;
            });

        // Обработчик статических файлов (Fallback)
        app.set_default_handler([](const http::Request& req) 
            {
                http::Response res;
                
                // Если запрашивают корень "/", отдаем index.html
                std::string filepath = (req.uri == "/") ? "/index.html" : req.uri;
                
                // Ищем файлы относительно папки запуска (корень проекта)
                // "../" потому что мы запускаем бинарник из папки build/
                std::string full_path = "../public" + filepath; 

                std::string file_content;
                if (utils::read_file(full_path, file_content)) 
                {
                    res.status_code = http::StatusCode::OK;
                    res.body = std::move(file_content);
                    res.headers["Content-Type"] = utils::get_mime_type(filepath);
                } 
                else 
                {
                    res.status_code = http::StatusCode::NOT_FOUND;
                    res.body = "<h1>404 Not Found</h1>";
                    res.headers["Content-Type"] = "text/html";
                }
                
                res.headers["Content-Length"] = std::to_string(res.body.size());
                return res;
            });

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