#include "server/Socket.hpp"

#include <sys/socket.h>
#include <unistd.h> // For the close() function
#include <sys/time.h> // For struct timeval
#include <cerrno>     // For errno, EAGAIN, EWOULDBLOCK
#include <stdexcept>
#include <system_error>


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

    void Socket::set_rcv_timeout(int seconds)
    {
        struct timeval timeout{};
        timeout.tv_sec = seconds;
        timeout.tv_usec = 0;

        // Set the SO_RCVTIMEO option on the socket
        if (::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) 
        {
            throw std::system_error(errno, std::generic_category(), "Failed to set SO_RCVTIMEO");
        }
    }

    void Socket::set_snd_timeout(int seconds)
    {
        struct timeval timeout{};
        timeout.tv_sec = seconds;
        timeout.tv_usec = 0;

        if (::setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) 
        {
            throw std::system_error(errno, std::generic_category(), "Failed to set SO_SNDTIMEO");
        }
    }

    std::string Socket::recv(size_t max_bytes) 
    {
        std::string buffer(max_bytes, '\0');
        
        // ::recv returns the number of bytes read (ssize_t is a signed integer)
        ssize_t bytes_received = ::recv(fd_, buffer.data(), max_bytes, 0);
        
        if (bytes_received < 0) 
        {
            // If a timeout occurs, recv returns -1 and errno is EAGAIN or EWOULDBLOCK.
            // We treat this as a normal end of waiting (no data received from the client).
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNRESET) 
            {
                return ""; 
            }
            throw std::system_error(errno, std::generic_category(), "Failed to receive data");
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
            // Use MSG_NOSIGNAL so Linux does not raise SIGPIPE on connection reset
            ssize_t sent = ::send(fd_, data.data() + total_sent, data.size() - total_sent, MSG_NOSIGNAL);
            
            if (sent < 0) 
            {
                throw std::system_error(errno, std::generic_category(), "Failed to send data");
            }
            
            total_sent += static_cast<size_t>(sent);
        }
    }

} // namespace server