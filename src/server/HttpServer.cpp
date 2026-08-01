#include "server/HttpServer.hpp"
#include "server/ThreadPool.hpp"
#include "http/Parser.hpp"
#include "utils/Logger.hpp"

#include <memory>
#include <charconv>
#include <algorithm>
#include <cctype>
#include <memory_resource>

#include <MemCore/MallocUpstream.hpp>
#include <MemCore/ArenaAllocator.hpp>
#include <MemCore/PmrAdapter.hpp>

namespace 
{

    struct HeaderInspection 
    {
        size_t content_length = 0;
        bool has_content_length = false;
        bool is_chunked = false;
        bool invalid = false;
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

            while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.remove_prefix(1);
            while (!val.empty() && (val.back() == ' ' || val.back() == '\t'))   val.remove_suffix(1);

            if (iequals(key, "Content-Length")) 
            {
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

    void send_rejection(server::Socket* socket, std::pmr::memory_resource* pmr, http::StatusCode code, std::string_view body) 
    {
        try 
        {
            http::Response res(pmr);
            res.status_code = code;
            res.body = std::string(body);
            res.headers["Connection"] = "close";
            // res.serialize() automatically handles Content-Length
            socket->send(res.serialize());
        } 
        catch (...) {}
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
    
    // FIX: Using Config struct directly
    void HttpServer::listen(const Config& config)
    {
        tcp_server_.start(config.port);
        LOG_INFO("HttpServer is listening on port {}...", config.port);

        ThreadPool pool(config.threads); 
        LOG_INFO("Thread pool started with {} workers.", config.threads);
        
        is_running_ = true;

        while (is_running_) 
        {
            std::string client_ip;
            Socket client;

            try 
            {
                client = tcp_server_.accept_connection(&client_ip);
            }
            catch (const std::system_error& e)
            {
                if (!is_running_) 
                    break;

                int err = e.code().value();
                if (err == EMFILE || err == ENFILE) 
                {
                    LOG_WARN("Out of file descriptors! Throttling for 100ms...");
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }
                LOG_ERROR("Accept error: {}", e.what());
                continue;
            }
            
            if (!client.is_valid()) 
                break;
            
            auto client_ptr = std::make_shared<Socket>(std::move(client));
            
            bool accepted = pool.enqueue([this, client_ptr, client_ip, config]() 
                {
                    static thread_local MemCore::MallocUpstream upstream;

                    try 
                    {
                        client_ptr->set_rcv_timeout(config.keep_alive_timeout);
                        client_ptr->set_snd_timeout(config.keep_alive_timeout);
                    } 
                    catch (const std::exception& e) 
                    {
                        LOG_ERROR("Timeout config error: {}", e.what());
                        return;
                    }

                    std::string connection_buffer;
                    connection_buffer.reserve(8192);
                    std::vector<char> read_buf(4096);

                    while (true) 
                    {
                        MemCore::ArenaAllocator arena(upstream, 64 * 1024);
                        MemCore::PmrAdapter pmr_resource(arena);

                        try 
                        {
                            size_t headers_end = std::string::npos;
                            while ((headers_end = connection_buffer.find("\r\n\r\n")) == std::string::npos) 
                            {
                                if (connection_buffer.size() > config.max_header_size) 
                                {
                                    size_t first_line_end = connection_buffer.find("\r\n");
                                    if (first_line_end != std::string::npos && first_line_end > 8192) 
                                    {                                if (connection_buffer.size() > config.max_header_size) 

                                        send_rejection(client_ptr.get(), &pmr_resource, http::StatusCode::URI_TOO_LONG, "URI Too Long");
                                    } 
                                    else 
                                    {
                                        send_rejection(client_ptr.get(), &pmr_resource, http::StatusCode::HEADER_FIELDS_TOO_LARGE, "Headers Too Large");
                                    }
                                    return;
                                }

                                size_t bytes_read = client_ptr->recv(read_buf);
                                if (bytes_read == 0) break; 
                                connection_buffer.append(read_buf.data(), bytes_read);
                            }

                            if (connection_buffer.empty()) 
                                break;
                            if (headers_end == std::string::npos) 
                            {
                                LOG_ERROR("Connection closed before full headers received.");
                                break;
                            }

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

                            if (info.content_length > config.max_request_size) 
                            {
                                send_rejection(client_ptr.get(), &pmr_resource, http::StatusCode::PAYLOAD_TOO_LARGE, "Payload Too Large");
                                break;
                            }

                            size_t total_expected_size = headers_end + 4 + info.content_length;

                            while (connection_buffer.size() < total_expected_size) 
                            {
                                size_t bytes_read = client_ptr->recv(read_buf);
                                if (bytes_read == 0) 
                                {
                                    throw std::runtime_error("Connection dropped while reading body");
                                }
                                connection_buffer.append(read_buf.data(), bytes_read);
                            }

                            std::string raw_request = connection_buffer.substr(0, total_expected_size);
                            connection_buffer.erase(0, total_expected_size);

                            http::Request req(&pmr_resource);
                            req = http::parse_request(raw_request, &pmr_resource);

                            bool keep_alive = true;
                            auto it = req.headers.find("Connection");
                            if (it != req.headers.end()) 
                            {
                                if (it->second == "close" || it->second == "Close") keep_alive = false;
                            }

                            http::Response res(&pmr_resource);
                            
                            try 
                            {
                                res = this->router_.route(req);
                            }
                            catch (const std::exception& e)
                            {
                                LOG_ERROR("[{}] Handler crashed: {}", client_ip, e.what());
                                res.status_code = http::StatusCode::INTERNAL_SERVER_ERROR; 
                                res.body = "Internal Server Error";
                                keep_alive = false;
                            }
                            catch (...)
                            {
                                LOG_ERROR("[{}] Handler crashed with unknown exception", client_ip);
                                res.status_code = http::StatusCode::INTERNAL_SERVER_ERROR; 
                                res.body = "Internal Server Error";
                                keep_alive = false;
                            }

                            LOG_INFO("[{}] Request to '{}' -> HTTP {}", client_ip, std::string_view(req.uri), static_cast<int>(res.status_code));
                            
                            res.headers["Connection"] = keep_alive ? "keep-alive" : "close";
                                                        
                            client_ptr->send(res.serialize());

                            if (!keep_alive) break;
                            
                        } 
                        catch (const std::system_error& e) 
                        {
                            LOG_WARN("[{}] Network error: {}", client_ip, e.what());
                            break;
                        }
                        catch (const std::exception& e) 
                        {
                            LOG_ERROR("[{}] Thread error: {}", client_ip, e.what());
                            send_rejection(client_ptr.get(), &pmr_resource, http::StatusCode::BAD_REQUEST, "Bad Request");
                            break;
                        }
                    }
                });

            if (!accepted) 
            {
                LOG_WARN("[{}] Server is overloaded. Rejecting with 503.", client_ip);
                
                MemCore::MallocUpstream upstream;
                MemCore::ArenaAllocator arena(upstream, 1024);
                MemCore::PmrAdapter local_pmr(arena);
                
                send_rejection(client_ptr.get(), &local_pmr, http::StatusCode::SERVICE_UNAVAILABLE, "Service Unavailable - Server Overloaded");
            }
        }
        
        LOG_INFO("Server loop stopped. Waiting for pending tasks to finish...");
        LOG_INFO("Server shutdown gracefully.");
    }

    void HttpServer::stop()
    {
        is_running_ = false;
        tcp_server_.stop();
    }

} // namespace server