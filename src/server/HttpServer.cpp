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

    void HttpServer::listen(uint16_t port) 
    {
        tcp_server_.start(port);
        LOG_INFO("HttpServer is listening on port {}...", port);

        // Create a thread pool. std::thread::hardware_concurrency() returns
        // the number of logical cores on your CPU (for example, 8 or 16).
        size_t threads = std::thread::hardware_concurrency();
        ThreadPool pool(threads > 0 ? threads : 4); 
        
        LOG_INFO("Thread pool started with {} workers.", (threads > 0 ? threads : 4));
        LOG_INFO("Press Ctrl+C to stop.\n");

        while (true) 
        {
            // 1. The main thread accepts a client
            Socket client = tcp_server_.accept_connection();
            
            // Wrap the socket in a shared_ptr so it can be safely passed to std::function
            auto client_ptr = std::make_shared<Socket>(std::move(client));
            
            // Enqueue a task in the pool instead of creating a new std::thread
            pool.enqueue([this, client_ptr]() 
                {
                    try 
                    {
                        std::string raw_request = client_ptr->recv();
                        if (raw_request.empty()) return;

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
                    // When the shared_ptr is destroyed, it deletes the Socket,
                    // and the Socket destructor closes the connection.
                });
        }
    }

} // namespace server