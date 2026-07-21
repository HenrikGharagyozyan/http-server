#pragma once

#include <utility>

namespace server 
{

    class Socket 
    {
    public:
        // By default create an "empty" invalid socket
        Socket() noexcept = default;
        
        // Constructor that takes ownership of a raw descriptor
        explicit Socket(int fd) noexcept;
        
        // Destructor closes the socket
        ~Socket();

        // Copy is strictly prohibited (copy deleted)
        Socket(const Socket&) = delete;
        Socket& operator=(const Socket&) = delete;

        // Move is allowed (move added)
        Socket(Socket&& other) noexcept;
        Socket& operator=(Socket&& other) noexcept;

        [[nodiscard]] int get() const noexcept { return fd_; }
        [[nodiscard]] bool is_valid() const noexcept { return fd_ != -1; }

        // Manual close
        void close() noexcept;

    private:
        int fd_{-1}; // -1 on Linux means an invalid descriptor
    };

} // namespace server