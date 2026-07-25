#include "server/HttpServer.hpp"
#include "server/ThreadPool.hpp"
#include "http/Parser.hpp"
#include "utils/Logger.hpp"

#include <iostream>
#include <thread>
#include <memory>


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

    
    void HttpServer::set_default_handler(Handler handler)
    {
        router_.set_default_handler(std::move(handler));
    }

    
    void HttpServer::listen(uint16_t port, size_t thread_count)
    {
        tcp_server_.start(port);
        LOG_INFO("HttpServer is listening on port {}...", port);

        ThreadPool pool(thread_count); 
        LOG_INFO("Thread pool started with {} workers.", thread_count);
        
        is_running_ = true;

        while (is_running_) 
        {
            Socket client = tcp_server_.accept_connection();
            
            // Если мы вернули невалидный сокет из-за остановки сервера, выходим из цикла
            if (!client.is_valid()) 
            {
                break;
            }
            
            auto client_ptr = std::make_shared<Socket>(std::move(client));
            
            pool.enqueue([this, client_ptr]() 
                {
                    try 
                    {
                        std::string raw_request = client_ptr->recv();
                        if (raw_request.empty()) 
                            return;

                        http::Request req = http::parse_request(raw_request);
                        http::Response res = this->router_.route(req);
                        client_ptr->send(res.serialize());
                    } 
                    catch (const std::exception& e) 
                    {
                        LOG_ERROR("Thread error: {}", e.what());
                        http::Response res;
                        res.status_code = http::StatusCode::BAD_REQUEST;
                        res.body = "Bad Request";
                        res.headers["Content-Length"] = std::to_string(res.body.size());
                        client_ptr->send(res.serialize());
                    }
                });
        }
        
        LOG_INFO("Server loop stopped. Waiting for pending tasks to finish...");
        // При выходе из области видимости 'pool' вызовется деструктор ThreadPool,
        // который через worker.join() дождется завершения всех текущих соединений.
        LOG_INFO("Server shutdown gracefully.");
    }

    void HttpServer::stop()
    {
        is_running_ = false;
        tcp_server_.stop();  // Разблокируем accept_connection
    }

} // namespace server