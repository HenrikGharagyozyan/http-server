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
                    // The upstream lives forever for each thread (thread_local)
                    static thread_local MemCore::MallocUpstream upstream;

                    // Set the Keep-Alive timeout (e.g. 5 seconds of inactivity)
                    try 
                    {
                        client_ptr->set_rcv_timeout(5);
                    } 
                    catch (const std::exception& e) 
                    {
                        LOG_ERROR("Timeout config error: {}", e.what());
                        return;
                    }


                    // Start the Keep-Alive loop
                    while (true) 
                    {
                        // The arena is created FOR EACH REQUEST!
                        // At the end of the loop it is destroyed, returning all memory to upstream.
                        // No fragmentation during long-lived connections.
                        MemCore::ArenaAllocator arena(upstream, 64 * 1024);
                        MemCore::PmrAdapter pmr_resource(arena);

                        try 
                        {
                            std::string raw_request = client_ptr->recv();
                            
                            // If empty (client closed the connection OR the 5-second timeout fired)
                            if (raw_request.empty()) 
                            {
                                break; // Exit the Keep-Alive loop and close the socket
                            }

                            http::Request req(&pmr_resource);
                            req = http::parse_request(raw_request, &pmr_resource);

                            // HTTP/1.1 uses Keep-Alive by default
                            bool keep_alive = true;
                            auto it = req.headers.find("Connection");
                            if (it != req.headers.end()) 
                            {
                                // Simple check (ideally should be case-insensitive)
                                if (it->second == "close" || it->second == "Close") 
                                {
                                    keep_alive = false;
                                }
                            }

                            http::Response res(&pmr_resource);
                            res = this->router_.route(req);
                            
                            // Set the correct Connection header in the response
                            if (keep_alive) 
                                res.headers["Connection"] = "keep-alive";
                            else 
                                res.headers["Connection"] = "close";
                            
                            client_ptr->send(res.serialize());

                            // If the client requested to close the connection, exit
                            if (!keep_alive) 
                                break;
                        } 
                        catch (const std::exception& e) 
                        {
                            LOG_ERROR("Thread error: {}", e.what());
                            
                            // Safe send of 400 Bad Request.
                            // If the socket has already disconnected, send() will throw an exception.
                            // We catch it here so it does not bring down the whole server via std::terminate.
                            try 
                            {
                                http::Response res(&pmr_resource);
                                res.status_code = http::StatusCode::BAD_REQUEST;
                                res.body = "Bad Request";
                                res.headers["Content-Length"] = std::to_string(res.body.size());
                                res.headers["Connection"] = "close";
                                client_ptr->send(res.serialize());
                            } 
                            catch (const std::exception& send_err) 
                            {
                                LOG_ERROR("Failed to send 400 Bad Request to client: {}", send_err.what());
                            }

                            break; // On parse error always close the connection
                        }
                    }
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