#pragma once

#include <utility>
#include <string>
#include <span>


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

        // Sets a timeout for reading and sending (to prevent the thread from hanging forever)
        void set_rcv_timeout(int seconds);
        void set_snd_timeout(int seconds);

        [[nodiscard]] std::string recv(size_t max_bytes = 4096);
        // Новая zero-alloc версия чтения
        size_t recv(std::span<char> buffer);
        
        void send(std::span<const char> data);

    private:
        int fd_{-1}; // -1 on Linux means an invalid descriptor
    };

} // namespace server