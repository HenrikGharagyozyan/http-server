#pragma once

#include "server/TcpServer.hpp"
#include "server/Router.hpp"

#include <string>
#include <atomic>


namespace server 
{

    class HttpServer 
    {
    public:
        HttpServer() = default;

        // Proxy methods for registering routes
        void get(const std::string& uri, Handler handler);
        void post(const std::string& uri, Handler handler);

        void set_default_handler(Handler handler);

        // Main startup method (contains the while loop)
        void listen(uint16_t port, size_t thread_count);

        void stop();

    private:
        TcpServer tcp_server_;
        Router router_;
        std::atomic<bool> is_running_{ false };

        // --- Safety limits ---
        static constexpr size_t MAX_HEADER_SIZE = 8192;           // 8 KB
        static constexpr size_t MAX_BODY_SIZE = 2 * 1024 * 1024;  // 2 MB
    };

} // namespace server