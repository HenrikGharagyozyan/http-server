#include "server/Socket.hpp"

#include <sys/socket.h>
#include <unistd.h> // For the close() function
#include <stdexcept>


namespace server 
{

    Socket::Socket(int fd) noexcept 
        : fd_(fd) 
    {
    }

    Socket::~Socket() 
    {
        close();
    }

    Socket::Socket(Socket&& other) noexcept 
        : fd_(std::exchange(other.fd_, -1)) 
    {
    }

    Socket& Socket::operator=(Socket&& other) noexcept 
    {
        if (this != &other) 
        {
            close(); // Close our current socket if one exists
            fd_ = std::exchange(other.fd_, -1); // Take ownership and reset the source
        }
        return *this;
    }

    void Socket::close() noexcept 
    {
        if (fd_ != -1) 
        {
            ::close(fd_); // Call the POSIX close system call
            fd_ = -1;     // Guard against double close
        }
    }

    std::string Socket::recv(size_t max_bytes) 
    {
        std::string buffer(max_bytes, '\0');
        
        // ::recv returns the number of bytes read (ssize_t is a signed integer)
        ssize_t bytes_received = ::recv(fd_, buffer.data(), max_bytes, 0);
        
        if (bytes_received < 0) 
        {
            throw std::runtime_error("Failed to receive data from socket");
        }
        
        if (bytes_received == 0) 
        {
            // If ::recv returned 0, the client closed the connection gracefully.
            return "";
        }
        
        // Trim the string to the actual number of bytes received
        buffer.resize(static_cast<size_t>(bytes_received));
        return buffer;
    }

    void Socket::send(std::span<const char> data) 
    {
        size_t total_sent = 0;
        
        // Ensure we send all bytes
        while (total_sent < data.size()) 
        {
            ssize_t sent = ::send(fd_, data.data() + total_sent, data.size() - total_sent, 0);
            
            if (sent < 0) 
            {
                throw std::runtime_error("Failed to send data to socket");
            }
            
            total_sent += static_cast<size_t>(sent);
        }
    }

} // namespace server