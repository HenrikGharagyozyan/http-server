#include "server/HttpServer.hpp"
#include "server/ThreadPool.hpp"
#include "http/Parser.hpp"
#include "utils/Logger.hpp"

#include <memory>

#include <MemCore/MallocUpstream.hpp>
#include <MemCore/ArenaAllocator.hpp>
#include <MemCore/PmrAdapter.hpp>


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
            
            // If we returned an invalid socket because the server is stopping, break the loop
            if (!client.is_valid()) 
            {
                break;
            }
            
            auto client_ptr = std::make_shared<Socket>(std::move(client));
            
            pool.enqueue([this, client_ptr]() 
                {
                    // 1. The upstream lives forever for each thread (thread_local)
                    static thread_local MemCore::MallocUpstream upstream;
                    
                    // 2. Create an arena with the upstream and block size (64 KB).
                    // It does not allocate memory immediately; it does so lazily on the first allocation.
                    MemCore::ArenaAllocator arena(upstream, 64 * 1024);
                    
                    // 3. Wrap the arena in a standard PMR interface
                    MemCore::PmrAdapter pmr_resource(arena);

                    try 
                    {
                        std::string raw_request = client_ptr->recv();
                        if (raw_request.empty()) 
                        {
                            return; // When exiting scope, the arena destructor will clean everything up
                        }

                        // 4. The parser uses pmr_resource for all internal allocations
                        http::Request req(&pmr_resource);
                        req = http::parse_request(raw_request, &pmr_resource);

                        http::Response res(&pmr_resource);
                        res = this->router_.route(req);
                        
                        client_ptr->send(res.serialize());
                    } 
                    catch (const std::exception& e) 
                    {
                        LOG_ERROR("Thread error: {}", e.what());
                        http::Response res(&pmr_resource);
                        res.status_code = http::StatusCode::BAD_REQUEST;
                        res.body = "Bad Request";
                        res.headers["Content-Length"] = std::to_string(res.body.size());
                        client_ptr->send(res.serialize());
                    }
                    
                    // When leaving the block, ~ArenaAllocator() is called.
                    // It will walk its linked list (m_head) and return all blocks to upstream.
                    // No leaks, no manual deallocate calls!
                });
        }
        
        LOG_INFO("Server loop stopped. Waiting for pending tasks to finish...");
        // When 'pool' goes out of scope, its ThreadPool destructor is called,
        // which waits for all active connections to finish via worker.join().
        LOG_INFO("Server shutdown gracefully.");
    }

    void HttpServer::stop()
    {
        is_running_ = false;
        tcp_server_.stop();  // Unblock accept_connection
    }

} // namespace server