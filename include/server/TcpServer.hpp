#pragma once

#include "server/Socket.hpp"
#include <string>
#include <cstdint>

namespace server 
{

    class TcpServer 
    {
    public:
        TcpServer() = default;
        ~TcpServer() = default;

        // Disallow copying and moving the server (must be unique)
        TcpServer(const TcpServer&) = delete;
        TcpServer& operator=(const TcpServer&) = delete;
        TcpServer(TcpServer&&) = delete;
        TcpServer& operator=(TcpServer&&) = delete;

        // Start the server on the specified port
        void start(uint16_t port);

        // Wake accept() and stop accepting new clients
        void stop();

        // Опционально возвращает IP клиента
        Socket accept_connection(std::string* client_ip = nullptr);

    private:
        Socket listen_socket_;
    };

} // namespace server