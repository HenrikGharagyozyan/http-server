#include "server/TcpServer.hpp"

#include <sys/socket.h> // Core socket API
#include <netinet/in.h> // IPv4 socket structures
#include <stdexcept>
#include <cstring>

namespace server 
{

    void TcpServer::start(uint16_t port) 
    {
        // 1. Create a raw socket
        // AF_INET = IPv4, SOCK_STREAM = TCP, 0 = default protocol
        int raw_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (raw_fd < 0) 
        {
            throw std::runtime_error("Failed to create socket");
        }
        
        // Transfer ownership of the raw descriptor to our RAII wrapper
        listen_socket_ = Socket(raw_fd);

        // 2. Configure SO_REUSEADDR
        // This prevents the OS from blocking the port for a few minutes after restarting the server
        int opt = 1;
        if (::setsockopt(listen_socket_.get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) 
        {
            throw std::runtime_error("Failed to set SO_REUSEADDR");
        }

        // 3. Bind the socket to an address and port
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET; // IPv4
        server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces (0.0.0.0)
        // Network byte order is big-endian. htons() converts our port to network format.
        server_addr.sin_port = htons(port); 

        // The POSIX API is written in C, so we need a pointer cast
        if (::bind(listen_socket_.get(), reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0) 
        {
            throw std::runtime_error("Failed to bind to port " + std::to_string(port));
        }

        // 4. Put the socket into listening mode
        // SOMAXCONN - maximum backlog size for pending clients (OS-dependent)
        if (::listen(listen_socket_.get(), SOMAXCONN) < 0) 
        {
            throw std::runtime_error("Failed to listen on socket");
        }
    }

    Socket TcpServer::accept_connection() 
    {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        // This call blocks until a client arrives
        int client_fd = ::accept(listen_socket_.get(), reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
        
        if (client_fd < 0) 
        {
            throw std::runtime_error("Failed to accept client connection");
        }

        // Return an RAII wrapper around the client socket
        return Socket(client_fd);
    }

} // namespace server