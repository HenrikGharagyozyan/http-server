#include "server/Socket.hpp"

#include <unistd.h> // For the close() function

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

} // namespace server