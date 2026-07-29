#include "server/HttpServer.hpp"
#include "server/ThreadPool.hpp"
#include "http/Parser.hpp"
#include "utils/Logger.hpp"

#include <memory>
#include <charconv>
#include <algorithm> // For std::search
#include <cctype>   // For std::tolower
#include <memory_resource>

#include <MemCore/MallocUpstream.hpp>
#include <MemCore/ArenaAllocator.hpp>
#include <MemCore/PmrAdapter.hpp>


namespace 
{

    // --- Safety limits ---
    constexpr size_t MAX_HEADER_SIZE = 16 * 1024;        // 16 KB
    constexpr size_t MAX_BODY_SIZE   = 10 * 1024 * 1024; // 10 MB
    constexpr size_t MAX_URI_SIZE    = 8 * 1024;         // 8 KB


    struct HeaderInspection 
    {
        size_t content_length = 0;
        bool has_content_length = false;
        bool is_chunked = false;
        bool invalid = false; // Set for duplicates, non-numeric values, or overflow
    };

    HeaderInspection inspect_headers(std::string_view headers) 
    {
        HeaderInspection info;
        size_t pos = 0;

        auto iequals = [](std::string_view a, std::string_view b) 
                            {
                                return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                                    [](char c1, char c2) { return std::tolower(c1) == std::tolower(c2); });
                            };

        while (pos < headers.size()) 
        {
            size_t next = headers.find("\r\n", pos);
            std::string_view line = (next == std::string_view::npos) 
                ? headers.substr(pos) 
                : headers.substr(pos, next - pos);

            pos = (next == std::string_view::npos) ? headers.size() : next + 2;

            if (line.empty()) 
                continue;

            size_t colon = line.find(':');
            if (colon == std::string_view::npos) 
                continue;

            std::string_view key = line.substr(0, colon);
            std::string_view val = line.substr(colon + 1);

            // Trim leading and trailing spaces
            while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.remove_prefix(1);
            while (!val.empty() && (val.back() == ' ' || val.back() == '\t'))   val.remove_suffix(1);

            if (iequals(key, "Content-Length")) 
            {
                // Protection against HTTP request smuggling
                if (info.has_content_length) 
                {
                    info.invalid = true;
                    return info;
                }
                info.has_content_length = true;

                if (val.empty()) 
                {
                    info.invalid = true;
                    return info;
                }

                size_t parsed_val = 0;
                auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), parsed_val);
                
                if (ec != std::errc{} || ptr != val.data() + val.size()) 
                {
                    info.invalid = true;
                    return info;
                }
                info.content_length = parsed_val;
            } 
            else if (iequals(key, "Transfer-Encoding")) 
            {
                if (val.find("chunked") != std::string_view::npos) 
                {
                    info.is_chunked = true;
                }
            }
        }

        return info;
    }

    // Helper function for quickly sending errors to the client
    void send_rejection(server::Socket* socket, std::pmr::memory_resource* pmr, http::StatusCode code, std::string_view body) 
    {
        try 
        {
            http::Response res(pmr);
            res.status_code = code;
            res.body = std::string(body); // Convert string_view to string
            res.headers["Content-Length"] = std::to_string(res.body.size());
            res.headers["Connection"] = "close";
            socket->send(res.serialize());
        } 
        catch (...) 
        {
            // Ignore socket errors when sending rejection responses
        }
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
                    static thread_local MemCore::MallocUpstream upstream;

                    try 
                    {
                        client_ptr->set_rcv_timeout(5);
                    } 
                    catch (const std::exception& e) 
                    {
                        LOG_ERROR("Timeout config error: {}", e.what());
                        return;
                    }

                    std::string connection_buffer;
                    connection_buffer.reserve(8192);

                    while (true) 
                    {
                        MemCore::ArenaAllocator arena(upstream, 64 * 1024);
                        MemCore::PmrAdapter pmr_resource(arena);

                        try 
                        {
                            // === PHASE 1: Read headers with limit checking ===
                            size_t headers_end = std::string::npos;
                            while ((headers_end = connection_buffer.find("\r\n\r\n")) == std::string::npos) 
                            {
                                // Check limits BEFORE reading new bytes
                                if (connection_buffer.size() > MAX_HEADER_SIZE) 
                                {
                                    size_t first_line_end = connection_buffer.find("\r\n");
                                    if (first_line_end != std::string::npos && first_line_end > MAX_URI_SIZE) 
                                    {
                                        send_rejection(client_ptr.get(), &pmr_resource, http::StatusCode::URI_TOO_LONG, "URI Too Long");
                                    } 
                                    else 
                                    {
                                        send_rejection(client_ptr.get(), &pmr_resource, http::StatusCode::HEADER_FIELDS_TOO_LARGE, "Headers Too Large");
                                    }
                                    return; // Immediately terminate the connection
                                }

                                std::string chunk = client_ptr->recv(4096);
                                if (chunk.empty()) 
                                    break; 
                                connection_buffer.append(chunk);
                            }

                            if (connection_buffer.empty()) 
                                break; // Normal connection close

                            if (headers_end == std::string::npos) 
                            {
                                LOG_ERROR("Connection closed before full headers received.");
                                break;
                            }

                            // === PHASE 2: Inspect headers ===
                            std::string_view headers_view(connection_buffer.data(), headers_end);
                            HeaderInspection info = inspect_headers(headers_view);

                            if (info.invalid) 
                            {
                                LOG_ERROR("Malformed or duplicate Content-Length header.");
                                send_rejection(client_ptr.get(), &pmr_resource, http::StatusCode::BAD_REQUEST, "Bad Request");
                                break;
                            }

                            if (info.is_chunked) 
                            {
                                send_rejection(client_ptr.get(), &pmr_resource, http::StatusCode::NOT_IMPLEMENTED, "Transfer-Encoding: chunked is not implemented");
                                break;
                            }

                            if (info.content_length > MAX_BODY_SIZE) 
                            {
                                send_rejection(client_ptr.get(), &pmr_resource, http::StatusCode::PAYLOAD_TOO_LARGE, "Payload Too Large");
                                break;
                            }

                            size_t total_expected_size = headers_end + 4 + info.content_length;

                            // === PHASE 3: Read the body to completion ===
                            while (connection_buffer.size() < total_expected_size) 
                            {
                                std::string chunk = client_ptr->recv(4096);
                                if (chunk.empty()) 
                                {
                                    throw std::runtime_error("Connection dropped while reading body");
                                }
                                connection_buffer.append(chunk);
                            }

                            // === PHASE 4: Parsing and routing ===
                            std::string raw_request = connection_buffer.substr(0, total_expected_size);
                            connection_buffer.erase(0, total_expected_size);

                            http::Request req(&pmr_resource);
                            req = http::parse_request(raw_request, &pmr_resource);

                            bool keep_alive = true;
                            auto it = req.headers.find("Connection");
                            if (it != req.headers.end()) 
                            {
                                if (it->second == "close" || it->second == "Close") 
                                    keep_alive = false;
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
                            send_rejection(client_ptr.get(), &pmr_resource, http::StatusCode::BAD_REQUEST, "Bad Request");
                            break;
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