#include "server/HttpServer.hpp"
#include "server/ThreadPool.hpp"
#include "http/Parser.hpp"
#include "utils/Logger.hpp"

#include <memory>

#include <MemCore/MallocUpstream.hpp>
#include <MemCore/ArenaAllocator.hpp>
#include <MemCore/PmrAdapter.hpp>


namespace 
{
    size_t extract_content_length(std::string_view headers) 
    {
        const std::string_view search_str = "content-length:";
        // Case-insensitive search
        auto it = std::search(
            headers.begin(), headers.end(),
            search_str.begin(), search_str.end(),
            [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); }
        );

        if (it != headers.end()) 
        {
            std::string_view val(it + search_str.size(), headers.end() - (it + search_str.size()));
            size_t start = val.find_first_not_of(" \t"); // Skip spaces after the colon
            if (start != std::string_view::npos) 
            {
                size_t end = val.find("\r\n", start);
                if (end != std::string_view::npos) 
                {
                    try 
                    {
                        return std::stoull(std::string(val.substr(start, end - start)));
                    } 
                    catch (...) 
                    {
        
                    }
                }
            }
        }
        return 0;
    }
}


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

                    // CONNECTION BUFFER: lives outside the Keep-Alive loop!
                    // Retains data between requests (pipelining)
                    std::string connection_buffer;
                    connection_buffer.reserve(8192);

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
                            // === PHASE 1: Read headers until \r\n\r\n ===
                            size_t headers_end = std::string::npos;
                            while ((headers_end = connection_buffer.find("\r\n\r\n")) == std::string::npos) 
                            {
                                std::string chunk = client_ptr->recv(4096);
                                if (chunk.empty()) 
                                    break; 
                                connection_buffer.append(chunk);
                            }

                            // Normal connection close by the client
                            if (connection_buffer.empty()) 
                                break;

                            // Connection dropped in the middle of headers
                            if (headers_end == std::string::npos) 
                            {
                                LOG_ERROR("Connection closed before full headers received.");
                                break;
                            }

                            // === PHASE 2: Look for Content-Length ===
                            std::string_view headers_view(connection_buffer.data(), headers_end);
                            size_t content_length = extract_content_length(headers_view);
                            size_t total_expected_size = headers_end + 4 + content_length;

                            // === PHASE 3: Read the rest of the request body ===
                            while (connection_buffer.size() < total_expected_size) 
                            {
                                std::string chunk = client_ptr->recv(4096);
                                if (chunk.empty()) 
                                {
                                    throw std::runtime_error("Connection dropped while reading body");
                                }
                                connection_buffer.append(chunk);
                            }

                            // === PHASE 4: Slice and hand over to the parser ===
                            // Extract exactly one complete request
                            std::string raw_request = connection_buffer.substr(0, total_expected_size);
                            
                            // REMOVE it from the buffer. If the client sent 2 requests at once,
                            // the start of the second one is now safely kept in connection_buffer!
                            connection_buffer.erase(0, total_expected_size);

                            http::Request req(&pmr_resource);
                            req = http::parse_request(raw_request, &pmr_resource);

                            // Handle Keep-Alive
                            bool keep_alive = true;
                            auto it = req.headers.find("Connection");
                            if (it != req.headers.end()) 
                            {
                                if (it->second == "close" || it->second == "Close") keep_alive = false;
                            }

                            http::Response res(&pmr_resource);
                            res = this->router_.route(req);
                            
                            res.headers["Connection"] = keep_alive ? "keep-alive" : "close";
                            client_ptr->send(res.serialize());

                            if (!keep_alive) 
                                break;
                            
                        } 
                        catch (const std::exception& e) 
                        {
                            LOG_ERROR("Thread error: {}", e.what());
                            
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

                            break; // On parse error, always terminate the connection
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