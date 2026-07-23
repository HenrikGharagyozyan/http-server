#include "server/HttpServer.hpp"
#include "http/Parser.hpp"
#include <iostream>

namespace server 
{

    void HttpServer::get(const std::string& uri, Handler handler) 
    {
        router_.get(uri, std::move(handler));
    }

    void HttpServer::post(const std::string& uri, Handler handler) 
    {
        router_.post(uri, std::move(handler));
    }

    void HttpServer::listen(uint16_t port) 
    {
        // 1. Запускаем TCP-сервер
        tcp_server_.start(port);
        std::cout << "HttpServer is listening on port " << port << "...\n";
        std::cout << "Press Ctrl+C to stop.\n\n";

        // 2. Скрываем бесконечный цикл здесь
        while (true) 
        {
            Socket client = tcp_server_.accept_connection();
            std::string raw_request = client.recv();
            
            if (raw_request.empty()) 
            {
                continue;
            }

            try 
            {
                http::Request req = http::parse_request(raw_request);
                
                // Логгируем каждый успешный запрос для удобства
                std::cout << "[LOG] Request URI: " << req.uri << "\n";
                
                http::Response res = router_.route(req);
                client.send(res.serialize());
            } 
            catch (const std::exception& e) 
            {
                std::cerr << "[ERROR] Parse/Routing error: " << e.what() << "\n";
                
                http::Response res;
                res.status_code = http::StatusCode::BAD_REQUEST;
                res.body = "Bad Request";
                res.headers["Content-Length"] = std::to_string(res.body.size());
                client.send(res.serialize());
            }
        }
    }

} // namespace server