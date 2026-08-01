#pragma once

#include "server/TcpServer.hpp"
#include "server/Router.hpp"
#include "server/Config.hpp"

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
        void listen(const Config& config);

        void stop();

    private:
        TcpServer tcp_server_;
        Router router_;
        std::atomic<bool> is_running_{ false };

    };

} // namespace server