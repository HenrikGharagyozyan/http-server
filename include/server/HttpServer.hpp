#pragma once

#include "server/TcpServer.hpp"
#include "server/Router.hpp"
#include <string>

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
        void listen(uint16_t port);

    private:
        TcpServer tcp_server_;
        Router router_;
    };

} // namespace server